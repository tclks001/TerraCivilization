# SL4.6：地表晨昏散射带设计稿

> 父稿：[SpacePlanetLightingDesign.md](SpacePlanetLightingDesign.md) §10 的 **SL4.6：地表晨昏散射带**。
>
> 前置阶段：[SL4_5_PlanetAerialPerspectiveDesign.md](SL4_5_PlanetAerialPerspectiveDesign.md)。本稿解决双向平行光造成的晨昏线“最黑腰带”：`DL_Sun` 在 `N·L=0` 处衰减到零，反向 `DL_SpaceFill` 也在同一处衰减到零，因此仅靠灯光无法形成柔和明亮的过渡。

---

## 1. 目标与边界

本阶段在平原、森林、山脉共用的地表母材质中增加一个**弱 Emissive 晨昏散射带**。它不是第二盏太阳，不投射阴影，不改变 `DL_Sun` 的主光和山体影子；它只在全局球面法线接近太阳晨昏线、且略偏夜侧的位置增加低饱和空气色。

```text
DL_Sun                 -> 真实直射、阴影、昼面塑形
DL_SpaceFill           -> 最低限度夜面玩法可读性
TerminatorScattering   -> 晨昏交界的柔和空气散射带
HISMHighlightEmissive  -> CPU 驱动的选中 / 行动 / Hover 高亮
                         ↓
                    Add 后统一输出 Emissive Color
```

本阶段不做真实体积散射、Rayleigh/Mie 视线积分或新的雾 Actor。远景朦胧仍由 SL4.5 后处理负责；晨昏散射带只解决局部明暗过渡与行星大气感。

---

## 2. 光照原理与为什么需要材质项

令 `N` 为从行星中心指向当前像素的径向单位法线，`L` 为从当前像素指向太阳的单位方向：

```text
SunDirect   = max(dot(N, L), 0)
SpaceFill   = FillScale * max(-dot(N, L), 0)
```

当 `dot(N, L) = 0` 时，上述两项均为零，因此晨昏线会形成黑带。晨昏散射带使用一个以 `dot(N,L)` 为中心的平滑带状函数：

```text
BandCenter = dot(N, L) + TerminatorNightOffset
Band = 1 - smoothstep(0, TerminatorWidth, abs(BandCenter))
```

`TerminatorNightOffset > 0` 时，带中心移动到 `dot(N,L) < 0` 的夜侧，让晨昏带看起来像太阳光掠过背光边缘的大气，而不是贴在昼面上的发光边线。

宏观 `N` 必须通过 `normalize(AbsoluteWorldPosition - PlanetCenterWS)` 重建，不能使用 `PixelNormalWS`：后者会受岩石细节、法线贴图和山坡影响，把连续的行星晨昏带打碎成噪声斑块。

---

## 3. 前置条件

1. 平原、森林、山脉已使用同一母材质或同一 Material Function 链路。
2. 母材质当前至少包含：

```text
BaseColorTexture -> Base Color
NormalTexture    -> Normal
RoughnessTexture -> Roughness
现有 CPU 高亮   -> HISMHighlightEmissive -> Emissive Color
```

本文用 `HISMHighlightEmissive` 指代现有高亮链路的最终颜色输出。若工程中实际变量 / 节点名称拼写不同，不改动既有 CPU 参数名和行为，只在最终 `Add` 节点接入该已有输出。

3. `DL_Sun` 已是唯一投射阴影的主太阳，地表法线和 Tile Static Mesh 绕序已验证正确。
4. `DL_SpaceFill` 如存在，强度已保持在最低玩法可读性水平，不承担晨昏线亮度。
5. `PPV_SpacePlanet` 的固定曝光与 SL4.5 空气透视已通过，避免曝光自动适应掩盖散射带效果。

---

## 4. 创建 MPC_SpaceLighting

晨昏带需要知道行星中心和太阳方向。使用 Material Parameter Collection 可让平原、森林、山脉三个材质实例共享同一份世界参数。

### 4.1 创建 Collection

1. 在 `/Game/Materials` 创建 `Material Parameter Collection`：

```text
MPC_SpaceLighting
```

2. 添加两个 Vector Parameter：

| 参数名 | 默认值 | 含义 |
| --- | --- | --- |
| `PlanetCenterWS` | `(0, 0, 0, 0)` | 行星球心的世界坐标 |
| `SunDirectionWS` | `(0, 0, 1, 0)` | 从行星表面指向太阳的归一化方向 |

