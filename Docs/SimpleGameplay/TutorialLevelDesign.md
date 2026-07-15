# TerraCivilization 教学关卡与关卡生成器设计稿

> 本文是教学关卡及其关卡生成器的唯一设计入口。后续有关教学关卡的配置格式、运行时初始化、镜头、AI 脚本、单关编排与验收，均在本文维护；`SimpleGameplayDesign.md` 仅保留主玩法规则，不重复维护关卡设计。

---

## 0. 目标与边界

本期先实现“可由配置文件生成固定 Gameplay 局面”的关卡生成器，并为五个教学关卡预留配置与编排位置。

教学关卡不是新的 Gameplay 规则分支。它复用既有棋盘、回合、移动、跳跃、吃子、胜负、G10 中立单位、G11 阵营归属、G12 装备掉落与升变规则；差异只在于：

- 默认随机生成的地形、初始棋子和中立单位由关卡配置确定性覆盖。
- 初始镜头由关卡配置确定。
- NPC 的行动不依赖 LLM，而是按配置中预编排的行动序列执行。
- 不增加操作屏蔽、强制点击、专用教学 UI 或额外高亮。玩家通过现有 Idle/选择/行动高亮理解局面。
- 某些 NPC 脚本可在玩家若干回合未完成目标时，用预期招式吃掉玩家棋子，以局面变化本身展示规则。

本验收点不实现具体教学关卡的 CellId 布局、具体棋子招式或 UI 文案。这些内容在各关正式实现时补写到本文第 7 节。

---

## 1. 关卡运行模型

### 1.1 地图与配置的职责

首期采用“一张教学外壳地图 + 多个关卡配置文件”的模型：

```text
L_TutorialShell.umap
  -> APlanetTessellatedMesh 读取启动参数
  -> 加载 UTerraTutorialScenarioData
  -> UPlanetGameplayComponent
  -> FTerraGameplayContainer 的固定局面初始化
  -> 现有渲染、高亮、回合与 AI 执行链路
```

当前默认游戏地图可直接作为教学运行外壳，持有场景、星球棋盘 Actor、相机、输入与灯光。它不保存某一关的棋子、地形或 AI 行动，以避免复制多个 `.umap` 后产生配置漂移。

每一关使用一个 `UTerraTutorialScenarioData` Data Asset。Data Asset 是本期所说的“配置文件”：它可被编辑器序列化、被关卡管理器以 Soft Object Path 加载，并可在未来改为 JSON 导入/导出而不改变 Gameplay 初始化模型。

### 1.2 关卡进入

首期不新增教学外壳地图或 URL Options。教学入口以进程启动参数提供 Scenario 资源路径：

```cpp
TerraCivilization.exe -TutorialScenario=/Game/Tutorial/Scenarios/DA_Tutorial_01_BasicMove
```

`APlanetTessellatedMesh::BeginPlay` 从命令行读取 `TutorialScenario`，同步加载对应 Data Asset；仅当参数存在且资源加载成功时，才调用棋盘 Gameplay 的关卡初始化入口。未提供参数、资源无法加载或配置校验失败时，输出明确错误并走既有默认随机初始化路线，避免生成半初始化棋盘。

未来菜单、调试控制台和自动化测试也复用同一个 Scenario 路径；首期可直接以不同启动参数启动指定关卡，稳定复现局面。

### 1.3 初始化时序

```text
加载 L_TutorialShell
  -> Manager 读取并校验 Scenario
  -> 按 Scenario 确定拓扑细分等级
  -> 创建/重建棋盘拓扑与基础渲染
  -> 以平地为默认值写入地形，再应用地形覆盖
  -> 固定放置初始棋子与初始装备掉落
  -> 应用首回合阵营与回合序号
  -> 应用镜头初始状态
  -> 注册并等待 NPC 行动脚本
  -> 进入现有 Idle 状态与高亮链路
```

固定关卡初始化完成前不得调用常规 `RebuildGameplay()` 的随机布阵、中立刷新或默认阵营生成。教学配置未显式列出的 Cell 地形均为平地，未显式列出的棋子与掉落均不存在。

---

## 2. 配置数据模型

