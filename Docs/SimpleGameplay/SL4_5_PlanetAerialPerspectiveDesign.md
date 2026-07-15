# SL4.5：球面行星远景空气透视设计稿

> 父稿：[SpacePlanetLightingDesign.md](SpacePlanetLightingDesign.md) §10 的 **SL4.5：球面远景空气透视**。
>
> 前置阶段：[SL4_VisualHierarchyAndAtmosphereDesign.md](SL4_VisualHierarchyAndAtmosphereDesign.md)。本稿解决“行星表面远近都过于清晰、像月球或白模”的问题：在不启用 `ExponentialHeightFog`、体积云或 `SkyAtmosphere` 的前提下，仅让远处地形逐渐混入低饱和空气色。

---

## 1. 目标与边界

大气薄壳只表现行星外轮廓的侧向散射，不能模拟视线穿过近地空气后产生的远景朦胧。SL4.5 在后处理阶段对**远处地表**进行受控混色：

```text
近处地形 / 棋子：保持清晰
        ↓ 视线距离增加
远处地表 / 山脉：逐渐降低对比和饱和度，混入淡灰蓝空气色
        ↓
星空、星点、天空盒：完全不参与空气透视
```

本阶段采用“相机距离空气透视”，而非真实体积散射积分。它适合当前固定尺度、以俯瞰整颗小行星为主的玩法镜头，能提供稳定的远近层次，不改变太阳方向、阴影结构和球面光照。

明确不做：

- `ExponentialHeightFog`、`VolumetricCloud`、`SkyAtmosphere`。
- 对世界 Z 高度起雾。
- 对星空、`SpaceSky`、棋子和交互高亮进行全屏雾化。
- 用大幅提高 `Sky Light`、`DL_SpaceFill` 或曝光来伪造空气透视。

---

## 2. 方案结构

本阶段由三部分组成：

| 部分 | 名称 | 职责 |
| --- | --- | --- |
| 地形遮罩 | `CustomStencil = 17` | 明确哪些像素允许被雾化 |
| 后处理材质 | `M_PP_PlanetAerialPerspective` | 基于 Scene Depth 计算远近雾因子并混色 |
| 后处理实例 | `MI_PP_PlanetAerialPerspective` | 向 `PPV_SpacePlanet` 提供可调参数 |

只为三类地表 HISM 开启遮罩：`PlainTileHISMComp`、`ForestTileHISMComp`、`MountainTileHISMComp`。棋子、选择高亮、行动范围、`SpaceSky` 和 `PlanetAtmosphere` 默认不写入该 Stencil，因此不会被后处理材质改变。

---

## 3. 前置条件

1. SL3 的固定曝光、唯一 `DL_Sun`、`SL_SpaceAmbient` 已稳定通过。
2. SL4 的太阳构图、Bloom、棋子与 Cell 高亮可读性已通过。
3. 地表瓦片的法线、NormalDX 贴图、阴影方向已修复；空气透视不能用来掩盖法线或材质问题。
4. 当前主地图为 `/Game/Map/TessellatedMeshTestMap`。
5. 在 Project Settings 搜索 `Custom Depth-Stencil Pass`，将其设为：

```text
Enabled with Stencil
```

修改该项目设置后按编辑器提示重启。没有 Stencil 缓冲时，后处理无法可靠地区分地表和无限远星空。

---

## 4. 配置地表 CustomStencil 遮罩

### 4.1 建立遮罩编号约定

| Stencil 值 | 所属对象 | SL4.5 处理 |
| ---: | --- | --- |
| `17` | 平原、森林、山脉 HISM 地表 | 允许空气透视 |
| 其他 / `0` | 棋子、Cell 高亮、星空、大气壳、UI | 不处理 |

`17` 是本阶段约定值。不要与项目既有 CustomStencil 用途冲突；若已被占用，选择一个未使用的统一值，并在材质实例中同步修改 `TerrainStencilId`。

### 4.2 在地表 HISM 上设置

打开关卡中的 `APlanetTessellatedMesh` / 对应蓝图实例，在 Components 面板依次选中：

```text
PlainTileHISMComp
ForestTileHISMComp
MountainTileHISMComp
```

每个组件均设置：

| 分类 | 属性 | 值 |
| --- | --- | --- |
| Rendering | Render CustomDepth Pass | 开启 |
| Rendering | CustomDepth Stencil Value | `17` |
| Rendering | CustomDepth Stencil Write Mask | 默认 `255` / 全位写入 |

不要在棋子 HISM、`SpaceSky` 或 `PlanetAtmosphere` 上复制这组设置。

