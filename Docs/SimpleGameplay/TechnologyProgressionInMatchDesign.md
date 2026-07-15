# TerraCivilization SimpleGameplay 局内科技成长设计总稿

> T 系列为每个阵营提供一套 Rogue 风格的局内永久成长系统。阵营在自己的回合结束时结算本回合得分；累积得分达到门槛后获得一次三选一科技机会。科技仅在当前对局中生效，游戏重新初始化后全部清空。
>
> 本稿只定义 Gameplay 状态、结算顺序、规则查询接口、科技里程碑与验收标准。不涉及 UI、动画、特效、音效、AI 行为树、LLM Agent 决策或网络同步表现。

---

## 1. 目标与边界

### 1.1 目标

- 让主动行动、争夺地形、吃子、升变和击败阵营都能转化为阵营的长期优势。
- 每个阵营独立积累成长，科技在本局永久生效，且能叠加为差异化战术路线。
- 将科技效果收敛到 Gameplay Container 的合法行动、捕获、地形与胜负判定中，避免各表现层自行修改规则。
- 所有候选生成和选择结果可由种子与行动序列复现，便于调试、复盘和固定教学关卡。

### 1.2 不在本期范围

- 科技选择面板、按键、弹窗、卡牌美术、镜头、动画和音效。
- AI/LLM 对候选科技的评价、选择和行动策略。
- 联机复制、存档格式与跨对局元进度。
- 数值平衡最终定稿。本文默认数值是 Gameplay 可运行的初值，后续可通过编辑器参数调整。

### 1.3 基本约束

- 科技归属于 `FactionId`，不归属于单个棋子。
- 科技的判定必须是纯 Gameplay 逻辑。
- 得分到科技获得的过程，发生在该阵营本回合行动确认、所有其他 Gameplay 结算完毕后。如果科技的效果使该阵营获得了新的科技，也将会在该回合一并确认，直到该阵营选择了所有的科技后，才真正结束本回合并开启下一回合。

---

## 2. 核心流程

```text
阵营回合内发生事件
  -> 记录本回合成长得分事件
  -> 玩家确认结束回合
  -> 先完成待捕获棋子、装备掉落、升变后的既有结算
  -> 处理阵营失败与胜负判定
  -> 结算行动阵营本回合成长得分并累计至总成长值
  -> 若达到下一档门槛：生成或恢复一组 3 个候选科技，进入“等待选择”状态
  -> 选择一个候选科技后永久写入阵营状态，并完成该科技触发的全部 Gameplay 结算
  -> 再次检查本回合新增得分与解锁门槛；有新的待选科技则重复选择和结算
  -> 当前阵营没有任何待选科技后，才真正推进回合
```

### 2.1 得分事件

T 系列不从棋盘最终状态倒推分数，而是在规则事件发生时记录，避免同一成果被重复计分。

| 事件 | 默认分数 | 归属 | 说明 |
| --- | ---: | --- | --- |
| 占领可计分地形 | 10 | 行动阵营 | 首次将指定 Cell 计入该阵营控制时计分；普通经过不重复计分。 |
| 吃掉步兵 | 10 | 发起捕获的阵营 | 仅在捕获真正结算后记录。 |
| 吃掉弓兵或骑兵 | 15 | 发起捕获的阵营 | 含由 G12 升变得到的对应兵种。 |
| 吃掉弓骑兵 | 25 | 发起捕获的阵营 | 同时对应弓和马的高价值单位。 |
| 本回合发生一次升变 | 10 | 升变棋子所属阵营 | 一枚棋子每回合最多记一次。 |
| 击败一个阵营 | 50 | 征服阵营 | 仅主将被吃、G11 转归完成时记录一次。 |

“占领可计分地形”的具体地形和控制规则应由后续子里程碑定义。第一版可以先关闭此来源，仅保留捕获、升变与击败阵营计分。

### 2.2 阈值与多次解锁

默认采用逐档递增阈值：

```text
首次科技：100 分
第二次科技：220 分
第三次科技：360 分
其后每次：在上一档基础上增加 160 分
```

