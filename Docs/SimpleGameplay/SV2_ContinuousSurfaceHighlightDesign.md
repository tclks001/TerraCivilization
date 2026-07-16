# SV2：连续基础表面 Hex/Pent 高亮设计稿

> 状态：C++ 已落地，待在编辑器按第 6 节创建材质并进行 PIE 视觉验收。
>
> 编码：UTF-8，简体中文。
>
> 前置：SV0 `TerrainVisual` 独立模块与 SV1 连续基础表面、点击 `CellId` 桥接均已验收。

---

## 1. 目标与范围

SV2 让连续基础表面承担当前 HISM 瓦片承担的所有**地表高亮表现**：鼠标 hover、当前阵营棋子、行动目标、吃子预览和现有 Gameplay 通过 `FTerraGameplayCellHighlight` 输出的颜色/强度。

SV2 不修改 Gameplay 状态、地形规则、棋子规则、鼠标 `CellId` 解析、棋子高度射线或连续表面几何高度。它只改变“同一 Cell 高亮状态如何被画到地面上”。

连续模式完成 SV2 后：

```text
Surface Hit -> CellId -> existing Gameplay
                         |
                         +-> existing Gameplay highlight query
                         +-> TerrainVisual hover state
                                      |
                                      v
                            SurfaceHighlightLUT (RGBA8)
                                      |
                                      v
                           Continuous Surface Material border band
```

Legacy HISM 的 PICD 高亮保留为 Debug 对照，但连续模式正常运行时不显示它。

## 2. 复用与不复用

### 2.1 复用

复用旧 R11/SDF 路线中已经验证的三件事：

1. 球面 Cell 判定：当前像素只在其 primal 三角形的三个候选 Cell 中计算归属。
2. `deltaI = thetaI - min(thetaOthers)`：`-deltaI` 是像素在 Cell I 内部距 Hex/Pent 边的角距离；它是高亮带唯一使用的边界距离。
3. Gameplay 高亮数据：继续调用 `FTerraGameplayContainer::GetHighlightForCell`、`IsCurrentFactionPieceCell`、`IsCurrentActionTargetCell` 和吃子预览查询，不复制或改写 Gameplay 状态。

### 2.2 不复用

不得把旧 R8 的整段 HLSL 原样带入：

- 不接入 `CellAttrLUT`、`CellTintLUT`、`CellHSVRoughLUT`、`CellNSpecLUT`；它们属于 SV3 地表材质。
- 不接入 Triplanar PBR、多层颜色混合、3D 噪声、边界扰动、Raymarch 或 PixelDepthOffset。
- 不使用旧版本 `gap = wI - max(wOthers)`；旧 `wA/wB/wC` 是 R5 软归属权重，并非球面重心权重，会导致整 Cell 发光。
- 不使用 HISM `PerInstanceCustomData` 作为连续表面高亮数据源。

## 3. 几何与 Cell Context

SV1 的 Section 0 保持共享顶点、`QueryOnly` 碰撞，是唯一鼠标命中表面。SV2 在**同一 `UTerrainVisualSurfaceComponent`** 增加一个无碰撞的 Section 1：

| Section | 顶点方式 | 碰撞 | 用途 |
| --- | --- | --- | --- |
| 0 | 共享顶点 | `QueryOnly`，仅 `ECC_Visibility` | SV1 鼠标命中，不承载 Cell 材质数据。 |
| 1 | 每个 primal triangle 展开三个独立顶点 | 无 | 连续表面可视材质和 Hex/Pent 高亮。 |

Section 1 的重复顶点在 Position 和 Normal 上与 Section 0 完全相同，故不会制造几何裂缝、阴影断层或轮廓差异。展开只用于使一个渲染三角形内的三 CellId 保持常量。

每个 `FSphereTopology::PrimalTris[T] = (c0, c1, c2)` 的三个输出顶点写入同一组 Cell Context：

| 顶点属性 | 数据 | 解码 |
| --- | --- | --- |
| `UV0.x` | `c0` | `round(UV0.x)` |
| `UV0.y` | `c1` | `round(UV0.y)` |
| `VertexColor.r` | `c2` 高 8 位 / 255 | `round(VertexColor.r * 255)` |
| `VertexColor.g` | `c2` 低 8 位 / 255 | `round(VertexColor.g * 255)` |

当前默认 Cell 数为 642，编码仍按 16 位保留，未来逻辑拓扑提升到 `sub=5` 时也不需要改材质协议。

## 4. LUT 设计

SV2 使用两张 transient LUT，均为 `NumCells x 1`、Nearest、非 sRGB、无 mip、`NeverStream`。

