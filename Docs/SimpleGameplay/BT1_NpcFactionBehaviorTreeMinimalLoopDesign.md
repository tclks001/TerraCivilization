# BT1 阵营级 NPC 行为树最小闭环设计稿

日期：2026-07-15  
前置设计：[NpcFactionBehaviorTreeDesign.md](NpcFactionBehaviorTreeDesign.md)  
前置玩法接口：[A3ExternalLlmAgentNpcExecutionLoopDesign.md](A3ExternalLlmAgentNpcExecutionLoopDesign.md)

## 0. 本阶段目标

BT1 只验收一个可靠闭环：**一个非玩家阵营在自己的回合，由 UE 行为树选择并执行一次合法行动，然后正常推进回合。**

```text
玩家阵营回合
  -> 玩家按现有方式操作并结束回合
  -> 当前阵营切换为 NPC 阵营
  -> NPC 阵营 Brain 的行为树识别到自己的回合
  -> 从 Gameplay 返回的合法行动中稳定选择一项
  -> Gameplay 二次校验并执行
  -> 当前阵营切换到下一阵营
  -> Brain 回到等待
```

本阶段不接 LLM、不调用 MCP、不做风险评分、不做寻路，也不让每个棋子拥有一个 AI。一个 `ATerraNpcBrainPawn` 只代表一个阵营。

## 1. 权责边界

| 层级 | BT1 的职责 | 明确不做 |
| --- | --- | --- |
| `Gameplay` | 合法行动枚举、回合快照检查、最终执行、棋盘与高亮刷新 | 决定策略风格 |
| `NpcBehaviorTree` | 检测该阵营回合、选择候选、请求已验证执行、记录日志 | 改写棋盘规则、直改状态 |
| UE 编辑器资产 | 把 Blackboard、BT 和阵营 Brain 实例连成可调试流程 | 存储行动数组或玩法规则 |
| `NpcMcp` / 外部 Agent | BT1 不参与 | 不得同时执行同一个 NPC 回合 |

BT1 的执行路径是：

```text
UBTTask_TerraSelectDeterministicAction
  -> UTerraNpcBehaviorTreeSubsystem
  -> UPlanetGameplayComponent
  -> FTerraGameplayContainer::CollectCurrentFactionLegalActions

UBTTask_TerraExecuteValidatedAction
  -> UTerraNpcBehaviorTreeSubsystem
  -> UPlanetGameplayComponent::TryExecuteNpcValidatedAction
  -> FTerraGameplayContainer::TryExecuteValidatedAction
```

行为树不经过本机 MCP HTTP 服务；MCP 在 BT3/BT4 才作为外部观察或提案边界接入。

## 2. C++ 交付物

新增 Runtime 模块：`NpcBehaviorTree`。

```text
Source/NpcBehaviorTree
  NpcBehaviorTree.Build.cs
  Public/NpcBehaviorTree.h
  Public/TerraNpcBehaviorTreeSubsystem.h
  Public/TerraNpcBrainPawn.h
  Public/TerraNpcAIController.h
  Public/BTService_TerraUpdateTurnContext.h
  Public/BTTask_TerraSelectDeterministicAction.h
  Public/BTTask_TerraExecuteValidatedAction.h
  Private/...
```

模块运行时依赖 `AIModule`、`GameplayTasks` 与 `Gameplay`。它没有 `NavigationSystem` 依赖，因为本项目当前执行的是 CellId 棋盘行动，而不是 Pawn 在三维场景中寻路。

### 2.1 阵营 Brain

`ATerraNpcBrainPawn` 是不可见、无碰撞意义的协调 Pawn：

- `ControlledFactionId`：此实例控制的阵营 ID。
- `BehaviorTreeAsset`：本实例应运行的 `BT_TerraFactionNpc`。
- `AutoPossessAI=PlacedInWorldOrSpawned`：放入地图或运行时生成后自动拥有 AIController。

