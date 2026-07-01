# TerraCivilization SimpleGameplay G2 Gameplay 容器设计稿

> 本稿对应 [SimpleGameplayDesign.md](SimpleGameplayDesign.md) §14 的 **G2：选择与普通移动**。
>
> G1 和新版 HISM RGB+Intensity 高亮方案已经验收完成；G2 开始把棋子状态、Cell 逻辑状态、回合与交互判断从渲染 Actor 中拆出，放入新的 `Gameplay` 模块。

---

## 1. G2 目标

G2 只实现最小闭环：

1. 新增独立 `Gameplay` Runtime 模块。
2. 在 `Gameplay` 模块中创建一个总容器，统一管理：
   - Cell 逻辑副本。
   - WorldGen 地形副本。
   - 棋子状态。
   - Cell 占用表。
   - 当前阵营与回合。
   - 当前选中的棋子与当前交互阶段。
   - Gameplay 高亮输出。
3. 初始化时沿用 G1 布阵：
   - 12 个五边形作为 12 个阵营基地。
   - 每阵营 1 军旗、5 步兵、5 骑兵、5 弓兵。
4. 左键点击：
   - 点击当前阵营的可移动棋子：选中它，并把棋子脚下 Cell 高亮为黄色。
   - 已选中棋子时，点击相邻空 Cell：普通移动 1 格，并把新脚下 Cell 高亮为蓝色。
   - 棋子已经移动 1 格后，再次点击脚下 Cell：停在本格，立即结束回合。
5. 删除旧的 `SelectedNow` / `ToggleSelected` 交互语义。今后 Gameplay 不再需要 HISM 多选集合。

G2 暂不实现：

- 跳跃与连跳。
- 二吃一。
- 兵种特殊能力。
- 胜负判定。
- 真实棋子 Actor / Mesh。
- AI 或 UI 面板。

---

## 2. 新模块：`Gameplay`

新增目录：

```text
Source/Gameplay
  Gameplay.Build.cs
  Public/Gameplay.h
  Private/Gameplay.cpp
  Public/TerraGameplayTypes.h
  Public/TerraGameplayContainer.h
  Private/TerraGameplayContainer.cpp
```

模块依赖：

```cpp
PublicDependencyModuleNames = Core, CoreUObject, Engine
```

`Gameplay` 模块不依赖 `TerraCivilization` 主模块，避免渲染层与玩法层循环依赖。

---

## 3. Gameplay 容器定位

核心类：

```cpp
class FTerraGameplayContainer
```

它是一个纯 C++ 容器，不是 Actor，也不直接持有渲染组件。

职责边界：

| 模块 | 职责 |
| --- | --- |
| `TerraCivilization` / `APlanetTessellatedMesh` | 负责 WorldGen、HISM 实例、鼠标命中 CellId、把高亮写入 `PerInstanceCustomData` |
| `Gameplay` / `FTerraGameplayContainer` | 负责棋子、Cell 逻辑、回合、移动合法性、Gameplay 高亮状态 |

调用关系：

```text
APlanetTessellatedMesh::RebuildAll_()
  -> WorldGen.Generate()
  -> RebuildHISMTileInstances_()
  -> RebuildGameplay_()
       -> 从 CellTopology 复制 Cell 邻接/五边形信息
       -> 从 WorldGen 复制三地形状态
       -> GameplayContainer.Initialize(...)

APlanetTessellatedMesh::HandleHISMClickHit(Hit)
  -> TryResolveHISMHitToCellId(Hit, CellId)
  -> GameplayContainer.HandleCellClick(CellId, DirtyCells)
  -> 对 DirtyCells 调用 WriteHISMHighlightForCell_()
```

---

## 4. Cell 逻辑副本

`Gameplay` 模块不直接保存 `FCell` 或 `FCellGeoData`，而是保存自己的轻量副本。