| 资源 | 格式 | 通道 | 写入频率 |
| --- | --- | --- | --- |
| `SurfaceCellDirectionLUT` | `PF_A32B32G32R32F` | RGB = `Cell.UnitCenter` | 仅拓扑/重建时整张上传。 |
| `SurfaceHighlightLUT` | `PF_B8G8R8A8` | RGB = 最终高亮颜色，A = 最终强度 | hover 或 Gameplay dirty cell 时仅更新对应 texel。 |

高亮 LUT 保存的是最终颜色和强度，而不是只有 hover/select 两个布尔通道。这是为了无损兼容当前 HISM 方案的优先级：

```text
Gameplay action/capture highlight
  > 当前阵营棋子（hover 时使用棋子 hover 色）
  > 普通 hover
  > 无高亮
```

这份优先级只在表现层重新合成；Gameplay Container 仍是原始状态唯一来源。

## 5. C++ 表现层职责

### 5.1 TerrainVisual 模块

`UTerrainVisualSurfaceComponent` 持有 `SurfaceHighlightLUT` 与 CPU 像素镜像，并提供创建、清空、更新单个 RGBA texel 的接口。它只接受调用者已合成好的 `FLinearColor + Intensity`，不知道棋子、阵营、行动或吃子规则。

SV2 为了严格保持已验收的 0.5 秒 hover 淡出语义，暂时由既有交互组件维护 hover 的时间状态；连续模式只读取其最终 hover CellId 并写入 TerrainVisual LUT。该复用仅限交互时序，不读取旧 HISM 的可见实例数据或 PICD。后续将 hover 时间状态迁入 `TerrainVisual`，但这不是 SV2 的视觉验收前置条件。

### 5.2 主模块桥接

`APlanetTessellatedMesh` 在连续模式：

- 读取当前 `PlanetGameplayComponent->GetGameplayContainer()`；
- 用与旧 `FPlanetHISMTileRenderer::WriteHighlightForCell` 相同的优先级合成最终色和强度；
- 在连续表面 hover 切换、淡出到期、Gameplay 返回 dirty CellIds、重建后分别刷新对应 LUT texel；
- 将两张 LUT 与全局材质参数绑定给 Section 1 的 MID。

原有 HISM 高亮组件和渲染器不被删除；Legacy 模式继续使用它们。连续模式只关闭其可见输出，不改变 Gameplay 的 dirty-cell 产生逻辑。

## 6. 材质编辑方案

### 6.1 新建资产

在编辑器中创建材质：

```text
/Game/Materials/TerrainVisual/M_TerrainVisual_SurfaceHighlight
```

建议设置：

| 属性 | 值 |
| --- | --- |
| Material Domain | Surface |
| Blend Mode | Opaque |
| Shading Model | Default Lit |
| Two Sided | false |
| Use Material Attributes | false |

SV2 只做高亮验收，基础颜色使用 `BaseGroundColor` 参数即可；SV3 才替换为 SDF/PBR 地表材质。

### 6.2 必需节点

除一个 Custom 节点外，仅需下列节点，避免大量拉线：

| 节点 | 参数名/值 | 连接 |
| --- | --- | --- |
| `TextureCoordinate` | Index 0 | Custom 输入 `CellContextUV`。 |
| `VertexColor` | 默认 | Custom 输入 `CellContextColor`。 |
| `Absolute World Position` | 默认 | Custom 输入 `WorldPos`。 |
| `Texture Object Parameter` | `SurfaceCellDirectionLUT` | Custom 输入同名项；默认贴图留空，由 C++ 注入。 |
| `Texture Object Parameter` | `SurfaceHighlightLUT` | Custom 输入同名项；默认贴图留空，由 C++ 注入。 |
| `Vector Parameter` | `PlanetCenter`，默认 `(0,0,0)` | Custom 输入 `PlanetCenter`。 |
| `Vector Parameter` | `BaseGroundColor`，默认 `(0.08,0.10,0.06)` | Custom 输入 `BaseGroundColor`。 |
| `Scalar Parameter` | `HighlightPaddingRad`，默认 `0.03` | Custom 输入同名项。 |
| `Scalar Parameter` | `HighlightStrength`，默认 `1.5` | Custom 输入同名项。 |

Custom 输出连接：

| Custom 输出 | 材质输入 |
| --- | --- |
| 主输出 `SurfaceColor` (`Float3`) | `Base Color` |
| Additional Output `OutEmissive` (`Float3`) | `Emissive Color` |
| Additional Output `OutRoughness` (`Float1`) | `Roughness` |

