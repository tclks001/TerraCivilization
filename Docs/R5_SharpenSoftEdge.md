# R5：球面 Voronoi 软边控制（基于绝对弧度的 Sharpen）

> 本文档是 [SphericalSDFTerrainDesign.md §6.3.1](SphericalSDFTerrainDesign.md#631-r5-落地方案基于球面角距离的绝对弧度软边路径-b) 的独立落地文档，与 [R2_TopologyDebugMaterial.md](R2_TopologyDebugMaterial.md) / [R3_CellAttrLUTMaterial.md](R3_CellAttrLUTMaterial.md) / [R4_VoronoiBoundary.md](R4_VoronoiBoundary.md) 风格一致。
>
> 阅读本文档前必须先理解 [R4_VoronoiBoundary.md](R4_VoronoiBoundary.md) §2 的"球面 Voronoi 判别"——R5 直接构筑在 R4 的 dot 距离架构之上，新增的只是"过渡带宽度"。
>
> ⚠ **路线调整说明（2026-06-29）**：本文档中"R11 PTG 路线零修改复用"等表述**已过期**——原 R11 PTG 路线已废弃，新生产路线是 [SDF 主稿 §16](SphericalSDFTerrainDesign.md#16-自研球面网格生产路线) 折叠到 [TessellatedMeshDesign.md](TessellatedMeshDesign.md) 的"T 阶段自研球面网格"。R5 PS HLSL **仍然零改动**搬运到 T 阶段，区别仅在 c0/c1/c2 来源（cpp 预计算灌顶点而非 GPU FindNearestCell）；EdgeWidth 跨 sub 等级语义不变同样适用于自研球面网格。

---

## 0. R5 一句话目标

在 R4 的球面 Voronoi 硬边（cell A 与 cell B 的分界 = 它们中心方向的垂直平分大圆弧）基础上，让 hex/pent 边变成一条**有宽度的等距弧度带**，过渡带宽度由参数 `EdgeWidth` 控制（**单位：绝对弧度**），过渡带几何严格沿 R4 的测地线大圆弧两侧对称展开。

```
R4（EdgeWidth = 0）            R5（EdgeWidth = 0.05 弧度）
┌─────────┬─────────┐         ┌─────────░░░░░░░░░░─────────┐
│ Cell A  │ Cell B  │         │ Cell A   过渡带 0.05弧度    Cell B │
│ 纯色    │ 纯色    │         │ 纯色   ░░░░░░░░░░░          纯色   │
└─────────┴─────────┘         └─────────░░░░░░░░░░─────────┘
        硬边                         测地线大圆弧两侧 ±0.025 弧度的等距带
```

| EdgeWidth 值 | 视觉效果 | 几何含义 |
| --- | --- | --- |
| `0.0` | **退化为 R4 严格硬边** | smoothstep → Heaviside 阶跃 |
| `0.02` | 微妙抗锯齿（约 1.15°） | 边带总宽 0.02 弧度 |
| `0.05` | 清晰可见软边（约 2.86°） | 边带总宽 0.05 弧度 |
| `0.10` | 中等柔软渐变（约 5.73°） | 边带总宽 0.10 弧度 |
| `0.30` | 大幅柔软（约 17.2°） | 边带总宽 0.30 弧度 |

---

## 1. 几何与数学

### 1.1 R4 几何回顾

R4 把判别准则定为 **`argmax_i(dot(dir, V_i))`**——等价于 `argmin_i(θ_i)`，其中 $\theta_i = \arccos(\hat{d}\cdot V_i)$ 是 fragment 方向 $\hat{d}$ 到 cell $i$ 中心方向 $V_i$ 的**球面角距离**。

cell A 与 cell B 的 Voronoi 边 = $\theta_A = \theta_B$ 的轨迹 = 一条平面 $\{x \cdot (V_A - V_B) = 0\}$ 与单位球面的交圆 = **球面大圆弧（测地线）**。

### 1.2 R5 核心：到 Voronoi 边的有符号弧度距离

**定义** cell $i$ 的有符号弧度距离 $\delta_i$：

$$
\delta_i = \theta_i - \min_{j\neq i}\theta_j
$$

几何含义：

- $\delta_i > 0$：cell $i$ **不是**最近的，到它最近的 Voronoi 边的距离是 $|\delta_i|$ 弧度
- $\delta_i = 0$：像素**正好**在 cell $i$ 的 Voronoi 边上
- $\delta_i < 0$：cell $i$ **是**最近的（即 R4 的 chosen），到它**自己的** Voronoi 边（即与第二近 cell 的分界）的距离是 $|\delta_i|$ 弧度

$\delta_i$ 的取值范围：约 $[-\text{TriRadius}, +\text{TriRadius}]$，其中 TriRadius 是 cell 中心到三角形外心的角距离（sub=3 时约 0.18 弧度）。

**关键性质**（决定为什么 EdgeWidth 是绝对弧度）：

- $\delta_i$ 是**真实球面距离**，单位是弧度，与三角形大小、sub 等级、星球半径都**无关**
- 跨 mesh 边时 $\delta_i$ 连续——因为两侧三角形的 $\min_{j\neq i}\theta_j$ 在共享边上由相同的 $V_A, V_B$ 决定（与 $V_C$ 无关）

### 1.3 R5 软边权重

$$
w_i = \mathrm{smoothstep}\!\left(\frac{\text{EdgeWidth}}{2},\ -\frac{\text{EdgeWidth}}{2},\ \delta_i\right)
$$

注意 smoothstep 的边界传参顺序——HLSL `smoothstep(edge0, edge1, x)` 在 `edge0 > edge1` 时给出**降序** smoothstep（在 [edge1, edge0] 内从 1 降到 0）。这里我们想要的恰好是降序：$\delta_i$ 越大 → cell $i$ 越远 → 权重越小。

**关键值表**：

| $\delta_i$ | $w_i$ | 几何位置 |
| --- | --- | --- |
| $\geq +\text{EdgeWidth}/2$ | 0 | 完全在 cell $i$ 的 Voronoi 区域之外 |
| $0$ | 0.5 | 正好在 cell $i$ 的 Voronoi 边上 |
| $\leq -\text{EdgeWidth}/2$ | 1 | 完全在 cell $i$ 的 Voronoi 区域内核 |
| 其它 | $\in (0,1)$ | 过渡带内，平滑 smoothstep 曲线 |

**EdgeWidth = 0** 时 smoothstep 退化为 Heaviside 阶跃（HLSL 实际行为：当 `edge0 == edge1` 时返回 `step(edge0, x)`），$w_i$ 严格为 0/1，**完全等价 R4 硬边**。

### 1.4 颜色三层独立加权

$$
\text{color} = \frac{\sum_{i} w_i \cdot \text{hash}(\text{layer}_i + 1)}{\sum_i w_i + \epsilon}
$$

**三层加权 vs argmax 直接 hash 的差别**：

- **三层加权**（R5 选用）：每个 cell 独立查 layer、独立 hash → 颜色 → 按 $w_i$ 加权混合。在过渡带内会看到**两个 hash 色的连续插值**（视觉上是软边）。
- **argmax 直接 hash**（R4 选用）：先选 chosen cell、再 hash。EdgeWidth = 0 时与 R5 严格等价；EdgeWidth > 0 无法表达。

**三层加权在 EdgeWidth = 0 时的退化性**：当 $\delta_{\text{chosen}} \to -\infty$、其它 $\delta_i \to +\infty$ 时，$w_{\text{chosen}} = 1$、其它 $w = 0$，color = $\text{hash}(\text{layer}_{\text{chosen}})$，与 R4 完全一致。

### 1.5 数值稳定性

$\sum_i w_i + \epsilon$ 中的 $\epsilon$（推荐 $10^{-6}$）防止三个 cell 都因为远离 Voronoi 边而 $w_i = 0$（理论上不会发生——任何方向至少落在一个 cell 的 Voronoi 区内 → 至少一个 $\delta_i \leq 0$ → $w_i \geq 0.5$）。但浮点误差 + EdgeWidth 极小（如 0.0001 弧度）时可能短暂三方同 0，$\epsilon$ 防爆。

---

## 2. 与 R4 / R11 的关系

### 2.1 R4 → R5 的最小增量

R4 PS 已有：
1. 解出 c0/c1/c2
2. 三次 `CellDirLUT.Load` 取 V_A/V_B/V_C
3. 算 dir = normalize(WorldPos - PlanetCenter)
4. 算 $d_i = \text{dot}(\text{dir}, V_i)$
5. argmax 选 chosen
6. CellAttrLUT.Load 取 layer，hash 输出色

R5 在第 4 步之后插入两步、改写第 5/6 步：

| 步骤 | R4 | R5 |
| --- | --- | --- |
| 4 | $d_i = \text{dot}(\text{dir}, V_i)$ | 同 R4 |
| 4.5 | — | $\theta_i = \arccos(d_i)$ |
| 4.6 | — | $\delta_i = \theta_i - \min_{j\neq i}\theta_j$ |
| 4.7 | — | $w_i = \text{smoothstep}(\frac{\text{EdgeWidth}}{2}, -\frac{\text{EdgeWidth}}{2}, \delta_i)$ |
| 5 | argmax 选 chosen | 三次 `CellAttrLUT.Load` 取 $\text{layer}_i$ |
| 6 | 一次 hash 输出 | 三次 hash + 加权混合 |

**新增 GPU 成本**：3 次 acos（~6 cycle）+ 6 次 min/max（~3 cycle）+ 3 次 smoothstep（~9 cycle）+ 3 次额外 CellAttrLUT.Load（~3 cycle）+ 3 次额外 hash（~9 cycle）≈ 30 cycle。相对 R3/R4 已有 ~60 cycle 而言增量小。

### 2.2 与 R11 PTG 路线的同构性

R5 的所有计算都基于 dir 与 V_i，不依赖任何顶点流——**R5 的 PS 核心 HLSL 在 R11 PTG 路线下零修改复用**。唯一变化是 c0/c1/c2 的来源（R5 用 UV 还原 / R11 用 GPU FindNearestCell），但 acos δ smoothstep 公式完全相同。

### 2.3 与 §6.3 原文路径 A 的差异

§6.3 原文用 `Sharpen(λ)`（在 λ 空间做 smoothstep）：

```hlsl
// §6.3 路径 A：基于 λ 的 Sharpen（已被 R5 路径 B 取代）
float Sharpen(float l) { return smoothstep(0.5 - EdgeWidth, 0.5 + EdgeWidth, l); }
```

**问题**：

1. 等位线在 mesh 边中点处是折线（与 R3 argmax(λ) 路径 = 折线硬边一致），与 R4 的测地线大圆弧硬边**不对齐** → 过渡带相位错位
2. EdgeWidth 是**相对量**（λ 单位），跨 sub 等级语义不同——sub=3 时三角形大、EdgeWidth=0.05 看起来很窄；sub=4 时三角形小、同样的 0.05 看起来宽得多
3. EdgeWidth=0 时退化为"R3 折线硬边"，而非 R4 测地线硬边

R5 路径 B 解决以上 3 个问题：

1. ✅ 过渡带几何 = 测地线大圆弧两侧的等距弧度带，与 R4 完全对齐
2. ✅ EdgeWidth 是**绝对弧度**，跨 sub 等级、跨 R11 PTG 完全等效
3. ✅ EdgeWidth=0 严格退化为 R4 硬边

---

## 3. cpp 端改动

### 3.1 [PlanetTopologyDebugMesh.h](../Source/TerraCivilization/Public/Render/PlanetTopologyDebugMesh.h)

新增 UPROPERTY：

```cpp
/**
 * R5：软边过渡带的总弧度宽度（绝对量，单位为弧度）。
 *
 * 0     ：退化为 R4 严格硬边（smoothstep → Heaviside）
 * 0.05  ：约 2.86°，清晰可见软边
 * 0.10  ：约 5.73°，中等柔软
 * 0.30  ：约 17.2°，大幅柔软（接近 sub=3 三角形外接圆半径）
 *
 * 上限：sub=3 时正二十面体三角形最长边 ≈ 1.107 弧度，建议 ≤ 0.3。
 * 跨 sub 语义不变——sub=4 时同样的 0.05 弧度看起来与 sub=3 视觉宽度相同（都是约 2.86°）。
 */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|R5",
          meta = (ClampMin = "0.0", ClampMax = "0.5"))
float EdgeWidth = 0.05f;
```

### 3.2 [PlanetTopologyDebugMesh.cpp](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp)

**`Rebuild()` 末尾 MID 注入（在 R4 已有的 CellDirLUT/PlanetCenter 之后追加）**：

```cpp
// R5 新增注入：软边过渡带宽度（弧度）
MID->SetScalarParameterValue(TEXT("EdgeWidth"), EdgeWidth);
```

**反射诊断升级**（把 R4 期望 8 输入改为 R5 期望 9 输入）：

```cpp
const TArray<FString> ExpectedR5Inputs = {
    TEXT("uv0"), TEXT("uv1"), TEXT("uv2"), TEXT("uv3"),
    TEXT("worldpos"), TEXT("planetcenter"),
    TEXT("cellattrlut"), TEXT("celldirlut"),
    TEXT("edgewidth")    // R5 新增
};
```

合规判定逻辑保持 R4 同款风格——所有期望输入都齐全 → ✓ R5 compliant；缺哪个 → ✗ Error 提示"这是 R3/R4 旧材质而不是 R5"。

**Output Log 字符串**：

```cpp
UE_LOG(LogPlanetTopologyDebugMesh, Log,
    TEXT("[PlanetTopologyDebugMesh] Rebuilt (R5: argmax(dot) + acos δ smoothstep soft edge). ")
    TEXT("SubdivisionLevel=%d  Cells=%d  Corners=%d  Verts=%d  Tris=%d  Radius=%.1f  Smooth=%s  ")
    TEXT("CellAttrLUT=%s  CellDirLUT=%s  PlanetCenter=(%.1f,%.1f,%.1f)  ")
    TEXT("EdgeWidth=%.4f rad (%.2f°)  NumLayersHint=%d"),
    /* ... */, EdgeWidth, EdgeWidth * 180.0 / PI, NumLayersHint);
```

### 3.3 不需要新建 LUT 纹理

R5 完全复用 R4 的 `CellAttrLUT` + `CellDirLUT`——没有任何新增 cpp 端纹理资源。

---

## 4. 材质搭建（M_TopologyDebug_R5）

### 4.1 复制 R4 材质

复制 `M_TopologyDebug_R4.uasset` 重命名为 `M_TopologyDebug_R5.uasset`，保留所有 R4 现有节点（Texture Object Parameters `CellAttrLUT`/`CellDirLUT`、Vector Parameter `PlanetCenter`、Scalar Parameters `NumLayersHint`/`NumCells`）。

### 4.2 新增 Material 参数

| 类型 | 名字 | 默认值 | 范围 |
| --- | --- | --- | --- |
| Scalar Parameter | `EdgeWidth` | 0.05 | [0, 0.5] |

### 4.3 改写 Custom 节点

#### 4.3.1 Inputs（在 R4 8 个基础上新增 1 个，第 9 位）

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

> ⚠ **Inputs 顺序必须与 Code 形参一一对应**——UE 按 Inputs 顺序生成 HLSL 形参，顺序错乱会导致编译失败或运行时错位。

#### 4.3.2 Code（直接复制粘贴）

```hlsl
// =====================================================================
// R5：球面 Voronoi 软边（acos δ smoothstep + 三层加权）
//
// 数学：
//   θ_i = acos(dot(dir, V_i))     球面角距离
//   δ_i = θ_i - min(θ_{j≠i})       到 Voronoi 边的有符号弧度距离
//   w_i = smoothstep(EW/2, -EW/2, δ_i)
//   color = Σ w_i · hash(layer_i+1) / Σ w_i
//
// EdgeWidth = 0 时 smoothstep 退化为 step → 严格还原 R4 硬边
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

// ---- R5 新增 Step 1：球面角距离 θ_i = acos(dot) ----
//   acos 输入 dot(dir, V_i) 已落在 [-1,1]（dir 与 V_i 都是单位向量），数值稳定
//   sub=3 时三角形内 θ_i ∈ [0, ~0.4]，精度充足
float thetaA = acos(saturate(dot(dir, V_A) * 0.5 + 0.5) * 2.0 - 1.0);
float thetaB = acos(saturate(dot(dir, V_B) * 0.5 + 0.5) * 2.0 - 1.0);
float thetaC = acos(saturate(dot(dir, V_C) * 0.5 + 0.5) * 2.0 - 1.0);
//   ↑ saturate(...) 形式是为了对抗罕见的 |dot| > 1 的浮点误差（acos 域外会 NaN）

// ---- R5 新增 Step 2：到 Voronoi 边的有符号弧度距离 δ_i ----
//   δ_i = θ_i - min(θ_{j≠i})
float deltaA = thetaA - min(thetaB, thetaC);
float deltaB = thetaB - min(thetaA, thetaC);
float deltaC = thetaC - min(thetaA, thetaB);

// ---- R5 新增 Step 3：smoothstep 软边权重 ----
//   smoothstep(edge0, edge1, x)：edge0 > edge1 时给出降序曲线
//   edge0 = +EW/2 → δ ≥ +EW/2 时 w = 0（cell 在远端外）
//   edge1 = -EW/2 → δ ≤ -EW/2 时 w = 1（cell 在近端内核）
float halfEW = EdgeWidth * 0.5;
float wA = smoothstep(halfEW, -halfEW, deltaA);
float wB = smoothstep(halfEW, -halfEW, deltaB);
float wC = smoothstep(halfEW, -halfEW, deltaC);

// ---- R5 新增 Step 4：三层独立 hash + 加权混合 ----
int layerA = (int)(CellAttrLUT.Load(int3(c0, 0, 0)).r * 255.0 + 0.5);
int layerB = (int)(CellAttrLUT.Load(int3(c1, 0, 0)).r * 255.0 + 0.5);
int layerC = (int)(CellAttrLUT.Load(int3(c2, 0, 0)).r * 255.0 + 0.5);

float hueA = frac((layerA + 1) * 0.6180339887);
float hueB = frac((layerB + 1) * 0.6180339887);
float hueC = frac((layerC + 1) * 0.6180339887);

float3 colA = saturate(float3(frac(hueA * 1.0), frac(hueA * 7.123), frac(hueA * 13.456)) + 0.25);
float3 colB = saturate(float3(frac(hueB * 1.0), frac(hueB * 7.123), frac(hueB * 13.456)) + 0.25);
float3 colC = saturate(float3(frac(hueC * 1.0), frac(hueC * 7.123), frac(hueC * 13.456)) + 0.25);

// ---- 加权混合（含 ε 防 0/0）----
float wsum = wA + wB + wC + 1e-6;
return (colA * wA + colB * wB + colC * wC) / wsum;
```

### 4.4 输出与材质赋值

1. Custom 节点输出 → Material 的 **Emissive Color**
2. **Apply + Save**（必须 Save，否则 .uasset 不更新；详见 [SDF §11.2.3](SphericalSDFTerrainDesign.md#1123-uasset-二进制-dump-工具对-ue-5x-材质不可用)）
3. 编辑器场景里选中 `APlanetTopologyDebugMesh` 实例 → Details
4. ⚠ 把材质挂到 **`PlanetTopology > Material`** 槽位，**不是** `渲染 > 材质 > 元素 0`（详见 [SDF §11.2.2](SphericalSDFTerrainDesign.md#1122-材质必须挂在-actor-的-planettopology--material-槽位极易踩坑)）
5. OnConstruction 自动 Rebuild → cpp 端注入 EdgeWidth → 立即看到 R5 效果

---

## 5. 验收清单

| 项 | 期望 |
| --- | --- |
| **A. EdgeWidth = 0** | 视觉与 R4 完全一致——hex/pent 边是测地线大圆弧硬边，无任何过渡渐变 |
| **B. EdgeWidth = 0.02** | 边变为微妙的抗锯齿带（几乎看不出来宽度，但能看到锯齿被消除） |
| **C. EdgeWidth = 0.05** | 清晰可见软边（约 2.86°）；边带等宽沿大圆弧两侧对称延伸 |
| **D. EdgeWidth = 0.20** | 大幅柔软渐变；hex 边几乎与中央内核混合 |
| **E. 几何对齐性** | 任何 EdgeWidth 下，过渡带的中心线（$\delta = 0$）严格与 R4 硬边重合，不存在 R3 路径 A 那种"折线偏移大圆弧"的相位错位 |
| **F. 跨 sub 一致性** | EdgeWidth=0.05 在 sub=3 与 sub=4 下视觉宽度相同（都是约 2.86°），三角形小不会让边带相对变宽 |
| **G. NumLayersHint 联动** | 调小 NumLayersHint 时大片相邻 hex 颜色合并 + 同 layer 内**完全无可见边**（因为 colA = colB = colC，加权后还是同色） |
| **H. Output Log** | `Rebuilt (R5: argmax(dot) + acos δ smoothstep soft edge) ... EdgeWidth=0.0500 rad (2.86°) ...` |
| **I. 反射诊断** | 9 个 Inputs 全部 `Connected=YES`；`✓ R5 compliance` |

### 5.1 R4 → R5 视觉前后对比

| 观察目标 | R4（EdgeWidth=0） | R5（EdgeWidth=0.05） |
| --- | --- | --- |
| hex 边几何 | 测地线大圆弧硬边 | 测地线大圆弧两侧 ±0.025 弧度的等距软带 |
| 同色相邻 hex | 完全融合（没有可见边） | 完全融合（同 layer → colA=colB） |
| 异色相邻 hex | 硬边切换 | 软边渐变（colA → colB 连续） |
| 三 cell 角点（Corner） | 三色锐角相会 | 三色 1/3 重心混合（约等于 §14.7 λ=(1/3,1/3,1/3) 时的视觉效果） |

---

## 6. 排错表

| 症状 | 根因 | 修复 |
| --- | --- | --- |
| 球面变成纯黑 / NaN 闪烁 | acos 输入超出 [-1,1]（浮点误差） | 检查代码中 `saturate(dot * 0.5 + 0.5) * 2 - 1` 的 saturate 是否正确写成；不能直接 `acos(dot(...))` |
| 改 EdgeWidth 视觉无反应 | MID 没注入 EdgeWidth；或材质 Custom 节点没声明该 Input | 检查 Output Log 是否打 R5 字符串 + EdgeWidth=N rad；反射诊断中 EdgeWidth Input 是否 Connected=YES |
| EdgeWidth=0 时与 R4 不一致 | smoothstep 在 edge0==edge1 时 GPU 实现差异 | UE/HLSL 标准规定此时返回 `step(edge0, x)`；如有问题用条件分支 `(EdgeWidth < 1e-6) ? (delta < 0 ? 1 : 0) : smoothstep(...)` 兜底 |
| 整球粉色 / 单色 | CellDirLUT 缺失 → V_A/V_B/V_C 全 0 → δ 全 0 → w 全 0.5 → 三色平均 | 检查 `CellDirLUT=OK`；材质挂 `PlanetTopology > Material` 槽位 |
| 过渡带宽度比预期窄/宽 | EdgeWidth 单位被误以为相对量 | 它是绝对弧度。0.05 ≈ 2.86°；不是百分比 |
| 过渡带几何与 R4 硬边对不齐 | 误用了 §6.3 路径 A 的 `Sharpen(λ)` | 必须用 §6.3.1 路径 B 的 acos δ；不能用 UV3 的 OneHot 重心权重做 smoothstep |
| 颜色出现不连续色块 | Inputs 顺序错位（如 EdgeWidth 形参实际拿到的是 Texture2D） | 严格按 §4.3.1 顺序添加 Inputs；UE 按声明顺序生成形参 |

---

## 7. 后续路径预告（R6+）

| 阶段 | 接续动作 |
| --- | --- |
| **R6** | 在 δ 上叠 per-cell 3D 噪声扰动：`δ̃_i = δ_i + Noise3D(dir * NoiseScale + V_i * 7.919) * NoiseAmplitude`，让 cell 边变蜿蜒不规则。EdgeWidth = 0 + 噪声 → 硬边但形状有机；EdgeWidth > 0 + 噪声 → 软边且形状有机。两个参数正交（详见 [R6_BoundaryNoise.md](R6_BoundaryNoise.md)） |
| **R7** | `hash(layer)` 哈希色换成 `Triplanar(TerrainAlbedoArray, layer, WorldPos, Normal)` 真实地表纹理；R5 的 $w_i$ 加权混合直接复用，每层换成 Triplanar 采样即可 |
| **R8** | 接入 WorldGen 的 `FCellGeoData → LayerIndex` |
| **R11** | R5 的 PS 核心 HLSL 在 PTG mesh 上零修改复用，仅 c0/c1/c2 换成 GPU `FindNearestCell` |

---

## 附录 A：为什么用 acos 而不是直接用 dot 差量

理论上可以用 dot 差量替代 acos δ：

```hlsl
// 备选：dot 差量（不推荐）
float deltaA = max(dB, dC) - dA;
float deltaB = max(dA, dC) - dB;
float deltaC = max(dA, dB) - dC;
```

dot 差量与 acos 差量在 sub=3 三角形内 4 阶等价（[§14.7.5](SphericalSDFTerrainDesign.md#1475-为什么三重积公式-球面重心坐标)）。但我们选 acos：

| 方面 | acos | dot 差量 |
| --- | --- | --- |
| EdgeWidth 单位 | 绝对弧度（物理量） | 抽象 dot 差量（无直观量纲） |
| 跨 sub 语义 | 不变（0.05 弧度永远 2.86°） | 变化（同样的 0.001 dot 差量在 sub=3 / sub=4 / R11 上视觉宽度不同） |
| 设计师可调性 | 直接以"度数"思考（×180/π） | 需要按 sub 重新校准 |
| GPU 成本 | 3 次 acos ≈ 6 cycle | 0 次 acos |
| sub=3 数值精度 | 充足（acos 在 [0, 0.4] 内梯度平稳） | 充足 |

R5 选 acos：6 cycle 的开销换来"绝对弧度物理量"+ 跨 sub 不变性 + 设计师友好——值得。

---

## 附录 B：smoothstep 在 EdgeWidth=0 处的极限行为

HLSL 标准 `smoothstep(edge0, edge1, x)` 的定义：

```
t = saturate((x - edge0) / (edge1 - edge0));
return t * t * (3 - 2*t);
```

当 `edge0 == edge1` 时除零——但 GPU 的 IEEE 754 浮点会产生 ±Inf，再被 saturate clamp 到 [0,1]，最终结果取决于 (x - edge0) 的符号：

- `x > edge0` 且 `edge0 > edge1`（我们的情况）→ (x - edge0) > 0，分母 (edge1 - edge0) < 0，但极限不存在
- 实际硬件实现通常返回 `step(edge0, x)` 或 `step(edge1, x)`

**安全做法**：在 cpp 端 `EdgeWidth` UPROPERTY 加 `ClampMin = "0.0001"`（即最小 0.0001 弧度，约 0.006°），在视觉上等同于 0 但不触发除零。或者在 HLSL 里加显式分支：

```hlsl
float wA, wB, wC;
if (EdgeWidth < 1e-5)
{
    wA = (deltaA <= 0) ? 1.0 : 0.0;
    wB = (deltaB <= 0) ? 1.0 : 0.0;
    wC = (deltaC <= 0) ? 1.0 : 0.0;
}
else
{
    float halfEW = EdgeWidth * 0.5;
    wA = smoothstep(halfEW, -halfEW, deltaA);
    wB = smoothstep(halfEW, -halfEW, deltaB);
    wC = smoothstep(halfEW, -halfEW, deltaC);
}
```

**R5 默认采用前者**（cpp 端 ClampMin=0.0001），保持 HLSL 简洁。如出现 EdgeWidth=0 视觉问题，再切到分支版。
