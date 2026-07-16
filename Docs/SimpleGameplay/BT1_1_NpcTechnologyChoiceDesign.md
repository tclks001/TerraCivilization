# BT1.1 NPC 自主选择科技设计稿

日期：2026-07-15  
前置设计：[BT1_NpcFactionBehaviorTreeMinimalLoopDesign.md](BT1_NpcFactionBehaviorTreeMinimalLoopDesign.md)  
相关玩法设计：[T0TechnologyProgressionFrameworkDesign.md](T0TechnologyProgressionFrameworkDesign.md)

## 0. 本阶段目标

BT1.1 在 BT1 的最小闭环上增加一个能力：**当非玩家阵营进入待选科技状态时，由该阵营的 NPC 行为树自主选择一个合法科技，不再阻塞等待玩家点击 UI。**

本阶段不实现真实科技效果，也不做策略偏好。NPC 只需要从 Gameplay 当前给出的 `PendingTechnologyChoices` 中选择一个即可。为了方便复现和调试，C++ 默认选择枚举值最小的候选科技。

验收闭环：

```text
NPC 阵营执行一次合法行动
  -> Gameplay 结算科技积分
  -> 如果达到科技门槛，当前阵营保持为该 NPC，进入 bWaitingForTechnologyChoice
  -> 行为树优先进入 Choose Pending Technology 分支
  -> NPC 选择一个 PendingTechnologyChoices 中的科技
  -> Gameplay 清除等待状态并继续推进回合
  -> NPC Brain 回到等待或下一个 NPC Brain 接管
```

## 1. 当前科技实现摘要

科技权威状态在 `FTerraGameplayContainer` 中，按 `FactionId` 保存：

```cpp
struct FTerraGameplayFactionTechnologyState
{
    int32 FactionId;
    int32 AccumulatedScore;
    int32 NextUnlockScore;
    int32 UnlockCount;
    bool bWaitingForTechnologyChoice;
    TArray<ETerraGameplayTechnologyId> OwnedTechnologies;
    TArray<ETerraGameplayTechnologyId> PendingTechnologyChoices;
};
```

当前占位科技枚举：

```cpp
None
PlaceholderTraining
PlaceholderLogistics
PlaceholderDoctrine
```

相关 Gameplay API：

```cpp
bool IsFactionWaitingForTechnologyChoice(int32 FactionId) const;
bool GetPendingTechnologyChoices(int32 FactionId, TArray<ETerraGameplayTechnologyId>& OutChoices) const;
bool ChoosePendingTechnology(int32 FactionId, ETerraGameplayTechnologyId TechnologyId, FString& OutError);
```

回合结束时，Gameplay 会调用 `FinalizeFactionTurnTechnologyProgress_`。若该阵营达到科技门槛，会生成 `PendingTechnologyChoices` 并让 `bWaitingForTechnologyChoice=true`，此时不会推进到下一阵营。只有 `ChoosePendingTechnology` 成功后，Gameplay 才会清空候选、写入已拥有科技，并调用后续回合推进逻辑。

## 2. 权责边界

| 层级 | BT1.1 职责 | 不做 |
| --- | --- | --- |
| `FTerraGameplayContainer` | 判断是否待选、提供候选、校验并提交选择、推进回合 | 区分玩家 UI 或 NPC 策略 |
| `UPlanetGameplayComponent` | 暴露 `ChoosePendingTechnology`，广播科技状态变化 | 替 NPC 挑选科技 |
| `UTerraNpcBehaviorTreeSubsystem` | 为 BT 提供受控查询和选择入口，检查当前阵营与回合激活门闩 | 直接改写科技状态 |
| 行为树 | 在 NPC 自己的回合且需要科技时，先选择科技 | 在科技未选完时继续移动棋子 |
| UI | 当前仍会响应 `OnTechnologyChoiceRequested` 打开选择界面 | 本阶段不新增玩家阵营识别逻辑 |

注意：BT1.1 的目标是先打通 NPC 自主选择。UI 目前仍可能在 NPC 触发科技时短暂打开科技选择界面；NPC 行为树下一次 Tick 选择成功后，Gameplay 会推进状态。后续建议新增“玩家主控阵营”判断，让 UI 只响应玩家阵营的科技选择请求。