### 4.3 遮罩验收

在编辑器 Viewport 打开：

```text
View Mode -> Buffer Visualization -> Custom Stencil
```

预期：三类地表瓦片显示同一种非黑颜色；星空、棋子、交互高亮与大气壳为黑色或非 `17` 值。若整屏都显示地表颜色，优先检查 `SpaceSky` 是否误开了 CustomDepth。

---

## 5. 创建 M_PP_PlanetAerialPerspective

### 5.1 新建材质与基础属性

在 `/Game/Materials` 新建 Material：

```text
M_PP_PlanetAerialPerspective
```

在材质 Details 中设置：

| 属性 | 值 |
| --- | --- |
| Material Domain | `Post Process` |
| Blendable Location | `Scene Color Before Bloom` |
| Blendable Priority | `0` |

当前 UE 编辑器将旧版文档常见的 `Before Tonemapping` 拆分为多个场景颜色阶段。空气透视需要在 HDR 场景颜色阶段完成混色、并让 Bloom 与色调映射随后统一处理，因此选择 `Scene Color Before Bloom`。不要选择 `Scene Color After Tonemapping`，否则会在显示空间混色，空气颜色、Bloom 与曝光关系会不自然。

该材质只输出 `Emissive Color`。不连接 Base Color、Normal、Roughness、Opacity 或材质 Normal。

### 5.2 所需参数

创建以下参数，稍后由材质实例调节：

| 参数名 | 类型 | 初始值 | 用途 |
| --- | --- | ---: | --- |
| `FogStartCM` | Scalar | `18000` | 开始明显混入空气色的相机距离 |
| `FogEndCM` | Scalar | `42000` | 达到最大空气透视的相机距离 |
| `HazeStrength` | Scalar | `0.12` | 最大混色比例上限 |
| `HazeColor` | Vector | `#7D909D` | 低饱和灰蓝空气色 |
| `TerrainStencilId` | Scalar | `17` | 必须与地表 HISM 的 Stencil 一致 |

初始深度仅作为 `GlobeRadiusCM = 15000` 的战略镜头测试起点，不能视为绝对值；第 7 节会给出按实际相机距离校准的方法。

### 5.3 材质节点与 Custom 输入

材质图只保留以下输入节点和一个 `Custom` 节点。所有计算都放入 Custom HLSL，避免用大量 `Subtract`、`Divide`、`If`、`Lerp` 节点铺满材质图。

创建以下 8 个节点。三个 `SceneTexture` 节点的 UV 输入保持未连接，使用后处理材质默认的当前屏幕 UV。

| 节点 | 设置 | 接入 Custom 的输入名 |
| --- | --- | --- |
| `SceneTexture` | Scene Texture Id = `PostProcessInput0`，取 `Color.RGB` | `SceneColor` |
| `SceneTexture` | Scene Texture Id = `SceneDepth`，取 `Color.R` | `SceneDepthCM` |
| `SceneTexture` | Scene Texture Id = `CustomStencil`，取 `Color.R` | `CustomStencil` |
| `Scalar Parameter` | `FogStartCM` | `FogStartCM` |
| `Scalar Parameter` | `FogEndCM` | `FogEndCM` |
| `Scalar Parameter` | `HazeStrength` | `HazeStrength` |
| `Vector Parameter` | `HazeColor`，取 RGB | `HazeColor` |
| `Scalar Parameter` | `TerrainStencilId` | `TerrainStencilId` |

添加一个 `Custom` 节点并设置：

| Custom 属性 | 值 |
| --- | --- |
| Description | `Planet aerial perspective: terrain stencil only` |
| Output Type | `CMOT Float3` |
| Inputs | 严格使用上表的 8 个输入名与连接 |

将 Custom 输出直接连接到材质主节点的 `Emissive Color`。

### 5.4 Custom HLSL

将下列代码完整复制到 Custom 节点的 `Code`：

```hlsl
// SceneDepthCM is in Unreal world units (centimeters).
float DepthRange = max(FogEndCM - FogStartCM, 1.0);
float Depth01 = saturate((SceneDepthCM - FogStartCM) / DepthRange);

// Smoothstep-shaped interpolation avoids a visible fog ring.
float SmoothDepth = Depth01 * Depth01 * (3.0 - 2.0 * Depth01);

// Only fog terrain pixels whose CustomStencil matches TerrainStencilId.
// The 0.5 tolerance keeps the comparison stable for the stencil sample.
float StencilDelta = abs(CustomStencil - TerrainStencilId);
float TerrainMask = 1.0 - step(0.5, StencilDelta);

float FogFactor = saturate(SmoothDepth * HazeStrength) * TerrainMask;
return lerp(SceneColor, HazeColor, FogFactor);
```

