# TerraCivilization 简易玩法 WorldGen 设计稿

> 本稿属于 `Docs/SimpleGameplay` 下的简易玩法迭代文档，用于指导后续改造 `WorldGen` 模块。
>
> 目标：**不再使用复杂自然地理流水线**，改为服务棋类玩法的简单随机地形生成。
>
> 配套玩法规则见 [SimpleGameplayDesign.md](SimpleGameplayDesign.md)。

---

## 1. 设计目标

当前简易玩法只需要三种地形：

| 地形 | 玩法作用 |
| --- | --- |
| 平原 | 默认地形，无额外效果 |
| 森林 | 保护其中目标免受弓兵远程吃子 |
| 山脉 | 骑兵不可进入 / 不可越过；弓兵在山脉上远程距离 +1 |

因此，`WorldGen` 初版不再追求自然地理真实性，不再生成板块、海陆、气候、温度、湿度、河流、生物群系等复杂数据。

新目标是：

- **固定拓扑规模**：本项目简易玩法固定球面拓扑细分层级为 `3`。
- **保证开局公平**：所有五边形基地半径 2 范围内都必须是平原。
- **生成可读地形**：山脉呈条状，森林呈块状。
- **参数少且可控**：只暴露山脉和森林的数量、平均节点数。
- **结果可复现**：同一 `RandomSeed` + 同一拓扑应得到相同地形。

---

## 2. 与旧 `WorldGen` 设计的关系

旧版 `WorldGen` 以自然地理为目标，大致包含：

```text
板块划分 -> 高程 -> 海陆 -> 湿度 -> 温度 -> 生物群系 -> 河流 -> 基地
```

简易玩法阶段应整体替换为：

```text
初始化全部平原
    -> 找到 12 个五边形基地
    -> 标记所有基地半径 2 保护区
    -> 在保护区外生成条状山脉
    -> 在保护区外生成块状森林
    -> 输出每个 Cell 的三地形结果
```

保留项：

- `RandomSeed`。
- `FCellGeoData.CellId`。
- `FCellGeoData.bIsPentagon`。
- `FCellGeoData.TerrainTag` 或后续等价的简易地形字段。
- `BaseCellIds`，仍由 12 个五边形 Cell 得到。

废弃 / 暂停使用项：

- `PlateCount`、`OceanicPlateRatio`、`PlateBoundaryThreshold`。
- `SeaLevel`、`ElevationField`、`ElevationNoise*`。
- `MoistureField`、`TemperatureField`、风带、雨影、海距。
- `RiverDischargeThreshold`、`LakeDischargeThreshold`。
- 基于 `UTerrainDefinition::ScoreFor` 的复杂生物群系分类。

> 后续实现时可以先保留旧字段以降低改动风险，但简易玩法路径不应再读取这些旧地理参数。

---

## 3. 编辑器参数

`FWorldGenSettings` 面向简易玩法需要暴露以下参数：

```cpp
UPROPERTY(EditAnywhere, Category = "WorldGen|SimpleGameplay")
int32 RandomSeed = 0;

UPROPERTY(EditAnywhere, Category = "WorldGen|SimpleGameplay|Mountain", meta = (ClampMin = "0"))
int32 MountainStripCount = 24;

UPROPERTY(EditAnywhere, Category = "WorldGen|SimpleGameplay|Mountain", meta = (ClampMin = "1"))
float AverageMountainCellCount = 8.0f;

UPROPERTY(EditAnywhere, Category = "WorldGen|SimpleGameplay|Forest", meta = (ClampMin = "0"))
int32 ForestPatchCount = 36;

UPROPERTY(EditAnywhere, Category = "WorldGen|SimpleGameplay|Forest", meta = (ClampMin = "1"))
float AverageForestCellCount = 10.0f;
```

参数含义：

