# SV9：HISM + SDF 山脊与尖峰资产设计稿

> 编码：UTF-8，简体中文。前置 SV7、SV8 已验收。资产生成见 [SphericalTileAssetGeneratorDesign.md](SphericalTileAssetGeneratorDesign.md)，操作见 [TerraSphericalTileGenerator使用说明.md](TerraSphericalTileGenerator使用说明.md)。

## 1. 目标与边界

SV9 首次把跨 Cell 地形外壳从验证实例升级为正式 Ridge/Peak HISM：

- WorldGen 的 `MountainRidgeSegments` 决定 Ridge 位置和朝向。
- Mountain Cell 与稳定随机决定 Peak 位置。
- Ridge/Peak 复用 SV8 HISM+SDF 材质和 SV7 空间投影交互。
- 新组件开启查询碰撞，使其表面可接收鼠标 Hover/Click。
- Gameplay、WorldGen 规则、CellTopology、UI 和旧三种 Tile HISM 不改。

首版每类只使用一个 StaticMesh，不做资产池或视觉变体切换；但生成器形态参数、`SV9AssetVariantSeed` 和稳定候选顺序为后续差异化留出接口。

## 2. 组件与兼容

Actor 持有：

| 组件 | 职责 |
| --- | --- |
| `SV8VerificationRidgeMidpointHISMComp` | 原 SV8 验证组件原地演进为正式 Ridge HISM。保留 Native Default Subobject 名称，避免已有 Blueprint/地图模板失配。 |
| `SV9PeakHISMComp` | 新增 Peak HISM。 |

两者只在 `HISMSDFExperiment` 可见。每个组件只有一个 StaticMesh；后续多变体需要按 StaticMesh 建立组件池，不把不同 Mesh 塞进同一 HISM。

## 3. Ridge 摆放

对山脊段两端 Cell 单位向量 `A/B`：

```text
Up      = normalize(A+B)
Tangent = normalize(B-Up*dot(B,Up))
Rotation= MakeFromXZ(Tangent,Up)
```

`Up` 是两端测地线短弧中点；`Tangent` 是该点朝向 B 的大圆切线。Rotation 同时满足：

```text
Asset local +X -> Ridge geodesic tangent
Asset local +Z -> Planet radial up
```

实例 Location 保持 Actor 局部原点。生成资产本身位于 `BaseRadius` 球壳上，实例通过统一 Scale 放大到星球半径：

```text
TargetRadius = GlobeRadiusCM
             + HISMTileRadiusOffsetCM
             + SV9TerrainAssetRadiusOffsetCM

Scale = TargetRadius/HISMTileSourceRadiusCM
      * HISMTileAdditionalUniformScale
      * SV9TerrainAssetUniformScale
```

每条有效 RidgeSegment 恰好生成一个实例。首版不沿长线再细分；WorldGen 当前段本身连接相邻 Cell，资产角宽度应覆盖该距离并允许端部重叠。

## 4. Peak 摆放

候选集合是所有 `SimpleTerrainType == Mountain` 的 Cell。按排序后的 CellId 顺序，用：

```text
Seed = WorldGenSettings.RandomSeed XOR SV9AssetVariantSeed
spawn when FRand <= SV9PeakSpawnProbability
```

生成后以 BFS 阻塞 `SV9PeakMinCellSteps` 范围内的邻居，避免相邻 Mountain Cell 每格一个尖峰。Peak 局部 `+Z` 对齐 Cell 径向，并加入绕径向的稳定随机 Yaw；圆锥首版旋转不明显，但未来不对称 Peak 可直接复用该变换。

当前 `bEnableSV9AssetVariantSelection` 仅声明未来变体路线并进入诊断日志，不伪造多资产选择。后续实现时应使用稳定哈希选择 `MeshVariantIndex`，并为每种 Mesh 建一个 HISM 组件。

## 5. 材质

Ridge/Peak 通过 `ApplyHISMSDFExperimentMaterials_` 获得各自 MID，注入与 Plain/Forest/Mountain 相同的：

- Cell Direction/Terrain/Highlight LUT。
- SV8 Topology Node/Leaf LUT。
- 河网与湖泊 LUT。
- 三平面纹理、Tint、粗糙度和法线参数。
- `HISMSDFProjectionRadiusCM` 与球心。

材质查找 Cell 完全基于像素世界方向，不读取 RidgeSegment、Peak CellId、InstanceId 或 Owner/Neighbor PICD。因此一个 Ridge 横跨两 Cell 时，两侧会自动得到各自 SDF 地形与高亮。

### 5.1 径向基础法线契约

Plain/Forest/Mountain Tile 与 Ridge/Peak 必须共享同一基础法线语义：StaticMesh 顶点法线是位移前的球面径向，而不是各资产最终位移几何的斜面法线。运行时仍以同一份 HISM+SDF MID 覆盖全部材质槽，主材质保持 `Tangent Space Normal=true`。

当前材质的 `WorldAlignedNormal -> FlattenNormal` 在 `TerrainVisualNormalStrength=0` 时输出切线空间 `(0,0,1)`，转换到世界空间后正好等于 StaticMesh 顶点法线。因此：