该代码依次完成：按相机深度计算平滑雾因子、匹配地表 Stencil、将最终场景色混入空气色。`SceneColor` 始终是 `lerp` 的第一项，因此非地表像素与近处地表保持原样。

### 5.5 必须遵守的材质约束

- `PostProcessInput0` 是原始场景颜色，必须始终作为 `lerp` 的 A 输入；不能只输出空气色。
- 雾因子必须乘 `TerrainMask`。没有遮罩时，远景星空会按无限深度被混成灰蓝色。
- `HazeStrength` 必须小于 `0.20`；超出此值会像全局白雾，破坏宇宙场景。
- 该材质不依赖 `WorldPosition` 或世界 Z，不会在球体某一纬度形成“平地雾”。
- `CustomStencil` 在当前项目应读取为 `0..255` 的 Stencil 值。若第 7.1 节的洋红遮罩测试完全无效，临时将 `TerrainStencilId` 改为 `17 / 255 = 0.066667` 测试；以实际 Buffer Visualization 与测试结果为准，确认后固定一种约定。

### 5.6 创建材质实例

右键 `M_PP_PlanetAerialPerspective`，创建：

```text
/Game/Materials/MI_PP_PlanetAerialPerspective
```

勾选并填入第 5.2 节的所有参数。后续只修改实例，不直接修改母材质。

---

## 6. 添加到 PPV_SpacePlanet

1. 在 Outliner 选中 `PPV_SpacePlanet`。
2. 确认 `Infinite Extent (Unbound)` 仍开启，且 Exposure 仍为 SL3 已验收的固定模式。
3. 在 Details 找到 `Post Process Materials` / `Rendering Features -> Post Process Materials`。
4. 添加一个 Array 元素，Asset 选择 `MI_PP_PlanetAerialPerspective`。
5. 确认该 Blendable 的 Weight 为 `1.0`。
6. 不新增第二个 Unbound PPV。所有调参都通过材质实例完成。

此后在编辑器与 PIE 中，后处理会自动应用到整个相机画面，但只有 CustomStencil 为 `17` 的地表像素实际参与空气透视。

---

## 7. 距离与强度校准

### 7.1 先验证遮罩，再验证距离

首次接入时，暂时将材质实例参数设为：

```text
FogStartCM = 0
FogEndCM = 1
HazeStrength = 0.50
HazeColor = (1, 0, 1)
```

预期：只有地表 HISM 被洋红色覆盖，棋子、星空、Cell 高亮和大气壳不变。确认后立即恢复正式参数；此测试不能作为最终画面保存。

若星空变洋红，检查 `SpaceSky` 是否写入 Stencil `17`。若棋子变洋红，检查棋子组件是否误写入 `17`。若地表没有变化，检查项目 `Custom Depth-Stencil Pass`、HISM 的 `Render CustomDepth Pass` 和 PPV Blendable 是否都已生效。

### 7.2 采集深度范围

1. 在默认战略镜头打开 `Buffer Visualization -> SceneDepth`。
2. 记录最近可见地表、画面中心地表和最远可见地表的大致深度分布；重点是相对远近，而不是颜色绝对值。
3. 回到 Lit 模式，在材质实例中先使用：

```text
FogStartCM = 最近可见地表距离 + 20% * 可见地表深度跨度
FogEndCM   = 最近可见地表距离 + 80% * 可见地表深度跨度
```

4. 以 `HazeStrength = 0.12` 起步。近处应基本不变，远处山脉、地表纹理和阴影应稍微降低对比。

若无法从 SceneDepth 精确读数，可先用 `18000 / 42000 / 0.12` 起步，每轮只修改一个参数：

```text
距离过早变灰：提高 FogStartCM
远处仍过于锐利：降低 FogEndCM，或将 HazeStrength 增加 0.02
整颗行星像被白雾洗掉：提高 FogEndCM，或将 HazeStrength 降低 0.02
```

### 7.3 推荐参数范围

| 参数 | 常用范围 | 上限 | 调参规则 |
| --- | ---: | ---: | --- |
| `FogStartCM` | 按镜头深度确定 | 无固定值 | 不应雾化玩家当前近景交互区 |
| `FogEndCM` | `FogStartCM + 10000 - 30000` | 按镜头深度确定 | 应覆盖远山与远侧地表 |
| `HazeStrength` | `0.08 - 0.16` | `0.20` | 优先调距离，再调强度 |
| `HazeColor` | 灰蓝 / 灰青 | 不用高饱和蓝 | 必须低饱和、低亮度 |