不要连接 Normal；SV2 使用 Section 1 的连续几何法线。不要添加 PDO、噪声、SDF 纹理、材质函数或其他 LUT。

### 6.3 Custom 节点 Inputs

按以下顺序创建 Inputs：

| # | Name | Type |
| --- | --- | --- |
| 1 | `CellContextUV` | Float2 |
| 2 | `CellContextColor` | Float4 |
| 3 | `WorldPos` | Float3 |
| 4 | `PlanetCenter` | Float3 |
| 5 | `SurfaceCellDirectionLUT` | Texture2D |
| 6 | `SurfaceHighlightLUT` | Texture2D |
| 7 | `BaseGroundColor` | Float3 |
| 8 | `HighlightPaddingRad` | Float1 |
| 9 | `HighlightStrength` | Float1 |

Additional Outputs：

| Name | Type |
| --- | --- |
| `OutEmissive` | Float3 |
| `OutRoughness` | Float1 |

主 Output Type 设为 `Float3`，Output Name 可保持默认。

### 6.4 可直接粘贴的完整 HLSL

将下列内容完整粘贴到 Custom 节点 Code。该版本仅依赖上述九个 Inputs。

```hlsl
// SV2: continuous surface Hex/Pent border highlight.
// CellContext is constant for every expanded primal triangle.

int c0 = (int)round(CellContextUV.x);
int c1 = (int)round(CellContextUV.y);
int c2 = (int)round(CellContextColor.r * 255.0) * 256
       + (int)round(CellContextColor.g * 255.0);

float3 dir = normalize(WorldPos - PlanetCenter);

float3 v0 = normalize(SurfaceCellDirectionLUT.Load(int3(c0, 0, 0)).rgb);
float3 v1 = normalize(SurfaceCellDirectionLUT.Load(int3(c1, 0, 0)).rgb);
float3 v2 = normalize(SurfaceCellDirectionLUT.Load(int3(c2, 0, 0)).rgb);

float theta0 = acos(clamp(dot(dir, v0), -1.0, 1.0));
float theta1 = acos(clamp(dot(dir, v1), -1.0, 1.0));
float theta2 = acos(clamp(dot(dir, v2), -1.0, 1.0));

// delta < 0 means the pixel belongs to that Cell.
float delta0 = theta0 - min(theta1, theta2);
float delta1 = theta1 - min(theta0, theta2);
float delta2 = theta2 - min(theta0, theta1);

// PF_B8G8R8A8 Load returns RGBA logical channels. RGB is color, A is intensity.
float4 h0 = SurfaceHighlightLUT.Load(int3(c0, 0, 0));
float4 h1 = SurfaceHighlightLUT.Load(int3(c1, 0, 0));
float4 h2 = SurfaceHighlightLUT.Load(int3(c2, 0, 0));

float padding = max(HighlightPaddingRad, 1e-5);
float depth0 = -delta0;
float depth1 = -delta1;
float depth2 = -delta2;

float edge0 = (1.0 - smoothstep(0.0, padding, depth0)) * step(0.0, depth0);
float edge1 = (1.0 - smoothstep(0.0, padding, depth1)) * step(0.0, depth1);
float edge2 = (1.0 - smoothstep(0.0, padding, depth2)) * step(0.0, depth2);

float3 highlight = edge0 * h0.rgb * h0.a
                 + edge1 * h1.rgb * h1.a
                 + edge2 * h2.rgb * h2.a;

OutEmissive = highlight * HighlightStrength;
OutRoughness = 0.82;
return BaseGroundColor;
```

## 7. 验收

1. 连续模式下 HISM 默认隐藏时，鼠标 hover 可在逻辑 Hex/Pent 边缘出现描边，且不沿 sub=7 渲染三角形边显示。
2. 点击、行动目标、吃子预览、当前阵营棋子颜色与 Legacy HISM 模式一致。
3. 移出星球后 hover 按当前 0.5 秒语义消失；在期限内回到同 Cell 不出现闪烁。
4. `CellHighlightLUT` 和 `SurfaceCellDirectionLUT` 在 Rebuild、PIE 开始、PIE 退出回 Editor 后重新绑定到 MID。
5. 关闭 Section 1 或不挂 SV2 材质时，Section 0 的点击和 HISM 棋子高度仍正常。

## 8. 后续

SV3 用同一 `c0/c1/c2 + SurfaceCellDirectionLUT + SurfaceHighlightLUT` 协议替换 `BaseGroundColor` 的简单输出，加入连续 SDF/PBR 地表。SV2 的高亮函数无需重写。