`PlanetCenterWS` 取关卡中 `APlanetTessellatedMesh` 的世界位置。当前星球在世界中心时保持 `(0,0,0)`；不得把它误填为相机位置或某个 Tile 实例位置。

### 4.2 设置 SunDirectionWS

对于 UE 的 Directional Light，Actor Forward Vector 是光线从太阳射向场景的方向。晨昏散射需要的是从场景指回太阳的方向，因此：

```text
SunDirectionWS = -normalize(DL_Sun.ActorForwardVector)
```

太阳暂不旋转时，可在 Collection Instance 中手动输入一个归一化向量。推荐在后续 SL5 将同步写入现有 CPU / 蓝图流程：每次 `DL_Sun` 旋转后更新 `MPC_SpaceLighting.SunDirectionWS`。

### 4.3 方向验证

首次接入材质后，若散射带出现在昼面中央而不是晨昏附近，说明方向符号相反。将 `SunDirectionWS` 整体乘以 `-1` 后重试；不要通过把 `TerminatorNightOffset` 设成很大值掩盖方向错误。

---

## 5. 地表母材质补充节点

### 5.1 保留既有 PBR 与高亮链路

原有连接保持不变：

```text
BaseColorTexture -> Base Color
NormalTexture    -> Normal
RoughnessTexture -> Roughness
```

找到现有高亮链路最终输出，记为：

```text
HISMHighlightEmissive
```

不要把晨昏散射接入 `Base Color`、`Normal` 或 `Roughness`。它是一个受控的视觉空气项，应直接作为独立 Emissive 颜色相加；这样不会改变地表贴图的切线空间法线、不会产生第二套阴影，也不会重现已修复的材质法线问题。

### 5.2 新建材质参数

在同一母材质创建以下参数：

| 参数名 | 类型 | 初始值 | 常用范围 | 作用 |
| --- | --- | ---: | ---: | --- |
| `TerminatorWidth` | Scalar | `0.24` | `0.18 - 0.32` | 晨昏带在 `N·L` 空间的半宽 |
| `TerminatorNightOffset` | Scalar | `0.06` | `0.03 - 0.10` | 正值将带中心推向夜侧 |
| `TerminatorProfile` | Scalar | `1.25` | `0.8 - 2.0` | 带中心到边缘的视觉衰减形状 |
| `TerminatorIntensity` | Scalar | `0.028` | `0.010 - 0.050` | 散射 Emissive 强度 |
| `TerminatorColor` | Vector | `#789EAD` | 低饱和灰青蓝 | 晨昏空气色 |

平原、森林、山脉共享这些母材质参数，先使用相同初值。SL5 之前若需要按材质实例微调，推荐倍率：平原 `1.0`、森林 `0.75`、山脉 `0.60`；山地强散射会显得像发光岩石。

### 5.3 新建节点与连接

新建下列节点：

| 节点 | 设置 | 接入 Custom 输入名 |
| --- | --- | --- |
| `Absolute World Position` | 取 RGB | `AbsoluteWorldPosition` |
| `Collection Parameter` | Collection = `MPC_SpaceLighting`，Parameter = `PlanetCenterWS`，取 RGB | `PlanetCenterWS` |
| `Collection Parameter` | Collection = `MPC_SpaceLighting`，Parameter = `SunDirectionWS`，取 RGB | `SunDirectionWS` |
| `Scalar Parameter` | `TerminatorWidth` | `TerminatorWidth` |
| `Scalar Parameter` | `TerminatorNightOffset` | `TerminatorNightOffset` |
| `Scalar Parameter` | `TerminatorProfile` | `TerminatorProfile` |
| `Scalar Parameter` | `TerminatorIntensity` | `TerminatorIntensity` |
| `Vector Parameter` | `TerminatorColor`，取 RGB | `TerminatorColor` |

添加一个 `Custom` 节点：

| Custom 属性 | 值 |
| --- | --- |
| Description | `Planet terminator scattering band` |
| Output Type | `CMOT Float3` |
| Inputs | 严格使用上表的 8 个输入名与连接 |

Custom 输出命名为：

```text
TerminatorScatteringEmissive
```

### 5.4 完整 Custom HLSL

将以下代码完整复制到 Custom 节点 `Code`：

