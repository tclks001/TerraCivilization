# TerraCivilization SimpleGameplay G1 棋子初始化设计稿

> 本稿对应 [SimpleGameplayDesign.md](SimpleGameplayDesign.md) §14 的 **G1：棋盘状态与开局布阵**。
>
> 当前阶段只验收“球面上对应格子里出现应有兵种”，暂不实现兵种职能、移动、吃子、回合与 UI。

---

## 1. 目标

G1 的目标是在现有球面 Cell 拓扑上生成 12 个阵营的初始棋子布局，并用 `DrawDebugSphere` 临时代替真实棋子模型。

验收内容：

- 找到全部 12 个五边形 Cell。
- 每个五边形 Cell 作为一个阵营的大本营。
- 每个阵营显示 16 个调试棋子：
  - 1 个大本营 / 军旗。
  - 5 个步兵。
  - 5 个骑兵。
  - 5 个弓兵。
- 不实现移动、攻击、回合与胜负。
- 不创建真实棋子 Actor / Component / StaticMesh，只使用调试球。

---

## 2. 可视化约定

调试球颜色：

| 类型 | 颜色 | 含义 |
| --- | --- | --- |
| 大本营 / 军旗 | 红色 | 五边形基地 Cell |
| 步兵 | 黄色 | 相邻两个骑兵之间的第二圈 Cell |
| 骑兵 | 蓝色 | 每个弓兵背向基地方向的第二圈 Cell |
| 弓兵 | 绿色 | 基地周围第一圈 Cell |

调试球位置：

```text
WorldPosition = ActorTransform.TransformPosition(Cell.UnitCenter * (GlobeRadiusCM + G1DebugPieceHeightOffsetCM))
```

默认将棋子画在球面上方一点点，避免被 HISM 瓦片遮挡。

---

## 3. 初始放置规则

### 3.1 大本营

遍历 `CellTopology->Cells`，收集 `bIsPentagon=true` 的 Cell。

- 按 `CellId` 从小到大使用。
- 每个五边形对应一个 `FactionId`。
- 在五边形 Cell 放置红色大本营。

### 3.2 弓兵

读取基地 Cell 的 5 个有效邻居：

```text
Base.NeighborCellIds[0..4]
```

每个有效邻居上放置一个绿色弓兵。

### 3.3 骑兵

对每个弓兵 Cell，找到“与大本营相反”的格子：

1. 在弓兵 Cell 的邻居环里找到基地 Cell 的索引 `BaseNeighborIndex`。
2. 弓兵所在的内圈 Cell 通常是六边形，因此反方向邻居为：

```text
CavalryCell = Archer.NeighborCellIds[(BaseNeighborIndex + 3) % 6]
```

3. 在该 Cell 放置蓝色骑兵。

如果异常遇到非六边形 Cell，则跳过该方向并输出日志；G1 不引入复杂五边形穿越规则。

### 3.4 步兵

步兵位于相邻两个骑兵之间，即原弓兵外圈位置。

对环形顺序中的每对相邻骑兵：

```text
Cavalry[i]
Cavalry[(i + 1) % 5]
```

寻找它们共同相邻的 Cell，并过滤掉：

- 基地 Cell。
- 本阵营已使用的弓兵 Cell。
- 本阵营已使用的骑兵 Cell。
- 无效 Cell。

如果存在多个候选，选择方向最接近两个骑兵中间方向的 Cell：

```text
Score = dot(Cell.UnitCenter, normalize(CavalryA.UnitCenter + CavalryB.UnitCenter))
```

得分最高者作为黄色步兵 Cell。

### 3.5 初始化序号机制

运行时初始化棋子时，先计算本阵营的全部开局 Cell，再按固定兵种顺序写入 `Pieces` 数组，而不是在位置计算过程中交替追加棋子。

目的：遍历一个玩家所有棋子时保持恒定的棋子顺序，便于 UI、调试、存档、网络同步或后续 AI 逻辑稳定引用同一类棋子。

在一个阵营完整生成 16 个棋子的正常情况下，该阵营内相对序号固定为：

```text
Flag = 0
Archer = 1,2,3,4,5
Cavalry = 6,7,8,9,10
Infantry = 11,12,13,14,15
```

若使用全局 `PieceId` 表示，则该阵营第一个棋子的基准为：

