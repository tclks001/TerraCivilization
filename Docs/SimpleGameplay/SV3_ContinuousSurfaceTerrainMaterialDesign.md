# SV3：连续基础表面 SDF 地形材质设计稿

> 状态：C++ 数据管线已落地，待在编辑器创建材质并进行 PIE 视觉验收。
>
> 编码：UTF-8，简体中文。
>
> 前置：SV1 连续基础表面和点击桥接、SV2 Hex/Pent 高亮均已验收。

---

## 1. 目标与边界

SV3 将连续基础表面的单一底色升级为三类玩法地形的连续 PBR 表现：平原、森林、山地。它复用 SV2 的 Cell Context、方向 LUT 和高亮 LUT，在材质中使用解析距离场（Cell 边界距离、世界空间噪声、岩层带）完成表面视觉。

本阶段不做如下事项：

- 不改变连续表面几何、半径、碰撞、点击 `CellId` 解析。
- 不修改 Gameplay、WorldGen 的三类规则地形、棋子高度射线或 UI。
- 不做 WPO/PDO、Raymarch、全局体积 SDF 或动态地形变更。
- 不做河流、湖泊、水面；水文作为后续独立子阶段，不能让水体暗示 Gameplay 障碍。
- 不增加 HISM 草、树、岩石等装饰；它们属于后续 HISM 装饰阶段。

SV3 的 SDF 指的是材质内解析距离场，而非取代连续网格的 Raymarch SDF：连续网格继续承担深度、轮廓、阴影与点击，距离场只负责地表颜色、粗糙度和微观图案。

## 2. 数据流

```text
FCellGeoData.SimpleTerrainType
  -> SurfaceTerrainLUT (SV3, static after rebuild)
  -> CellContext c0/c1/c2 + SurfaceCellDirectionLUT
  -> spherical Voronoi ownership / edge distance
  -> terrain PBR color + detail field
  -> SurfaceHighlightLUT (SV2 final overlay)
  -> Base Color / Emissive / Roughness
```

### 2.1 SurfaceTerrainLUT

`UTerrainVisualSurfaceComponent` 在重建时创建 `NumCells x 1` 的 `PF_B8G8R8A8` transient 纹理，Nearest、非 sRGB、无 mip、NeverStream。

| 通道 | 语义 | 当前来源 |
| --- | --- | --- |
| R | TerrainClass：0=Plain、127=Forest、255=Mountain | `FCellGeoData.SimpleTerrainType` |
| G | CellVariation | 全局种子派生的确定性随机数 |
| B | DetailScale | 全局种子派生的确定性随机数 |
| A | RoughnessHint | Mountain=230，其他=190 |

该 LUT 仅消费现有 WorldGen 输出，不写回 `FCellGeoData`，也不影响 Gameplay 地形规则。后续高度、水文、湿润度与植被密度将扩展为独立 LUT，不能复用未定义的通道语义。

## 3. 材质资产

新建材质：

```text
/Game/Materials/TerrainVisual/M_TerrainVisual_SurfaceTerrain
```

在 `BP_PlanetTessellatedMesh` 的 `TerrainVisualSurfaceMaterial` 指向该资产。为空时 C++ 自动回退 `TerrainVisualHighlightMaterial`，这是 SV2 回归保护。

| 属性 | 值 |
| --- | --- |
| Material Domain | Surface |
| Blend Mode | Opaque |
| Shading Model | Default Lit |
| Two Sided | false |
| Use Material Attributes | false |

## 4. 节点方案

仅使用一个 Custom 节点与 14 个输入节点；不要额外拉混合节点、Texture Sample、噪声函数或材质函数。两张 LUT 由 C++ 注入，纹理对象参数默认贴图保持空。

| 节点 | 参数名/值 | 接入 Custom Input |
| --- | --- | --- |
| TextureCoordinate | Index 0 | `CellContextUV` |
| VertexColor | 默认 | `CellContextColor` |
| Absolute World Position | 默认 | `WorldPos` |
| Texture Object Parameter | `SurfaceCellDirectionLUT` | 同名 |
| Texture Object Parameter | `SurfaceTerrainLUT` | 同名 |
| Texture Object Parameter | `SurfaceHighlightLUT` | 同名 |
| Vector Parameter | `PlanetCenter`，`(0,0,0)` | 同名 |
| Vector Parameter | `PlainColor`，`(0.16,0.24,0.08)` | 同名 |
| Vector Parameter | `ForestColor`，`(0.045,0.14,0.055)` | 同名 |
| Vector Parameter | `MountainColor`，`(0.18,0.17,0.14)` | 同名 |
| Scalar Parameter | `TerrainCellBlendRad`，`0.012` | 同名 |
| Scalar Parameter | `TerrainDetailFrequency`，`0.00055` | 同名 |
| Scalar Parameter | `HighlightPaddingRad`，`0.03` | 同名 |
| Scalar Parameter | `HighlightStrength`，`1.5` | 同名 |

Custom 主输出为 `Float3`，连接 `Base Color`。Additional Outputs：

| Output | Type | 材质输入 |
| --- | --- | --- |
| `OutEmissive` | Float3 | Emissive Color |
| `OutRoughness` | Float1 | Roughness |

不要连接 Normal、World Position Offset、Pixel Depth Offset 或 Opacity。

## 5. Custom Inputs

按如下顺序添加。名称必须完全一致。

