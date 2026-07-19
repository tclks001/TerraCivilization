# TerraSphericalTileGenerator 使用说明

> 编码：UTF-8，简体中文。本文只说明实际操作；算法和资产契约见 [SphericalTileAssetGeneratorDesign.md](SphericalTileAssetGeneratorDesign.md)。

## 1. 打开工具

1. 编译项目并重启 Unreal Editor。
2. 在主菜单选择 `Tools -> Terra Spherical Terrain Asset Generator`。
3. 面板参数会保存在当前项目的编辑器用户设置中。

仍可通过 Blueprint 调用 `Generate Spherical Tile Static Mesh Asset` 生成旧 Tile；新 UI 和自动化应调用 `Generate Spherical Terrain Static Mesh Asset`。

## 2. 准备纹理

可使用同一套 Rock PBR：

| 字段 | 纹理要求 |
| --- | --- |
| `BaseColorTexture` | BaseColor/Color，通常 `sRGB=true`。 |
| `NormalTexture` | 只使用 NormalDX；Normalmap 压缩，`sRGB=false`。 |
| `RoughnessTexture` | Roughness，`sRGB=false`。 |
| `HeightTexture` | Height/Displacement，建议 `sRGB=false`，必须保留 Source 数据。 |

Height 可为空。为空时 Ridge/Peak 仍有程序化基础高度，只是不叠加纹理位移。

## 3. 准备父材质

若勾选 `bCreateMaterialInstance`，父材质至少暴露：

```text
BaseColorTexture
NormalTexture
RoughnessTexture
HeightTexture
```

`NormalTexture` Sample 的 `Sampler Type` 必须是 `Normal`。SV9 运行时会用统一 HISM+SDF 材质覆盖资产槽，因此父材质可只用于生成后预览。

## 4. 生成 Ridge

在面板中先使用以下起始值：

```text
AssetShape                     = Ridge
SubdivisionsPerSide            = 128
BaseRadius                     = 100
SphereExtensionAmplitude       = 8
PatchAngularSizeDegrees        = 18
RidgeHeightCM                  = 42
RidgeProfileExponent           = 2.5
RidgeEndTaperFraction          = 0
RidgeCenterlineWaviness        = 0.08
RidgeHeightmapAmplitudeCM      = 10
ShapeVariationSeed             = 1337
ShapeNoiseStrength             = 0
ShapeNoiseFrequency            = 3.5
RidgeStaticMeshAssetPathAndName= /Game/Generated/SphericalTiles/SV9/SM_SV9_Ridge
bEnableCollision               = true
bEnableNanite                  = true
```

点击 `Generate Ridge`。打开资产后检查：

- 长轴是局部 X。
- 脊顶在中间附近，可轻微左右摆动。
- 局部 Y 两侧边缘回到基础球面并继续受压边控制。
- 默认 X 两端仍可保持山高，便于多个实例首尾遮挡。
- Collision 视图存在复杂碰撞，Nanite 已启用。

如果希望短山脊两端落地，把 `RidgeEndTaperFraction` 调到 `0.10~0.25`。

## 5. 生成 Peak

推荐起始值：

```text
AssetShape                    = Peak
SubdivisionsPerSide           = 128
BaseRadius                    = 100
SphereExtensionAmplitude      = 8
PatchAngularSizeDegrees       = 18
PeakHeightCM                  = 55
PeakProfileExponent           = 2.0
PeakHeightmapAmplitudeCM      = 14
ShapeVariationSeed            = 1337
ShapeNoiseStrength            = 0
ShapeNoiseFrequency           = 3.5
PeakStaticMeshAssetPathAndName= /Game/Generated/SphericalTiles/SV9/SM_SV9_Peak
bEnableCollision              = true
bEnableNanite                 = true
```

点击 `Generate Peak`。中心应形成尖圆锥，圆形外圈回到基础球面；提高 `PeakHeightCM` 会整体增高，提高 `PeakProfileExponent` 会让山峰更窄。

