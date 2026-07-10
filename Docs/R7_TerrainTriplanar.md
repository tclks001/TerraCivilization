# R7：Triplanar 真实地表纹理（Texture2DArray 多层混合）

> 本文档是 [SphericalSDFTerrainDesign.md](SphericalSDFTerrainDesign.md) §6.5 / §6.5.1 的独立落地文档，与 [R2_TopologyDebugMaterial.md](R2_TopologyDebugMaterial.md) / [R3_CellAttrLUTMaterial.md](R3_CellAttrLUTMaterial.md) / [R4_VoronoiBoundary.md](R4_VoronoiBoundary.md) / [R5_SharpenSoftEdge.md](R5_SharpenSoftEdge.md) / [R6_BoundaryNoise.md](R6_BoundaryNoise.md) 风格一致。
>
> 阅读本文档前必须先理解 [R6_BoundaryNoise.md §1.2](R6_BoundaryNoise.md) 的 dirP 切向偏移——R7 不修改 R6 的几何链路，只把最后输出的"三层 hash 哈希色加权"换成"三层 Triplanar 真实纹理采样加权"。

---

## 0. R7 一句话目标

把 R6 输出公式中的 `hash(layer_i+1)` 占位色换成 `SampleTriplanar(TerrainAlbedoArray, layer_i, WorldPos, dir)` 真实地表纹理采样。**所有 R4/R5/R6 的几何链路（Voronoi 边、软边过渡、噪声扰动）原封不动保留**——R7 仅是"颜色源"的升级。

```
R6（哈希色）                                 R7（真实纹理）
┌──────────────────┐                       ┌──────────────────┐
│  Cell A: 紫红色  │                       │  Cell A: 草地纹理 │
│   ────~~~~~~───  │                       │   ────~~~~~~───   │
│  Cell B: 蓝绿色  │                       │  Cell B: 沙地纹理 │
└──────────────────┘                       └──────────────────┘
   procedural 占位                            真实 PBR 地表
```

| (R5 EdgeWidth, R6 NoiseAmplitude, R7 启用) | 视觉效果 |
| --- | --- |
| (0, 0, ❌) | R4 哈希色硬直边（参考帧） |
| (0, 0, ✅) | **R7 哈希色** = 真实地表 + Civ 风格硬直棱柱 |
| (0.05, 0, ✅) | 真实地表 + Old World 软直边 |
| (0.05, 0.05, ✅) | **真实地表 + 软蜿蜒边**（E&D 风格的最终形态）|

---

## 1. 几何与数学

### 1.1 R6 输出公式回顾

R6 最终颜色：

$$
\text{color} = \frac{\sum_i w_i \cdot \text{hash}(\text{layer}_i + 1)}{\sum_i w_i + \epsilon}
$$

其中 $w_i$ 由 R6 的 `dirP` 决定（含噪声扰动），$\text{layer}_i$ 来自 `CellAttrLUT.r * 255`。

### 1.2 R7 核心：哈希色 → Triplanar 真实纹理采样

R7 替换公式：

$$
\boxed{\ \text{color} = \frac{\sum_i w_i \cdot \text{Triplanar}(\text{Albedo}, \text{layer}_i, \mathbf{x}, \hat{n})}{\sum_i w_i + \epsilon}\ }
$$

其中：

- $\mathbf{x}$ = WorldPosition（fragment 的世界坐标）
- $\hat{n}$ = **未扰动的球面外法**（等于 R6 中的原始 `dir = normalize(WorldPos - PlanetCenter)`，**不是** R6 dirP）
- $\text{Albedo}$ = `Texture2DArray<float4>`，每个 slice = 1 个 layer 的 BaseColor

### 1.3 Triplanar 采样数学