- `AccumulatedScore` 只增不减。
- `NextUnlockScore` 表示下一次可选择科技的累计门槛。
- 一个回合跨越多个门槛时，只创建一份待选科技；完成选择后若仍达到下一档，则在同一回合结算完成前继续生成下一份待选科技，直至累计分数不足下一档。
- 科技数量可通过 `MaxTechnologyCount` 限制；达到上限后仍记录分数，但不再生成候选。

### 2.3 三选一与确定性

候选由 Gameplay 生成，但其展示和输入不属于本稿范围。

- 每次解锁固定生成 3 个不同且当前可选的科技。
- 使用专用 `FRandomStream TechnologyRandomStream`，其种子由编辑器暴露的 `TechnologyRandomSeed` 初始化。
- 生成时先排除已拥有、前置条件不满足、与已拥有科技互斥、当前地图不支持的科技。
- 若可选池不足 3 个，则返回全部可选项；若为空，则本次解锁标记为跳过并推进下一档门槛。
- 候选一旦生成，必须写入阵营状态；在实际选择前不得因后续查询而重新随机。
- Gameplay 通过 `ChoosePendingTechnology(FactionId, TechnologyId, OutError)` 接受外部选择。调用者可以是未来 UI、调试命令或测试代码，但 Container 不依赖它们。

---

## 3. 数据模型与接口

### 3.1 科技枚举

```cpp
enum class ETerraGameplayTechnologyId : uint8
{
    None,
    CavalryIgnoreBlocking,
    PiecesBlockEnemyJump,
    ArcherRangePlusOne,
    CommanderShield,
    CavalryLandingCapture,
    ArcherPiercingShot,
    ArcherSplashShot,
    TerrainConversion,
    PortalPair,
};
```

枚举值只追加、不重排，保证日志、测试和未来存档的稳定性。

### 3.2 阵营成长状态

```cpp
struct FTerraGameplayFactionTechnologyState
{
    int32 FactionId = INDEX_NONE;
    int32 AccumulatedScore = 0;
    int32 ScoreEarnedThisTurn = 0;
    int32 NextUnlockScore = 100;
    int32 UnlockCount = 0;
    int32 TechnologyRandomSeed = 0;
    bool bWaitingForTechnologyChoice = false;
    TArray<ETerraGameplayTechnologyId> OwnedTechnologies;
    TArray<ETerraGameplayTechnologyId> PendingTechnologyChoices;
    int32 CommanderShieldCharges = 0;
    TSet<int32> ConvertedTerrainCellIds;
    TArray<FPortalPairState> PortalPairs;
};
```

`FPortalPairState` 至少保存两个合法 `CellId`、所属阵营与启用状态。它必须由 Gameplay Container 所有，不依赖场景 Actor。

### 3.3 Container 公开查询接口

```cpp
bool HasFactionTechnology(int32 FactionId, ETerraGameplayTechnologyId TechnologyId) const;
bool GetFactionTechnologyState(int32 FactionId, FTerraGameplayFactionTechnologyState& OutState) const;
bool GetPendingTechnologyChoices(int32 FactionId, TArray<ETerraGameplayTechnologyId>& OutChoices) const;
bool ChoosePendingTechnology(int32 FactionId, ETerraGameplayTechnologyId TechnologyId, FString& OutError);
bool IsFactionWaitingForTechnologyChoice(int32 FactionId) const;
```

### 3.4 内部规则接口

以下接口是规则层唯一允许读取科技效果的位置：

```cpp
void RecordFactionScoreEvent_(int32 FactionId, ETerraGameplayScoreEvent Event);
void FinalizeFactionTurnTechnologyProgress_(int32 FactionId);
void GeneratePendingTechnologyChoices_(FTerraGameplayFactionTechnologyState& State);
bool DoesPieceReceiveFactionTechnology_(const FTerraGameplayPieceState& Piece) const;
bool CanPieceBeJumpedOver_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId) const;
int32 GetArcherEffectiveRange_(const FTerraGameplayPieceState& Piece) const;
bool TryConsumeCommanderShield_(int32 DefenderFactionId, int32 CommanderPieceId);
```

科技状态必须进入 `FInteractionUndoSnapshot` 的设计审查范围。由于 G9 不允许撤销已提交的回合，正常情况下科技选择发生在回合提交后；但若未来允许在回合结算阶段选择或调试强制选择，快照仍须能完整恢复阵营科技状态、候选、护盾、地形修改和传送门状态。

---

