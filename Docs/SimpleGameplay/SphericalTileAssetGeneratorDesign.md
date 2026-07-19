# TerraSphericalTileGenerator 插件设计稿

> 编码：UTF-8，简体中文。
>
> 本稿维护插件职责、资产契约、几何算法和扩展边界。逐步操作见 [TerraSphericalTileGenerator使用说明.md](TerraSphericalTileGenerator使用说明.md)。

## 1. 目标与边界

`TerraSphericalTileGenerator` 是 Editor-only GeometryScript 资产生成插件。它离线生成以局部原点为球心、局部 `+Z` 为径向中心的高细分 StaticMesh，供运行时 HISM 实例化。

当前支持三种形态：

| `AssetShape` | 用途 | 形态 |
| --- | --- | --- |
| `Tile` | 旧 Plain/Forest/Mountain 地块 | 带压边和高度图起伏的球面方形 patch。 |
| `Ridge` | SV9 连续山脊外壳 | 沿局部 `+X` 延伸、从中间折起的球面网格片。 |
| `Peak` | SV9 山脉尖峰 | 中心尖、外圈落回基础球面的圆锥状 patch。 |

插件只负责生成和保存资产，不参与 WorldGen、Gameplay、HISM 摆放、点击或材质 SDF 查询。SV9 运行时只消费生成后的 StaticMesh。

## 2. 坐标与资产契约

所有形态统一使用：

```text
Local origin = 资产球心
Local +Z     = patch 中心的球面径向
Local +X     = Ridge 山脊延伸方向
Local +Y     = Ridge 横跨山脊的方向
BaseRadius   = 资产基础球面半径，默认 100 cm
UV0          = 参数域 (u,v) 的 0..1 映射
```

运行时不把实例平移到 Cell 中心，而是让实例保持在 Planet Actor 局部原点，通过旋转把局部 `+Z` 对齐目标方向，并以 `TargetRadius/BaseRadius` 统一缩放。因此 Ridge/Peak 和旧 Tile 必须使用相同的 `BaseRadius` 契约。

## 3. 模块结构

```text
TerraSphericalTileGenerator (Editor module)
├─ FTerraSphericalTileGeneratorModule
│  ├─ Tools 菜单入口
│  └─ Nomad Tab
├─ STerraSphericalTileGeneratorPanel
│  ├─ IDetailsView
│  ├─ Generate Selected
│  ├─ Generate Ridge
│  └─ Generate Peak
├─ UTerraSphericalTileGeneratorSettings
│  └─ EditorPerProjectUserSettings 持久化
└─ UTerraSphericalTileGeneratorLibrary
   ├─ GenerateSphericalTerrainStaticMeshAsset
   └─ GenerateSphericalTileStaticMeshAsset（旧 API 兼容）
```

UI、Blueprint Function Library 和后续自动化统一调用同一生成入口。旧 `GenerateSphericalTileStaticMeshAsset` 强制生成 `Tile`，已有 Editor Utility Blueprint 不会因新增形态改变语义。

## 4. 公共球面网格基础

规则网格包含 `(N+1)^2` 个顶点和 `2*N^2` 个三角形：

```text
u = x/N
v = y/N
sx = (2u-1) * tan(PatchAngularSize/2)
sy = (2v-1) * tan(PatchAngularSize/2)
Dir = normalize(float3(sx, sy, 1))
Position = Dir * Radius
```

基础半径为：

```text
Radius = BaseRadius
       + EdgeOcclusionOffset
       + AnalyticShapeOffset
       + HeightTextureOffset
```

`EdgeOcclusionOffset` 沿用旧 Tile 的中心鼓起、外缘压低逻辑，使实例边缘可以被相邻 patch 自然遮挡。Tile/Ridge/Peak 都把位移前的球面方向显式写入 Primary Normal Overlay，StaticMesh Build 阶段保留该径向法线并只重算 MikkTSpace 切线。这是 HISM+SDF 材质的基础法线契约：`TerrainVisualNormalStrength=0` 时，所有地形资产都必须退化为同一连续径向法线，而不是暴露各自的位移几何法线。

## 5. Ridge 算法

### 5.1 横截面

Ridge 以局部 `+X` 为山脊方向。脊顶位置允许沿 `u` 做低频侧向摆动，得到 `CrestV(u)`。左右两侧分别归一化到自己的边缘：

```text
if v <= CrestV:
    CrestDistance = (CrestV-v)/CrestV
else:
    CrestDistance = (v-CrestV)/(1-CrestV)

CrossProfile = pow(saturate(1-CrestDistance), RidgeProfileExponent)
```

该分段归一化保证 `v=0` 和 `v=1` 的形态高度严格为零，即局部 `Y` 正负两侧都落回基础球面。脊顶即使侧向摆动，也不会抬起任一侧边缘。

### 5.2 X 端部与高度图

默认 `RidgeEndTaperFraction=0`，局部 `X` 两端可以保持高度，由相邻山脊实例互相遮挡。需要独立短山脊时可把该参数调大，使端部用 `smoothstep` 落地。

