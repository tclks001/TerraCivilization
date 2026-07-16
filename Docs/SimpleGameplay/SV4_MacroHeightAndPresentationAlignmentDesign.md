# SV4：宏观高度与表现层对齐设计稿

> 状态：实现稿。
>
> 编码：UTF-8，简体中文。
>
> 前置：SV1 连续表面点击、SV2 高亮、SV3 SDF 地表材质均已验收。

---

## 1. 目标与边界

SV4 为连续基础表面加入仅由现有 `CellTopology + FCellGeoData` 推导的低频宏观高度，并将棋子和相机 Cell 焦点对齐到同一表面查询。它不改写 `CellId`、地形规则、寻路、战斗、UI 或 WorldGen 输出。

本阶段明确不做：

- 高频宏观噪声、尖峰贴片、岩层、草、树或新的 HISM Decor；
- 河流、湖泊、湿润岸带、独立水面或水文数据；
- WPO/PDO、运行时网格布尔、动态地形编辑；
- Gameplay 地形高度、移动消耗或碰撞规则。

## 2. 高度场

统一高度接口仍为：

```text
QuerySurface(UnitDirection) -> WorldPosition, WorldNormal, SurfaceRadiusCM, CellId
```

`UnitDirection` 是 Actor 局部单位方向。总半径为：

```text
SurfaceRadius = GlobeRadiusCM + MountainHeight + ForestHeight
```

平原不贡献低频高度。森林仍由 Forest Cell 中心的球面 SDF 核构成；山地则严格由 WorldGen 山脉生长过程输出的山脊弧段数组构成，禁止 TerrainVisual 从 Mountain Cell 邻接关系反推山脊。

每条山脊段的端点为相邻 Mountain Cell 的单位中心 `a`、`b`，并取两点之间的短测地线弧。对待采样方向 `p`：

```text
greatCircleNormal = normalize(cross(a, b))
foot = normalize(p - dot(p, greatCircleNormal) * greatCircleNormal)

if angle(a, foot) + angle(foot, b) <= angle(a, b) + epsilon:
    distanceToSegment = angle(p, foot)
else:
    distanceToSegment = min(angle(p, a), angle(p, b))

distanceToRidge = min(distanceToSegment for every ridge segment)
mountainWeight = pow(saturate(1 - distanceToRidge / supportRadius), MountainExponent)
mountainHeight = MountainPeakHeight * mountainWeight
```

`foot` 是 `p` 到山脊所在大圆的垂足候选。若它不在短弧内部，算法退化为到较近端点的距离。山脊线上任意点的距离均为零，因此不会再在相邻 Mountain Cell 中心之间形成马鞍塌陷。

| 地形 | 默认峰值 | 指数 | 视觉意图 |
| --- | ---: | ---: | --- |
| Mountain | 2400 cm | 3.0 | 高次横向衰减使高度从山脊线向两侧快速下降，形成较尖的连续山脊。 |
| Forest | 500 cm | sigmoid，默认 steepness=8 | 归一化 sigmoid 在森林边界为零、中心为一，避免 `sqrt` 在边界的突兀抬升。 |
| Plain | 0 cm | - | 保持当前基准球半径。 |

高度参数为 `APlanetTessellatedMesh` 的 `Terrain Visual|SV4 Macro Height` 属性；更改属性后沿用已有 `OnConstruction -> RebuildAll_`，结果由 WorldGen seed 和当前三类地形确定。

森林令 `interior = saturate(1 - distance / supportRadius)`，再计算端点归一化 logistic：

```text
L(x) = 1 / (1 + exp(-steepness * (x - 0.5)))
forestWeight = (L(interior) - L(0)) / (L(1) - L(0))
```

这保证森林边界严格回到零高度，中心严格达到峰值，并让边缘导数小于 `sqrt(interior)`，从平原进入森林时更平缓。

### 2.1 山脊数据所有权与动态更新