### 2.1 `UTerraTutorialScenarioData`

```cpp
UCLASS(BlueprintType)
class UTerraTutorialScenarioData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere)
    FName ScenarioId;

    UPROPERTY(EditAnywhere)
    FText DisplayName;

    UPROPERTY(EditAnywhere)
    int32 CellSubdivisionLevel = 3;

    UPROPERTY(EditAnywhere)
    int32 InitialTurnFactionId = 0;

    UPROPERTY(EditAnywhere)
    int32 InitialTurnIndex = 0;

    UPROPERTY(EditAnywhere)
    TArray<FTerraTutorialPiecePlacement> InitialPieces;

    UPROPERTY(EditAnywhere)
    TArray<FTerraTutorialTerrainPlacement> TerrainOverrides;

    UPROPERTY(EditAnywhere)
    TArray<FTerraTutorialEquipmentDropPlacement> InitialEquipmentDrops;

    UPROPERTY(EditAnywhere)
    FTerraTutorialCameraConfig InitialCamera;

    UPROPERTY(EditAnywhere)
    TArray<FTerraTutorialNpcAction> NpcActionSequence;
};
```

`ScenarioId` 是稳定标识，不以 Asset 文件名作为 Gameplay 逻辑判断条件。`DisplayName` 仅供入口 UI 使用，不参与规则。

### 2.2 初始棋子

```cpp
USTRUCT(BlueprintType)
struct FTerraTutorialPiecePlacement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    int32 PieceId = INDEX_NONE;

    UPROPERTY(EditAnywhere)
    int32 CellId = INDEX_NONE;

    UPROPERTY(EditAnywhere)
    int32 OwnerFactionId = INDEX_NONE;

    UPROPERTY(EditAnywhere)
    ETerraGameplayPieceType PieceType = ETerraGameplayPieceType::Infantry;
};
```

- `PieceId` 直接成为 `FTerraGameplayPieceState::PieceId`，必须从 `0` 开始连续编号且不重复；当前 Gameplay 容器以数组下标查询棋子 ID。这样 AI 脚本和日志可以稳定引用棋子。
- `OwnerFactionId` 是棋子的原生阵营。关卡中仍按 G11 规则处理阵营失败后的棋子归属。
- `CellId` 必须存在于当前细分等级生成的拓扑中，且所有初始棋子不得占用同一 Cell。
- 可配置 `Commander`、`Infantry`、`Cavalry`、`Archer`、`ArcherCavalry` 与现有中立单位类型；中立单位仍遵守 G10：不能移动或被击杀，当前回合视作当前阵营的支点/攻击参与者。

本期不从 G1 的标准 16 子阵型补齐未配置棋子。配置数组就是该关完整初始棋子集合。

### 2.3 地形与装备

```cpp
USTRUCT(BlueprintType)
struct FTerraTutorialTerrainPlacement
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) int32 CellId = INDEX_NONE;
    UPROPERTY(EditAnywhere) ETerraGameplayTerrainType Terrain = ETerraGameplayTerrainType::Plain;
};

USTRUCT(BlueprintType)
struct FTerraTutorialEquipmentDropPlacement
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) int32 CellId = INDEX_NONE;
    UPROPERTY(EditAnywhere) bool bHasBow = false;
    UPROPERTY(EditAnywhere) bool bHasHorse = false;
};
```

- 全部 Cell 的初始地形默认为 `Plain`；`TerrainOverrides` 只记录森林和山脉等偏离默认值的 Cell，也允许显式写入 `Plain` 方便审阅。教学配置是地形权威，可显式把随机 WorldGen 的基地保护区设为森林或山脉。
- 骑兵不可初始放置在山脉；弓骑兵可在山脉。配置校验器必须拒绝违反该规则的 Scenario。
- 初始装备掉落用于 G12 教学局面，不占格；同一 Cell 的多条掉落配置合并为弓/马布尔状态。

### 2.4 镜头

```cpp
USTRUCT(BlueprintType)
struct FTerraTutorialCameraConfig
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) int32 FocusCellId = INDEX_NONE;
    UPROPERTY(EditAnywhere) float DistanceToFocusCM = 8000.0f;
};
```