```cpp
enum class ETerraGameplayTerrainType : uint8
{
    Plain,
    Forest,
    Mountain,
};

struct FTerraGameplayCellState
{
    int32 CellId = INDEX_NONE;
    bool bIsPentagon = false;
    TStaticArray<int32, 6> NeighborCellIds;
    ETerraGameplayTerrainType TerrainType = ETerraGameplayTerrainType::Plain;
};
```

复制原因：

- Gameplay 容器拥有独立逻辑状态，不依赖渲染 Actor 的内部拓扑生命周期。
- 后续若 Gameplay 需要修改 `OwnerId`、占领状态、临时阻挡等，避免反写 WorldGen 原始数据。
- 地图状态由 WorldGen 传入一份副本后，由 Gameplay 模块统一管理。

---

## 5. 棋子与阵营状态

棋子类型：

```cpp
enum class ETerraGameplayPieceType : uint8
{
    Flag,
    Infantry,
    Cavalry,
    Archer,
};
```

棋子状态：

```cpp
struct FTerraGameplayPieceState
{
    int32 PieceId = INDEX_NONE;
    int32 OwnerFactionId = INDEX_NONE;
    int32 CellId = INDEX_NONE;
    ETerraGameplayPieceType PieceType = ETerraGameplayPieceType::Infantry;
    bool bAlive = true;
    bool bCanMove = true;
};
```

阵营状态：

```cpp
struct FTerraGameplayFactionState
{
    int32 FactionId = INDEX_NONE;
    int32 BaseCellId = INDEX_NONE;
    int32 FlagPieceId = INDEX_NONE;
    bool bAlive = true;
};
```

占用表：

```cpp
TArray<int32> CellToPieceId;
```

约定：

- `CellToPieceId[CellId] == INDEX_NONE` 表示空 Cell。
- 非空时值为 `Pieces` 数组中的 `PieceId`。

---

## 6. 初始化布阵

G2 继续沿用 G1 的具体摆放规则，保证视觉验收结果连续：

- 五边形：军旗。
- 五边形周围一圈：步兵。
- 每个步兵背向军旗的格子：骑兵。
- 相邻骑兵之间：弓兵。

初始化输出：

```text
12 factions × 16 pieces = 192 pieces
```

初始化完成后：

```cpp
CurrentFactionId = 0;
TurnIndex = 0;
SelectedPieceId = INDEX_NONE;
InteractionPhase = Idle;
GameplayHighlights.Empty();
```

---

## 7. G2 交互状态机

状态枚举：

```cpp
enum class ETerraGameplayInteractionPhase : uint8
{
    Idle,
    PieceSelected,
    PieceMovedCanEndTurn,
};
```

### 7.1 Idle

点击 Cell：

```text
如果 Cell 上有当前阵营的可移动棋子：
    SelectedPieceId = PieceId
    Phase = PieceSelected
    高亮该 Cell 为黄色
否则：
    无操作
```

### 7.2 PieceSelected

点击 Cell：

```text
如果点击的是另一个当前阵营可移动棋子：
    切换选择到新棋子
    新棋子脚下高亮黄色

否则如果点击的是当前选中棋子的相邻空 Cell：
    普通移动 1 格
    更新 Piece.CellId
    更新 CellToPieceId
    Phase = PieceMovedCanEndTurn
    新脚下 Cell 高亮蓝色

否则：
    无操作
```

普通移动合法性：

```text
目标 Cell 有效
目标 Cell 与起点相邻
目标 Cell 为空
棋子 bCanMove = true
棋子不是军旗
```

G2 暂不实现骑兵山脉限制，因此所有非军旗棋子都可普通移动到相邻空 Cell。

### 7.3 PieceMovedCanEndTurn

点击 Cell：

```text
如果点击的是当前选中棋子脚下 Cell：
    清空选择
    清空 Gameplay 高亮
    CurrentFactionId 切到下一个存活阵营
    TurnIndex += 1
    Phase = Idle
否则：
    无操作
```

G2 中一次行动只允许普通移动 1 格；移动后只能点击脚下 Cell 停下并结束回合。

---

## 8. 高亮输出