`ATerraNpcAIController` 在 Possess 时运行 Pawn 上指定的行为树。这样每个放入地图的 Brain 实例可独立指定阵营 ID 和行为树，不需要为每个阵营单独写 C++ 类。

### 2.2 Gameplay Bridge

`UTerraNpcBehaviorTreeSubsystem` 是 BT 模块内的 World Subsystem。它在当前 World 找到第一个 `APlanetTessellatedMesh` 的 `UPlanetGameplayComponent`，并只暴露三类受控操作：

- 查询当前阵营、回合号、对局结束状态。
- 获取当前阵营合法行动。
- 执行带 `ExpectedTurnIndex + ExpectedFactionId` 快照检查的行动。

BT1 的地图前提是一个 World 内仅有一个有效的 `APlanetTessellatedMesh`。如果以后需要多棋盘/多对局并存，BT2 之前应把这个“找第一个”规则替换为显式 BoardId 或 Gameplay Provider 绑定。

### 2.3 确定性选择规则

`UBTTask_TerraSelectDeterministicAction` 只对 `CollectCurrentFactionLegalActions` 的结果排序，不自行计算规则：

1. 吃子数量多的优先。
2. 吃子数量相同，跳跃行动优先。
3. 仍相同，`PieceId` 小的优先。
4. 仍相同，`ToCellId` 小的优先。

这套规则不是最终 AI 设计；它的价值是稳定、可复现，适合作为 BT1 的验收基线。

## 3. Blackboard 精确配置

在 UE 编辑器创建 Blackboard 时，键名、类型和大小写必须与下表一致。C++ 节点按这些固定键名读写。

| Key Name | Key Type | 初始值 | 写入者 | 用途 |
| --- | --- | --- | --- | --- |
| `ControlledFactionId` | Int | `-1` | `Update Terra Turn Context` | 当前 Brain 的阵营 ID |
| `CurrentFactionId` | Int | `-1` | `Update Terra Turn Context` | Gameplay 当前回合阵营 |
| `TurnIndex` | Int | `-1` | `Update Terra Turn Context` | 当前回合号 |
| `bGameplayReady` | Bool | `false` | `Update Terra Turn Context` | Gameplay 是否已完成初始化 |
| `bMatchEnded` | Bool | `false` | `Update Terra Turn Context` | 对局是否已结束 |
| `bIsMyTurn` | Bool | `false` | `Update Terra Turn Context` | 是否轮到 `ControlledFactionId`，且 Gameplay 的 `bTurnActivationReady` 已打开 |
| `SelectedPieceId` | Int | `-1` | `Select Deterministic Action` | 将执行的棋子 ID |
| `SelectedToCellId` | Int | `-1` | `Select Deterministic Action` | 将执行的目标 Cell ID |
| `DecisionSource` | Int | `-1` | `Select Deterministic Action` | BT1 固定写 `0`，表示 Deterministic |
| `LastError` | String | 空 | Service/Task | 只用于调试失败原因 |

不要把合法行动 `TArray`、吃子详情或完整 Gameplay Snapshot 放进 Blackboard。BT1 只需要用 Blackboard 展示和传递轻量 ID；复杂数据在 Task 的即时调用中使用。

## 4. UE 编辑器配置步骤

本节是完成 C++ 编译后必须手工做的资产配置。`.uasset` 是二进制资产，本阶段不由 C++ 自动创建。

### 4.1 编译并重新打开编辑器