| 参数 | 含义 |
| --- | --- |
| `RandomSeed` | 随机种子，保证生成可复现 |
| `MountainStripCount` | 生成多少条山脉 |
| `AverageMountainCellCount` | 每条山脉平均包含多少个 Cell |
| `ForestPatchCount` | 生成多少片森林 |
| `AverageForestCellCount` | 每片森林平均包含多少个 Cell |

### 3.1 单次生成节点数

每一次山脉或森林生成时，实际节点数不是固定值，而是围绕平均节点数做正态分布采样。

建议规则：

```text
ActualCount = RoundToInt(Normal(Mean = AverageCount, StdDev = AverageCount * 0.25))
ActualCount = Clamp(ActualCount, 1, MaxReasonableCount)
```

说明：

- 标准差暂定为平均值的 `25%`。
- 采样结果最少为 `1`。
- `MaxReasonableCount` 可按实现需要限制，例如不超过全部可用 Cell 数量。
- 若后续希望更可控，可以再暴露 `MountainCellCountStdDev` / `ForestCellCountStdDev`，但初版不需要。

---

## 4. 地形数据模型

简易玩法只需要三种逻辑地形：

```cpp
enum class ETerraSimpleTerrainType : uint8
{
    Plain,
    Forest,
    Mountain,
};
```

如果继续沿用 `FCellGeoData.TerrainTag`，建议映射为：

| 简易地形 | 推荐 Tag |
| --- | --- |
| 平原 | `Terrain.Plain.Grass` |
| 森林 | `Terrain.Forest.Temperate` |
| 山脉 | `Terrain.Mountain.Peak` |

注意：

- 简易玩法逻辑层不应关心温带 / 热带 / 针叶林等细分语义。
- 渲染层如果仍依赖旧 `Terrain.*` Tag，可以先用上表做兼容映射。
- 后续若引入 `ETerraTerrainType` 作为玩法字段，应以玩法字段为准，Tag 仅作为渲染或资产查表输入。

---

## 5. 固定拓扑规模

本项目简易玩法阶段固定球面拓扑细分层级为 `3`。

设计约束：

- `WorldGen` 不需要再为不同细分层级做玩法平衡。
- 山脉 / 森林参数的默认值应按 `SubdivisionLevel=3` 调整。
- 若运行时检测到拓扑细分层级不是 `3`，建议输出 Warning，但初版不必阻止运行。

如果 `FSphereTopology` 当前没有直接保存 `SubdivisionLevel` 字段，可以先在创建拓扑的调用处保证传入固定值 `3`。

---

## 6. 基地保护区

### 6.1 五边形基地

简易玩法中，12 个正五边形 Cell 是 12 个阵营的大本营。

生成地形前应先收集：

```text
BaseCellIds = all cells where bIsPentagon == true
```

建议继续按 `CellId` 升序排序，作为 `FactionId=0..11` 的基地顺序。

### 6.2 半径 2 保护区

每个基地的半径 2 范围就是开局 16 子区域：

```text
半径 0：军旗
半径 1：5 个步兵
半径 2：5 个骑兵 + 5 个弓兵
```

这些 Cell 必须全部设为平原。

实现上可直接使用已有邻接查询：

```cpp
FSphereTopologyQuery::CollectCellDisk(BaseCellId, 2, ProtectedCells);
```

保护区规则：

- 初始化时全部地形都是平原。
- `ProtectedCells` 中的 Cell 永远保持平原。
- 山脉种子不能选在保护区内。
- 山脉生长不能进入保护区。
- 森林种子不能选在保护区内。
- 森林生长不能进入保护区。
- 如果生成过程走到保护区边界，应停止或改选其他候选。

---

## 7. 总生成流程

建议主流程：

```text
GenerateSimpleGameplayTerrain()
    1. 初始化所有 Cell 为 Plain
    2. 收集 12 个五边形 BaseCellIds
    3. 对每个 BaseCellId 收集半径 2，合并为 ProtectedCells
    4. 生成 MountainStripCount 条山脉
    5. 生成 ForestPatchCount 片森林
    6. 将结果写回 CellData / TerrainTag / 简易地形字段
    7. 输出统计日志
```

