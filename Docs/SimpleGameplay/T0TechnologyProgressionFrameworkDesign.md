# TerraCivilization SimpleGameplay T0 局内科技成长框架设计稿

> T0 落地局内科技成长的最小 Gameplay 闭环：阵营回合得分、累计门槛、确定性三选一、待选状态、外部选择接口与回合结算阻塞。T0 的三个科技均为无效果占位科技，用于稳定验证成长流程；实际规则效果从 T1 起逐项接入。
>
> 本稿不实现科技 UI。UI 只读取本稿规定的 Gameplay 查询结果并提交玩家选择，不保存科技真相状态，也不自行推进回合。

---

## 1. 范围

### 1.1 T0 实现

- 每个 `FactionId` 保存独立成长状态。
- 在行动确认、捕获、装备掉落、升变、阵营失败和胜负判定完成后，结算行动阵营本回合分数。
- 计入既有的捕获、升变和击败阵营得分事件。
- 累计分数达到门槛后，生成一次固定的三个不同无效果候选科技。
- 当前阵营有待选科技时，阻止棋盘继续交互和下一回合推进。
- 提供可复现随机种子、查询接口和选择接口。
- 选择完成后，若没有下一份待选科技，才推进至下一阵营。

### 1.2 T0 不实现

- 任意会改变移动、跳跃、捕获、地形或棋子状态的真实科技效果。
- 占领地形得分、科技连锁产生新得分、科技互斥/前置条件、传送门与地形改造。
- UI、动画、AI、LLM Agent 和网络同步。

---

## 2. 成长状态与默认数值

`FTerraGameplayFactionTechnologyState` 由 `FTerraGameplayContainer` 按 `FactionId` 存储，至少包含：

```cpp
int32 AccumulatedScore;
int32 ScoreEarnedThisTurn;
int32 NextUnlockScore;
int32 UnlockCount;
bool bWaitingForTechnologyChoice;
TArray<ETerraGameplayTechnologyId> OwnedTechnologies;
TArray<ETerraGameplayTechnologyId> PendingTechnologyChoices;
```

T0 默认配置：

| 参数 | 默认值 | 说明 |
| --- | ---: | --- |
| `bEnableTechnologyProgression` | `true` | 总开关。 |
| `TechnologyRandomSeed` | `0` | 候选随机种子。 |
| `FirstTechnologyUnlockScore` | `100` | 首次解锁门槛。 |
| `TechnologyUnlockScoreIncrement` | `160` | 后续档位增量。 |
| `MaxTechnologyCount` | `1` | T0 只允许选择一次占位科技。 |
| `ScoreForInfantryCapture` | `10` | 捕获步兵。 |
| `ScoreForEliteCapture` | `15` | 捕获弓兵或骑兵。 |
| `ScoreForArcherCavalryCapture` | `25` | 捕获弓骑兵。 |
| `ScoreForPromotion` | `10` | 一次升变。 |
| `ScoreForFactionDefeat` | `50` | 击败阵营。 |

候选固定为 `PlaceholderTraining`、`PlaceholderLogistics` 与 `PlaceholderDoctrine`。它们只记录为已拥有科技，不产生任何规则修正。

---

## 3. 回合结算

```text
玩家确认结束回合
  -> ResolvePendingCaptures
  -> G12 掉落与升变相关结算已完成
  -> G11 阵营失败与胜负判定完成
  -> 将 ScoreEarnedThisTurn 累加到 AccumulatedScore
  -> 未结束对局且达到门槛：写入三个 PendingTechnologyChoices，保持当前阵营
  -> UI 选择候选之一
  -> 将科技写入 OwnedTechnologies，清空待选候选
  -> 推进下一回合
```

T0 的占位科技不会产生新的得分或新的候选；后续 T1+ 科技若能触发新得分，选择后必须重复“得分结算 -> 候选生成 -> 选择”流程，直至没有待选科技。

---

## 4. UI 对接接口

UI 使用 Gameplay Container 的以下接口，不应直接访问状态容器：

```cpp
bool IsFactionWaitingForTechnologyChoice(int32 FactionId) const;
bool GetFactionTechnologyState(int32 FactionId, FTerraGameplayFactionTechnologyState& OutState) const;
bool GetPendingTechnologyChoices(int32 FactionId, TArray<ETerraGameplayTechnologyId>& OutChoices) const;
bool ChoosePendingTechnology(int32 FactionId, ETerraGameplayTechnologyId TechnologyId, FString& OutError);
```

### 4.1 UI 轮询语义

- 在玩家确认行动后，UI 读取 `IsFactionWaitingForTechnologyChoice(CurrentFactionId)`。
- 返回 `true` 时，读取 `GetPendingTechnologyChoices`，显示返回的三个枚举值。
- UI 只允许提交当前 `FactionId` 且位于候选数组中的一个 `TechnologyId`。
- `ChoosePendingTechnology` 成功后，Container 会清除候选并推进回合；UI 应重新读取 `CurrentFactionId`、回合号和等待状态。
- 失败时 `OutError` 为稳定错误码：`technology_progression_disabled`、`invalid_faction`、`no_pending_technology_choice`、`technology_not_in_pending_choices`。

### 4.2 UI 显示数据

T0 仅返回枚举，不在 Gameplay 中保存显示名称、描述、图标或稀有度。UI 可将三个占位枚举映射为本地化文案；未来科技 UI 设计稿负责建立该映射。

---

## 5. 编辑器配置

`APlanetTessellatedMesh -> PlanetGameplayComponent` 暴露：

```text
PlanetTopology|Tess|SimpleGameplay T0
  T0TechnologyProgressionConfig
```

该配置在 `RebuildGameplay()` 和教程固定场景初始化时传入 `FTerraGameplayContainer`。同一配置与同一行动序列必须得到同样的候选顺序和成长状态。

---

## 6. 验收

- 捕获、升变和阵营失败按配置记入行动阵营的本回合得分，未确认行动不计入累计得分。
- 分数达到门槛后，当前阵营不推进，棋盘点击不再产生新行动。
- UI 查询到三个不同的无效果候选；提交其中一个后，候选清空、已拥有科技增加、回合正常推进。
- 无效阵营、无待选状态和非候选科技的选择均被拒绝且不改变状态。
- 关闭总开关时，回合行为与 G12 之前完全一致。
- 相同种子、相同行动序列下，候选顺序和选择后的状态可复现。