```text
ShapeOffset = RidgeHeightCM * CrossProfile * EndMask
TextureOffset = (HeightSample-0.5) * 2
              * RidgeHeightmapAmplitudeCM
              * CrossProfile * EndMask
```

高度图位移与剖面遮罩相乘，因此不会破坏 `Y` 两侧落地契约。没有 Height 纹理时位移为零，不按黑色高度图处理。

## 6. Peak 算法

Peak 使用圆形参数距离：

```text
x = 2u-1
y = 2v-1
r = length(float2(x,y))
PeakProfile = pow(saturate(1-r), PeakProfileExponent)
```

`r>=1` 的外圈高度为零，得到中心尖、四周落地的尖圆锥 patch：

```text
ShapeOffset = PeakHeightCM * PeakProfile
TextureOffset = (HeightSample-0.5) * 2
              * PeakHeightmapAmplitudeCM
              * PeakProfile
```

首版不做不对称双峰或侵蚀缺口。生成器已暴露 `ShapeVariationSeed/ShapeNoiseStrength/ShapeNoiseFrequency`：本轮默认 Noise Strength 为 0；以后只需更改 Seed 和输出路径即可生成确定性 Ridge/Peak 变体，且噪声始终乘在基础 Profile 上，不破坏落地边缘。

## 7. PBR、碰撞与 Nanite

- `BaseColor/NormalDX/Roughness/Height` 可写入生成的材质实例。
- Normal 必须是 DirectX 切线空间法线，纹理导入为 Normalmap、`sRGB=false`，父材质 Sampler Type 为 `Normal`。
- Height 纹理必须保留可读 Source 顶层 Mip；支持 `G8/G16/BGRA8/RGBA16/RGBA16F`。
- SV9 Ridge/Peak 应开启碰撞，否则 HISM 组件即使 `QueryOnly` 也没有可命中的复杂碰撞数据。
- 高细分 Ridge/Peak 推荐开启 Nanite；是否开启由 `bEnableNanite` 控制。
- 生成器创建 StaticMesh 后由运行时 HISM+SDF 材质覆盖其材质槽，因此资产自带 MI 主要用于编辑器单体预览。

## 8. UI 与保存

插件注册 `Tools -> Terra Spherical Terrain Asset Generator`。面板用 `IDetailsView` 显示同一个强类型设置对象，参数保存在 `EditorPerProjectUserSettings`。

按钮语义：

- `Generate Selected`：按当前 `AssetShape` 生成。
- `Generate Ridge`：切换到 Ridge 并使用 Ridge 输出路径生成。
- `Generate Peak`：切换到 Peak 并使用 Peak 输出路径生成。

输出对象路径根据形态选择：Tile 使用旧通用路径，Ridge/Peak 使用各自的 SV9 路径。插件不会把 DynamicMesh 保存到地图；DynamicMesh 只是编辑器内临时烘焙源，最终资产是 StaticMesh。

## 9. 验收与排错

验收：

1. Editor 目标编译，Tools 菜单可打开面板，参数重开后保留。
2. 旧 Tile Blueprint API 继续生成旧球面 patch。
3. Ridge 沿局部 X 延伸，Y 两侧落地，默认 X 两端不强制落地。
4. Peak 中心形成尖圆锥，外圈落地。
5. Height 纹理能在形态遮罩内产生位移，不抬起接缝边缘。
6. StaticMesh 可开启 Nanite 和复杂碰撞，并保留 UV0、显式径向法线和据此重算的切线。
7. 在 HISM+SDF 材质下把 `TerrainVisualNormalStrength` 设为 0，Tile/Ridge/Peak 的 `World Normal` 均连续径向；Ridge/Peak 不得残留尖峰或陡坡几何法线。

| 现象 | 检查 |
| --- | --- |
| Tools 菜单没有入口 | 插件是否在 `.uproject` 启用，Editor 是否在重新编译后重启。 |
| Ridge 横向摆放 | 资产和 SV9 都约定 `+X=山脊方向`；检查资产是否由 Ridge 模式生成。 |
| Ridge Y 边缘悬空 | 确认使用最新版分段 `CrestV` 剖面，且高度图位移乘了同一 Profile。 |
| 无 Height 纹理时整体下陷 | 缺失 Height 必须视为零位移，不得把采样默认 0 当作中心值 0.5。 |
| Hover/Click 穿过 Ridge/Peak | 生成资产必须启用碰撞，Actor 的总 HISM Collision 开关也必须开启。 |
| `NormalStrength=0` 时 Ridge/Peak 仍显示陡坡法线 | 资产由旧版生成器烘焙了几何法线；删除或换路径重新生成资产，确认生成器保留径向 Normal Overlay 且 `bEnableRecomputeNormals=false`。 |
| `NormalStrength>0` 时 World Normal 偏黄或单向受光 | NormalDX 导入、压缩、sRGB、父材质 Sampler Type 或重算切线配置错误。 |