## 3. C++ 交付物

新增任务节点：

```text
UBTTask_TerraChoosePendingTechnology
  NodeName = "Choose Pending Technology"
```

扩展 `FTerraNpcBehaviorTreeTurnContext`：

```cpp
bool bNeedsTechnologyChoice = false;
```

扩展 `Update Terra Turn Context` Service 写入 Blackboard：

```text
bNeedsTechnologyChoice = 当前 CurrentFactionId 是否处于 bWaitingForTechnologyChoice
```

扩展 `UTerraNpcBehaviorTreeSubsystem`：

```cpp
bool QueryPendingTechnologyChoices(int32 FactionId, TArray<ETerraGameplayTechnologyId>& OutChoices, FString& OutError);
bool ChoosePendingTechnology(int32 FactionId, ETerraGameplayTechnologyId TechnologyId, FString& OutError);
```

Subsystem 会检查：

- Gameplay 可用且已初始化。
- 对局未结束。
- `UPlanetGameplayComponent::IsTurnActivationReady()` 为 true。
- 请求的 `FactionId` 是 Gameplay 当前阵营。
- 当前阵营确实有待选科技。

这保证 NPC 科技选择和 BT1 行动一样，会等待上一回合表现动画的回合激活门闩打开后再执行。

## 4. 行为树结构

BT1.1 在原 `BT1 Turn Dispatcher` 下插入一个更高优先级分支：

```text
Root
└─ Selector: BT1.1 Turn Dispatcher [Service: Update Terra Turn Context]
   ├─ Sequence: Choose NPC Technology
   │  ├─ Decorator: bGameplayReady is true
   │  ├─ Decorator: bIsMyTurn is true
   │  ├─ Decorator: bNeedsTechnologyChoice is true
   │  └─ Task: Choose Pending Technology
   ├─ Sequence: My NPC Turn
   │  ├─ Decorator: bGameplayReady is true
   │  ├─ Decorator: bIsMyTurn is true
   │  ├─ Decorator: bNeedsTechnologyChoice is false
   │  ├─ Task: Select Deterministic Action
   │  └─ Task: Execute Terra Validated Action
   └─ Task: Wait (0.25 s)
```

`Choose NPC Technology` 必须放在 `My NPC Turn` 前面。Selector 会从左到右尝试分支；科技待选时应先清掉科技阻塞，再进入普通行动。

`My NPC Turn` 建议新增 `bNeedsTechnologyChoice is false` Decorator。这样即使服务刷新和任务执行落在同一帧，普通行动也不会在待选科技期间调用 `CollectCurrentFactionLegalActions`。

## 5. Blackboard 配置变更

在 BT1 的 `BB_TerraFactionNpc_BT1` 上新增一个 Bool Key：

| Key Name | Key Type | 初始值 | 写入者 | 用途 |
| --- | --- | --- | --- | --- |
| `bNeedsTechnologyChoice` | Bool | `false` | `Update Terra Turn Context` | 当前 Gameplay 阵营是否正在等待科技选择 |

其他 Key 保持 BT1 不变。

## 6. UE 编辑器升级步骤

以下步骤假设你已经完成 BT1，并已有：

```text
Content/AI/NPC/BT1/BB_TerraFactionNpc_BT1
Content/AI/NPC/BT1/BT_TerraFactionNpc_BT1
Content/AI/NPC/BT1/BP_TerraFactionBrain_BT1
```

### 6.1 编译 C++ 并重开编辑器