| # | Name | Type |
| --- | --- | --- |
| 1 | `CellContextUV` | Float2 |
| 2 | `CellContextColor` | Float4 |
| 3 | `WorldPos` | Float3 |
| 4 | `PlanetCenter` | Float3 |
| 5 | `SurfaceCellDirectionLUT` | Texture2D |
| 6 | `SurfaceTerrainLUT` | Texture2D |
| 7 | `SurfaceHighlightLUT` | Texture2D |
| 8 | `PlainColor` | Float3 |
| 9 | `ForestColor` | Float3 |
| 10 | `MountainColor` | Float3 |
| 11 | `TerrainCellBlendRad` | Float1 |
| 12 | `TerrainDetailFrequency` | Float1 |
| 13 | `HighlightPaddingRad` | Float1 |
| 14 | `HighlightStrength` | Float1 |

## 6. 可直接粘贴的 HLSL

以下完整内容粘贴到 Custom 节点 `Code`。它直接复用 SV2 的高亮协议；高亮始终在所有地表颜色计算之后叠加。

```hlsl
int c0 = (int)round(CellContextUV.x);
int c1 = (int)round(CellContextUV.y);
int c2 = (int)round(CellContextColor.r * 255.0) * 256 + (int)round(CellContextColor.g * 255.0);

float3 dir = normalize(WorldPos - PlanetCenter);
float3 v0 = normalize(SurfaceCellDirectionLUT.Load(int3(c0, 0, 0)).rgb);
float3 v1 = normalize(SurfaceCellDirectionLUT.Load(int3(c1, 0, 0)).rgb);
float3 v2 = normalize(SurfaceCellDirectionLUT.Load(int3(c2, 0, 0)).rgb);

float theta0 = acos(clamp(dot(dir, v0), -1.0, 1.0));
float theta1 = acos(clamp(dot(dir, v1), -1.0, 1.0));
float theta2 = acos(clamp(dot(dir, v2), -1.0, 1.0));
float delta0 = theta0 - min(theta1, theta2);
float delta1 = theta1 - min(theta0, theta2);
float delta2 = theta2 - min(theta0, theta1);

float4 t0 = SurfaceTerrainLUT.Load(int3(c0, 0, 0));
float4 t1 = SurfaceTerrainLUT.Load(int3(c1, 0, 0));
float4 t2 = SurfaceTerrainLUT.Load(int3(c2, 0, 0));

float blendWidth = max(TerrainCellBlendRad, 1e-5);
float w0 = smoothstep(-blendWidth, blendWidth, -delta0);
float w1 = smoothstep(-blendWidth, blendWidth, -delta1);
float w2 = smoothstep(-blendWidth, blendWidth, -delta2);
float wSum = max(w0 + w1 + w2, 1e-5);
w0 /= wSum;
w1 /= wSum;
w2 /= wSum;

float4 terrain = t0 * w0 + t1 * w1 + t2 * w2;
float terrainClass = terrain.r;
float forestWeight = smoothstep(0.24, 0.51, terrainClass);
float mountainWeight = smoothstep(0.74, 0.98, terrainClass);
forestWeight *= 1.0 - mountainWeight;
float plainWeight = saturate(1.0 - forestWeight - mountainWeight);

float3 p = WorldPos * max(TerrainDetailFrequency, 1e-6);
float macro = frac(sin(dot(p.xy + p.z, float2(12.9898, 78.233))) * 43758.5453);
float grain = frac(sin(dot(p.yz + p.x * 1.73, float2(39.346, 11.135))) * 24634.6345);
float variation = (terrain.g * 2.0 - 1.0) * 0.10 + (macro - 0.5) * 0.12;
float rockBand = smoothstep(0.42, 0.78, grain + macro * 0.45);

float3 baseColor = PlainColor * plainWeight + ForestColor * forestWeight + MountainColor * mountainWeight;
baseColor *= 1.0 + variation;
baseColor = lerp(baseColor, baseColor * float3(0.72, 0.70, 0.66), mountainWeight * rockBand * 0.65);

float4 h0 = SurfaceHighlightLUT.Load(int3(c0, 0, 0));
float4 h1 = SurfaceHighlightLUT.Load(int3(c1, 0, 0));
float4 h2 = SurfaceHighlightLUT.Load(int3(c2, 0, 0));
float padding = max(HighlightPaddingRad, 1e-5);
float edge0 = (1.0 - smoothstep(0.0, padding, -delta0)) * step(0.0, -delta0);
float edge1 = (1.0 - smoothstep(0.0, padding, -delta1)) * step(0.0, -delta1);
float edge2 = (1.0 - smoothstep(0.0, padding, -delta2)) * step(0.0, -delta2);
float3 highlight = edge0 * h0.rgb * h0.a + edge1 * h1.rgb * h1.a + edge2 * h2.rgb * h2.a;

OutEmissive = highlight * HighlightStrength;
OutRoughness = saturate(lerp(0.82, terrain.a, 0.80) + mountainWeight * rockBand * 0.08);
return saturate(baseColor);
```

## 7. 验收

1. Plain、Forest、Mountain 在连续表面上有明显但低频连续的视觉差异，不沿 sub=7 渲染三角形产生接缝。
2. 同一 WorldGen seed 下 `SurfaceTerrainLUT` 结果稳定；改变 seed 后变化只影响视觉微变，不改变 Gameplay 地形和规则。
3. SV2 hover、当前阵营棋子、行动目标、吃子预览颜色与边界位置保持一致。
4. 点击、棋子移动、棋子高度射线及 UI 全部保持原行为。
5. 未设置 `TerrainVisualSurfaceMaterial` 时自动回退 SV2 材质，连续模式仍可运行。

## 8. 后续

SV3 后续子阶段可新增连续高度场、山脊距离场和独立水文 LUT；它们必须建立在本稿 `SurfaceTerrainLUT` 之外，不能把“水”写成 Cell 整块类型，也不能改变 Gameplay Cell 规则。