Gameplay 容器输出的是逻辑高亮，不直接写材质。

```cpp
struct FTerraGameplayCellHighlight
{
    FLinearColor Color = FLinearColor::Black;
    float Intensity = 0.0f;
};
```

G2 使用颜色：

| 状态 | 颜色 | 强度 |
| --- | --- | --- |
| 选中棋子脚下 Cell | 黄色 `(1, 1, 0)` | `1` |
| 移动后可停下 Cell | 蓝色 `(0, 0.35, 1)` | `1` |

`APlanetTessellatedMesh::WriteHISMHighlightForCell_()` 的优先级：

```text
Gameplay 高亮 > Hover 高亮 > 无高亮
```

因此：

- 鼠标移到已选中/可停下的 Cell 上，不会覆盖 Gameplay 高亮颜色。
- 普通 hover 仍可在没有 Gameplay 高亮的 Cell 上显示。
- 旧 `HISMSelectedCellIds` 不再参与高亮合成。

---

## 9. Dirty Cell 刷新策略

每次 Gameplay 交互前后，高亮集合可能变化。

容器接口：

```cpp
bool HandleCellClick(int32 CellId, TArray<int32>& OutDirtyCellIds);
```

约定：

- `OutDirtyCellIds` 包含交互前存在高亮、交互后存在高亮、以及棋子移动起点/终点。
- 渲染层只对这些 Cell 调用 `WriteHISMHighlightForCell_()`。
- 不需要整图刷新。

---

## 10. `APlanetTessellatedMesh` 接线变更

移除旧逻辑：

```cpp
TSet<int32> HISMSelectedCellIds;
SetHISMSelected_()
ToggleHISMSelected_()
SelectedNow 日志
```

新增字段：

```cpp
TUniquePtr<FTerraGameplayContainer> GameplayContainer;
```

新增函数：

```cpp
void RebuildGameplay_();
void RefreshGameplayHighlights_(const TArray<int32>& DirtyCellIds);
```

点击流程改为：

```cpp
bool APlanetTessellatedMesh::HandleHISMClickHit(const FHitResult& Hit)
{
    int32 CellId = INDEX_NONE;
    if (!TryResolveHISMHitToCellId(Hit, CellId)) return false;

    TArray<int32> DirtyCellIds;
    const bool bHandled = GameplayContainer && GameplayContainer->HandleCellClick(CellId, DirtyCellIds);
    RefreshGameplayHighlights_(DirtyCellIds);
    LastHISMClickedCellId = CellId;
    return bHandled;
}
```

---

## 11. G2 验收清单

### 11.1 编译验收

- 新增 `Gameplay` 模块后项目能编译。
- `.uproject` 的 Modules 列表包含 `Gameplay`。
- `TerraCivilization.Build.cs` 依赖 `Gameplay`。

### 11.2 初始化验收

Output Log 应出现类似：

```text
[Gameplay][G2] Initialized. Cells=642 Factions=12 Pieces=192 CurrentFaction=0
```

### 11.3 选择验收

- PIE 中左键点击当前阵营的非军旗棋子。
- 该棋子脚下 Cell 变黄。
- 点击非本方棋子或空 Cell 不会选中。

### 11.4 普通移动验收

- 选中棋子后，点击相邻空 Cell。
- 棋子状态移动到目标 Cell。
- 原黄色消失，新脚下 Cell 变蓝。

### 11.5 回合结束验收

- 移动后再次点击脚下蓝色 Cell。
- 蓝色高亮消失。
- 回合切换到下一个阵营。
- Output Log 打印当前 `TurnIndex` 与新 `CurrentFactionId`。

---

## 12. 后续里程碑预留

G3 起可以在同一个容器中继续扩展：

- `StepForwardBranches`。
- 标准跳跃与骑兵特殊跳跃。
- 连跳中的可停下蓝色高亮。
- 可移动目标绿色高亮。
- 二吃一与弓兵远程攻击红色高亮。
- 军旗失败与胜负结算。