镜头以 `FocusCellId` 对应 Cell 为目标，按 `DistanceToFocusCM` 设置既有球面相机初始距离后直接聚焦。本期仅配置初始位置；绕焦角度、倾角与分步骤自动拉镜头留给后续扩展。

### 2.5 NPC 行动脚本

```cpp
USTRUCT(BlueprintType)
struct FTerraTutorialNpcAction
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) int32 TurnIndex = INDEX_NONE;
    UPROPERTY(EditAnywhere) int32 PieceId = INDEX_NONE;
    UPROPERTY(EditAnywhere) int32 TargetCellId = INDEX_NONE;
};
```

`NpcActionSequence` 是确定性的逐回合脚本，不是策略树，也不调用 A1-A3 外部 LLM Agent。每条脚本只有 `TurnIndex`、`PieceId` 与 `TargetCellId`：当该回合到来时，Gameplay 复用 Agent 的 `TryExecuteValidatedAction` 入口，自动完成选子、判断普通移动/跳跃/连跳终点、吃子结算与回合推进。

脚本不描述 `Jump`、`RemoteCapture`、路径或结束回合等交互细节。若 `TargetCellId` 是可达的连跳最终落点，Gameplay 按其已有合法行动计算与执行；若该棋子、终点或当前局面不合法，则输出 `TutorialNpcScriptInvalid` 错误、不执行该动作，并停止余下脚本，绝不伪造移动或绕过规则。

脚本为空或脚本耗尽时，NPC 不移动。脚本不按玩家是否完成某个显式教学目标分支；关卡作者通过棋盘位置和回合号编排“玩家若未主动完成，则 NPC 以同样招式吃子”的演示局面。首期不实现条件分支，确保重放完全确定。

---

### 2.6 Data Asset 配置方法

1. 在 Content Browser 的 `/Game/Tutorial/Scenarios/` 目录中，选择 `Miscellaneous -> Data Asset`，类型选择 `TerraTutorialScenarioData`；建议命名为 `DA_Tutorial_01_BasicMove` 等。
2. 填写唯一且稳定的 `ScenarioId`、`CellSubdivisionLevel`、首回合阵营和首回合索引。当前推荐全部教学关使用细分等级 `3`，以保证后续填写的 CellId 可复用。
3. 在 `TerrainOverrides` 填入非平地 Cell；未填写的 Cell 自动为平地。再在 `InitialPieces` 填写完整棋子集合，`PieceId` 必须从 `0` 连续递增。每个参与阵营必须恰有一个主帅，主帅所在 Cell 作为该阵营基地。
4. 可选地在 `InitialEquipmentDrops` 配置弓/马掉落；在 `InitialCamera` 填写开局聚焦 Cell 和距离。
5. 在 `NpcActionSequence` 以回合号升序填写 NPC 动作。每行只需填 `TurnIndex`、`PieceId`、`TargetCellId`；不填写任何行即为无 NPC 行动。
6. 使用启动参数运行，例如：`TerraCivilization.exe -TutorialScenario=/Game/Tutorial/Scenarios/DA_Tutorial_01_BasicMove`。编辑器 PIE 或 Standalone 的 Additional Launch Parameters 同样填写该参数。

Data Asset 被成功读取但内容不合法时，运行时会记录字段级错误并回退默认随机对局。关卡作者应先确认 Output Log 中的 `Tutorial` 错误，再修正资产，不应依赖非法配置产生特殊局面。

---

## 3. C++ 生成入口设计

### 3.1 新增职责

| 类型 | 职责 |
| --- | --- |
| `UTerraTutorialScenarioData` | 持有关卡静态配置，不保存运行中状态。 |
| `APlanetTessellatedMesh` | 读取启动参数、加载/校验 Scenario，并驱动教学初始化。 |
| `UPlanetGameplayComponent` | 暴露教学初始化入口，将配置转换为 Gameplay Cell、棋子、掉落与首回合。 |
| `FTerraGameplayContainer` | 新增固定局面初始化与脚本行动复用入口，继续作为规则唯一权威。 |

### 3.2 建议接口