1. 关闭 Unreal Editor，避免旧 DLL 占用。
2. 在项目根目录执行：

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' TerraCivilizationEditor Win64 Development -Project='C:\workspace\TerraCivilization\TerraCivilization.uproject' -WaitMutex -NoHotReloadFromIDE
```

3. 看到 `Result: Succeeded` 后重新打开项目。

### 6.2 给 Blackboard 增加 Key

1. 打开 `Content/AI/NPC/BT1/BB_TerraFactionNpc_BT1`。
2. 在左侧或上方的 `Keys` 面板点击 `New Key`。
3. 选择 `Bool`。
4. 将 Key Name 改成：

```text
bNeedsTechnologyChoice
```

5. 默认值保持 `false`。
6. 保存 Blackboard。

检查点：Key 名必须完全一致，首字母小写 `b`，中间没有空格。

### 6.3 插入科技选择分支

1. 打开 `BT_TerraFactionNpc_BT1`。
2. 找到 Root 下挂着 `Update Terra Turn Context` Service 的 Selector。
3. 可以将 Selector 重命名为 `BT1.1 Turn Dispatcher`，便于调试器识别。
4. 在 Selector 下新增一个 `Sequence`，重命名为：

```text
Choose NPC Technology
```

5. 将 `Choose NPC Technology` 拖到 `My NPC Turn` 左侧，让它成为 Selector 的第一个子分支。
6. 选中 `Choose NPC Technology` 节点，在 Details 面板添加三个 Blackboard Decorator：
   - `bGameplayReady`：`Is True`
   - `bIsMyTurn`：`Is True`
   - `bNeedsTechnologyChoice`：`Is True`
7. 在 `Choose NPC Technology` 下添加 Task，搜索并选择：

```text
Choose Pending Technology
```

如果搜索不到 `Choose Pending Technology`，说明新的 C++ 类尚未被编辑器加载。确认全量编译成功后关闭并重开 UE Editor。

### 6.4 防止普通行动抢跑

1. 选中原来的 `My NPC Turn` Sequence。
2. 保留已有的两个 Decorator：
   - `bGameplayReady`：`Is True`
   - `bIsMyTurn`：`Is True`
3. 新增第三个 Blackboard Decorator：

```text
bNeedsTechnologyChoice is false
```

设置方式：

- Key 选择 `bNeedsTechnologyChoice`。
- Operation 选择 `Is Not Set` 或 `Is False`，取决于 UE 5.8 当前中文界面的翻译。
- 最终含义必须是：只有 `bNeedsTechnologyChoice=false` 时才允许进入 `My NPC Turn`。

4. 保存 Behavior Tree。

### 6.5 目标结构检查

保存后，行为树应类似：

```text
Root
└─ Selector: BT1.1 Turn Dispatcher
   Service: Update Terra Turn Context
   ├─ Sequence: Choose NPC Technology
   │  Decorators: bGameplayReady true, bIsMyTurn true, bNeedsTechnologyChoice true
   │  └─ Task: Choose Pending Technology
   ├─ Sequence: My NPC Turn
   │  Decorators: bGameplayReady true, bIsMyTurn true, bNeedsTechnologyChoice false
   │  ├─ Task: Select Deterministic Action
   │  └─ Task: Execute Terra Validated Action
   └─ Task: Wait
```

## 7. 验收建议

为了更快触发科技选择，可临时在 `UPlanetGameplayComponent` 的 `T0TechnologyProgressionConfig` 中把门槛调低：

```text
FirstTechnologyUnlockScore = 1
MaxTechnologyCount = 1
```

验收步骤：

1. 玩家结束回合，让某个 NPC 阵营行动。
2. NPC 行动后触发科技积分达标。
3. Behavior Tree 调试器中看到 `bNeedsTechnologyChoice=true`。
4. `Choose NPC Technology` 分支执行。
5. Output Log 出现：

```text
LogTerraNpcBT: TechnologyChoice=Succeeded Faction=... Technology=...
LogTerraGameplay: [Gameplay][T0] TechnologyChosen Faction=...
```

6. 该阵营的 `bWaitingForTechnologyChoice` 清除，回合继续推进。
7. 玩家阵营若触发科技选择，仍按现有 UI 由玩家点击。

## 8. 已知限制与后续建议

- BT1.1 只选择第一个合法科技，不理解科技效果。
- UI 当前仍可能响应 NPC 的 `OnTechnologyChoiceRequested`。后续应引入玩家主控阵营配置，让 `UTerraUISubsystem` 只为玩家阵营打开科技选择界面。
- 若未来科技有真实效果，BT2 可把选择策略替换为“按阵营风格和当前局面打分”，但仍应调用 `ChoosePendingTechnology`，不直接写 Gameplay 状态。
- 若未来外部 LLM 参与科技选择，建议让 LLM 只返回候选 ID 和理由，BT 负责超时、回退和最终提交，Gameplay 保持唯一权威。