```text
FactionPieceBaseId = FactionId * 16
```

对应：

```text
Flag      = FactionPieceBaseId + 0
Archer    = FactionPieceBaseId + 1..5
Cavalry   = FactionPieceBaseId + 6..10
Infantry  = FactionPieceBaseId + 11..15
```

---

## 4. C++ 实现位置

初版直接接入 `APlanetTessellatedMesh`，原因：

- 它已经持有 `CellTopology`。
- 它的 `RebuildAll_()` 已经是当前球面拓扑、WorldGen、HISM 瓦片、高亮的统一重建入口。
- 当前只做调试球，不需要独立 Gameplay Actor 或真实棋子 Component。

新增内容：

```cpp
enum class ETerraG1DebugPieceType : uint8
{
    Base,
    Infantry,
    Cavalry,
    Archer,
};

struct FTerraG1DebugPiece
{
    int32 FactionId;
    int32 CellId;
    ETerraG1DebugPieceType PieceType;
};
```

`APlanetTessellatedMesh` 新增字段：

```cpp
bool bEnableG1DebugPieces = true;
float G1DebugPieceRadiusCM = 140.0f;
float G1DebugPieceHeightOffsetCM = 260.0f;
TArray<FTerraG1DebugPiece> G1DebugPieces;
```

新增私有函数：

```cpp
void RebuildG1DebugPieces_();
void DrawG1DebugPieces_() const;
```

---

## 5. 重建与绘制时序

`RebuildAll_()` 中，在拓扑、WorldGen、HISM 瓦片重建完成后执行：

```text
RebuildHISMTileInstances_()
ApplyRenderModeVisibility_()
RebuildG1DebugPieces_()
```

其中：

- `RebuildG1DebugPieces_()` 只生成轻量数组，不生成 Actor / Component。
- `Tick()` 中每帧调用 `DrawG1DebugPieces_()`。
- `DrawDebugSphere` 使用短生命周期持续刷新，避免持久 debug 绘制残留。

Tick 开关条件扩展为：

```text
需要 HISM hover 防抖 tick 或 G1 debug pieces 开启时，启用 Tick。
```

---

## 6. 日志与异常处理

正常情况下默认 `CellSubdivisionLevel=3` 应生成：

```text
12 factions × 16 pieces = 192 debug pieces
```

输出日志示例：

```text
[Tess][G1] Rebuilt debug pieces: Factions=12 Bases=12 Infantry=60 Cavalry=60 Archer=60 Total=192
```

异常处理：

- 找不到 12 个五边形：输出 Warning，但仍按实际找到数量生成。
- 某方向无法找到骑兵反向 Cell：跳过该骑兵并输出 Warning。
- 某对骑兵无法找到共同步兵 Cell：跳过该步兵并输出 Warning。
- 如果不同阵营尝试占用同一个 Cell：跳过后来的棋子并输出 Warning。

---

## 7. 编辑器端使用方式

在关卡中选择 `BP_PlanetTessellatedMesh` / `APlanetTessellatedMesh` 实例：

1. 找到：

```text
PlanetTopology | Tess | SimpleGameplay G1
```

2. 确认：

```text
bEnableG1DebugPieces = true
G1DebugPieceRadiusCM = 140
G1DebugPieceHeightOffsetCM = 260
```

3. 点击 `Rebuild` 或修改任意 Tess 参数触发 `OnConstruction`。
4. 在视口或 PIE 中查看球面上的调试球。

---

## 8. G1 验收清单

- Output Log 出现 G1 重建日志。
- 球面上每个五边形基地出现红色调试球。
- 每个红色基地周围第一圈出现 5 个绿色弓兵。
- 每个绿色弓兵外侧出现 1 个蓝色骑兵。
- 相邻蓝色骑兵之间出现 1 个黄色步兵。
- 默认 12 个阵营合计约 192 个调试球。
- 关闭 `bEnableG1DebugPieces` 后调试球不再绘制。

---

## 9. 暂不实现内容

G1 不实现以下内容：

- 棋子真实 Actor / Component。
- 棋子 StaticMesh / 材质。
- 点击选择棋子。
- 高亮可移动目标。
- 普通移动、跳跃、连跳。
- 二吃一、弓兵远程吃子。
- 回合推进与胜负判定。