## 4. 回合与胜负交互

### 4.1 回合结束阻塞

当当前阵营有待选科技时，不能推进到下一阵营。Gameplay 的回合结算应返回“等待科技选择”，而不是直接调用下一回合初始化。

```text
ResolvePendingCaptures
  -> EliminateFaction / EvaluateWinState
  -> FinalizeFactionTurnTechnologyProgress
  -> bWaitingForTechnologyChoice ? 停留 : FinalizeTurnAfterResolution
```

若本回合已直接结束整局游戏，则不再生成科技候选。

### 4.2 主将护盾与失败判定

主将护盾在主将即将被捕获时优先消耗一层：

- 护盾消耗后，该主将不死亡、不触发 G11 阵营失败、不产生装备掉落。
- 本次捕获条目从最终结算列表中移除；捕获方不获得该主将的击杀分。
- 护盾只保护主将一次，不改变普通棋子的捕获规则。
- 多层护盾可叠加，但第一版默认科技只增加 1 层且最多 1 层。

### 4.3 G11 与中立单位

- 阵营失败后转归胜者的棋子，因其已成为胜者原生棋子，获得胜者全部已拥有科技。
- 中立单位只在当前回合临时视为当前阵营可操作单位，`DoesPieceReceiveFactionTechnology_` 必须对 `bIsNeutral` 返回 `false`。
- 中立单位参与跳跃或攻击时，仍遵守本体规则，不因当前回合阵营拥有科技而获得额外能力。

---

## 5. 里程碑

各里程碑按 Gameplay 实现依赖和风险由低到高排列。每项完成后都应可单独开启/关闭，并有固定场景测试。

### T0：成长框架与无效果候选

实现阵营成长状态、分数事件记录、阈值推进、确定性三选一、选择接口和回合结束阻塞；候选池可先只放测试科技或空效果科技。

验收：相同种子和相同行动序列下，每个阵营的得分、候选顺序、选择结果和下一档门槛完全一致。

### T1：弓兵射程加一

`ArcherRangePlusOne` 使受科技影响的 `Archer` 与 `ArcherCavalry` 的基础远程射程加 1；山地额外射程仍在基础加成后叠加，森林保护规则不变。

验收：科技前后只改变合法远程捕获目标的最大距离，不改变普通移动、跳跃和森林遮挡。

### T2：己方棋子阻止敌方跳过

`PiecesBlockEnemyJump` 令拥有科技阵营的原生棋子不能作为敌方跳跃的被跨越棋子。敌方不能以该棋子为标准跳跃或骑兵远跳的中间支点。

验收：同一跳跃路径在科技前合法、科技后非法；己方自身和中立单位的跳跃规则不受误伤。

### T3：骑兵无视山脉阻挡

`CavalryIgnoreBlocking` 放宽受科技影响的 `Cavalry` 与 `ArcherCavalry` 的山脉限制，使其可进入并跨越山脉。该科技只消除山脉限制，不消除棋子占位、森林射击遮挡或其他路径规则。

验收：骑兵的普通移动、标准跳跃、远跳和连跳均可正确经过山脉；无科技阵营保持原规则。

### T4：主将护盾

`CommanderShield` 为本阵营增加 1 层主将护盾，接入捕获最终结算和阵营失败判定。

验收：首次针对主将的有效捕获被抵消，第二次在无护盾时正常触发阵营失败和 G11 转归。

### T5：骑兵落点捕获

`CavalryLandingCapture` 允许受科技影响的骑兵与弓骑兵在合法跳跃的最终落点直接捕获敌方棋子。落点捕获必须定义为新的合法行动分支，不能把敌方落点当作普通空 Cell。

验收：仅骑兵类、仅拥有科技的一方可执行；标准二吃一、先锋和主攻结算不被破坏，装备掉落与得分按被捕获棋子正确结算。

### T6：弓箭穿透

`ArcherPiercingShot` 使一次远程攻击可沿既有有效射线穿过首个被捕获目标，并继续检查后续可捕获目标。第一版限定最多额外穿透 1 个敌方棋子，森林仍停止射线。

验收：远程预览、待捕获条目、最终捕获、装备掉落、阵营失败及成长得分均覆盖全部被命中的棋子，且不重复结算。

### T7：弓箭溅射