伪代码：

```cpp
void FWorldGenerator::Generate()
{
    InitCellDataAsPlain();
    CollectBaseCellsFromPentagons();
    BuildProtectedBaseDisk(/*Radius=*/2);

    GenerateMountainStrips();
    GenerateForestPatches();

    WriteTerrainTags();
    LogSimpleTerrainSummary();
}
```

推荐地形覆盖优先级：

```text
Protected Plain > Mountain > Forest > Plain
```

也就是说：

- 保护区最高优先级，永远平原。
- 山脉生成后，森林不覆盖山脉。
- 森林只填充仍为平原的非保护区 Cell。

---

## 8. 山脉生成：条状生长

### 8.1 基本形态

山脉应呈条状。

一次山脉生成从一对相邻 Cell 开始：

```text
A -- B
```

然后从这条初始边分别向两边生长：

```text
... <- A -- B -> ...
```

其中：

- `A` 和 `B` 都必须在保护区外。
- `A` 和 `B` 都不能已经是山脉。
- 初版不用考虑五边形情况：山脉生长过程中遇到五边形或无法取得明确对向方向时，可以停止该方向。

### 8.2 山脉步进规则

已知当前方向的前一格 `Prev` 和当前格 `Cur`，下一格从 `Cur` 的邻居中选择。

由于山脉不用考虑五边形情况，初版只在六边形上继续生长。

若 `Cur` 是六边形，且：

```text
Cur.NeighborCellIds[x] == Prev
```

则候选方向为：

| 候选 | 邻居索引 | 概率 |
| --- | --- | --- |
| 正对面 | `(x + 3) % 6` | `1/2` |
| 左侧对面 1 格 | `(x + 2) % 6` | `1/4` |
| 右侧对面 1 格 | `(x + 4) % 6` | `1/4` |

也就是：

```text
50%：继续直走
25%：轻微左偏
25%：轻微右偏
```

如果选中的候选不可用，则该方向停止。初版不要求重新抽样，以免形态过度追踪可用格而失去随机性。

候选不可用包括：

- 候选 Cell 不存在。
- 候选 Cell 在基地半径 2 保护区内。
- 候选 Cell 是五边形。
- 候选 Cell 已经是山脉。
- 候选 Cell 越界或不在 `Topology->Cells` 范围内。

### 8.3 双向生长

一次山脉生成目标节点数为 `ActualMountainCellCount`。

推荐流程：

1. 随机选一对合法相邻 Cell：`A`、`B`。
2. 将 `A`、`B` 标为山脉。
3. 剩余需要生成的节点数为 `ActualMountainCellCount - 2`。
4. 维护两个生长端：
   - 端点 1：`Prev = B, Cur = A`，从 `A` 向远离 `B` 的方向生长。
   - 端点 2：`Prev = A, Cur = B`，从 `B` 向远离 `A` 的方向生长。
5. 两端轮流尝试生长，每成功加入一个 Cell，剩余数量减 1。
6. 如果某一端停止，另一端可以继续生长。
7. 如果两端都停止，本条山脉提前结束。

伪代码：

```cpp
void GrowMountainStrip(int32 TargetCount)
{
    FAdjacentPair Seed = PickRandomValidAdjacentPair();
    AddMountain(Seed.A);
    AddMountain(Seed.B);

    FGrowTip TipA{Seed.B, Seed.A, true};
    FGrowTip TipB{Seed.A, Seed.B, true};

    int32 Remaining = TargetCount - 2;
    while (Remaining > 0 && (TipA.bActive || TipB.bActive))
    {
        TryGrowMountainTip(TipA, Remaining);
        if (Remaining <= 0) break;
        TryGrowMountainTip(TipB, Remaining);
    }
}
```

### 8.4 山脉与其他地形的关系

