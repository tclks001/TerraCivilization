# R6：边界 3D 噪声扰动（让 cell 边变蜿蜒）

> 本文档是 [SphericalSDFTerrainDesign.md §6.4.1](SphericalSDFTerrainDesign.md#641-r6-落地方案在-r5-球面距离-δ-上叠加噪声扰动与-r4r5-几何兼容) 的独立落地文档，与 [R2_TopologyDebugMaterial.md](R2_TopologyDebugMaterial.md) / [R3_CellAttrLUTMaterial.md](R3_CellAttrLUTMaterial.md) / [R4_VoronoiBoundary.md](R4_VoronoiBoundary.md) / [R5_SharpenSoftEdge.md](R5_SharpenSoftEdge.md) 风格一致。
>
> 阅读本文档前必须先理解 [R5_SharpenSoftEdge.md](R5_SharpenSoftEdge.md) §1 的"球面有符号弧度距离 δ"——R6 直接构筑在 R5 的 δ 计算之上，新增的只是"对 δ 的噪声扰动"。

---

## 0. R6 一句话目标

在 R5 的球面 Voronoi 软边基础上，对每个 cell 的 $\delta_i$ 叠加一个 per-cell 3D 噪声值，让本应是测地线大圆弧的 cell 边变成**蜿蜒曲线**（海岸线、山脚、沙漠/草原过渡带的有机形态），同时**保持跨 mesh 边的形状连续性**（不出现接缝）。

```
R5（NoiseAmplitude = 0）          R6（NoiseAmplitude = 0.05, NoiseScale = 10）
┌─────────────────┐                ┌────~~~~~~~~~~~~┐
│  Cell A         │                │  Cell A      ~~│
│   ─────────────  │                │     ~~~~~~~~~~~ │
│  Cell B         │                │  Cell B      ~~│
└─────────────────┘                └────~~~~~~~~~~~~┘
   测地线大圆弧                      蜿蜒曲线（仍可识别原 cell 形状）
```

| (EdgeWidth, NoiseAmplitude) | 视觉效果 |
| --- | --- |
| (0, 0) | R4 测地线硬直边（参考帧） |
| (0, 0.05) | **硬边但蜿蜒**——Civ 风格但带有机形态 |
| (0.05, 0) | R5 软直边（无噪声渐变） |
| (0.05, 0.05) | **软边且蜿蜒**——E&D 风格的有机过渡（最丰富） |

**EdgeWidth 与 NoiseAmplitude 在视觉上完全正交**：前者控制"边带的软硬"，后者控制"边形的直蜿"，可独立调节。

---

## 1. 几何与数学

### 1.1 R5 几何回顾（必读）

R5 的核心量是**有符号球面弧度距离** $\delta_i$：

$$
\delta_i = \theta_i - \min_{j\neq i}\theta_j, \quad \theta_i = \arccos(\hat{d}\cdot V_i)
$$

软边权重：

$$
w_i = \mathrm{smoothstep}\!\left(\tfrac{\text{EdgeWidth}}{2},\ -\tfrac{\text{EdgeWidth}}{2},\ \delta_i\right)
$$

cell $i$ 与 cell $j$ 的 Voronoi 边 = $\delta_i = \delta_j = 0$ 的轨迹（或等价地 $\theta_i = \theta_j$，一条测地线大圆弧）。

### 1.2 R6 核心：在 δ 上叠 per-cell 噪声

$$
\boxed{\ \tilde\delta_i = \delta_i + n_i(\hat{d}) \cdot \text{NoiseAmplitude}\ }
$$

软边权重沿用 R5 公式，但用 $\tilde\delta_i$ 替代 $\delta_i$：

$$
w_i = \mathrm{smoothstep}\!\left(\tfrac{\text{EdgeWidth}}{2},\ -\tfrac{\text{EdgeWidth}}{2},\ \tilde\delta_i\right)
$$

**几何含义**：噪声 $n_i$ 让 cell $i$ 的"代表距离"在原始 $\delta_i$ 基础上随空间位置波动 ±NoiseAmplitude 弧度。被波动的 Voronoi 边 = $\tilde\delta_i = \tilde\delta_j$ 的轨迹，原本的大圆弧被噪声扰动成蜿蜒曲线。

### 1.3 关键约束 1：noise 必须 per-cell 独立

$n_i$ 不能是"全局 3 分量噪声 `noise3D(dir).xyz` 的第 i 分量"（§6.4 路径 A 原文写法）——这种做法在 R4/R5 路径 B 下会出现严重伪影：

**问题场景**：fragment 在 c0 与 c1 的 Voronoi 边附近，c2 实际离很远但仍参与三角形顶点。如果 $n_2 = \text{globalNoise.z}$ 与 $n_0, n_1$ 共用同一个空间位置的噪声，那么"c0/c1 边的形状"会受 c2 的位置影响——而 c2 在跨 mesh 边时会换成另一个 cell（c2'），导致边形状跳变 → mesh 边可见接缝。

**正确做法**：$n_i$ 只能是 **(dir, $V_i$) 的函数**，与 c2 无关：

$$
n_i(\hat{d}) = \text{noise3D}\!\left(\hat{d}\cdot \text{NoiseScale} + V_i \cdot \text{HashOffset}\right)
$$

这样 c0、c1 的噪声在跨 mesh 边时**完全不变**（V_0, V_1 跨边都是同一个 cell）→ 边形状跨边连续。

### 1.4 关键约束 2：噪声采样输入用 $\hat{d}$ 而非 WorldPos

球面渲染下，$\hat{d}$（单位方向）与 WorldPos = $\hat{d} \times \text{Radius} + \text{Center}$ 是一一对应的——但用 $\hat{d}$ 采样：

- ✅ 跨星球半径变化时，噪声视觉频率保持一致（NoiseScale 直接决定每弧度多少周期）
- ✅ 跨 sub 等级时，噪声形态跟物理弧度对齐，不被三角形大小耦合
- ✅ R11 PTG 路线下零修改：dir = normalize(WorldPos - PlanetCenter)，PTG 高细分 mesh 同样如此

如果用 WorldPos 采样：Radius 变化 10 倍 → 噪声频率变化 10 倍；不同行星视觉风格不一致。

### 1.5 跨 mesh 边连续性证明

考虑 mesh 边 $V_A V_B$ 上的一点 $\hat{d}_M$（其归属的两个相邻三角形分别有 cell 集合 $\{A, B, C_1\}$ 与 $\{A, B, C_2\}$）。

- $n_A(\hat{d}_M)$、$n_B(\hat{d}_M)$ 完全由 $(\hat{d}_M, V_A, V_B)$ 决定，与 $C_1$ 或 $C_2$ 无关 → 跨边相同
- $\delta_A, \delta_B$ 在 mesh 边上同样跨边相同（详见 [R4_VoronoiBoundary.md §2.3](R4_VoronoiBoundary.md#23-几何性质为什么没有折角)）
- ⇒ $\tilde\delta_A, \tilde\delta_B$ 跨 mesh 边连续 ⇒ cell A 与 cell B 的扰动 Voronoi 边在 mesh 边上**完全重合，没有接缝**

只有 $C_1$ 和 $C_2$ 各自的 $\tilde\delta_C$ 在边上不同——但这两个 cell 在 mesh 边上 $\delta_C \to +\infty$（即 $\theta_C \gg \theta_A, \theta_B$），$w_C \to 0$，对最终颜色无贡献。所以视觉上无可见跳变。

### 1.6 数值边界与上限

NoiseAmplitude 的合理范围是 $[0, \min_\text{Tri}(\text{TriRadius})]$，其中 TriRadius 是三角形外心到顶点的角距离：

| sub | NumCells | 三角形外接圆半径（弧度） | NoiseAmplitude 上限 |
| --- | --- | --- | --- |
| 3 | 642 | ≈ 0.184 | 0.10（推荐 ≤ 0.1） |
| 4 | 2562 | ≈ 0.092 | 0.05 |
| 5 | 10242 | ≈ 0.046 | 0.025 |

超过该值会让噪声把 cell 边推出三角形覆盖范围，导致 cell A 的 "argmin" 在某些位置变成 cell B 但 fragment 实际不在 cell B 三角形内 → 颜色乱变（"互锁"伪影，见 §12 风险表）。

R6 默认 `NoiseAmplitude ∈ [0, 0.1]`，cpp 端 ClampMax = 0.1。

---

## 2. HLSL 噪声实现

### 2.1 选型

UE 没有内置可用于 Custom 节点的 simplex noise。三种实现路径：

| 选项 | 做法 | 优点 | 缺点 |
| --- | --- | --- | --- |
| A. hash sin 噪声 | `frac(sin(dot(p, c)) * 43758)` | 1 行实现 | 条带状不均匀，方向感强 |
| B. UE Material `Noise` 节点 | 用材质图节点 | 质量好 | Custom 节点内不易调用，需在外层包装 |
| **C. hash-based 3D value noise**（**推荐**） | 8 角点哈希 + 三线性插值 | 球面方向各向同性、可微、无条带 | 约 30 行 HLSL（采样点） |

R6 默认采用**选项 C**——质量足够、嵌入 Custom 节点直接复制粘贴。

> ⚠ **UE Custom 节点的 HLSL 限制（必读）**：UE 把 Custom 节点的 Code 字段包裹在自动生成的 `MaterialFloat3 CustomExpressionN(FMaterialPixelParameters Parameters, ...)` 函数体内（详见 [`HLSLMaterialTranslator::CustomExpression`](C:/Program%20Files/Epic%20Games/UE_5.8/Engine/Source/Runtime/Engine/Private/Materials/HLSLMaterialTranslator.cpp)）。这意味着：
> - ❌ **不能在 Code 内定义辅助函数**（`function definition is not allowed here`）
> - ❌ **不能在 Code 内 `#include` 头文件**（要用 Custom 节点的 `IncludeFilePaths` 字段）
> - ✅ **可以用 `{ ... }` scoped block 让局部变量作用域不冲突**
> - ✅ **可以在 Code 内用 `#define` 宏**（但宏不能含 statement，仅能是表达式）
>
> 因此 R6 不能写 `float Hash13(...) { ... }` —— 必须**把噪声逻辑完全 inline 到调用点**，下文给出的就是这个 inline 形态。

### 2.2 噪声实现：scoped block 内联（每个采样点 1 个 block）

每次采样写成一个 `{ ... }` 块，输入 `p`（采样坐标）输出 `n`（噪声值）。块内所有局部变量（`i`, `u`, `q`, `n000..n111`）作用域局部，多次采样互不干扰：

```hlsl
// ====== 单次 3D value noise 采样 ======
// 输入：float3 p ∈ R³（任意 3D 坐标）
// 输出：float n ∈ [-1, 1]（C⁰ 连续，三线性插值，方向各向同性）
// 算法：8 角点 hash + 三线性插值
// 开销：~50 ALU（8 次 hash × ~4 ALU + 7 次 lerp）
// 注意：这是**模板**，不是函数；下文 §4.3.2 把它直接 inline 到调用点

float n;            // 在外层声明输出变量
{
    float3 _p = p;                                       // 临时副本，避免修改外部变量
    float3 _i = floor(_p);
    float3 _f = frac(_p);
    float3 _u = _f * _f * (3.0 - 2.0 * _f);              // smoothstep 平滑插值
    float3 _q;                                           // hash 用临时向量

    // ---- 8 个立方体角点的 hash 值 ----
    // 单点 hash 公式（无函数、无副作用）：
    //   _q = frac((i + offset) * 0.1031);
    //   _q += dot(_q, _q.yzx + 33.33);
    //   n??? = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;       ∈ [-1, 1]
    _q = frac((_i + float3(0,0,0)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n000 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(1,0,0)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n100 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(0,1,0)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n010 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(1,1,0)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n110 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(0,0,1)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n001 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(1,0,1)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n101 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(0,1,1)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n011 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;
    _q = frac((_i + float3(1,1,1)) * 0.1031); _q += dot(_q, _q.yzx + 33.33); float n111 = frac((_q.x + _q.y) * _q.z) * 2.0 - 1.0;

    // ---- 三线性插值 ----
    n = lerp(lerp(lerp(n000, n100, _u.x), lerp(n010, n110, _u.x), _u.y),
             lerp(lerp(n001, n101, _u.x), lerp(n011, n111, _u.x), _u.y),
             _u.z);
}
// 此处 n ∈ [-1, 1] 可用
```

### 2.3 per-cell 采样规则

每个 cell 的噪声采样输入坐标必须**显式包含 V_i**（cell 中心方向），让"被扰动的 Voronoi 边"在跨 mesh 边时不依赖 c2（详见 §1.3）：

```hlsl
// 三个 cell 各采样一次，共 3 个 scoped block；每块输出独立的 nA / nB / nC
//
// Hash offset 7.919 是任意非整数大数，让不同 cell 的采样空间错开（详见附录 A）

float nA;
{ float3 p = dir * NoiseScale + V_A * 7.919;  /* ... 上面 §2.2 模板，输出到 n ... */  nA = n; }

float nB;
{ float3 p = dir * NoiseScale + V_B * 7.919;  /* 同上 */  nB = n; }

float nC;
{ float3 p = dir * NoiseScale + V_C * 7.919;  /* 同上 */  nC = n; }
```

> 上面是逻辑示意；§4.3.2 给出可直接复制粘贴的完整 Code 块。

每个 $n_i \in [-1, 1]$，乘以 `NoiseAmplitude` 后变为弧度量加到 $\delta_i$ 上。


---

## 3. cpp 端改动

### 3.1 [PlanetTopologyDebugMesh.h](../Source/TerraCivilization/Public/Render/PlanetTopologyDebugMesh.h)

新增两个 UPROPERTY：

```cpp
/**
 * R6：边界噪声振幅（绝对弧度，与 EdgeWidth 同制）。
 *
 *   0      ：无噪声扰动，退化为 R5 测地线直边
 *   0.02   ：约 1.15°，微妙起伏
 *   0.05   ：约 2.86°，明显蜿蜒（推荐默认值）
 *   0.10   ：约 5.73°，强变形
 *
 * 上限：必须 < TriRadius（sub=3 时 ≈ 0.184 弧度，sub=4 时 ≈ 0.092 弧度），
 *       超过会导致"互锁"伪影（见 SDF 设计稿 §12 风险表）。
 *       cpp 端 ClampMax = 0.1 防误用，跨 sub 都安全。
 *
 * 与 EdgeWidth 正交：
 *   (EdgeWidth, NoiseAmplitude) = (0, 0)     → R4 硬直边
 *   (EdgeWidth, NoiseAmplitude) = (0, 0.05)  → 硬蜿蜒边
 *   (EdgeWidth, NoiseAmplitude) = (0.05, 0)  → R5 软直边
 *   (EdgeWidth, NoiseAmplitude) = (0.05, 0.05) → 软蜿蜒边（最丰富）
 */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|R6",
          meta = (ClampMin = "0.0", ClampMax = "0.1"))
float NoiseAmplitude = 0.0f;

/**
 * R6：边界噪声频率（每弧度周期数）。
 *
 *   5    ：低频，大尺度起伏（海岸线）
 *   10   ：中频，明显蜿蜒（推荐默认值）
 *   20   ：高频，细密锯齿（沙地纹理）
 *   50   ：极高频，几乎成像素抖动
 *
 * 0 时噪声退化为常数（无视觉效果），所以 ClampMin = 0.1 兜底。
 */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|R6",
          meta = (ClampMin = "0.1", ClampMax = "100.0"))
float NoiseScale = 10.0f;
```

### 3.2 [PlanetTopologyDebugMesh.cpp](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp)

**`Rebuild()` MID 注入段（在 R5 已有的 EdgeWidth 之后追加）**：

```cpp
// R6 新增注入：边界噪声振幅与频率
//   NoiseAmplitude 单位：绝对弧度（与 EdgeWidth 同制）
//   NoiseScale     单位：每弧度周期数
MID->SetScalarParameterValue(TEXT("NoiseAmplitude"), NoiseAmplitude);
MID->SetScalarParameterValue(TEXT("NoiseScale"),     NoiseScale);
```

**反射诊断升级**（R5 期望 9 输入 → R6 期望 11 输入）：

```cpp
const TArray<FString> ExpectedR6Inputs = {
    TEXT("uv0"), TEXT("uv1"), TEXT("uv2"), TEXT("uv3"),
    TEXT("worldpos"), TEXT("planetcenter"),
    TEXT("cellattrlut"), TEXT("celldirlut"),
    TEXT("edgewidth"),
    TEXT("noiseamplitude"), TEXT("noisescale")  // R6 新增
};
```

合规判定逻辑保持 R4/R5 同款风格——所有期望输入都齐全 → ✓ R6 compliant；缺哪个 → ✗ Error 提示"这是 R3/R4/R5 旧材质而不是 R6"。

**Output Log 字符串**：

```cpp
UE_LOG(LogPlanetTopologyDebugMesh, Log,
    TEXT("[PlanetTopologyDebugMesh] Rebuilt (R6: per-cell noise on δ + soft edge). ")
    TEXT("SubdivisionLevel=%d  Cells=%d  Corners=%d  Verts=%d  Tris=%d  Radius=%.1f  Smooth=%s  ")
    TEXT("CellAttrLUT=%s  CellDirLUT=%s  PlanetCenter=(%.1f,%.1f,%.1f)  ")
    TEXT("EdgeWidth=%.4f rad (%.2f°)  NoiseAmplitude=%.4f rad (%.2f°)  NoiseScale=%.1f /rad  ")
    TEXT("NumLayersHint=%d"),
    /* ... */, NoiseAmplitude, NoiseAmplitude * 180.0 / PI, NoiseScale, NumLayersHint);
```

### 3.3 不需要新建 LUT 纹理

R6 完全复用 R3 `CellAttrLUT` + R4 `CellDirLUT`——没有任何新增 cpp 端纹理资源。所有新增数据都是 2 个标量参数。

---

## 4. 材质搭建（M_TopologyDebug_R6）

### 4.1 复制 R5 材质

复制 `M_TopologyDebug_R5.uasset` 重命名为 `M_TopologyDebug_R6.uasset`，保留所有 R5 现有节点。

### 4.2 新增 Material 参数

| 类型 | 名字 | 默认值 | 范围 |
| --- | --- | --- | --- |
| Scalar Parameter | `NoiseAmplitude` | 0.05 | [0, 0.1] |
| Scalar Parameter | `NoiseScale` | 10.0 | [0.1, 100] |

### 4.3 改写 Custom 节点

#### 4.3.1 Inputs（在 R5 9 个基础上新增 2 个，第 10/11 位）

| # | 名字 | 类型 | 连源 |
| --- | --- | --- | --- |
| 1 | `UV0` | Float2 | `TextureCoordinate(CoordinateIndex=0)` |
| 2 | `UV1` | Float2 | `TextureCoordinate(CoordinateIndex=1)` |
| 3 | `UV2` | Float2 | `TextureCoordinate(CoordinateIndex=2)` |
| 4 | `UV3` | Float2 | `TextureCoordinate(CoordinateIndex=3)` |
| 5 | `WorldPos` | Float3 | `WorldPosition`（Absolute World Position 节点） |
| 6 | `PlanetCenter` | Float3 | `PlanetCenter` Vector Parameter（取 RGB） |
| 7 | `CellAttrLUT` | Texture2D | `CellAttrLUT` Texture Object Parameter |
| 8 | `CellDirLUT` | Texture2D | `CellDirLUT` Texture Object Parameter |
| 9 | `EdgeWidth` | Float1 | `EdgeWidth` Scalar Parameter |
| 10 | `NoiseAmplitude` | Float1 | `NoiseAmplitude` Scalar Parameter |
| 11 | `NoiseScale` | Float1 | `NoiseScale` Scalar Parameter |

> ⚠ Inputs 顺序必须与 Code 形参一一对应——UE 按声明顺序生成 HLSL 形参。

#### 4.3.2 Code（直接复制粘贴）

```hlsl
// =====================================================================
// R6：球面 Voronoi 软边 + per-cell 3D 噪声扰动
//
// 数学：
//   θ_i  = acos(dot(dir, V_i))
//   δ_i  = θ_i - min(θ_{j≠i})
//   n_i  = Noise3D(dir * NoiseScale + V_i * 7.919)   ∈ [-1, 1]
//   δ̃_i = δ_i + n_i * NoiseAmplitude                  绝对弧度
//   w_i  = smoothstep(EdgeWidth/2, -EdgeWidth/2, δ̃_i)
//   color = Σ w_i · hash(layer_i+1) / Σ w_i
//
// 退化关系：
//   NoiseAmplitude = 0  → R5 软直边
//   EdgeWidth = 0       → R4 + 噪声硬蜿蜒边（噪声仍生效）
//   两者都 = 0          → R4 硬直边
// =====================================================================

// ----- 3D Hash + Value Noise -----
// UE Custom 节点不允许嵌套函数定义，所以噪声完全 inline 到调用点（每点一个 scoped block）。

// ---- 沿用 R3/R4：8-bit 拆分还原 c0/c1/c2 ----
int c0 = (int)(UV1.x + 0.5) * 256 + (int)(UV1.y + 0.5);
int c1 = (int)(UV2.x + 0.5) * 256 + (int)(UV2.y + 0.5);
int c2 = (int)(UV0.x + 0.5) * 256 + (int)(UV0.y + 0.5);

// ---- 沿用 R4：从 CellDirLUT 取三 cell 单位中心方向 ----
float3 V_A = CellDirLUT.Load(int3(c0, 0, 0)).rgb;
float3 V_B = CellDirLUT.Load(int3(c1, 0, 0)).rgb;
float3 V_C = CellDirLUT.Load(int3(c2, 0, 0)).rgb;

// ---- 沿用 R4：球面方向 dir ----
float3 dir = normalize(WorldPos - PlanetCenter);

// ---- 沿用 R5：球面角距离 θ_i ----
float thetaA = acos(saturate(dot(dir, V_A) * 0.5 + 0.5) * 2.0 - 1.0);
float thetaB = acos(saturate(dot(dir, V_B) * 0.5 + 0.5) * 2.0 - 1.0);
float thetaC = acos(saturate(dot(dir, V_C) * 0.5 + 0.5) * 2.0 - 1.0);

// ---- 沿用 R5：到 Voronoi 边的有符号弧度距离 δ_i ----
float deltaA = thetaA - min(thetaB, thetaC);
float deltaB = thetaB - min(thetaA, thetaC);
float deltaC = thetaC - min(thetaA, thetaB);

// ---- R6 新增 Step 1：per-cell 噪声 n_i ----
//   采样坐标 p_i = dir * NoiseScale + V_i * 7.919
//   关键：噪声采样种子用 V_i 而非全局共享，保证跨 mesh 边连续（详见 §1.3 / §1.5）
//   实现：3D value noise（8 角点 hash + 三线性插值），完全 inline 到 scoped block

float nA;
{
    float3 _p = dir * NoiseScale + V_A * 7.919;
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
    nA = lerp(lerp(lerp(n000, n100, _u.x), lerp(n010, n110, _u.x), _u.y),
              lerp(lerp(n001, n101, _u.x), lerp(n011, n111, _u.x), _u.y),
              _u.z);
}

float nB;
{
    float3 _p = dir * NoiseScale + V_B * 7.919;
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
    nB = lerp(lerp(lerp(n000, n100, _u.x), lerp(n010, n110, _u.x), _u.y),
              lerp(lerp(n001, n101, _u.x), lerp(n011, n111, _u.x), _u.y),
              _u.z);
}

float nC;
{
    float3 _p = dir * NoiseScale + V_C * 7.919;
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
    nC = lerp(lerp(lerp(n000, n100, _u.x), lerp(n010, n110, _u.x), _u.y),
              lerp(lerp(n001, n101, _u.x), lerp(n011, n111, _u.x), _u.y),
              _u.z);
}

// ---- R6 新增 Step 2：扰动 δ̃_i = δ_i + n_i * NoiseAmplitude ----
deltaA += nA * NoiseAmplitude;
deltaB += nB * NoiseAmplitude;
deltaC += nC * NoiseAmplitude;

// ---- 沿用 R5：smoothstep 软边权重（用扰动后的 δ̃）----
float halfEW = EdgeWidth * 0.5;
float wA = smoothstep(halfEW, -halfEW, deltaA);
float wB = smoothstep(halfEW, -halfEW, deltaB);
float wC = smoothstep(halfEW, -halfEW, deltaC);

// ---- 沿用 R5：三层独立 hash + 加权混合 ----
int layerA = (int)(CellAttrLUT.Load(int3(c0, 0, 0)).r * 255.0 + 0.5);
int layerB = (int)(CellAttrLUT.Load(int3(c1, 0, 0)).r * 255.0 + 0.5);
int layerC = (int)(CellAttrLUT.Load(int3(c2, 0, 0)).r * 255.0 + 0.5);

float hueA = frac((layerA + 1) * 0.6180339887);
float hueB = frac((layerB + 1) * 0.6180339887);
float hueC = frac((layerC + 1) * 0.6180339887);

float3 colA = saturate(float3(frac(hueA * 1.0), frac(hueA * 7.123), frac(hueA * 13.456)) + 0.25);
float3 colB = saturate(float3(frac(hueB * 1.0), frac(hueB * 7.123), frac(hueB * 13.456)) + 0.25);
float3 colC = saturate(float3(frac(hueC * 1.0), frac(hueC * 7.123), frac(hueC * 13.456)) + 0.25);

float wsum = wA + wB + wC + 1e-6;
return (colA * wA + colB * wB + colC * wC) / wsum;
```

### 4.4 输出与材质赋值

1. Custom 节点输出 → Material 的 **Emissive Color**
2. **Apply + Save**（必须 Save，否则 .uasset 不更新）
3. 编辑器场景里选中 `APlanetTopologyDebugMesh` 实例 → Details
4. ⚠ 把材质挂到 **`PlanetTopology > Material`** 槽位（详见 [SDF §11.2.2](SphericalSDFTerrainDesign.md#1122-材质必须挂在-actor-的-planettopology--material-槽位极易踩坑)）
5. OnConstruction 自动 Rebuild → cpp 端注入 NoiseAmplitude/NoiseScale → 立即看到 R6 效果

---

## 5. 验收清单

| 项 | 期望 |
| --- | --- |
| **A. NoiseAmplitude = 0** | 视觉与 R5 完全一致——hex/pent 边是测地线大圆弧，无任何蜿蜒 |
| **B. NoiseAmplitude = 0.02, NoiseScale = 10** | 边变为微妙起伏（约 ±1.15°），形态有机但仍可识别原 hex/pent 形状 |
| **C. NoiseAmplitude = 0.05, NoiseScale = 10** | 明显蜿蜒（约 ±2.86°），边形状自然像海岸线 |
| **D. NoiseAmplitude = 0.05, NoiseScale = 50** | 高频细密锯齿（每周期 1.15°） |
| **E. NoiseAmplitude = 0.10, NoiseScale = 5** | 强变形大尺度起伏（cell 形态明显失真但仍可识别） |
| **F. EdgeWidth = 0 + NoiseAmplitude = 0.05** | **硬边但形状蜿蜒**——Civ 风格 + 有机形态 |
| **G. EdgeWidth = 0.05 + NoiseAmplitude = 0.05** | **软边且形状蜿蜒**——E&D 风格的有机过渡 |
| **H. 跨 mesh 边连续性** | 把相机贴近 mesh 边中点——cell 边形状跨边平滑连续，**没有可见接缝**（关键验收点） |
| **I. 跨 sub 一致性** | 同样的 NoiseAmplitude=0.05 / NoiseScale=10，sub=3 与 sub=4 视觉效果"特征尺度相同"（噪声不被三角形大小耦合） |
| **J. NumLayersHint 联动** | 调小 NumLayersHint → 同 layer hex 完全融合 + 不同 layer 之间出现蜿蜒过渡 |
| **K. Output Log** | `Rebuilt (R6: per-cell noise on δ + soft edge) ... NoiseAmplitude=0.0500 rad (2.86°) NoiseScale=10.0 /rad ...` |
| **L. 反射诊断** | 11 个 Inputs 全部 `Connected=YES`；`✓ R6 compliance` |

### 5.1 R5 → R6 视觉前后对比

| 观察目标 | R5（NoiseAmplitude=0） | R6（NoiseAmplitude=0.05, NoiseScale=10） |
| --- | --- | --- |
| hex 边几何 | 测地线大圆弧 | 蜿蜒曲线（仍以大圆弧为均值） |
| pent 边形状 | 5 段大圆弧 | 5 段蜿蜒曲线 |
| 三 cell 角点 | 三色锐角 / 软三色 | 三色软角 + 形态扭曲 |
| mesh 边可见性 | 不可见（测地线连续） | 不可见（per-cell 噪声 + V_i 种子保证连续） |

---

## 6. 排错表

| 症状 | 根因 | 修复 |
| --- | --- | --- |
| 编译报错 `function definition is not allowed here` / `use of undeclared identifier 'Noise3D'` | 在 Custom 节点 Code 内写了 `float Hash13(...) { ... }` / `float Noise3D(...) { ... }` 函数定义。UE 把 Code 包裹在自动生成的 `CustomExpressionN(...)` 函数体内，**不允许嵌套函数** | 用 §2.2 的 scoped block 内联写法（每次采样一个 `{ ... }` 块），见 §4.3.2 完整 Code |
| mesh 边可见接缝（边在某些 mesh 边中点处突然换形态） | 噪声实现错了——用了全局 `noise3D(dir).xyz` 三分量 | 改成 per-cell：采样坐标 `dir * NoiseScale + V_i * 7.919`（详见 §1.3） |
| 调 NoiseScale 视觉无反应 | MID 没注入；或 Custom 节点没声明该 Input | 检查 Output Log NoiseScale 字段；反射诊断 NoiseScale Input 是否 Connected=YES |
| NoiseAmplitude > 0 时整球颜色乱跳 | 振幅超过 TriRadius 上限 | 限制 `NoiseAmplitude ≤ 0.1`（cpp 端 ClampMax 已限），或 sub=4+ 时改为 0.05 |
| 噪声看起来"条带状"或"网格状" | 用了 hash sin 噪声（选项 A）而非 value noise（选项 C） | 用 §2.2 的 inline scoped block 实现 |
| 跨星球大小视觉效果不同 | 噪声采样输入用了 WorldPos 而非 dir | 必须用 `dir * NoiseScale`（详见 §1.4） |
| EdgeWidth = 0 + NoiseAmplitude > 0 时**没看到蜿蜒** | smoothstep 在 EdgeWidth=0 时给 step（硬边），但应能看到 step 阈值的"蜿蜒"形状 | 这是预期：硬边会沿蜿蜒曲线突变；如果一片纯色，可能 NoiseAmplitude 太小（ < 0.005），调大 |
| 颜色出现不连续色块 | Inputs 顺序错位 | 严格按 §4.3.1 顺序添加 Inputs |
| GPU 性能下降明显 | 噪声 8 角点 hash + 三线性插值，每像素 3 次调用 = 24 次 hash | sub=3 642 cells / 1080p 视口下应在 ~0.3 ms 增量内；如有压力可改 4 角点 2D 噪声（球面切平面近似） |

---

## 7. 后续路径预告（R7+）

| 阶段 | 接续动作 |
| --- | --- |
| **R7** | 把 `hash(layer)` 哈希色换成 `Triplanar(TerrainAlbedoArray, layer, WorldPos, Normal)` 真实地形纹理；R6 的噪声扰动直接复用——它工作在 δ 上，与颜色采样方式无关 |
| **R8** | 接入 WorldGen 的 `FCellGeoData → LayerIndex` |
| **R9** | 进阶："`LayerPair → NoiseParams`" LUT——不同 layer 对（草-沙、城-野）用不同 NoiseAmplitude/NoiseScale，让海岸蜿蜒、城市直角；可在 §6.4 末尾的进阶版本中实现 |
| **R11** | R6 的 PS 核心 HLSL 在 PTG mesh 上零修改复用，仅 c0/c1/c2 来源换成 GPU `FindNearestCell` |

---

## 附录 A：HashOffset 常数选择

R6_HASH_OFFSET = `7.919` 是任意一个**不规则的素数倍**——用它把不同 cell 的噪声采样空间错开。任何如下数值都行：

| 选项 | 值 | 说明 |
| --- | --- | --- |
| 7.919 | 素数 7919 / 1000 | R6 默认 |
| 6.283 | 2π | 物理意义清晰但有规律性 |
| 13.37 | 任意非整数 | 简单可记 |
| ≥ 100 | 大数 | 让相邻 cell 的采样空间差距远大于 NoiseScale 周期 |

**避免**：整数（会让 V_i 取整后碰撞） / 简单分数如 0.5 / 1.0 / 2.0。

## 附录 B：噪声形态调参参考

| 想要的视觉风格 | NoiseAmplitude | NoiseScale | EdgeWidth |
| --- | --- | --- | --- |
| Civ VI 棱柱 | 0 | 任意 | 0 |
| Civ VI + 微妙抗锯齿 | 0 | 任意 | 0.005 |
| Old World 风 | 0 | 任意 | 0.02 |
| 海岸线（大尺度） | 0.05 | 5 | 0.02 |
| E&D 沼泽 | 0.04 | 15 | 0.04 |
| 沙漠纹理（细密） | 0.03 | 50 | 0.01 |
| 卡通游戏（强变形） | 0.10 | 8 | 0.05 |

## 附录 C：为什么不在 cpp 端做噪声

理论上可以在 cpp 端预计算每像素的 $\tilde\delta_i$ 并写到 LUT/纹理——但这要求每帧重算（dir 是 fragment 级别的，cpp 端没有 fragment）。所以噪声必须在 PS 里实时算。

唯一例外：如果 R8 接入 WorldGen 后，cell 的 layer 边界形态是设计师手工指定的（如手画河流），那 cell 本身的 $\delta_i$ 可以预烘焙为 LUT。但 R6 阶段噪声纯由参数驱动 → PS 实时算最合适。