```cpp
// UPlanetGameplayComponent
bool InitializeTutorialScenario(const UTerraTutorialScenarioData& Scenario, FString& OutError);

// FTerraGameplayContainer
bool InitializeFixedScenario(
    const TArray<FTerraGameplayCellState>& Cells,
    const TArray<FTerraGameplayPieceState>& InitialPieces,
    const TArray<FTerraGameplayEquipmentDropState>& InitialDrops,
    int32 InitialTurnFactionId,
    int32 InitialTurnIndex,
    FString& OutError);
```

`InitializeFixedScenario` 不复制一套移动或吃子逻辑。它只写入经过校验的固定初始状态，之后所有玩家操作、NPC 脚本、G11 阵营转归、G12 掉落拾取、日志和高亮都继续走已有容器流程。

### 3.3 与随机开局的分流

`APlanetTessellatedMesh::RebuildAll_()` 维持默认对战用途。带有有效 `-TutorialScenario=` 参数时，它先按 Data Asset 的细分等级重建基础拓扑，然后调用 `InitializeTutorialScenario` 替换随机 Gameplay 结果；不带参数时不触碰现有默认流程。

实现时应保证下列顺序：先完成拓扑和 HISM Cell 实例，再写教学地形并刷新材质，随后固定初始化 Gameplay，最后同步棋子/装备 Actor 和高亮。这样 `CellId` 既可用于 Gameplay，也能直接定位渲染与镜头。

### 3.4 配置校验

加载前必须校验：

- `ScenarioId` 非空，细分等级合法，`InitialTurnFactionId` 合法。
- 所有引用的 `CellId` 存在。
- 初始棋子 `PieceId` 唯一、`CellId` 不重复、阵营与兵种合法。
- 骑兵不在山脉；其他既有兵种与地形限制同样不得被配置绕过。
- 装备掉落 Cell 合法，不与规则冲突。
- 镜头焦点 Cell 合法。
- 所有 AI 动作的回合号、棋子和终点字段完整；完整合法性仍在实际执行前由 Gameplay 再校验。

校验错误应包含 `ScenarioId`、字段名和 CellId/PieceId，方便关卡作者直接定位 Asset 配置。

---

## 4. 高亮、回合与胜负

- 不增加教学专用高亮。Idle 阶段继续使用既有逻辑：仅浅红色高亮当前阵营原生棋子所在 Cell；中立单位不在 Idle 高亮中出现。
- 选中棋子、行动目标、连跳路径、远射目标等，继续由当前 Gameplay 高亮逻辑驱动。
- 教学局面也按既有 G6/G11 胜负逻辑运行。主帅被吃后，该阵营失败；其存活棋子转归当前回合阵营，并被视为新阵营原生棋子。
- 关卡没有额外“完成按钮”或操作拦截。具体关卡的完成时机以既有胜负、指定局面或后续显式关卡完成条件扩展处理；首期生成器只保证局面可确定性加载与 NPC 脚本可执行。

---

## 5. 重开与调试

- 重开当前教学关卡：沿用当前 `Scenario` URL 参数重新打开 `L_TutorialShell`，而不是在残留运行状态上手动复位。
- 调试时可通过控制台/菜单直接传入 Scenario 路径，跳转到任一关。
- Gameplay 日志增加 ScenarioId 前缀或单独的 `Tutorial` 类别，至少记录加载成功、配置校验失败、NPC 脚本动作开始/成功/失败。
- 因初始 `PieceId` 固定，同一 Scenario 的日志、AI 动作和复现问题都可以稳定关联到同一棋子。

---

## 6. 验收点一：关卡生成器

以下全部满足时，验收点一完成：

1. 可通过 URL `Scenario` 参数加载 `L_TutorialShell` 中的任一教学 Data Asset。
2. Gameplay 初始化结果只包含配置中的初始棋子；每枚棋子的 `PieceId`、阵营、兵种和 CellId 与配置一致。
3. 未覆盖地形全部为平地；覆盖地形正确渲染并正确影响既有 Gameplay 规则。
4. 配置的初始弓、马掉落正确出现，且可按 G12 规则拾取。
5. 初始镜头稳定聚焦配置 Cell，位置、距离和朝向符合配置。
6. 启用 NPC 时，NPC 仅按写死行动序列执行，且每一步均经 Gameplay 合法性校验；未启用或脚本耗尽时 NPC 不移动。
7. 非法配置或非法脚本不会造成崩溃、重叠棋子或绕过规则的行动，并有可定位日志。
8. 教学局面的高亮、胜负、G11 阵营转归、G12 掉落与升变均与普通对局一致。