1. 关闭本项目相关的 Unreal Editor 进程，避免链接 DLL 被占用。
2. 在项目根目录执行：

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' TerraCivilizationEditor Win64 Development -Project='C:\workspace\TerraCivilization\TerraCivilization.uproject' -WaitMutex -NoHotReloadFromIDE
```

3. 确认输出有 `Result: Succeeded`，再打开 `TerraCivilization.uproject`。
4. 若编辑器提示新模块或 C++ 类需要重新编译，选择重新编译；不要用 Live Coding 替代首次模块加载验证。

### 4.2 创建内容目录

在 Content Browser 中创建：

```text
Content/AI/NPC/BT1
```

本稿以下示例统一使用该目录。不要把这些资产放到 `NpcMcp` 或编辑器 Toolset 的目录中。

### 4.3 创建 Blackboard

1. 在 `Content/AI/NPC/BT1` 空白处右键。
2. 选择 `Artificial Intelligence > Blackboard`。
3. 命名为 `BB_TerraFactionNpc_BT1`。
4. 双击打开该 Blackboard。
5. 在 `Keys` 面板逐项点 `New Key`，完全按第 3 节的 Key Name、Key Type 和初始值创建 10 项。
6. 保存资产。

检查点：`bIsMyTurn` 必须是 Bool，`LastError` 必须是 String；键名不能加前缀、空格或改大小写。

### 4.4 创建 Behavior Tree

1. 在同目录右键，选择 `Artificial Intelligence > Behavior Tree`。
2. 命名为 `BT_TerraFactionNpc_BT1`。
3. 双击打开它，在右侧 Details 的 `Blackboard Asset` 选择 `BB_TerraFactionNpc_BT1`。
4. **不要点击 Root 节点的 `New Service`。** 该按钮会创建一个新的 Blueprint Service 资产，因此 UE 会弹出“另存为”；BT1 不需要创建这个资产，按 `Cancel` 取消即可。
5. Root 下创建一个 `Selector`，可重命名为 `BT1 Turn Dispatcher`。
6. 在画布中央的 `BT1 Turn Dispatcher` 的**深色 Selector 节点主体上右键**，不要在 Root、空白画布或顶部工具栏上右键。
7. 在弹出的节点菜单中选择 `Add Service...`（中文界面通常显示为“添加服务…”）。这会打开一个仅用于选择已有 BT Service 类的节点选择器，**不会**要求另存为资产。
8. 在该节点选择器顶部的搜索框输入 `Update Terra Turn Context`。选择原生 C++ 节点 `Update Terra Turn Context`；它随后会以绿色小 Service 条目附着在 Selector 上。
9. 若右键菜单没有 `Add Service...`，确认右键的是 `Selector` 节点主体而不是 Root；Root 不应作为 BT1 的 Service 宿主。
10. 若 `Add Service...` 的选择器里搜索不到该节点，先确认 C++ 全量编译成功、关闭并重开 UE Editor；不要改用 `New Service` 代替。重新打开后仍找不到时，在 Output Log 搜索 `NpcBehaviorTree` 和 `LogTerraNpcBT`，确认 Runtime 模块已加载。
11. 在 Selector 下创建一个 `Sequence`，可重命名为 `My NPC Turn`。
12. 选中 `My NPC Turn`，添加两个 Blackboard Decorator：
   - Key 选 `bGameplayReady`，Operation 选 `Is True`。
   - Key 选 `bIsMyTurn`，Operation 选 `Is True`。
13. 在 `My NPC Turn` 下按顺序添加两个 Task：
   - `Select Deterministic Action`
   - `Execute Terra Validated Action`
14. 在 `BT1 Turn Dispatcher` 的第二个子节点添加内置 Task `Wait`，将 `Wait Time` 设为 `0.25` 秒，Random Deviation 设为 `0`。
15. 点击 Save 保存 `BT_TerraFactionNpc_BT1` 本身。此过程不应额外产生任何 `BTService_...` Blueprint 资产。

目标结构：

```text
Root
└─ Selector: BT1 Turn Dispatcher [Service: Update Terra Turn Context]
   ├─ Sequence: My NPC Turn
   │  ├─ Decorator: bGameplayReady is true
   │  ├─ Decorator: bIsMyTurn is true
   │  ├─ Task: Select Deterministic Action
   │  └─ Task: Execute Terra Validated Action
   └─ Task: Wait (0.25 s)