- 山脉不会生成到基地保护区。
- 山脉优先级高于森林。
- 如果森林已经先生成，山脉也可以覆盖森林；但推荐流程是先生成山脉再生成森林，避免覆盖规则复杂化。
- 山脉之间可以相邻，也可以连接成更长山系。
- 初版不要求山脉之间保持距离。

---

## 9. 森林生成：块状向心生长

### 9.1 基本形态

森林应呈块状。

一次森林生成从一个种子 Cell 开始：

```text
Seed
```

然后每次从当前森林外围一圈中选择一个最靠近森林中心方向的 Cell 加入。

这里的“靠近中心”使用球面单位中心向量点积判断。

### 9.2 森林中心

对于当前森林中的所有 Cell，计算它们中心方向的平均值：

```cpp
FVector Sum = FVector::ZeroVector;
for (int32 CellId : ForestCells)
{
    Sum += Topology->Cells[CellId].UnitCenter;
}
FVector CenterDir = Sum.GetSafeNormal();
```

`CenterDir` 是当前森林块的平均中心方向。

### 9.3 外围可延伸范围

每次生长前，统计当前森林所有 Cell 的邻居，得到外围候选集合：

```text
Frontier = 所有与森林相邻、但尚未属于森林的 Cell
```

候选 Cell 必须满足：

- 不在基地半径 2 保护区内。
- 当前仍是平原。
- 不是山脉。
- 不是五边形基地。
- 未被加入当前森林。

### 9.4 选择下一个森林 Cell

在 `Frontier` 中选择与 `CenterDir` 点积最大的 Cell：

```cpp
BestCell = argmax Dot(Topology->Cells[Candidate].UnitCenter, CenterDir)
```

含义：

- 点积越大，说明候选 Cell 越靠近当前森林平均中心方向。
- 每次都选择点积最大的外围 Cell，会让森林更倾向于填补中心附近空洞，形成紧凑块状，而不是细长随机游走。

如果有多个候选点积分数接近，可以随机打破平局。

建议平局规则：

```text
如果 BestDot 差值小于 1e-5，则使用 RandomSeed 派生的随机顺序打破平局。
```

### 9.5 单片森林流程

一次森林生成目标节点数为 `ActualForestCellCount`。

推荐流程：

1. 随机选一个合法平原 Cell 作为森林种子。
2. 将种子标为森林。
3. 重复直到达到目标节点数：
   1. 计算当前森林平均中心方向。
   2. 收集当前森林外围可延伸候选。
   3. 选择点积最大的候选 Cell。
   4. 将该 Cell 标为森林。
4. 如果外围没有合法候选，则当前森林提前结束。

伪代码：

```cpp
void GrowForestPatch(int32 TargetCount)
{
    int32 Seed = PickRandomValidForestSeed();
    TArray<int32> ForestCells;
    AddForest(Seed, ForestCells);

    while (ForestCells.Num() < TargetCount)
    {
        FVector CenterDir = ComputeAverageCenter(ForestCells);
        TArray<int32> Frontier = CollectForestFrontier(ForestCells);
        if (Frontier.IsEmpty()) break;

        int32 NextCell = PickFrontierCellWithMaxDot(Frontier, CenterDir);
        AddForest(NextCell, ForestCells);
    }
}
```

### 9.6 森林与其他地形的关系

- 森林不会生成到基地保护区。
- 森林不会覆盖山脉。
- 森林只从当前仍为平原的 Cell 中生长。
- 多片森林可以相邻，最终视觉上合并成大森林也允许。
- 初版不要求森林之间保持距离。

---

## 10. 随机选择策略

### 10.1 可复现随机

所有随机都必须来自 `FRandomStream Rng`：

```cpp
Rng.Initialize(Settings.RandomSeed);
```

不得混用不可控的全局随机函数，否则同一 Seed 无法稳定复现。

### 10.2 合法候选池