```hlsl
// Reconstruct a stable planet-scale normal. Do not use PixelNormalWS here:
// texture and mountain detail must not fragment the global terminator band.
float3 Radial = AbsoluteWorldPosition - PlanetCenterWS;
float RadialLengthSq = max(dot(Radial, Radial), 1e-8);
float3 PlanetNormalWS = Radial * rsqrt(RadialLengthSq);

// SunDirectionWS must point from the surface toward the sun.
float3 LightDirectionWS = normalize(SunDirectionWS);
float NoL = dot(PlanetNormalWS, LightDirectionWS);

// A positive offset moves the band center slightly into the night hemisphere.
float CenteredNoL = NoL + TerminatorNightOffset;
float Width = max(TerminatorWidth, 1e-4);
float Band = 1.0 - smoothstep(0.0, Width, abs(CenteredNoL));
Band = pow(saturate(Band), max(TerminatorProfile, 1e-3));

return TerminatorColor * (Band * TerminatorIntensity);
```

### 5.5 与既有高亮 Emissive 合并

添加一个 `Add` 节点：

```text
Add.A = HISMHighlightEmissive
Add.B = TerminatorScatteringEmissive
Add -> Emissive Color
```

高亮链路必须保持在 Add 的 A 输入、晨昏散射在 B 输入，便于检查和临时断开。两项都是 Emissive，顺序数学上无差异；本稿固定命名只是为了排错时快速定位。

最终材质结构：

```text
BaseColorTexture ------------------------> Base Color
NormalTexture ---------------------------> Normal
RoughnessTexture ------------------------> Roughness

CPU Highlight ---------------------------> HISMHighlightEmissive --+
Terminator Custom -----------------------> TerminatorScattering --+--> Add --> Emissive Color
```

不要将 `TerminatorScatteringEmissive` 乘上 CPU 高亮遮罩。晨昏散射是整颗地表的空气现象，高亮只是一层独立玩法表现。

---

## 6. 调参步骤

### 6.1 首次方向和带宽验证

1. 先将 `TerminatorColor` 临时设为高可见洋红 `(1,0,1)`，`TerminatorIntensity = 0.20`。
2. 保持 `TerminatorWidth = 0.24`、`TerminatorNightOffset = 0.06`。
3. 关闭或降到最低 `DL_SpaceFill`，从完整行星镜头检查带是否沿晨昏线连续环绕。
4. 若带跑到昼面中央，按 §4.3 反转 `SunDirectionWS`。
5. 若带在山地纹理处破碎，确认 Custom 使用的是 `AbsoluteWorldPosition - PlanetCenterWS`，没有误接 `PixelNormalWS` 或 Normal Map。
6. 方向正确后立即恢复正式空气色和低强度；洋红仅用于验证，不能保留在正式画面。

### 6.2 正式起点

```text
TerminatorWidth       = 0.24
TerminatorNightOffset = 0.06
TerminatorProfile     = 1.25
TerminatorIntensity   = 0.028
TerminatorColor       = #789EAD
```

在 `DL_Sun` 与 `DL_SpaceFill` 均开启的正式镜头下调节。每轮只修改一个参数：

| 症状 | 先调参数 | 调整方向 |
| --- | --- | --- |
| 黑色晨昏腰带仍很明显 | `TerminatorWidth` | 每次增加 `0.02` |
| 散射带侵入昼面太多 | `TerminatorNightOffset` | 每次增加 `0.01`，推向夜侧 |
| 散射带像荧光描边 | `TerminatorIntensity` | 每次降低 `0.005` |
| 带太宽、像整面背光发蓝 | `TerminatorWidth` | 每次降低 `0.02` |
| 带中心很亮、边缘太硬 | `TerminatorProfile` | 降到 `1.0` 或 `0.8` |
| 带过于平、缺少中心层次 | `TerminatorProfile` | 提高至 `1.5 - 2.0` |
| 山地像发光岩石 | 山地 MI 的 Intensity | 乘以 `0.60`，不改全局太阳 |

### 6.3 与 Bloom 和空气透视的协同