```

不要在 `My NPC Turn` 下添加 `Wait`。行动执行成功后，Gameplay 会推进回合；挂在 Selector 上的 Service 会把 `bIsMyTurn` 刷新为 false，Selector 自然进入等待分支。

### 4.4.1 “另存为”弹窗的处理

如果点击 Service 相关按钮后弹出资产保存对话框，先看弹窗建议的资产类型：

| 弹窗内容 | 它表示什么 | BT1 应怎么做 |
| --- | --- | --- |
| `BTService_BlueprintBase` / New Service | 正在创建一个新的 Blueprint Service | 点击 `Cancel`；在 Selector 上右键，使用 `Add Service...` 搜索并添加 `Update Terra Turn Context` |
| `BT_TerraFactionNpc_BT1` 未保存 | 正在保存你已经创建的行为树本体 | 正常保存到 `Content/AI/NPC/BT1` |
| Blackboard 未保存 | 正在保存 `BB_TerraFactionNpc_BT1` | 正常保存到 `Content/AI/NPC/BT1` |

BT1 使用的是 `UBTService_TerraUpdateTurnContext` 原生 C++ 类。它不需要、也不应先创建一个 `BTService_BlueprintBase` 子类；新建 Blueprint Service 只在以后需要无 C++ 的自定义服务逻辑时才有用。

### 4.4.2 回合激活门闩

当上一回合仍在播放移动、攻击或吃子表现时，Gameplay 已经可以在规则层切换 `CurrentFactionId`，但 `bTurnActivationReady=false`。此时本 Service 会保持 `bIsMyTurn=false`，所以对应 NPC Brain 只能走 `Wait`，不会抢在动画结束前行动。门闩打开后，Service 的下一次 0.25 秒刷新才会允许该阵营进入 `My NPC Turn`。

### 4.5 创建阵营 Brain Blueprint

1. 在 Content Browser 中点击 `Add > Blueprint Class`。
2. 在父类选择窗口选择 `All Classes`，搜索 `TerraNpcBrainPawn`。
3. 创建并命名为 `BP_TerraFactionBrain_BT1`，放到 `Content/AI/NPC/BT1`。
4. 打开 Blueprint，在 Class Defaults 中确认：
   - `AI Controller Class` 为 `TerraNpcAIController`。
   - `Auto Possess AI` 为 `Placed in World or Spawned`。
   - `Behavior Tree Asset` 为 `BT_TerraFactionNpc_BT1`。
   - 不要添加 Mesh、碰撞、移动组件或 NavMesh 配置。
5. 编译并保存 Blueprint。

`ControlledFactionId` 是实例级设置，因此不要在这个共享 Blueprint 默认值里把它当成多阵营配置来源；每个放进地图的 Brain 实例单独设置。

### 4.6 在关卡中放置一个 NPC 阵营

1. 打开包含唯一 `APlanetTessellatedMesh` 的测试地图。
2. 从 Content Browser 将 `BP_TerraFactionBrain_BT1` 拖到关卡空白位置。它会在游戏中隐藏，位置不影响棋盘行动。
3. 在 World Outliner 把实例改名为 `NPCBrain_Faction1_BT1`。
4. 在 Details 中设置 `Controlled Faction Id` 为一个**不是玩家控制**的阵营 ID，例如 `1`。
5. 保存地图。

BT1 只放置一个 Brain，先验证一个 NPC 阵营。多个阵营时复制该实例，分别设置不同的 `ControlledFactionId`；绝不能让两个 Brain 使用相同阵营 ID。

### 4.7 禁用其他回合执行者

BT1 验收时，同一 `(TurnIndex, FactionId)` 必须只有行为树执行。请确保：

- 外部 A3 deterministic Agent 没有运行，或不会执行该阵营。
- LLM Agent 不会调用 `terra.execute_validated_action`。
- Tutorial 的脚本化 NPC 行动序列不覆盖同一阵营回合。
- 不在 NPC 回合手动通过 MCP Inspector 执行 action。

这些路径可以保留在工程中，但不能与 BT1 同时对同一阵营拥有写入权。

## 5. 行为树调试与验收

### 5.1 首次 PIE 验收

1. 启动 PIE。
2. 正常完成玩家阵营的一次行动，使回合切到配置的 NPC 阵营。
3. 打开 `BT_TerraFactionNpc_BT1`。
4. 在 Behavior Tree 编辑器顶部 Debug 下拉框中选择 `NPCBrain_Faction1_BT1` 对应的 AIController。
5. 确认运行高亮进入 `My NPC Turn`，随后经过两个自定义 Task。
6. 确认棋子移动/吃子、高亮刷新和回合推进与原 Gameplay 路径一致。
7. 回合离开该阵营后，确认树停在 `Wait`，没有再次行动。

### 5.2 日志验收

在 Output Log 过滤：

```text
LogTerraNpcBT
```

一次成功行动至少应看到等价记录：

```text
Decision=Deterministic Piece=31 To=428 Captures=1 Jump=0
Execute=Succeeded Faction=1 Turn=10 Piece=31 To=428 TurnAfter=11
```

其中具体 ID 依地图不同而变化。关键是：选择日志的 Piece/To 与成功执行日志一致，并且 `TurnAfter` 大于执行前的回合号。

### 5.3 必须通过的验收清单

- [ ] Editor 编译结果为 `Result: Succeeded`。
- [ ] `BB_TerraFactionNpc_BT1` 有第 3 节的全部键，类型和名字完全一致。
- [ ] `BT_TerraFactionNpc_BT1` 的 `BT1 Turn Dispatcher` Selector 挂载 `Update Terra Turn Context` Service。
- [ ] `My NPC Turn` 同时要求 `bGameplayReady=true`、`bIsMyTurn=true`。
- [ ] 玩家阵营回合不会触发 NPC 执行。
- [ ] 进入目标 NPC 阵营回合后，只执行一次合法行动。
- [ ] `TryExecuteValidatedAction` 成功后回合推进。
- [ ] 对局结束后没有新的 NPC 执行日志。
- [ ] 断开/不启动 MCP 与 LLM Agent 时，BT1 仍可完整工作。

## 6. 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 自定义 Service/Task 在菜单里找不到 | 新模块未成功编译或 Editor 没重启加载模块 | 先完成全量 Editor Build，关闭后重新打开 Editor |
| Brain 显示“has no BehaviorTreeAsset” | Pawn 实例/BP 没指定行为树 | 在 `BP_TerraFactionBrain_BT1` Class Defaults 设置 `Behavior Tree Asset` |
| 行为树永远在 Wait | Blackboard 键名/类型不符，或 Gameplay 尚未初始化 | 逐项核对第 3 节；在 Debug 面板查看 `bGameplayReady`、`CurrentFactionId` |
| NPC 在错误回合行动 | `ControlledFactionId` 设置错误，或两个 Brain 使用相同 ID | 核对关卡实例的 Faction ID；每个阵营只保留一个 Brain |
| 执行被拒绝 | 回合已变化、手动 UI 交互仍在进行、候选失效 | 查看 `LastError` 和 `LogTerraNpcBT`；不要让外部 Agent/MCP 同时执行 |
| 看不到 Brain | 这是正常行为 | Brain 在游戏中默认隐藏，不代表棋盘棋子 |
| 找不到 Gameplay | 当前 World 没有有效 `APlanetTessellatedMesh`，或有多个棋盘 | BT1 测试地图只保留一个有效棋盘 Actor |

## 7. BT1 完成后的边界

BT1 验收通过只说明“行为树可以可靠地编排一个合法 NPC 回合”。它不说明 NPC 已经聪明。

后续阶段应严格按总稿推进：

- BT2：在同一权威接口上增加风险、策略档案和确定性分支。
- BT3：让 MCP 对齐观察 BT 状态和 decision log，仍不让 MCP 反向驱动 BT。
- BT4：让外部 LLM Agent 提交关联提案，BT 负责超时、验证和确定性回退。