为避免无限循环，推荐每次选择种子时先构建候选数组，再随机抽取。

山脉初始边候选：

```text
所有满足条件的相邻 Cell Pair：
- 两端都在保护区外
- 两端都不是五边形
- 两端都不是山脉
```

森林种子候选：

```text
所有满足条件的 Cell：
- 在保护区外
- 当前是平原
- 不是五边形
```

如果候选池为空，则跳过本次生成并输出 Verbose / Warning 日志。

---

## 11. 输出与日志

生成完成后建议输出：

```text
[WorldGen] SimpleGameplay OK, Cells=N, Bases=12, Protected=P, Plain=X, Mountain=Y, Forest=Z, Seed=S
```

还可以输出：

- 实际生成山脉条数。
- 山脉平均实际节点数。
- 实际生成森林片数。
- 森林平均实际节点数。
- 因保护区 / 候选不足提前停止的次数。

这些日志有助于调整默认参数。

---

## 12. 实现建议

### 12.1 建议新增或替换的函数

可以把旧 `Step_*` 地理流水线替换为更直接的函数：

```cpp
void InitSimpleTerrainAsPlain();
void CollectSimpleBaseCells();
void BuildSimpleProtectedCells(int32 Radius);
void GenerateMountainStrips();
void GenerateForestPatches();
void WriteSimpleTerrainToCellData();
```

山脉辅助函数：

```cpp
bool PickRandomValidMountainSeedPair(int32& OutA, int32& OutB) const;
bool TryGrowMountainTip(FGrowTip& Tip, int32& RemainingCount);
int32 PickMountainNextCell(int32 PrevCellId, int32 CurCellId);
```

森林辅助函数：

```cpp
bool PickRandomValidForestSeed(int32& OutSeed) const;
FVector ComputeForestCenter(const TArray<int32>& ForestCells) const;
void CollectForestFrontier(const TArray<int32>& ForestCells, TSet<int32>& OutFrontier) const;
int32 PickBestForestFrontierCell(const TSet<int32>& Frontier, const FVector& CenterDir);
```

### 12.2 中间状态

推荐在 `FWorldGenerator` 中维护：

```cpp
TArray<ETerraSimpleTerrainType> SimpleTerrainField;
TSet<int32> ProtectedCellSet;
```

如果为了性能避免 `TSet`，也可以使用：

```cpp
TArray<bool> bProtectedCell;
```

由于 `SubdivisionLevel=3` 下 Cell 数量较小，两种方式都可以接受。

### 12.3 与玩法层对接

玩法层需要快速查询某个 Cell 的三地形：

```cpp
ETerraSimpleTerrainType GetSimpleTerrainType(int32 CellId) const;
```

如果暂时不新增接口，也可以从 `FCellGeoData.TerrainTag` 映射回简易地形，但这不是长期推荐方案。

---

## 13. 验收标准

实现完成后应满足：

- **固定拓扑**：简易玩法默认使用 `SubdivisionLevel=3`。
- **基地安全**：12 个五边形及其半径 2 范围内全部是平原。
- **山脉形态**：山脉整体呈条状，且不进入基地保护区。
- **森林形态**：森林整体呈块状，且不进入基地保护区、不覆盖山脉。
- **编辑器参数**：可在编辑器里调整山脉数量、山脉平均节点数、森林数量、森林平均节点数。
- **随机可复现**：同一 `RandomSeed` 生成结果一致。
- **玩法可用**：输出地形能直接支持骑兵山脉限制、弓兵山脉加成、森林防远程规则。

---

## 14. 暂不纳入初版的内容

以下内容不属于简易玩法 `WorldGen` 初版范围：

- 海洋 / 海岸。
- 板块构造。
- 高程连续场。
- 湿度 / 温度。
- 风带 / 雨影。
- 河流 / 湖泊。
- 多生物群系评分。
- 资源分布。
- 基地周围特殊 Trait。
- 山脉或森林的阵营平衡优化。