球面没有非奇异全局 UV 参数化（详见 [SDF §6.5](SphericalSDFTerrainDesign.md#65-triplanar-解决球面-uv-接缝)）。Triplanar 用三个轴对齐平面的投影 UV 各采样一次，按法线 |分量|² 加权混合：

$$
\text{Triplanar}(T, l, \mathbf{x}, \hat{n}) = w_x \cdot T(\mathbf{x}_{yz}/s, l) + w_y \cdot T(\mathbf{x}_{xz}/s, l) + w_z \cdot T(\mathbf{x}_{xy}/s, l)
$$

其中：

- 权重 $\mathbf{w} = |\hat{n}|^k / \||\hat{n}|^k\|_1$，$k = $ TriplanarSharpness（典型 4）
- $s = $ TileScale（每米一个纹理周期，典型 100~500，看星球尺寸）
- $T(\cdot, l)$ = `T.Sample(samp, float3(uv, l))` 取出 layer $l$ 那个 slice 的 BaseColor

### 1.4 关键约束 1：$\hat{n}$ 必须用未扰动的 dir

R6 引入了 `dirP = normalize(dir + n*A)`——切向偏移让 cell 边蜿蜒。R7 的法线 $\hat{n}$ **不能**用 dirP，必须用原始 dir：

**为什么**：Triplanar 权重 $w_x, w_y, w_z$ 只看 $\hat{n}$ 的方向。如果用 dirP，每个 fragment 的 Triplanar 权重会随 R6 噪声拖拽——同一个 cell 内部紧邻的两个 fragment，dirP 相差几度时 Triplanar 三个面的混合权重会突变，**cell 内部出现 Triplanar 接缝伪影**。

而原始 dir 是 fragment 真实球面方向，跨整个 cell 平滑变化（cell 直径 ~7°，Triplanar 权重在该范围内变化连续），三个面混合稳定。

所以：

| 用途 | 用什么 |
| --- | --- |
| R6 的 $\theta_i = \arccos(\hat{d}'\cdot V_i)$（决定 cell 边形状） | **dirP**（含噪声扰动） |
| R7 的 Triplanar 法线（决定纹理三平面权重） | **dir**（未扰动） |

R6 与 R7 对 dir 的两种用法**严格分离**，互不耦合。详见附录 C。

### 1.5 关键约束 2：Triplanar 输入坐标 $\mathbf{x}$ = WorldPosition

不能用 `dir * Radius`——后者跨星球大小视觉一致（弧度不变），但纹理是按米平铺的，不应该跨星球大小自动缩放。

**正确**：用 fragment 真实 WorldPosition（cm 单位）。`TileScale` 单位为 cm/周期，控制纹理在世界空间的实际大小：

- `TileScale = 100`：每个纹理周期 100 cm = 1 米（典型草皮、沙地颗粒尺度）
- `TileScale = 500`：每个纹理周期 5 米（大尺度地貌）

跨星球大小时调 `TileScale` 而非依赖坐标缩放，这是 PBR 工作流的标准做法。

### 1.6 与 R6 dirP 的协同

R7 完整 PS 流程：

```
1. 解 c0/c1/c2（R3 沿用）
2. CellDirLUT.Load 取 V_A/V_B/V_C（R4 沿用）
3. dir = normalize(WorldPos - PlanetCenter)
4. 采样 nx/ny/nz 噪声分量（R6 沿用）
5. dirP = normalize(dir + n*NoiseAmplitude)（R6 沿用）
6. θ_i = acos(dot(dirP, V_i))（R6 沿用，用 dirP）
7. δ_i = θ_i - min(θ_{j≠i})（R5 沿用）
8. w_i = smoothstep(EW/2, -EW/2, δ_i)（R5 沿用）
9. CellAttrLUT.Load 取 layer_A/layer_B/layer_C（R3 沿用）
10. **R7 新增：3 次 SampleTriplanar(TerrainAlbedoArray, layer_i, WorldPos, dir)**
11. **R7 新增：colA/colB/colC 加权混合（替代 hash 输出）**
```

**不变的（R4/R5/R6）**：cell 拓扑、Voronoi 几何、软边过渡、噪声扰动。

**新增的（R7）**：3 路 Triplanar 采样 = 9 次 `Sample` + 3 次三平面权重计算。

---

## 2. HLSL 实现：SampleTriplanar 内联

### 2.1 UE Custom 节点限制（沿用 R6 §2.1 警示）

UE 把 Custom 节点 Code 包裹在自动生成的函数体内，**不允许嵌套函数定义**。所以 `SampleTriplanar(...)` 不能写成 `float3 SampleTriplanar(...) { ... }`，必须 inline 到调用点。

### 2.2 单点 Triplanar 采样 inline 模板

每次采样写成一个 `{ ... }` scoped block：

```hlsl
// 输入：layer ∈ [0, NumLayers-1]、WorldPos = mathbf{x}、dir = hat{n}
// 输出：col ∈ float3，BaseColor RGB
// 参数：TileScale（cm/周期）、TriplanarSharpness（推荐 4）

float3 col;
{
    float3 _absN = abs(dir);
    _absN = pow(_absN, TriplanarSharpness);
    _absN /= dot(_absN, float3(1.0, 1.0, 1.0));     // 归一化为权重和 = 1

    // UE Texture Object 输入参数 + 自动生成的 Sampler，命名规则：{InputName}, {InputName}Sampler
    //
    // ⚠ 使用 SampleLevel(... , 0) 而非 Sample(...)：
    //   Sample 需要屏幕空间导数（ddx/ddy）隐式计算 mip，在 Lumen / RT Shadows /
    //   RT Reflections 的 ClosestHit 阶段不合法（会报 "Opcode Sample not valid in
    //   shader model lib_6_6(closesthit)"）。
    //   SampleLevel(samp, uv, 0) 显式要求 mip 0，在光栅化 + RT 两路都合法。
    //   代价：远处纹理不会自动选 mip → 可能出现 moire（R7 验证期可接受；R8+ 可改用
    //   SampleGrad 手工传 ddx/ddy、或在 Custom 外部采样后当参数传入）。
    float3 _cX = TerrainAlbedoArray.SampleLevel(TerrainAlbedoArraySampler,
                  float3(WorldPos.yz / TileScale, (float)layer), 0).rgb;
    float3 _cY = TerrainAlbedoArray.SampleLevel(TerrainAlbedoArraySampler,
                  float3(WorldPos.xz / TileScale, (float)layer), 0).rgb;
    float3 _cZ = TerrainAlbedoArray.SampleLevel(TerrainAlbedoArraySampler,
                  float3(WorldPos.xy / TileScale, (float)layer), 0).rgb;

    col = _cX * _absN.x + _cY * _absN.y + _cZ * _absN.z;
}
// 此处 col 可用，_absN/_cX/_cY/_cZ 已超出作用域
```

### 2.3 三层加权混合规则

R7 需要 3 次采样（三个 cell 各一次），加权混合：

```hlsl
float3 colA;  { /* 上面模板，layer = layerA → colA */ }
float3 colB;  { /* 同上，layer = layerB → colB */ }
float3 colC;  { /* 同上，layer = layerC → colC */ }

float wsum = wA + wB + wC + 1e-6;
return (colA * wA + colB * wB + colC * wC) / wsum;
```

权重 $w_A, w_B, w_C$ 与 R6 完全相同——R7 没碰这个公式。

---

## 3. cpp 端改动

### 3.1 [PlanetTopologyDebugMesh.h](../Source/TerraCivilization/Public/Render/PlanetTopologyDebugMesh.h)

新增两个 UPROPERTY：

```cpp
/**
 * R7：地表 BaseColor 纹理数组。
 *
 * 每个 slice = 1 个 layer 的 BaseColor（sRGB）；slice 下标对应 CellAttrLUT.r * 255。
 * 通过编辑器在 Content/Textures/T_TerrainAlbedoArray.uasset 创建，由 19 张
 * T_<Name>_BaseColor 拼合而成。
 *
 * 详细分类与拼装步骤见 R7_TerrainTriplanar.md 附录 A。
 */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PlanetTopology|R7")
TObjectPtr<class UTexture2DArray> TerrainAlbedoArray;

/**
 * R7：地表 Normal 纹理数组（可选，R7 默认仅 BaseColor 通路即可）。
 *
 * 与 TerrainAlbedoArray 的 slice 一一对齐；空时材质退化为只用 BaseColor 平涂。
 */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PlanetTopology|R7")
TObjectPtr<class UTexture2DArray> TerrainNormalArray;

/**
 * R7：Triplanar 三个轴对齐采样平面的混合锐度。
 *
 * 越大 → 混合区越窄、面与面之间过渡越锐利（典型值 4）；
 * 越小 → 三个面更多重叠，看起来"模糊"。
 */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|R7",
          meta = (ClampMin = "1.0", ClampMax = "16.0"))
float TriplanarSharpness = 4.0f;

/**
 * R7：Triplanar 纹理周期长度（cm/周期）。
 *
 *   100  ：每米一个纹理周期，草皮/沙地等近距离细节
 *   500  ：每 5 米，中尺度地貌
 *   2000 ：每 20 米，大尺度地形（远观）
 *
 * 跨星球大小时调这个值——纹理按"米"平铺，不要跨星球自动缩放。
 */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|R7",
          meta = (ClampMin = "10.0", ClampMax = "10000.0"))
float TileScale = 100.0f;
```

### 3.2 [PlanetTopologyDebugMesh.cpp](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp)

**`Rebuild()` MID 注入段（在 R6 已有的 NoiseAmplitude/NoiseScale 之后追加）**：

```cpp
// R7 新增注入：地表纹理 Array + Triplanar 参数
if (TerrainAlbedoArray)
{
    // Texture2DArray 在 UE 5.x 通过 SetTextureParameterValue 直接绑定
    MID->SetTextureParameterValue(TEXT("TerrainAlbedoArray"), TerrainAlbedoArray);
}
if (TerrainNormalArray)
{
    MID->SetTextureParameterValue(TEXT("TerrainNormalArray"), TerrainNormalArray);
}
MID->SetScalarParameterValue(TEXT("TriplanarSharpness"), TriplanarSharpness);
MID->SetScalarParameterValue(TEXT("TileScale"),         TileScale);
```

**反射诊断升级**（R6 期望 11 输入 → R7 期望 13 输入）：

```cpp
const TArray<FString> ExpectedR7Inputs = {
    TEXT("uv0"), TEXT("uv1"), TEXT("uv2"), TEXT("uv3"),
    TEXT("worldpos"), TEXT("planetcenter"),
    TEXT("cellattrlut"), TEXT("celldirlut"),
    TEXT("edgewidth"),
    TEXT("noiseamplitude"), TEXT("noisescale"),
    TEXT("terrainalbedoarray"),                  // R7 新增
    TEXT("triplanarsharpness"), TEXT("tilescale") // R7 新增
};
// TerrainNormalArray 是 R7 可选项，反射诊断不强制要求
```

合规判定保持 R5/R6 同款风格——所有期望输入都齐全 → ✓ R7 compliant；缺哪个 → ✗ Error 提示"这是 R3/R4/R5/R6 旧材质而不是 R7"。

**Output Log 字符串**：

```cpp
UE_LOG(LogPlanetTopologyDebugMesh, Log,
    TEXT("[PlanetTopologyDebugMesh] Rebuilt (R7: 3-layer Triplanar real terrain). ")
    TEXT("SubdivisionLevel=%d  Cells=%d  Corners=%d  Verts=%d  Tris=%d  Radius=%.1f  Smooth=%s  ")
    TEXT("CellAttrLUT=%s  CellDirLUT=%s  PlanetCenter=(%.1f,%.1f,%.1f)  ")
    TEXT("EdgeWidth=%.4f rad (%.2f°)  NoiseAmplitude=%.4f rad (%.2f°)  NoiseScale=%.1f /rad  ")
    TEXT("TerrainAlbedoArray=%s  TileScale=%.0f cm  TriplanarSharpness=%.1f  ")
    TEXT("NumLayersHint=%d"),
    /* ... */,
    TerrainAlbedoArray ? TEXT("OK") : TEXT("MISSING"), TileScale, TriplanarSharpness,
    NumLayersHint);
```

### 3.3 不需要新建动态 LUT 纹理

R7 完全复用 R3 `CellAttrLUT` + R4 `CellDirLUT`——slice 下标从 `CellAttrLUT.r` 读取（已经是 R3 的 LayerIndex 设计）。`TerrainAlbedoArray` 是**静态资产**（编辑器手工创建一次，不在 cpp `Rebuild()` 时重建）。

---

## 4. 材质搭建（M_TopologyDebug_R7）

### 4.1 复制 R6 材质

复制 `M_TopologyDebug_R6.uasset` 重命名为 `M_TopologyDebug_R7.uasset`，保留所有 R6 现有节点（11 个 Inputs 全部保留）。

### 4.2 新增 Material 参数

| 类型 | 名字 | 默认值 | 说明 |
| --- | --- | --- | --- |
| Texture Object Parameter | `TerrainAlbedoArray` | （留空） | Texture2DArray，slice = layer 索引 |
| Texture Object Parameter | `TerrainNormalArray` | （留空） | 可选；R7 阶段可不接 |
| Scalar Parameter | `TriplanarSharpness` | 4.0 | 三平面混合锐度 |
| Scalar Parameter | `TileScale` | 100.0 | 纹理周期长度（cm） |

> ⚠ Texture Object Parameter 创建步骤：在材质图右键 → "Texture Object Parameter"。**不是** `Texture Sample` 节点——后者只能采固定纹理，不接受 MID 注入。

### 4.2.1 ⚠ Texture Object Parameter 必须在「材质图节点本身」上设置默认 Texture

> **R7 验证期实测踩坑（必读）**：仅在 cpp 端通过 MID `SetTextureParameterValue` 注入 `T_TerrainAlbedoArray`、或仅在主材质资产 Details 面板的 Parameter Defaults 列表里挂上 `T_TerrainAlbedoArray`，**都不够**。必须把 Texture2DArray 资产挂在「材质图里那个 Texture Object Parameter 节点本身」的 Details > Texture 槽上。否则材质 shader 编译期使用 1×1 白色 fallback texture 绑定，运行时三层加权采样恒为 (1,1,1)——表现为 **Lit 模式米白带阴影、Unlit 模式纯白**，且 Stats / Output Log / R7-compliance 反射诊断**全部通过、无任何 Error/Warning**，极难定位。

#### 现象

| 检查项 | 状态 |
| --- | --- |
| 材质 Stats 面板 | 无错误 ✓ |
| Output Log | 无 LogShaders / LogMaterial 错误 ✓ |
| R7 反射诊断 | `✓ R7 compliance: ALL 14 expected inputs present & connected` ✓ |
| cpp 端日志 | `TerrainAlbedoArray=OK` ✓ |
| **实际渲染** | **Lit 模式米白、Unlit 模式纯白**（≠ WorldGridMaterial 棋盘格，材质本身正常应用，但 BaseColor=(1,1,1)）❌ |

#### 根因：UE 材质有两套独立的「Parameter 默认值」位置

```
┌────────────────────────────────────────────────────────────────────┐
│  位置 A：材质图节点 Texture Object Parameter 自身的 Details > Texture  │
│  ─────────────────────────────────────────                          │
│  • 用途：shader 编译期的资源绑定（Bind Point 类型推断）                  │
│  • 留空 → UE 用 1×1 白色 fallback texture 绑定，且不报错                │
│  • R7 必须填：T_TerrainAlbedoArray                                     │
└────────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────────┐
│  位置 B：主材质资产 / Material Instance 的 Parameter Defaults 面板       │
│  ─────────────────────────────────────────                          │
│  • 用途：聚合显示 / 在 MI 上覆盖位置 A 的默认值                          │
│  • 主材质上设置该字段 → 仅作为 UI 显示，不回写位置 A、不影响 shader 编译  │
│  • MID 上 SetTextureParameterValue → 仅在位置 A 已正确绑定时才能正确覆盖 │
└────────────────────────────────────────────────────────────────────┘
```

关键 UE 行为：

1. **Shader 编译只读位置 A**。位置 A 留空 → UE 不报错，绑定全局 `WhiteDefaultTexture` fallback；后续无论 MI 还是 MID 都已经基于这个错误的「Texture2D 类型推断」绑定，覆盖也无法把它替换成 Texture2DArray。
2. **位置 B 在主材质上的设置不会回写位置 A**。所以你在主材质 Details 面板看到的 `Terrain Albedo Array` 字段挂上 Texture2DArray 后，视觉上以为生效，但 shader 编译实际仍走 fallback。
3. **MID `SetTextureParameterValue` 是「覆盖」位置 A 的现有绑定**。位置 A 必须先有正确类型（Texture2DArray）的绑定，MID 才能在运行时把 `T_TerrainAlbedoArray` 替换上去；位置 A 是空 → 类型推断错误 → MID 注入静默失败。

#### 强制要求

- **位置 A（必填）**：在材质图里**双击**（或选中后看 Details 面板）`TerrainAlbedoArray` Texture Object Parameter 节点：
    - **`Texture`**：拖入 `T_TerrainAlbedoArray`（Texture2DArray 资产）作为默认值 ✓
    - **`Sampler Type`**：`Color`（BC1/BC3/BC7 sRGB BaseColor）✓
    - **`Sampler Source`**：`Shared: Wrap`（建议；避免 Sampler Resource 上限耗尽）
    - **`Parameter Name`**：必须是 `TerrainAlbedoArray`（与 cpp `SetTextureParameterValue(TEXT("TerrainAlbedoArray"), ...)` 一一对应）
- **位置 B（不要管）**：主材质 Details 面板上聚合显示的 `Terrain Albedo Array` 字段**不需要也不应该手工设置**——它是位置 A 的 UI 投影，且对主材质 shader 编译无效。
- **cpp 端 MID 注入（保持）**：仍按 §3.2 调用 `MID->SetTextureParameterValue(TEXT("TerrainAlbedoArray"), TerrainAlbedoArray)`——这是为了支持「同一主材质 + 多 PMC Actor 实例使用不同 Texture2DArray」的运行时切换。位置 A 的默认值是 fallback；MID 是 per-Actor 覆盖。

> 同样的规则适用于 `TerrainNormalArray` 等所有 Texture Object Parameter 类型 Input：必须在节点本身的 Details > Texture 槽里挂默认值。

### 4.3 改写 Custom 节点

#### 4.3.1 Inputs（在 R6 11 个基础上新增 3 个，第 12/13/14 位）

| # | 名字 | 类型 | 连源 |
| --- | --- | --- | --- |
| 1~11 | UV0 / UV1 / UV2 / UV3 / WorldPos / PlanetCenter / CellAttrLUT / CellDirLUT / EdgeWidth / NoiseAmplitude / NoiseScale | （沿用 R6） | 沿用 R6 |
| 12 | `TerrainAlbedoArray` | Texture2DArray | `TerrainAlbedoArray` Texture Object Parameter |
| 13 | `TriplanarSharpness` | Float1 | `TriplanarSharpness` Scalar Parameter |
| 14 | `TileScale` | Float1 | `TileScale` Scalar Parameter |

> ⚠ Inputs 顺序必须与 Code 形参一一对应——UE 按声明顺序生成 HLSL 形参。

#### 4.3.2 Code（直接复制粘贴）

```hlsl
// =====================================================================
// R7：球面 Voronoi 软边 + dir 切向偏移 + 三层 Triplanar 真实地表
//
// 数学：
//   step 1-9 沿用 R6（dirP 求解 → δ → w）
//   step 10：col_i = Triplanar(TerrainAlbedoArray, layer_i, WorldPos, dir)
//   step 11：color = Σ w_i · col_i / Σ w_i
//
// 关键约束：
//   - Triplanar 法线 hat{n} 用未扰动 dir，而非 R6 dirP（详见 §1.4）
//   - WorldPos 单位 cm，TileScale 也用 cm（详见 §1.5）
// =====================================================================

// ---- 沿用 R3/R4：8-bit 拆分还原 c0/c1/c2 ----
int c0 = (int)(UV1.x + 0.5) * 256 + (int)(UV1.y + 0.5);
int c1 = (int)(UV2.x + 0.5) * 256 + (int)(UV2.y + 0.5);
int c2 = (int)(UV0.x + 0.5) * 256 + (int)(UV0.y + 0.5);

// ---- 沿用 R4：从 CellDirLUT 取三 cell 单位中心方向 ----
float3 V_A = CellDirLUT.Load(int3(c0, 0, 0)).rgb;
float3 V_B = CellDirLUT.Load(int3(c1, 0, 0)).rgb;
float3 V_C = CellDirLUT.Load(int3(c2, 0, 0)).rgb;

// ---- 沿用 R4：球面方向 dir（未扰动，R7 Triplanar 用它做法线）----
float3 dir = normalize(WorldPos - PlanetCenter);

// ---- 沿用 R6：3 个独立 3D value noise 分量 ----
float nx;
{
    float3 _p = dir * NoiseScale + float3(0.0, 0.0, 0.0);
    float3 _i = floor(_p);
    float3 _f = frac(_p);
    float3 _u = _f * _f * (3.0 - 2.0 * _f);
    float3 _q;
    _q = frac((_i + float3(0,0,0)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n000 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(1,0,0)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n100 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(0,1,0)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n010 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(1,1,0)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n110 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(0,0,1)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n001 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(1,0,1)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n101 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(0,1,1)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n011 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(1,1,1)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n111 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    nx = lerp(lerp(lerp(n000, n100, _u.x), lerp(n010, n110, _u.x), _u.y),
              lerp(lerp(n001, n101, _u.x), lerp(n011, n111, _u.x), _u.y),
              _u.z);
}

float ny;
{
    float3 _p = dir * NoiseScale + float3(17.13, 0.0, 0.0);
    float3 _i = floor(_p);
    float3 _f = frac(_p);
    float3 _u = _f * _f * (3.0 - 2.0 * _f);
    float3 _q;
    _q = frac((_i + float3(0,0,0)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n000 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(1,0,0)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n100 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(0,1,0)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n010 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(1,1,0)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n110 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(0,0,1)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n001 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(1,0,1)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n101 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(0,1,1)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n011 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(1,1,1)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n111 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    ny = lerp(lerp(lerp(n000, n100, _u.x), lerp(n010, n110, _u.x), _u.y),
              lerp(lerp(n001, n101, _u.x), lerp(n011, n111, _u.x), _u.y),
              _u.z);
}

float nz;
{
    float3 _p = dir * NoiseScale + float3(0.0, 31.41, 0.0);
    float3 _i = floor(_p);
    float3 _f = frac(_p);
    float3 _u = _f * _f * (3.0 - 2.0 * _f);
    float3 _q;
    _q = frac((_i + float3(0,0,0)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n000 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(1,0,0)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n100 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(0,1,0)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n010 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(1,1,0)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n110 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(0,0,1)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n001 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(1,0,1)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n101 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(0,1,1)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n011 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(1,1,1)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n111 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    nz = lerp(lerp(lerp(n000, n100, _u.x), lerp(n010, n110, _u.x), _u.y),
              lerp(lerp(n001, n101, _u.x), lerp(n011, n111, _u.x), _u.y),
              _u.z);
}

// ---- 沿用 R6：dir 切向偏移 + 球面归一化 ----
float3 dirP = normalize(dir + float3(nx, ny, nz) * NoiseAmplitude);

// ---- 沿用 R5：θ_i / δ_i / w_i（用 dirP）----
float thetaA = acos(saturate(dot(dirP, V_A) * 0.5 + 0.5) * 2.0 - 1.0);
float thetaB = acos(saturate(dot(dirP, V_B) * 0.5 + 0.5) * 2.0 - 1.0);
float thetaC = acos(saturate(dot(dirP, V_C) * 0.5 + 0.5) * 2.0 - 1.0);

float deltaA = thetaA - min(thetaB, thetaC);
float deltaB = thetaB - min(thetaA, thetaC);
float deltaC = thetaC - min(thetaA, thetaB);

float halfEW = EdgeWidth * 0.5;
float wA = smoothstep(halfEW, -halfEW, deltaA);
float wB = smoothstep(halfEW, -halfEW, deltaB);
float wC = smoothstep(halfEW, -halfEW, deltaC);

// ---- 沿用 R3：从 CellAttrLUT 取每 cell 的 LayerIndex ----
int layerA = (int)(CellAttrLUT.Load(int3(c0, 0, 0)).r * 255.0 + 0.5);
int layerB = (int)(CellAttrLUT.Load(int3(c1, 0, 0)).r * 255.0 + 0.5);
int layerC = (int)(CellAttrLUT.Load(int3(c2, 0, 0)).r * 255.0 + 0.5);

// ---- R7 新增 Step 10：3 路 Triplanar 真实地表采样 ----
//   关键：法线用未扰动 dir，不是 dirP（详见 §1.4）

float3 colA;
{
    float3 _absN = abs(dir);
    _absN = pow(_absN, TriplanarSharpness);
    _absN /= dot(_absN, float3(1.0, 1.0, 1.0));
    float3 _cX = TerrainAlbedoArray.SampleLevel(TerrainAlbedoArraySampler, float3(WorldPos.yz / TileScale, (float)layerA), 0).rgb;
    float3 _cY = TerrainAlbedoArray.SampleLevel(TerrainAlbedoArraySampler, float3(WorldPos.xz / TileScale, (float)layerA), 0).rgb;
    float3 _cZ = TerrainAlbedoArray.SampleLevel(TerrainAlbedoArraySampler, float3(WorldPos.xy / TileScale, (float)layerA), 0).rgb;
    colA = _cX * _absN.x + _cY * _absN.y + _cZ * _absN.z;
}

float3 colB;
{
    float3 _absN = abs(dir);
    _absN = pow(_absN, TriplanarSharpness);
    _absN /= dot(_absN, float3(1.0, 1.0, 1.0));
    float3 _cX = TerrainAlbedoArray.SampleLevel(TerrainAlbedoArraySampler, float3(WorldPos.yz / TileScale, (float)layerB), 0).rgb;
    float3 _cY = TerrainAlbedoArray.SampleLevel(TerrainAlbedoArraySampler, float3(WorldPos.xz / TileScale, (float)layerB), 0).rgb;
    float3 _cZ = TerrainAlbedoArray.SampleLevel(TerrainAlbedoArraySampler, float3(WorldPos.xy / TileScale, (float)layerB), 0).rgb;
    colB = _cX * _absN.x + _cY * _absN.y + _cZ * _absN.z;
}

float3 colC;
{
    float3 _absN = abs(dir);
    _absN = pow(_absN, TriplanarSharpness);
    _absN /= dot(_absN, float3(1.0, 1.0, 1.0));
    float3 _cX = TerrainAlbedoArray.SampleLevel(TerrainAlbedoArraySampler, float3(WorldPos.yz / TileScale, (float)layerC), 0).rgb;
    float3 _cY = TerrainAlbedoArray.SampleLevel(TerrainAlbedoArraySampler, float3(WorldPos.xz / TileScale, (float)layerC), 0).rgb;
    float3 _cZ = TerrainAlbedoArray.SampleLevel(TerrainAlbedoArraySampler, float3(WorldPos.xy / TileScale, (float)layerC), 0).rgb;
    colC = _cX * _absN.x + _cY * _absN.y + _cZ * _absN.z;
}

// ---- R7 新增 Step 11：3 层加权混合（替代 R6 的 hash 输出） ----
float wsum = wA + wB + wC + 1e-6;
return (colA * wA + colB * wB + colC * wC) / wsum;
```

### 4.4 输出与材质赋值

1. Custom 节点输出 → Material 的 **BaseColor**（不是 Emissive Color——R7 用真实纹理后应走标准 PBR 通路；R6 的 Emissive 是验证期偷懒做法）
2. 其它 PBR 通道（Roughness / Metallic / Normal）R7 默认空——R7 仅验证"颜色源升级"。如果想加 Normal 真实化，按附录 B
3. **位置 A**：在材质图里选中 `TerrainAlbedoArray` Texture Object Parameter 节点 → Details > **Texture** 槽 → 拖入已拼装的 `T_TerrainAlbedoArray.uasset`（详见附录 A）→ Sampler Type = `Color`（详见 §4.2.1，**这是 R7 渲染正常的硬性前提**）
4. **Apply + Save**
5. 选中 `APlanetTopologyDebugMesh` 实例 → Details → **`PlanetTopology > Material`** 槽位 → 设为 `M_TopologyDebug_R7`
6. **Details → R7 分类 → `Terrain Albedo Array`** 槽位（cpp UPROPERTY，用于 MID 注入；与材质 Details 上的同名字段是两回事）→ 设为同一个 `T_TerrainAlbedoArray.uasset`
    - 可与位置 A 不同：位置 A 是「材质 fallback 默认值」，此处是「per-Actor 运行时覆盖值」；同一主材质多 Actor 实例可挂不同 Texture2DArray
    - 两处都填同一资产 = 该 Actor 用默认地表纹理；不填此处仅填位置 A = 仍能渲染（用位置 A 的 fallback 默认值）
    - **不要只填这里、不填位置 A**——参见 §4.2.1 现象，会导致整球纯白
7. OnConstruction 自动 Rebuild → cpp 端 MID 注入 → 立即看到 R7 真实地表

---

## 5. 验收清单

| 项 | 期望 |
| --- | --- |
| **A. TerrainAlbedoArray 接入** | 反射诊断显示 `TerrainAlbedoArray=OK`；Output Log 含 `TileScale=100 cm  TriplanarSharpness=4.0` |
| **B. NumLayersHint=4 时** | 整球被分成 4 大色块，每块对应一种地表纹理（如草/沙/雪/岩），cell 边沿用 R6 蜿蜒（如果 NoiseAmplitude>0）|
| **C. NumLayersHint=16 时** | cell 之间纹理风格快速变化，邻接 cell 不同纹理时 R5/R6 的过渡带可见 |
| **D. cell 内部纹理无接缝** | 同一 cell 内三个 Triplanar 平面的过渡线（如 +X 与 +Y 面交界）不可见或仅在球极区微弱可见——这是 §1.4 用 dir 而非 dirP 的关键收益 |
| **E. TileScale 联动** | 调 TileScale 100 → 500，纹理周期变大 5 倍（每米一颗草 → 每 5 米一颗草）|
| **F. TriplanarSharpness 联动** | 4 → 1：三平面更多重叠（模糊）；4 → 16：三平面更锐利（球极区可见接缝） |
| **G. 跨 mesh 边纹理连续** | 把相机贴近任意 mesh 边——纹理不出现接缝（WorldPos 跨边连续，dir 跨边连续 → Triplanar 跨边连续）|
| **H. R6 几何完整保留** | EdgeWidth=0 + NoiseAmplitude=0 时 cell 边硬直；EdgeWidth=0.05 + NoiseAmplitude=0.05 时软蜿蜒——纹理在新边几何上正确分布 |
| **I. Output Log** | `Rebuilt (R7: 3-layer Triplanar real terrain) ... TerrainAlbedoArray=OK  TileScale=100 cm  TriplanarSharpness=4.0 ...` |
| **J. 反射诊断** | 14 个 Inputs 全部 `Connected=YES`；`✓ R7 compliance` |

### 5.1 R6 → R7 视觉前后对比

| 观察目标 | R6（哈希色） | R7（Triplanar 真实纹理） |
| --- | --- | --- |
| 单个 cell 颜色 | 纯色（黄金分割哈希） | 真实地表纹理（草/沙/岩等） |
| 同 layer 多 cell 之间 | 颜色完全相同 | 纹理风格相同但具体像素不同（Triplanar 按 WorldPos 采样 → 大星球上每个草地 cell 看起来都不重复）|
| cell 边形状 | R5/R6 控制 | 与 R6 相同（R7 没改） |
| 不同 layer 边界过渡 | 哈希色→哈希色 lerp | 真实纹理→真实纹理 lerp（视觉自然得多）|

---

## 6. 排错表

| 症状 | 根因 | 修复 |
| --- | --- | --- |
| **整球 Lit 模式漆黑（Unlit 模式纹理正常显示）⭐**（R7~R8 经典踩坑，R8 实测修订） | **顶点法线方向反了**：UE5 漫反射用 `saturate(dot(N, L))`（[`ForwardLightingCommon.ush:387-392`](../../Program%20Files/Epic%20Games/UE_5.8/Engine/Shaders/Private/ForwardLightingCommon.ush)）要求 N 与 L 同侧。**球外渲染的球面 mesh 顶点法线必须朝外**（`+UnitCenter`），与几何直觉一致。误填 `-UnitCenter`（朝球心）会让 `dot(N, L) ≤ 0` 大多数像素被 clamp 到 0 → 整球漆黑。⚠ 早期版本曾误归因为"应朝内"，已于 R8 期实测修订（[SphereTopologyReference.md §11.4](SphereTopologyReference.md#114-关联踩坑历史与文档修订)）。R1~R6 用 Unlit 不消费法线所以未暴露 | 直接手填 `Normal = +UnitCenter`（朝外）。Tangents 留空。**不要走 `KismetProceduralMeshLibrary::CalculateTangentsForMesh` 自动法线**——本项目"每 Corner 展开 3 独立顶点"的拓扑下该工具等价于 flat shading，会让阴影边界出三角棱面锯齿（[AgentWorkflow.md §3.13 / §3.14](AgentWorkflow.md)）。详见 [SphereTopologyReference.md §11](SphereTopologyReference.md) / [SphericalSDFTerrainDesign.md §11.2](SphericalSDFTerrainDesign.md#112-pmcptg-渲染契约顶点法线与-ue5-光照约定) |
| 整球纯黑 / 全粉色 | TerrainAlbedoArray 未挂载 | Details 面板 R7 分类下设置 TerrainAlbedoArray；查 Output Log 是否打 `TerrainAlbedoArray=OK` |
| **整球 Lit 米白、Unlit 纯白**（不是 WorldGridMaterial 棋盘格；R7-compliance ✓ 但视觉错） | **位置 A 空槽 fallback**：Texture Object Parameter 节点本身 Details > Texture 槽未挂 Texture2DArray，shader 编译用 1×1 白纹理绑定，三层加权采样恒为 (1,1,1) | 在材质图选中 `TerrainAlbedoArray` Texture Object Parameter 节点 → Details > Texture → 拖入 `T_TerrainAlbedoArray`（详见 §4.2.1）。**仅在 cpp 端 / 仅在主材质 Details 面板的 Parameter Defaults 上挂载都不够** |
| 整球纯紫色（缺纹理） | TerrainAlbedoArray 已挂但 slice 数 < CellAttrLUT.r 实际值范围 | 拼装时确保 slice 数 ≥ NumLayersHint 的最大值（目前 NumLayersHint ClampMax=256，但实际只用前 N 个 slice）|
| cell 内部出现"球极区接缝"（赤道附近清晰、两极有十字纹）| 这是 Triplanar 的固有特性，不是 bug | 可调小 TriplanarSharpness（4 → 2）让混合更平滑；或接受（生产上常见，远观不可见）|
| cell 内部出现"R6 噪声拖拽"伪影 | Triplanar 法线误用了 dirP | 改回未扰动 dir：`float3 _absN = abs(dir);` 而非 `abs(dirP);`（详见 §1.4 / 附录 C）|
| 调 TileScale 视觉无反应 | MID 没注入；或 Custom 节点没声明该 Input | Output Log 查 TileScale 字段；反射诊断查 TileScale Input Connected=YES |
| 纹理跨星球大小被自动缩放 | Triplanar 输入坐标用了 `dir * Radius` 之类与 Radius 耦合的量 | 必须用 WorldPos（fragment 真实坐标，cm 单位），跨星球大小通过调 TileScale 而非缩放坐标 |
| 法线方向纹理拉伸严重（赤道清晰，极区被拉成条带） | TriplanarSharpness 太小（< 2），三个面权重过于平滑 | 调到 4~8 |
| 颜色比预期暗 | TerrainAlbedoArray slice 是否 sRGB 设置正确（SRGB=true）| Texture2DArray 资产属性 → SRGB 勾选；Compression Settings = Default (BC1/BC3) 或 BC7 |
| 编译报错 `function definition is not allowed here` | 在 Code 内写了 `float3 SampleTriplanar(...) { ... }` | 必须 inline 到 scoped block，详见 §2.2 |
| 编译报错 `Opcode Sample not valid in shader model lib_6_6(closesthit)`（RT 阶段 dxil 验证失败） | 用了 `Texture.Sample(...)` 隐式求 mip，RT ClosestHit 阶段没有屏幕导数 | 改为 `Texture.SampleLevel(samp, uv, 0)`（显式 mip 0）；UE 5.8 默认会为材质编译 RT 变体（Lumen / RTShadows / RTReflections），只要开启其中任一项就会触发该验证。详见 §2.2 注释 |
| 颜色出现不连续色块 | Inputs 顺序错位（如 TerrainAlbedoArray 形参实际拿到的是 Float） | 严格按 §4.3.1 顺序添加 Inputs |
| GPU 性能下降明显 | 3 路 Triplanar = 9 次 `Sample`（Texture2DArray.Sample 走采样器，比 Load 慢约 2x）| sub=3 642 cells / 1080p 视口下应在 ~1.0 ms 增量内（与 SDF §11 估算一致）；如有压力降到 2 路（仅取 max(wA,wB) 两层）|

---

## 7. 后续路径预告（R8+）

| 阶段 | 接续动作 |
| --- | --- |
| **R8** | 接入 WorldGen 的 `FCellGeoData → LayerIndex` 替换 R3 的 Knuth 哈希 placeholder——`RebuildCellAttrLUT_` 改为按 `TerrainTag → LayerIndex` 写入 |
| **R9** | 加 Decor / Owner / Fog 三套独立 LUT（政治版图、战争迷雾、可达范围）—— Triplanar 通路保留，新增 `Texture2DArray` 通路单独混合 |
| **R10** | LOD 优化：远距离用 R4（无噪声、无 Triplanar，只哈希色 + 测地线硬边）、近距离用 R7 全套 |
| **R11** | R7 的 PS 核心 HLSL 在 PTG mesh 上零修改复用，仅 c0/c1/c2 来源换成 GPU `FindNearestCell` |

---

## 附录 A：当前已导入 19 个 layer 的分类与 Texture2DArray 拼装

### A.1 已导入资产清单（[Content/Textures/](../Content/Textures/)）

19 个 layer，每个含 `BaseColor` + `Normal` 双通道，按地表生物群系分类：

| 分类 | layer 数 | 资产名（前缀 `T_`，后缀 `_BaseColor` / `_Normal`） |
| --- | --- | --- |
| **草地** | 5 | `Grass_1` / `Grass_2` / `Grass_3` / `Grass_4` / `Lawn_Grass` |
| **沙地** | 2 | `Sand` / `Beach_Sand` |
| **海岸** | 1 | `Shoreline_Beach_Rocks` |
| **雪地** | 1 | `Snow` |
| **雪岩混合** | 5 | `SnowRock_1` / `SnowRock_2` / `SnowRock_3` / `SnowRock_4` / `SnowRock_5` |
| **岩石** | 3 | `Stone_1` / `Stone_2` / `Stone_3` |
| **特殊** | 2 | `Lava`（火山）/ `MarbleAquaBlue`（海洋面预留）|
| **合计** | **19** | — |

### A.2 推荐的 R7 验证期 layer 索引映射

R7 验证期不接 WorldGen，CellAttrLUT 用 R3 Knuth 哈希填 LayerIndex ∈ [0, NumLayersHint-1]。建议把 NumLayersHint 设为 **8** 或 **16** 选取代表性 layer：

| LayerIndex | NumLayersHint=8 时映射 | NumLayersHint=16 时映射 |
| --- | --- | --- |
| 0 | Grass_1 | Grass_1 |
| 1 | Sand | Grass_2 |
| 2 | Snow | Grass_3 |
| 3 | Stone_1 | Sand |
| 4 | SnowRock_1 | Beach_Sand |
| 5 | Lawn_Grass | Snow |
| 6 | Beach_Sand | SnowRock_1 |
| 7 | Lava | Stone_1 |
| 8 | — | SnowRock_2 |
| 9 | — | SnowRock_3 |
| 10 | — | Stone_2 |
| 11 | — | Stone_3 |
| 12 | — | Lawn_Grass |
| 13 | — | Shoreline_Beach_Rocks |
| 14 | — | MarbleAquaBlue |
| 15 | — | Lava |

> R8 接入 WorldGen 后，这个映射会被 `TerrainTag → LayerIndex` 取代。

### A.3 拼装 TerrainAlbedoArray 步骤

1. **Content Browser**：导航到 `Content/Textures/` 目录
2. **右键 → Texture → Texture 2D Array**：创建空数组资产，命名为 `T_TerrainAlbedoArray`
3. **打开 T_TerrainAlbedoArray**：Asset Editor 顶部 → "Source Textures" 面板
4. **按 §A.2 的顺序**把 19 个 `T_*_BaseColor` 拖入 Source Textures 列表（顺序就是 slice 下标）
5. **属性设置**：
    - SRGB = ✅（BaseColor 是 sRGB）
    - Compression Settings = `Default (BC1/BC3)` 或 `BC7`
    - Mip Gen Settings = `FromTextureGroup` 或 `Sharpen0`
    - Filter = `Default (Trilinear)`
    - **所有 19 张源纹理必须是同样的分辨率与同样的 Format**——UE 不允许 Texture2DArray 内 slice 尺寸不一致
6. **Save**

### A.4（可选）拼装 TerrainNormalArray

同上但：
- 拼 19 个 `T_*_Normal`
- SRGB = ❌（Normal 是 Linear）
- Compression Settings = `Normalmap (BC5)`

R7 阶段 Normal Array 可以暂不拼，仅 BaseColor 通路就能完成验收。

### A.5 资产分辨率与显存估算

每张源纹理大小约 ~1 MB（512² ~ 1024² BC1 压缩）。Texture2DArray 拼合 19 slice：

| 分辨率 | 单 slice 大小（BC1） | 19 slice 总 | 6 mip 含残留 | GPU 显存 |
| --- | --- | --- | --- | --- |
| 512² | 128 KB | 2.4 MB | 3.2 MB | 可忽略 |
| 1024² | 512 KB | 9.7 MB | 13 MB | 可忽略 |
| 2048² | 2 MB | 38 MB | 51 MB | 中等 |
| 4096² | 8 MB | 152 MB | 203 MB | 偏大 |

**建议 R7 阶段全部 1024²**——视觉质量足够、显存压力小、加载快。R8+ 切到生产纹理时再考虑 2048²。

---

## 附录 B：TileScale / TriplanarSharpness 调参参考

### B.1 TileScale（cm/周期）

控制纹理在世界空间的实际平铺密度。`TileScale = 100` 意味着每 100 cm（1 米）走一个完整纹理周期。

| 想要的视觉风格 | TileScale 推荐 | 适用场景 |
| --- | --- | --- |
| 极近距离草皮颗粒可见 | 50 | 单位人物视角 |
| 中近距离地表细节 | 100~200 | 玩家俯视 cell 视角（默认）|
| 中尺度地貌 | 500 | 地图视角，能看到整片大陆 |
| 大尺度抽象 | 2000 | 远观行星全景 |

> 跨星球大小时调这个值。半径 15000 cm 的小行星与半径 600000 cm 的大行星，TileScale 都用 100 不需要变（因为是按米平铺，不是按角度）。

### B.2 TriplanarSharpness

控制三个轴对齐采样平面的混合锐度。法线分量经 `pow(absN, sharpness)` 后归一化为权重。

| TriplanarSharpness | 视觉效果 | 适用场景 |
| --- | --- | --- |
| 1 | 三平面均匀混合（极模糊）| 不推荐 |
| 2 | 平滑过渡，球极区软糊 | 卡通风格 |
| **4** | **平衡（默认）**| 大多数地表 |
| 8 | 三平面快速切换，球极区可见但锐利 | 写实地形 |
| 16 | 接近硬切，球极区出现"+"形接缝 | 验证用 |

**不要超过 16**——会让 Triplanar 退化为非连续 piecewise，球极区出现明显伪影。

---

## 附录 C：为什么 Triplanar 法线必须用未扰动 dir

> R6 的 dirP = normalize(dir + n*A) 是为了让 cell 边蜿蜒——它在 cell 内部紧邻像素之间会因为噪声而有几度的方向抖动。
>
> Triplanar 权重是 `|n|^k / sum`：法线方向稍微偏离主轴时，三个面的权重就会快速重新分配（这是它能做"轴对齐三平面平滑混合"的根本机制）。
>
> 如果用 dirP 做法线：
> - cell 内部紧邻 fragment 的 dirP 因噪声有微小抖动
> - Triplanar 权重剧烈重新分配
> - **同一个 cell 内部出现"噪声形态的纹理切换"伪影**——草地纹理在某个区域突然变成另一种 Triplanar 平面权重组合
>
> 用未扰动 dir：
> - cell 内部紧邻 fragment 的 dir 跨像素平滑变化（球面外法是连续的）
> - Triplanar 权重平滑变化
> - cell 内部纹理风格统一
>
> R6 dirP 与 R7 dir 的两种用法**严格分离**：
>
> | 量 | 用途 | 形态 |
> | --- | --- | --- |
> | dir | Triplanar 法线、纹理坐标基础 | 平滑 |
> | dirP | 决定 cell 边形状的"形变后方向" | 含噪声 |
>
> 这两种方向都"正确"——它们各自服务不同目的。R7 必须严格区分。

如果你"故意"想让 cell 内部纹理也有噪声扰动（比如风吹草动效果），不应该改 Triplanar 法线，而应该在 step 11 之后加一层"颜色扰动"（如 `color *= 1 + 0.1 * Noise3D(WorldPos / 50)`）。这是 R8+ 的范畴。