`FWorldGenerator` 在 `GrowMountainStrip` 中记录初始相邻种子边及每次 Tip 延长边，输出 `TArray<FIntPoint>`；每一项的 `X/Y` 是一条山脊段的两个 CellId。`UPlanetGameplayComponent` 始终持有这份数组，TerrainVisual 只读取 Gameplay 所持数组。

未来回合制动态造山只修改 Gameplay 的山脊数组，然后根据受影响山脊段的包围角范围更新对应连续表面顶点高度。SV4 首版仍允许完整表面重建和碰撞重建；数据格式不要求通过 Mountain Cell 分布反推路径，因此可以自然升级为少量顶点偏移和局部网格块更新。

## 3. 几何和法线

`UTerrainVisualSurfaceComponent::RebuildBaseSphere` 对每个 sub=7 顶点调用 `ITerrainSurfaceQuery`：

```text
VertexPosition = UnitDirection * QuerySurface(UnitDirection).SurfaceRadiusCM
VertexNormal   = QuerySurface(UnitDirection).WorldNormal
```

法线不是简单径向向量。高度场以两个切线方向的小球面差分求表面位置，再叉乘取得朝外法线。这样山坡会接收正确的天光、阴影和 PBR 响应，且共享几何顶点没有接缝。

Section 1 的高亮/材质展开顶点始终复用 Section 0 已计算的位置和法线；SV2/SV3 的 Cell Context 编码不变。

## 4. 表现层迁移

| 消费者 | SV4 行为 |
| --- | --- |
| 地表碰撞与点击 | 仍由连续 Procedural Mesh 的 Section 0 接收；命中点解析 `UnitDirection -> CellId` 的逻辑不变。 |
| 高亮与 SDF 材质 | 沿位移后的 Section 1 渲染；不需要修改现有 HLSL。 |
| 棋子 | 连续模式下直接查询 `TerrainVisualField` 的高度位置，额外偏移和旋转 Up 始终使用标准球体径向方向；不再走 P2.5 HISM 多射线，也不使用坡面法线使棋子倾斜。Legacy 模式保留原链路。 |
| 相机 Cell 焦点 | 始终使用未形变标准球体的径向位置和法线；不得读取 `SurfaceQuery` 的高度或坡面法线，避免轨道相机因地形法线突变而旋转。 |
| HISM | 暂不移动或重放置旧 HISM；连续模式下它仍只作为隐藏的兼容高度/Debug 资产。SV6 再替换为无碰撞 Decor。 |

## 5. 材质编辑

SV4 不要求编辑 `M_TerrainVisual_SurfaceTerrain`，也不新增纹理、Custom 输入或 HLSL。顶点已被 C++ 位移，现有材质继续用 `WorldPos - PlanetCenter` 取得单位方向，故 Cell SDF、高亮与 SV3 地表细节会自然贴合形变后的表面。

不要在材质中添加 World Position Offset：否则网格碰撞、棋子位置和可见地表会失去一致性。

## 6. 验收

1. 固定 seed 下，Mountain Strip 的山脊弧线上高度连续且不出现 Cell 中心之间的马鞍塌陷；Forest 连通区域是较低、较缓的丘陵；Plain 保持基准球面。
2. 旋转镜头观察山坡，光照随非径向法线变化，无 sub=7 三角形接缝和裂缝。
3. 连续模式下棋子站在山坡/丘陵上，脚底不悬浮、不埋入，朝上方向保持标准球体径向；Legacy HISM 模式仍使用原 P2.5 射线。
4. hover、点击、移动、吃子、Undo、回合推进和 UI 与 SV3 一致；地形高度绝不改变行动合法性。
5. 相机选中 Cell、战争区焦点和自由焦点始终按标准球体稳定计算，不因山体高度或坡面法线发生额外旋转。
6. 编辑器、PIE、退出 PIE、重新打开地图和重新保存均可重建连续表面，不把 Procedural Mesh 顶点缓存进 External Actor 包。

## 7. 后续

SV5 以该高度场生成仅作装饰的河道 SDF：从高处源点沿下降方向流向低地，可汇流但不构成 Gameplay 水域。SV6 才接入长条山脊、岩石、草和树等 Nanite HISM Decor。