1. 晨昏散射带先在 Bloom 最低可见状态下校准；保持 SL4 的 Bloom 起点 `Intensity = 0.20`、`Threshold = 1.50`。
2. 若散射带被 Bloom 扩成宽亮环，先降低 `TerminatorIntensity`，再略提高 Bloom Threshold；不要把带宽做得极窄来抵消 Bloom。
3. SL4.5 的远景空气透视继续只雾化 `CustomStencil = 17` 的地表；它会自然降低远端散射带的对比，无需在 Custom HLSL 中重复计算距离雾。
4. 大气薄壳只服务外轮廓，不能提高强度来替代地表晨昏散射。

---

## 7. 验收成果

### 7.1 静态视觉验收

1. 关闭 `DL_Sun.Cast Shadows` 的诊断镜头下，球面受光、夜面填充和晨昏散射形成连续的三段过渡，不出现此前的黑色哑铃腰带。
2. 正式阴影开启后，山体阴影方向仍仅由 `DL_Sun` 决定；晨昏散射不会生成第二套山影。
3. 晨昏带沿球面连续分布，不随地表 Normal Map、山脉纹理或 HISM 瓦片边界破碎。
4. 散射颜色是低饱和灰青空气色，不是高亮霓虹环、蓝色地表滤镜或发光岩石。
5. 大气薄壳、远景空气透视与晨昏带同时启用时，球体具有柔和行星过渡而没有巨大透明气泡。

### 7.2 玩法验收

1. 在昼面、晨昏带中心、夜面分别选择棋子并显示行动范围。
2. 黄色选中、蓝色行动范围和阵营色仍明显高于散射带；晨昏带不能与黄色 / 蓝色高亮混淆。
3. 晨昏区域不再是整颗球最黑、最难操作的位置；棋子与 Cell 轮廓至少不弱于深夜面。
4. 相机绕球、缩放、聚焦与回合切换时，散射带随太阳方向稳定移动，不闪烁、不跳变、不随相机位置滑动。
5. 编辑器与 PIE 中的散射带位置、亮度、HISM 高亮和 CPU 材质参数行为一致。

### 7.3 完成定义

SL4.6 完成时：

- `DL_Sun` 仍是唯一主导直射与阴影的太阳。
- `DL_SpaceFill` 不再让晨昏线成为最黑的视觉断层。
- 地表母材质通过弱 Emissive 晨昏散射建立自然、连续的行星空气过渡。
- 既有 `HISMHighlightEmissive` 链路未被替换或削弱，只是在最终 Emissive 输出前与散射项相加。

---

## 8. 排错表

| 症状 | 原因 | 修复 |
| --- | --- | --- |
| 散射带完全不出现 | Custom 输出未接 Emissive、Intensity 为零、MPC 参数未绑定 | 检查 §5.3 / §5.5，先用 §6.1 洋红测试 |
| 散射带出现在昼面中央 | `SunDirectionWS` 符号相反 | 使用 `-DL_Sun.ActorForwardVector`；必要时整体反向 |
| 散射带贴在整条昼面边缘 | `TerminatorNightOffset` 为零或过小 | 增加到 `0.03 - 0.10` |
| 散射带断成岩石噪声 | 使用了 PixelNormalWS、法线贴图或切线空间 Normal | 只使用 `normalize(AbsoluteWorldPosition - PlanetCenterWS)` |
| 散射带像蓝色灯带 | Intensity / 饱和度过高，或 Bloom 扩散 | 降 Intensity，改灰青颜色，提高 Bloom Threshold |
| 山影方向被改变 | 误把散射接入 Base Color / Normal，或新增投影光 | 散射只接 Emissive；删除额外投影光 |
| 选中高亮消失或变暗 | 错误用散射替换了现有 Emissive | 使用 Add 合并，保留 `HISMHighlightEmissive` 原链路 |
| 散射随相机移动而滑动 | 使用了 Camera Vector、PixelDepth 或后处理坐标判定 | Custom 只使用世界位置、球心与太阳方向 |

---

## 9. 后续衔接

SL4.6 通过后进入 SL5。SL5 固化 `MPC_SpaceLighting` 的 CPU / 蓝图同步、三个地形 Material Instance 的晨昏参数、`DL_Sun`、`DL_SpaceFill`（如使用）、`PPV_SpacePlanet`、SL4.5 空气透视和可选大气薄壳的最终数值。

未来若需要真实 Rayleigh/Mie 散射，保留本稿的 `SunDirectionWS` 与 `PlanetCenterWS` 接口，在新的球壳视线积分阶段替换或叠加本稿的弱 Emissive 近似；不要重新启用开放世界高度雾。