`ArcherSplashShot` 在远程攻击主目标结算后，对其相邻 Cell 进行一次受限的二次捕获检查。第一版仅影响敌方普通棋子，不影响主将、不穿过森林、不触发二次溅射。

验收：主目标和相邻目标的捕获顺序确定；同一目标不能同时被穿透与溅射重复结算。

### T8：地形改造

`TerrainConversion` 给予阵营有限次数的地形改造权。第一版仅允许把一个非基地、非传送门、无棋子的 Cell 在平原与森林之间转换；不允许生成或移除山脉。

改造动作必须经 Gameplay 接口执行并验证阵营、次数、Cell 合法性。地形状态应覆盖原始地形而非直接修改拓扑结构。

验收：改造后移动、远程森林保护、Cell 高亮查询和 Undo/重置均读取同一份有效地形状态。

### T9：传送门对

`PortalPair` 在两个合法空 Cell 之间建立一条阵营拥有的传送连接。第一版传送只作为一次普通移动的替代边：棋子进入入口后可到达出口，不能在同一次行动中连续使用多个传送门。

传送门必须接入：普通移动目标生成、跳跃/远跳路径搜索的明确排除规则、捕获预览、行动路径日志和局部拓扑查询。默认不允许中立单位使用，也不允许将主将作为入口或出口。

验收：传送门不会制造无限循环、重复目标或跨阵营非法路径；摧毁或改造相关 Cell 的后续规则需显式拒绝或移除传送门。

---

## 6. 数值与配置

以下参数暴露在 Gameplay 模块编辑器配置中，建议归入 `SimpleGameplay|T0 Technology Progression`：

| 参数 | 默认值 | 说明 |
| --- | ---: | --- |
| `bEnableTechnologyProgression` | `true` | 总开关。 |
| `TechnologyRandomSeed` | `0` | 候选生成随机种子。`0` 表示使用运行时随机种子并记录实际值。 |
| `FirstTechnologyUnlockScore` | `100` | 首次解锁门槛。 |
| `TechnologyUnlockScoreIncrement` | `160` | 后续档位增量。 |
| `MaxTechnologyCount` | `6` | 单阵营科技数量上限。 |
| `ScoreForInfantryCapture` | `10` | 吃步兵得分。 |
| `ScoreForEliteCapture` | `15` | 吃弓兵或骑兵得分。 |
| `ScoreForArcherCavalryCapture` | `25` | 吃弓骑兵得分。 |
| `ScoreForPromotion` | `10` | 升变得分。 |
| `ScoreForFactionDefeat` | `50` | 击败阵营得分。 |
| `ArcherRangeBonus` | `1` | T1 射程加成。 |
| `MaxPierceTargets` | `2` | T6 总命中目标数上限。 |
| `TerrainConversionCharges` | `1` | T8 获得的改造次数。 |

---

## 7. 日志、调试与测试

### 7.1 日志

每个关键阶段输出可检索日志：

```text
[Gameplay][T0] ScoreEvent Faction=%d Event=%s Delta=%d TurnScore=%d Total=%d
[Gameplay][T0] TechnologyUnlock Faction=%d Unlock=%d Threshold=%d Choices=%s
[Gameplay][T0] TechnologyChosen Faction=%d Technology=%s Owned=%s
[Gameplay][T0] CommanderShieldConsumed Faction=%d Commander=%d
```

### 7.2 固定场景测试

- 相同 `TechnologyRandomSeed` 与相同行动序列的候选可复现。
- 低于门槛、恰好达到门槛、单回合跨越多档和科技数量上限。
- G12 升变、G11 阵营失败、G9 未提交操作撤销不产生重复得分。
- 每个里程碑科技分别覆盖“拥有科技”“未拥有科技”“中立单位”“G11 转归棋子”四种主体。
- 传送门和地形改造额外覆盖非法 Cell、占用 Cell、基地 Cell、回路与地图重置。

---

## 8. 后续可扩展科技

在 T0-T9 稳定后，可继续以相同模式加入：步兵首次跳跃额外一步、装备掉落双倍回收、森林伏击、山地据守、主将范围光环、一次性回合重掷候选、占领区持续得分等。新增科技必须先明确其影响的规则查询入口、互斥关系、得分交互和固定场景验收，再加入候选池。
