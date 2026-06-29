# R6：边界 3D 噪声扰动（让 cell 边变蜿蜒）

> 本文档是 [SphericalSDFTerrainDesign.md §6.4.1](SphericalSDFTerrainDesign.md#641-r6-落地方案在-r5-球面距离-δ-上叠加噪声扰动与-r4r5-几何兼容) 的独立落地文档，与 [R2_TopologyDebugMaterial.md](R2_TopologyDebugMaterial.md) / [R3_CellAttrLUTMaterial.md](R3_CellAttrLUTMaterial.md) / [R4_VoronoiBoundary.md](R4_VoronoiBoundary.md) / [R5_SharpenSoftEdge.md](R5_SharpenSoftEdge.md) 风格一致。
>
> 阅读本文档前必须先理解 [R5_SharpenSoftEdge.md](R5_SharpenSoftEdge.md) §1 的“球面有符号弧度距离 δ”——R6 不修改 R5 的 δ 公式，只是在调用 R5 之前把 fragment 方向 dir 做一次球面切向偏移。
>
> ⚠ **路线调整说明（2026-06-29）**：本文档中"R11 PTG 路线下零修改"等表述**已过期**——原 R11 PTG 路线已废弃，新生产路线是 [SDF 主稿 §16](SphericalSDFTerrainDesign.md#16-自研球面网格生产路线) 的"R8.5 自研球面网格"。R6 dir 切向偏移公式 + 噪声扰动**仍然零改动**搬运到 R8.5（dir = normalize(WorldPos - PlanetCenter) 起点不变）。

---

## 0. R6 一句话目标

在 R5 的球面 Voronoi 软边之前预插一步“**dir 球面切向偏移**”：把 fragment 的原始方向 $\hat{d}$ 整体形变为 $\hat{d}'$，然后让 R5 用 $\hat{d}'$ 走完原有流程。所有 cell 共享同一个形变后的方向——边仍由 $\theta_A(\hat{d}') = \theta_B(\hat{d}')$ 这个单一等式定义，**处处闭合，零接缝零重叠**。

```
R5（NoiseAmplitude = 0）          R6（NoiseAmplitude = 0.05, NoiseScale = 10）
┌─────────────────┐                ┌────~~~~~~~~~~~~┐
│  Cell A         │                │  Cell A      ~~│
│   ─────────────  │                │     ~~~~~~~~~~~ │
│  Cell B         │                │  Cell B      ~~│
└─────────────────┘                └────~~~~~~~~~~~~┘
   测地线大圆弧（R5）             蜿蜒曲线（R6）
```

| (EdgeWidth, NoiseAmplitude) | 视觉效果 |
| --- | --- |
| (0, 0) | R4 测地线硬直边（参考帧） |
| (0, 0.05) | **硬边但蜿蜒**——Civ 风格但带有机形态 |
| (0.05, 0) | R5 软直边（无噪声渐变） |
| (0.05, 0.05) | **软边且蜿蜒**——E&D 风格的有机过渡（最丰富） |

**EdgeWidth 与 NoiseAmplitude 在视觉上完全正交**：前者控制“边带的软硬”，后者控制“边形的直蔓”，可独立调节。

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

### 1.2 R6 核心：对 dir 做球面切向偏移（方案 B）

R6 **不修改 R5 的 δ 公式**，而是在 R5 流程开头把 fragment 的原始方向 $\hat{d}$ 做一次形变：

$$
\boxed{\ \hat{d}' = \mathrm{normalize}\!\left(\hat{d} + \mathbf{n}(\hat{d})\cdot\text{NoiseAmplitude}\right),\quad \mathbf{n}(\hat{d}) \in [-1,1]^3\ }
$$

然后用 $\hat{d}'$ 替换 $\hat{d}$ 走完 R5 的所有计算：

$$
\theta_i = \arccos(\hat{d}'\cdot V_i),\quad \delta_i = \theta_i - \min_{j\neq i}\theta_j,\quad w_i = \mathrm{smoothstep}(\tfrac{\text{EW}}{2},\ -\tfrac{\text{EW}}{2},\ \delta_i)
$$

其中 $\mathbf{n}(\hat{d}) = (n_x, n_y, n_z)$ 是 3 个独立的 3D value noise 分量，输入坐标都用 $\hat{d}\cdot\text{NoiseScale}$，靠不同 hash offset 错开。

**几何含义**：`normalize(dir + noise · A)` 把"dir + 小向量"投回单位球面——3D 偏移中沿 dir 径向的分量被压缩，留下的实际位移**几乎全部沿球面切向**（小角近似下）。每个 fragment 看到的"自己在哪个方向"被平滑地 wobble 一下，所有 cell 共用这个 wobble 后的方向 → cell 边几何整体沿 dir 空间被推拉，**形态变蜿蜒但所有边仍然处处闭合**。

### 1.3 为什么"零接缝零重叠"

设 cell $A$、$B$ 之间的 Voronoi 边 $e_{AB}$。在 R6 下边 $e_{AB}$ 的位置由：

$$
\theta_A(\hat{d}') = \theta_B(\hat{d}') \quad\Longleftrightarrow\quad \hat{d}'\cdot V_A = \hat{d}'\cdot V_B
$$

定义。这是**一个**等式，**对所有看向这条边的 fragment 都成立同一个等式**。无论 $\hat{d}'$ 怎么形变（只要它仍是单位向量），新边都是 $\{\hat{d}'\,:\,\hat{d}'\cdot(V_A-V_B)=0\}$ 与单位球面的交大圆——**还是测地线**，只是在 dir 空间里被推拉了。

**任意两个 fragment 在 R6 中算出的"我离边 X 远"在边的两侧符号相反、在边上同时为 0**——所以 cell A 和 cell B 的 $w$ 在边上恰好交叉于 0.5，不会出现"两边都 < 0.5（缝）"或"两边都 > 0.5（重叠）"。

### 1.4 为什么"零跨 mesh 边接缝"

mesh 边两侧的两个相邻三角形共享同一条物理曲线，对应 fragment 的真实球面方向 $\hat{d}$ 跨边连续（这是 R4 几何就保证的）。

- $\mathbf{n}(\hat{d})$ 是 $\hat{d}$ 的纯函数 → 跨 mesh 边连续
- $\hat{d}' = \mathrm{normalize}(\hat{d} + \mathbf{n}\cdot A)$ 是 $\hat{d}$ 和 $\mathbf{n}$ 的复合连续函数 → 跨 mesh 边连续
- $\theta_i, \delta_i, w_i$ 都是 $\hat{d}'$ 的连续函数 → 跨 mesh 边连续

所以 cell 边在 mesh 边处仍然平滑。

### 1.5 关键约束：噪声采样输入用 $\hat{d}$ 而非 WorldPos

球面渲染下，$\hat{d}$（单位方向）与 WorldPos = $\hat{d}\cdot\text{Radius} + \text{Center}$ 是一一对应的——但用 $\hat{d}$ 采样：

- ✅ 跨星球半径变化时，噪声视觉频率保持一致（NoiseScale 直接决定每弧度多少周期）
- ✅ 跨 sub 等级时，噪声形态跟物理弧度对齐，不被三角形大小耦合
- ✅ R11 PTG 路线下零修改：dir = normalize(WorldPos - PlanetCenter)，PTG 高细分 mesh 同样如此

如果用 WorldPos 采样：Radius 变化 10 倍 → 噪声频率变化 10 倍；不同行星视觉风格不一致。

### 1.6 数值边界与上限

NoiseAmplitude 的合理范围是 $[0, \text{TriRadius})$，其中 TriRadius 是三角形外心到顶点的角距离：

| sub | NumCells | 三角形外接圆半径（弧度） | NoiseAmplitude 上限 |
| --- | --- | --- | --- |
| 3 | 642 | ≈ 0.184 | 0.10（推荐 ≤ 0.1） |
| 4 | 2562 | ≈ 0.092 | 0.05 |
| 5 | 10242 | ≈ 0.046 | 0.025 |

为什么有这个上限：cell A、B、C 的几何只在它们的物理三角形 $\{V_A, V_B, V_C\}$ 上被 R5 公式正确表达；`dir` 的扰动量超过 TriRadius 时，被 normalize 后的 $\hat{d}'$ 可能跑到该三角形覆盖的几何域之外——此时 R5 算出的 $\delta_i$ 不对应真实最近 cell，颜色乱跳（"互锁"伪影，见 §6 排错表）。

R6 默认 `NoiseAmplitude ∈ [0, 0.1]`，cpp 端 ClampMax = 0.1。

### 1.7 已废弃路径：per-cell 噪声扰动 δ（不要使用）

> ⚠ **历史警示**：R6 早期版本曾尝试 $\tilde\delta_i = \delta_i + n_i(\hat{d}, V_i) \cdot A$——让每个 cell 独立采样自己的噪声扰动量再加到 $\delta_i$ 上。该方案有两个根本问题：
>
> 1. **A 看 B 的 θ 没经 B 的扰动**：R5 公式 $\delta_A = \theta_A - \min(\theta_B, \theta_C)$ 中的 $\theta_B$ 没换成 $\tilde\theta_B$。A 算出"我离 AB 边 X 远"、B 算出"我离 AB 边 Y 远"，由于 $\tilde\theta_A \neq \theta_A$ 且 $\tilde\theta_B \neq \theta_B$，X 与 Y 一般不互为相反数 → A、B 各自心目中的"边"位置不重合 → **要么留缝（两边 w < 0.5）要么重叠（两边 w > 0.5）**。
> 2. **跨 mesh 边时 min(θ_B, θ_C) 的第二项不连续**：三角形 1 = (A,B,C₁) 的 $\delta_A^{(1)}$ 用 $\min(\theta_B, \theta_{C_1})$；三角形 2 = (A,B,C₂) 的 $\delta_A^{(2)}$ 用 $\min(\theta_B, \theta_{C_2})$。即使 $n_A$ 跨 mesh 边连续，$\delta_A$ 经过这个 piecewise-min 后，扰动量在 mesh 边附近出现接缝。
>
> 这两个病理叠加，在实际渲染中表现为**密集黑细缝 + 局部白亮重影**沿大量 cell 边出现。本节方案 B（对 dir 整体形变）从源头规避：所有 cell 共用同一个 $\hat{d}'$，没有"A 看 B 用旧 θ 而 B 看 A 用新 θ"的不对称，也没有 piecewise-min 的不连续叠加。
>
> **结论**：永远不要在 $\delta_i$ 上叠加 per-cell 噪声。要让边蜿蜒，只能形变 dir。

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

### 2.3 方案 B 的调用规则：3 个独立噪声分量 + dir 偏移 + normalize

R6 需要 3 个独立的 noise 分量，组成偏移向量 $\mathbf{n}(\hat{d}) = (n_x, n_y, n_z) \in [-1,1]^3$，再做 `dirP = normalize(dir + n * NoiseAmplitude)`。

为了让 3 个分量互不相关，**采样输入坐标都用 $\hat{d}\cdot\text{NoiseScale}$，每个分量加上不同的 hash offset 错开采样空间**：

```hlsl
// 三次采样、3 个独立分量（共 3 个 scoped block）
float nx;
{ float3 p = dir * NoiseScale + float3( 0.0,  0.0,  0.0); /* §2.2 模板，输出到 n */ nx = n; }

float ny;
{ float3 p = dir * NoiseScale + float3(17.13,  0.0,  0.0); /* 同上 */                ny = n; }

float nz;
{ float3 p = dir * NoiseScale + float3( 0.0, 31.41,  0.0); /* 同上 */                nz = n; }

// dir 偏移 + 球面归一化（关键：normalize 把切向位移留下、压缩径向位移）
float3 dirP = normalize(dir + float3(nx, ny, nz) * NoiseAmplitude);
```

**为什么 3 个噪声采样输入用同一个 `dir * NoiseScale` 加 offset 而不是 3 个独立 hash 函数**：value noise 的 hash 已经在内部做了空间打散，平移 offset 17.13 / 31.41 弧度足以让 nx/ny/nz 在视觉上完全不相关；这样比写 3 个不同 hash 公式简单可读，性能等价。

**HashOffset 选择**：17.13 / 31.41 是任意非整数大数（取 π 系列易记），让三个采样空间错开。任意非整数都可以；不要用整数（会让 floor 后碰撞）或简单倍数（如 1, 2, 3）。

**与 R5 的衔接**：把 dirP 替换 R5 公式中所有 dir 的出现：

```hlsl
float thetaA = acos(saturate(dot(dirP, V_A) * 0.5 + 0.5) * 2.0 - 1.0);  // 用 dirP 而非 dir
//      ^^^^                      ^^^^
// δ_i / w_i / 颜色加权全部沿用 R5，dir 一处不动
```

> 上面是逻辑示意；§4.3.2 给出可直接复制粘贴的完整 Code 块。

每个 $n \in [-1, 1]$，3D 偏移向量被乘以 `NoiseAmplitude` 后加到 dir 上，再被 normalize 投回单位球面。


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
// R6：球面 Voronoi 软边 + dir 全局切向偏移（方案 B：3D 偏移 + normalize）
//
// 数学：
//   n = (nx, ny, nz) ∈ [-1,1]³，三个独立 3D value noise 分量
//   dirP = normalize(dir + n * NoiseAmplitude)             ← 关键
//   θ_i  = acos(dot(dirP, V_i))                            ← 用 dirP 而非 dir
//   δ_i  = θ_i - min(θ_{j≠i})
//   w_i  = smoothstep(EdgeWidth/2, -EdgeWidth/2, δ_i)
//   color = Σ w_i · hash(layer_i+1) / Σ w_i
//
// 退化关系：
//   NoiseAmplitude = 0  → dirP = dir → R5 软直边
//   EdgeWidth = 0       → R4 + 噪声硬蜿蜒边（dirP 仍生效）
//   两者都 = 0          → R4 硬直边
//
// 零接缝零重叠保证：所有 cell 共用同一个 dirP，cell 边由单一等式
//   θ_A(dirP) = θ_B(dirP) 定义，处处闭合（详见 §1.3 / §1.4）
// =====================================================================

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

// ---- R6 Step 1：采样 3 个独立 3D value noise 分量 ----
//   每个分量用同一坐标 dir * NoiseScale，加不同 hash offset 错开采样空间
//   实现：3D value noise（8 角点 hash + 三线性插值），完全 inline 到 scoped block
//   UE Custom 节点不允许嵌套函数，所以采样体内全部 inline

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

// ---- R6 Step 2：dir 切向偏移 + 球面归一化（关键的"全局连续形变"） ----
//   dir + 小向量 后 normalize 把结果投回单位球面：3D 偏移中沿 dir 径向的分量
//   被压缩，留下的位移几乎全部沿球面切向（小角近似下）。
//   所有 cell 共用这个 dirP，从根本上避免 per-cell 路径的接缝/重叠病理。
float3 dirP = normalize(dir + float3(nx, ny, nz) * NoiseAmplitude);

// ---- 沿用 R5：球面角距离 θ_i（用 dirP 替代 dir） ----
float thetaA = acos(saturate(dot(dirP, V_A) * 0.5 + 0.5) * 2.0 - 1.0);
float thetaB = acos(saturate(dot(dirP, V_B) * 0.5 + 0.5) * 2.0 - 1.0);
float thetaC = acos(saturate(dot(dirP, V_C) * 0.5 + 0.5) * 2.0 - 1.0);

// ---- 沿用 R5：到 Voronoi 边的有符号弧度距离 δ_i ----
float deltaA = thetaA - min(thetaB, thetaC);
float deltaB = thetaB - min(thetaA, thetaC);
float deltaC = thetaC - min(thetaA, thetaB);

// ---- 沿用 R5：smoothstep 软边权重 ----
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
| **H. 零接缝零重叠（关键验收点）** | 任意 NoiseAmplitude 下，球面上任意 cell 边处都看不到黑色细缝也看不到亮色重影（为 R6 走方案 B 全局形变的里程碑；per-cell 路径在该项上会失败） |
| **I. 跨 sub 一致性** | 同样的 NoiseAmplitude=0.05 / NoiseScale=10，sub=3 与 sub=4 视觉效果"特征尺度相同"（噪声不被三角形大小耦合） |
| **J. NumLayersHint 联动** | 调小 NumLayersHint → 同 layer hex 完全融合 + 不同 layer 之间出现蜿蜒过渡 |
| **K. Output Log** | `Rebuilt (R6: per-cell noise on δ + soft edge) ... NoiseAmplitude=0.0500 rad (2.86°) NoiseScale=10.0 /rad ...`（cpp 端字符串仍为 R5 历史名字，R6.1 可选改为 `dir tangent offset`，但不强制） |
| **L. 反射诊断** | 11 个 Inputs 全部 `Connected=YES`；`✓ R6 compliance` |

### 5.1 R5 → R6 视觉前后对比

| 观察目标 | R5（NoiseAmplitude=0） | R6（NoiseAmplitude=0.05, NoiseScale=10） |
| --- | --- | --- |
| hex 边几何 | 测地线大圆弧 | 蜿蜒曲线（仍以大圆弧为均值） |
| pent 边形状 | 5 段大圆弧 | 5 段蜿蜒曲线 |
| 三 cell 角点 | 三色锐角 / 软三色 | 三色软角 + 形态扭曲 |
| cell 边闭合性 | 完全闭合（测地线） | 完全闭合（全局 dirP 保证，零接缝零重叠） |
| mesh 边可见性 | 不可见 | 不可见（dirP 是 dir 的连续函数，跨 mesh 边连续） |

---

## 6. 排错表

| 症状 | 根因 | 修复 |
| --- | --- | --- |
| 编译报错 `function definition is not allowed here` / `use of undeclared identifier 'Noise3D'` | 在 Custom 节点 Code 内写了 `float Hash13(...) { ... }` / `float Noise3D(...) { ... }` 函数定义。UE 把 Code 包裹在自动生成的 `CustomExpressionN(...)` 函数体内，**不允许嵌套函数** | 用 §2.2 的 scoped block 内联写法（每次采样一个 `{ ... }` 块），见 §4.3.2 完整 Code |
| **cell 边处出现黑色细缝或亮色重影** | 走了已废弃的 per-cell 路径（把噪声叠加到 $\delta_i$ 上） | 改为方案 B：对 dir 做全局切向偏移 `dirP = normalize(dir + n*A)`，用 dirP 走完 R5 流程（详见 §1.2 / §1.7） |
| 调 NoiseScale 视觉无反应 | MID 没注入；或 Custom 节点没声明该 Input | 检查 Output Log NoiseScale 字段；反射诊断 NoiseScale Input 是否 Connected=YES |
| NoiseAmplitude > 0 时整球颜色乱跳 | 振幅超过 TriRadius 上限，dirP 被推到三角形覆盖范围外 | 限制 `NoiseAmplitude ≤ 0.1`（cpp 端 ClampMax 已限），或 sub=4+ 时改为 0.05 |
| 噪声看起来"条带状"或"网格状" | 用了 hash sin 噪声（选项 A）而非 value noise（选项 C） | 用 §2.2 的 inline scoped block 实现 |
| 跨星球大小视觉效果不同 | 噪声采样输入用了 WorldPos 而非 dir | 必须用 `dir * NoiseScale`（详见 §1.5） |
| EdgeWidth = 0 + NoiseAmplitude > 0 时**没看到蜿蜒** | smoothstep 在 EdgeWidth=0 时给 step（硬边），但应能看到 step 阈值的"蜿蜒"形状 | 这是预期：硬边会沿蜿蜒曲线突变；如果一片纯色，可能 NoiseAmplitude 太小（ < 0.005），调大 |
| 颜色出现不连续色块 | Inputs 顺序错位 | 严格按 §4.3.1 顺序添加 Inputs |
| GPU 性能下降明显 | 噪声 8 角点 hash + 三线性插值，每像素 3 次采样 = 24 次 hash | sub=3 642 cells / 1080p 视口下应在 ~0.3 ms 增量内；如有压力可改 4 角点 2D 噪声（球面切平面近似） |

---

## 7. 后续路径预告（R7+）

| 阶段 | 接续动作 |
| --- | --- |
| **R7** | 把 `hash(layer)` 哈希色换成 `Triplanar(TerrainAlbedoArray, layer, WorldPos, Normal)` 真实地形纹理；R6 的噪声扰动直接复用——它工作在 δ 上，与颜色采样方式无关 |
| **R8** | 接入 WorldGen 的 `FCellGeoData → LayerIndex` |
| **R9** | 进阶："`LayerPair → NoiseParams`" LUT——不同 layer 对（草-沙、城-野）用不同 NoiseAmplitude/NoiseScale，让海岸蜿蜒、城市直角；可在 §6.4 末尾的进阶版本中实现 |
| **R11** | R6 的 PS 核心 HLSL 在 PTG mesh 上零修改复用，仅 c0/c1/c2 来源换成 GPU `FindNearestCell` |

---

## 附录 A：噪声分量 hash offset 选择

方案 B 需要 3 个独立的噪声分量 nx / ny / nz。实现上让它们使用同一个采样坐标 `dir * NoiseScale`，但加上不同的 hash offset 向量错开采样空间：

```hlsl
float3 _p_x = dir * NoiseScale + float3( 0.0,  0.0, 0.0);   // nx 采样位置
float3 _p_y = dir * NoiseScale + float3(17.13, 0.0, 0.0);   // ny
float3 _p_z = dir * NoiseScale + float3( 0.0, 31.41, 0.0);  // nz
```

| 常量 | 值 | 来源 | 为什么 |
| --- | --- | --- | --- |
| `17.13` | ny offset | 任意中等非整数 | 让 ny 采样空间与 nx 错开足够远（>> 1），value noise 的连续区间不会泄露 |
| `31.41` | nz offset | 近似 10π | 同上，与 nx / ny 错开不同距离 |

**选择原则**：

- 每个 offset 是任意**非整数**（避免 hash 函数在 floor 后碰撞）
- 三个 offset 之间距离远远大于 value noise 的周期（1 单位）
- 推荐都在 [10, 100] 区间、互不成简单倍数。如 17.13 / 31.41 / 67.89 / 91.27

**避免**：整数（会让 floor 后碰撞） / 简单分数如 0.5 / 1.0 / 2.0 / 三个值接近（会出现三分量高相关）。

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