---

## 7. 教学关卡预编排

本节定义关卡顺序、教学主题和生成器所需的配置边界。具体 CellId、棋子编号、地形分布、镜头数值和 NPC 行动序列，待每关实现时在对应小节补充。

| 编号 | ScenarioId | 主题 | 必需系统 | NPC 预编排用途 |
| --- | --- | --- | --- | --- |
| T1 | `tutorial_basic_move_capture` | 基础移动、二吃一、胜负逻辑 | G1、G4、G6、G11 | 以相同二吃一局面进行反制/演示，并可触发主帅被吃。 |
| T2 | `tutorial_chain_jump` | 连跳 | G3、G4 | 预留连续跳跃路线；若玩家未连跳，NPC 用对应路线制造吃子。 |
| T3 | `tutorial_cavalry_mountain` | 骑兵远跳与山脉阻挡 | G3、地形、骑兵规则 | 演示骑兵可远跳、普通骑兵不可进入/越过山脉。 |
| T4 | `tutorial_archer_terrain` | 弓兵远射、山脉增加射程、森林防护 | G5、地形 | 演示普通远射、山地延程和森林对远射目标的保护。 |
| T5 | `tutorial_equipment_promotion` | 装备掉落与拾取升变 | G12 | 通过预置或 NPC 造成掉落，展示步兵/骑兵/弓兵拾取后升变。 |

### 7.1 T1 基础移动、二吃一、胜负逻辑

- 目标：让玩家在最小局面中理解一步移动、二吃一结算，以及主帅被吃导致阵营失败和 G11 转归。
- 配置状态：待实现时补充初始棋子、关键 CellId、镜头与 NPC 行动脚本。
- 验收局面：待具体编排时定义。

### 7.2 T2 连跳

- 目标：让玩家辨认跳跃支点、连续跳跃的多个落点，并理解连跳后吃子结算。
- 配置状态：待实现时补充固定跳跃路径与 NPC 对照动作。
- 验收局面：待具体编排时定义。

### 7.3 T3 骑兵远跳和山脉阻挡

- 目标：展示骑兵远跳可达位置，以及山脉对原生骑兵的进入/跨越限制。
- 配置状态：待实现时补充山脉 Cell、骑兵起点和合法/非法对照路径。
- 验收局面：待具体编排时定义。

### 7.4 T4 弓兵远射、山脉增加射程和森林防护

- 目标：展示弓兵远射；展示弓兵站在山脉时的延长射程；展示森林使目标免受远射并让射线继续扫描。
- 配置状态：待实现时补充射线方向、山脉射击位、森林保护位和 NPC 动作。
- 验收局面：待具体编排时定义。

### 7.5 T5 装备掉落和升变

- 目标：展示弓兵死亡掉弓、骑兵死亡掉马，以及步兵/骑兵/弓兵按 G12 规则拾取并升变为弓兵、骑兵或弓骑兵。
- 配置状态：待实现时补充初始掉落或 NPC 造成本局掉落的方案、拾取路径和升变后的行动展示。
- 验收局面：待具体编排时定义。

---

## 8. 后续扩展，不属于验收点一

- 基于当前局面、回合数或玩家是否执行指定动作的条件化 NPC 脚本。
- 关卡步骤、提示文本、专用目标面板、操作白名单/屏蔽。
- 多段镜头、自动镜头聚焦和关键行动回放。
- JSON/CSV 导入导出和编辑器批量校验工具。
- 关卡完成存档、章节解锁与难度变体。

这些扩展必须继续以 `UTerraTutorialScenarioData` 为中心，不能把单关专属规则散落进 `APlanetTessellatedMesh` 或 `FTerraGameplayContainer`。