### 7.4 与大气薄壳的配合

- `PlanetAtmosphere` 只保留轮廓散射，半径保持 `GlobeRadiusCM * 1.003 - 1.008`。
- 若 `GlobeRadiusCM = 15000`，壳体半径为 `15045 - 15120 cm`，`/Engine/BasicShapes/Sphere` 的 Scale 为约 `300.9 - 302.4`。
- 大气壳的 `RimPower` 推荐 `6 - 8`，`AtmosphereIntensity` 推荐 `0.01 - 0.03`。
- 看到完整的巨大蓝色透明圆壳时，说明壳体过大或 Fresnel 太宽；先收紧薄壳，不要用后处理雾去覆盖这个问题。

---

## 8. 验收成果

### 8.1 静态视觉验收

1. 默认战略镜头中，靠近相机的地表、棋子与 Cell 保持清晰。
2. 远处地表、远山、远侧阴影逐渐减少微观纹理和对比，但仍能判断地形大类与球面形体。
3. 星空维持黑色和清晰星点，不出现整体灰蓝蒙层。
4. 行星边缘只有薄的大气散射，不再出现包住整颗球的巨大透明蓝色气泡。
5. 绕球、缩放和相机聚焦时，雾化随相机距离连续变化，无突然断层或屏幕空间闪烁。

### 8.2 玩法验收

1. 在近景、昼夜线和远侧可玩区域分别执行 hover、棋子选择、行动范围显示和移动。
2. 棋子、黄色选中、蓝色行动范围和其他交互高亮不因空气透视褪色或被空气色染灰。
3. 远处地表可以朦胧，但不应让玩家无法区分山脉、森林、平原或判断可行动 Cell。
4. PIE 与编辑器的雾化范围、曝光、星空和交互结果一致。

### 8.3 阶段完成定义

满足以下条件时，SL4.5 完成：

- 画面具有近清远朦的地球式空气透视层次，而不是整颗球每个细节同样锐利的月球 / 白模读感。
- 后处理只影响 `CustomStencil = 17` 的地表，不影响星空、棋子、交互高亮和 UI。
- 大气薄壳仅表现行星轮廓，不替代远景空气透视，也不形成透明气泡。
- 未启用任何世界 Z 高度雾、体积云或开放世界天空 Actor。

---

## 9. 排错表

| 症状 | 根因 | 修复 |
| --- | --- | --- |
| 整个星空变灰蓝 | 未乘 TerrainMask，或 `SpaceSky` 写入了 Stencil `17` | 检查 §5.3 的 Mask 与 `SpaceSky` CustomDepth 设置 |
| 棋子 / Cell 高亮褪色 | 棋子或高亮组件误写入 `17` | 移除其 CustomDepth 或改用非 17 Stencil |
| 地表完全不受影响 | Stencil Pass 未启用、HISM 未写 CustomDepth、PPV 未挂实例 | 按 §3、§4、§6 逐项检查 |
| 近处地表也像白雾 | FogStartCM 太小或 HazeStrength 太高 | 提高 FogStartCM；优先将 Strength 降至 `0.08 - 0.12` |
| 远处仍然太清晰 | FogEndCM 太大或 HazeStrength 太低 | 降低 FogEndCM；每次 Strength 增加 `0.02` |
| 雾出现明显一圈屏幕空间边界 | FogStart / End 范围太窄 | 拉大二者差值，不靠提高强度制造效果 |
| 画面像平地雾 / 地平线雾 | 使用了 ExponentialHeightFog 或世界 Z 条件 | 移除高度雾；只保留 SceneDepth + TerrainStencil 方案 |
| 整颗球被巨大蓝色壳罩住 | `PlanetAtmosphere` 半径过大或 Rim 太宽 | 按 §7.4 收紧半径和 RimPower；大气壳可直接关闭 |

---

## 10. 后续衔接

SL4.5 通过后进入 SL5。SL5 固化所有最终参数：`DL_Sun`、`SL_SpaceAmbient`、`DL_SpaceFill`（如使用）、`PPV_SpacePlanet`、`MI_SpaceSky`、`MI_PlanetAtmosphere`（如使用）以及 `MI_PP_PlanetAerialPerspective`。

若未来需要严格物理的球壳体积散射，应建立独立的屏幕空间射线积分或体积材质设计稿；不得直接用 `ExponentialHeightFog` 替代本阶段的球面空气透视。