- 旧 Plain/Forest/Mountain 的径向顶点法线会得到连续球面法线。
- Ridge/Peak 若由生成器重算最终几何法线，会在强度为 0 时仍显示陡坡法线；这是生成器 Bug，不是 SV9 允许的视觉差异。
- 生成器必须把参数域点的球面方向写入 Primary Normal Overlay，StaticMesh Build 设置 `bEnableRecomputeNormals=false`，仅依据 UV0 和显式径向法线重算切线。
- SV10 仍可增加“径向基础法线、几何斜率信息与 SDF 高频法线”的高级混合，但不能以此推迟 SV9 的径向基础法线修复。

## 6. 碰撞与交互

可见状态下两组件设置：

```text
CollisionEnabled = QueryOnly
ObjectType       = WorldStatic
Visibility       = Block
```

还需同时满足 `bEnableHISMTileCollision=true`，且生成的 StaticMesh 含复杂碰撞。

鼠标路径不做 InstanceId 映射：

```text
Visibility trace hits Ridge/Peak HISM
 -> Hit.ImpactPoint
 -> ResolveTerrainCellFromWorldPosition
 -> FSphereTopologyQuery::FindNearestCell
 -> 原 Hover/Click/Gameplay/Highlight LUT
```

P2.5 棋子高度代码仍显式筛选 Plain/Forest/Mountain 三个 Tile HISM；虽然 Ridge/Peak 参与 WorldStatic 查询，结果会被该表现层过滤，不改变现有棋子高度。

## 7. 编辑器参数

| 参数 | 默认 | 说明 |
| --- | ---: | --- |
| `bEnableSV9RidgeHISM` | true | 生成并显示 Ridge。 |
| `SV9RidgeStaticMesh` | null | 球面 Ridge 资产。 |
| `bEnableSV9PeakHISM` | true | 生成并显示 Peak。 |
| `SV9PeakStaticMesh` | null | 球面 Peak 资产。 |
| `SV9PeakSpawnProbability` | 0.24 | Mountain Cell 尖峰概率。 |
| `SV9PeakMinCellSteps` | 1 | 已选 Peak 的邻接排斥距离。 |
| `SV9TerrainAssetRadiusOffsetCM` | 0 | 新资产独立径向微调。 |
| `SV9TerrainAssetUniformScale` | 1 | 新资产额外统一缩放。 |
| `bEnableSV9AssetVariantSelection` | false | 多变体预留开关，首版不执行切换。 |
| `SV9AssetVariantSeed` | 9137 | Peak 随机与未来变体的稳定 Seed。 |

## 8. 日志与验收

重建日志：

```text
[TerrainVisual][SV9] Rebuilt terrain asset HISM.
```

关注 `Segments/RidgeInstances/PeakCandidates/PeakInstances/Mesh/Radius/Scale`。资产槽为空时对应实例数为 0，不阻断旧 Tile 地形和 Gameplay。

验收：

1. 每条有效山脊段生成一个 Ridge，位置在两端 Cell 测地线中点，X 沿测地线。
2. Peak 只位于 Mountain Cell，数量受概率和最小间距控制。
3. 两类资产自动使用 SV8 材质，跨实例颜色、粗糙度、河网和高亮连续。
4. 在 Ridge/Peak 上 Hover 和 Click 时，返回命中位置所对应的 Cell，而非资产所属 Cell。
5. Legacy HISM Debug、ContinuousSurface、Gameplay、UI 和棋子高度不回归。
6. 关闭总 HISM Collision 后新增资产不再抢占鼠标查询；重新开启后恢复。
7. 将 `TerrainVisualNormalStrength` 设为 0 并查看 `World Normal`：Ridge/Peak 必须和 Plain/Forest/Mountain 一样输出连续径向法线，不得保留山脊斜面或尖峰轮廓。

## 9. 排错与性能

| 现象 | 根因/检查 |
| --- | --- |
| Ridge 全朝同一方向 | 只做了 `UpVector -> Midpoint` 旋转，没有把 local X 对齐测地线 Tangent。 |
| Ridge 位置正确但尺寸异常 | 资产 `BaseRadius` 与 `HISMTileSourceRadiusCM` 不一致。 |
| Peak 出现在非山地 | 候选错误地来自全 Cell；必须过滤 WorldGen Mountain。 |
| 点击得到端点 Cell | 不应读取 InstanceId/Segment 端点；检查 SV7 ImpactPoint 查询路径。 |
| 可见但无法命中 | StaticMesh 无碰撞、组件未 QueryOnly、Visibility 未 Block 或总开关关闭。 |
| 资产有自己的岩石颜色 | HISM+SDF MID 未覆盖该组件的全部材质槽。 |
| `NormalStrength=0` 时 Ridge/Peak 仍保留几何轮廓 | 当前引用的是旧版生成器资产，或 StaticMesh Build 又重算法线；使用修复后的生成器删除/换路径重建 Ridge/Peak，确认显式径向 Normal Overlay 被保留。 |
| `NormalStrength>0` 时出现方向性法线异常 | 检查 NormalDX 导入设置和切线重算；切线必须依据显式径向法线与 UV0 构建。 |

性能首版上限约为 `RidgeSegmentCount + MountainCellCount*Probability` 个额外实例，只有两个 HISM draw/cluster 集合。CPU Rebuild 是线性扫描；像素材质成本与 SV8 相同，不因 InstanceId 增加 LUT 查询复杂度。碰撞实例增加会扩大场景查询 BVH，但当前 642 Cell 规模下数量有限；后续多变体应控制 HISM 组件数量，而不是每实例创建组件。