本轮先保持 `ShapeNoiseStrength=0`。未来制作变体时，先使用较小的 `0.05~0.20`，为每个 Seed 指定不同输出路径；相同 Seed 和参数应得到相同形态。

## 6. 接入 SV9 Actor

在 `APlanetTessellatedMesh` 实例或其 Blueprint 的 Details 中：

1. 将 `TerrainVisualMode` 设为 `HISMSDFExperiment`。
2. 将生成的 Ridge 赋给 `SV9RidgeStaticMesh`。
3. 将生成的 Peak 赋给 `SV9PeakStaticMesh`。
4. 开启 `bEnableSV9RidgeHISM`、`bEnableSV9PeakHISM`、`bEnableHISMTileRendering` 和 `bEnableHISMTileCollision`。
5. 调用 `Rebuild` 或修改一个 WorldGen 参数触发重建。

初始摆放参数：

```text
SV9PeakSpawnProbability       = 0.24
SV9PeakMinCellSteps           = 1
SV9TerrainAssetRadiusOffsetCM = 0
SV9TerrainAssetUniformScale   = 1
SV9AssetVariantSeed           = 9137
```

Output Log 应出现：

```text
[TerrainVisual][SV9] Rebuilt terrain asset HISM.
```

其中 `RidgeInstances` 应接近有效 `MountainRidgeSegment` 数，`PeakInstances` 应大于零但明显少于 `PeakCandidates`。

## 7. 验收

1. 每条山脊段中点有一个 Ridge，局部 X 沿两端 Cell 的测地线。
2. 相邻 Ridge 的长边互相遮挡，Y 两侧落回山脉基础 Tile。
3. Peak 只出现在 Mountain Cell，且不会密集出现在相邻 Cell。
4. Ridge/Peak 自动使用 SV8 HISM+SDF 材质，颜色、粗糙度、河流和高亮按世界方向连续。
5. 鼠标停在 Ridge/Peak 上仍能 Hover；点击按命中点方向解析正确 Cell 并进入原 Gameplay 流程。
6. 棋子高度仍由原 Tile HISM 射线筛选，不会因 SV9 尖峰碰撞改变已有表现逻辑。
7. 将 `TerrainVisualNormalStrength` 设为 0，在 `Buffer Visualization -> World Normal` 中 Ridge/Peak 与旧三类 Tile 都呈连续径向法线。

修复径向法线契约前生成的 Ridge/Peak 资产不会随 C++ 自动更新。由于生成器拒绝覆盖已有对象，请先确认引用后手工删除旧资产，或修改输出路径生成新资产，再重新赋给 `SV9RidgeStaticMesh`、`SV9PeakStaticMesh` 并 Rebuild。

## 8. 常见问题

| 现象 | 处理 |
| --- | --- |
| 点击按钮提示创建失败 | 输出对象路径已存在或无效；换名称，或在确认无引用后手工删除旧资产再生成。 |
| Ridge/Peak 不显示 | 检查资产槽、模式、两个 Enable、总 HISM Rendering 开关和 SV9 日志实例数。 |
| 有显示但鼠标穿透 | 生成资产时开启碰撞，并确认 Actor 的 `bEnableHISMTileCollision=true`。 |
| Ridge 与山脊垂直 | 资产必须沿局部 X 生成；不要使用旧 Decor Ridge 的不同 Pivot/尺度资产代替。 |
| Ridge 两侧露缝 | 适当增加 `SphereExtensionAmplitude` 或统一缩放，确认 Y 边形态位移为零。 |
| Peak 太多或太少 | 调 `SV9PeakSpawnProbability`；过密时同时提高 `SV9PeakMinCellSteps`。 |
| SDF 材质没有覆盖 | 检查 `TerrainVisualHISMSDFMaterial` 和 SV8 Topology LUT ready 日志。 |
| `NormalStrength=0` 时仍看到 Ridge/Peak 斜面法线 | 当前仍在使用旧资产；用修复后的生成器删除/换路径重新生成，并重新赋值后 Rebuild。 |
