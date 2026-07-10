# R4：基于外心垂面的三角分割（球面 Voronoi 硬边）

> 本文档是 [SphericalSDFTerrainDesign.md §11.3](SphericalSDFTerrainDesign.md#113-r4-阶段说明基于外心垂面的三角分割消除-hex-边折角) 的独立落地文档，与 [R2_TopologyDebugMaterial.md](R2_TopologyDebugMaterial.md) / [R3_CellAttrLUTMaterial.md](R3_CellAttrLUTMaterial.md) 风格一致。
>
> 阅读本文档前请先理解 [SDF 设计稿 §2.1](SphericalSDFTerrainDesign.md#21-球面拓扑事实测地线球面--戈德堡多面体的对偶) 关于"primal mesh / dual hex 网格 / 1/3 角块"的拓扑事实。
>
> ⚠ **路线调整说明（2026-06-29）**：本文档中提到的"R11 PTG 路线零改动复用"等表述**已过期**——原 R11 PTG 路线已废弃，新生产路线是 [SDF 主稿 §16](SphericalSDFTerrainDesign.md#16-自研球面网格生产路线) 的"T 阶段自研球面网格（TessellatedMesh）"。但 R4 的 PS HLSL **仍然零改动复用**到 T 阶段，区别仅在 c0/c1/c2 来源（T 阶段由 cpp 端预计算灌顶点而非 UV 还原）。详见 [TessellatedMeshDesign.md](TessellatedMeshDesign.md)。

---

## 0. R4 一句话目标

把 R3 的 cell 判别准则从 **`argmax(λ)`（折线硬边）** 升级为 **`argmax(dot(dir, V_i))`（球面 Voronoi 测地线硬边）**，**消除 hex/pent 边在每条 mesh 边中点处的可见折角**，使整个球面的 cell 边界几何上是一系列光滑的大圆弧。

视觉差异：

| 阶段 | 等位线几何 | 单个 hex 视觉边数 | 折角位置 |
| --- | --- | --- | --- |
| R3 | 三角形外心 → 三个边中点的折线 | 12（每条原 hex 边在 mesh 边中点折成两段） | 每条 mesh 边中点都有折角 |
| **R4** | **球面 Voronoi 大圆弧（测地线）** | **6（pent: 5）** | **无** |

---

## 1. 问题再现与几何根因

### 1.1 R3 实测现象

R3 验收时观察到：球面上每个 hex / pent 区域**色块本身正确**（颜色 = LUT.Load(LayerIndex) 哈希），但**多边形的边并非平滑测地线**——每条 cell 边在它所跨过的 mesh 边的中点处出现一次明显的折角，使本应 6 边形的 hex 在视觉上变成"12 边形"：

> ![hex-edge-kink](https://assets.with.tencent.com/default/456359a0-bbd0-4db9-8b16-ea6e49332a43/image-9614dccc0bc749a8a6a8e24ad23af0d6.png)

### 1.2 几何根因（为什么 §14.7 解决不了它）

R3 PS 端使用 `argmax(λ)` 作为 cell 判别准则，其 1/3 角块边界在每个三角形内部 = "外心 $\hat{O}$ → 三个边中点 $M_{AB}, M_{BC}, M_{CA}$ 的三段折线"（详见 [SDF §14.7](SphericalSDFTerrainDesign.md#147-球面重心坐标与外心折角修正核心几何不变量)）。在两个共享一条 mesh 边 $V_A V_B$ 的相邻三角形里：

- 三角形 T₁ 用自己的外心 $\hat{O}_1$ 连到边中点 $M_{AB}$
- 三角形 T₂ 用自己的外心 $\hat{O}_2$ 连到同一个 $M_{AB}$
- IcoSphere sub≥1 后两个外心**通常不重合**（不在同一条以 $M_{AB}$ 为切点的方向上）
- 所以 cell 边界（即 cell A 与 cell B 的分界）在 $M_{AB}$ 处**两段方向不同的弧线相交**，必然出现折角

注意 §14.7 解决的是"**权重场连续性**"（保证 SDF 跨三角形边没有数值裂缝），它能保证 $\lambda$ 函数本身**连续**（C⁰），但不能保证 argmax(λ) 的**等位线**在 mesh 边上 C¹ 光滑。这是两个不同层次的问题：

- §14.7 → 函数 $\lambda(\text{dir})$ 跨边连续
- R4    → 函数 $\arg\max_i \lambda_i(\text{dir})$ 的等值线全局光滑（不仅 C⁰，而且是球面测地线）

R3 用的 `argmax(λ)` 等值线是分段折线，**不是测地线**——这是几何固有问题，不可能通过调"权重公式"解决，只能换"判别准则"。

---

## 2. R4 方案：球面 Voronoi（dot 距离 argmax）

### 2.1 核心观察

球面上**两个 cell 中心 $V_A, V_B$ 的垂直平分大圆弧** = 满足 $\hat{d}\cdot V_A = \hat{d}\cdot V_B$ 的方向集合。三个 cell 的 Voronoi 区域边界由三条这样的大圆弧相交于一个**外心 $\hat{O}$**（恰好满足 $\hat{O}\cdot V_A = \hat{O}\cdot V_B = \hat{O}\cdot V_C$）。这正是真正的 Goldberg 多面体 / 球面 Voronoi 图的边界。

### 2.2 R4 判别准则

$$
c_{\text{chosen}} = c_{\arg\max_i (\hat{d}\cdot V_i)}, \quad i \in \{0, 1, 2\}
$$

其中 $\hat{d}$ = fragment 方向（单位向量），$V_i$ = 三个候选 cell 中心方向。

### 2.3 几何性质（为什么没有折角）

- 等位线 $\hat{d}\cdot V_A = \hat{d}\cdot V_B$ = 平面 $\{x\cdot(V_A - V_B) = 0\}$ 与单位球面的交圆 = **球面大圆（测地线）**
- 在 mesh 边上（$\hat{d} = M_{AB}$ 附近）：等位线轨迹**完全由 $V_A, V_B$ 决定**，与 $V_C$ 无关 → 两侧三角形（T₁ 用 $V_A,V_B,V_{C_1}$，T₂ 用 $V_A,V_B,V_{C_2}$）给出的 Voronoi 边在 mesh 边上**完全重合，没有折角**
- 在外心处：三条 Voronoi 边正交三方相会，是 hex/pent 的 corner 几何点（与 §14.7 一致）

---

## 3. 方案选型：为什么必须走 CellDirLUT 纹理

### 3.1 三个备选 + 锁定理由

R4 PS 端需要拿到三个 cell 的中心方向 $V_A, V_B, V_C$。理论上有三种数据通路：

| 备选 | 做法 | R4 是否可行 | R11 PTG 是否可行 |
| --- | --- | --- | --- |
| A. 换 [`URealtimeMeshComponent`](https://github.com/TriAxis-Games/RealtimeMeshComponent) / 自定义顶点工厂 | 解锁 UV4~UV7，per-vertex 写入 V_A/V_B/V_C | ✅ | ❌ PTG mesh 没有"per-triangle 三个 cell 中心"概念 |
| B. 复用 VertexColor RGBA + 扩展 PMC 顶点缓冲 | 把方向打包到 4 通道 8-bit | ⚠ 精度不够（fp16/8 都不行） | ❌ 同 A |
| **C. CellDirLUT 纹理 + 3 次 `Texture2D.Load`** | 全局 1×NumCells 纹理，PS 用 c0/c1/c2 三次 Load | ✅ | ✅ |

### 3.2 锁定方案 C 的根本原因（与 R11 同构性）

设计稿 §14.3 PTG 路线 PS 端的着色流程：

```text
Step 1: dir = normalize(WorldPosition - PlanetCenter)          ← R4 已经这么做
Step 2: GPU_FindNearestTri(dir) → (TriId, c0, c1, c2)          ← R4 用 R3 已解码的 c0/c1/c2 替代
Step 3: 用三 cell 中心方向算权重 / 判别                          ← R4 用 CellDirLUT.Load(c_i) 已实现
Step 4: CellAttrLUT.Load(chosen) → LayerIndex                  ← R4 完全一致
Step 5: 三层 Triplanar（R7 起）                                 ← R4 暂用 hash placeholder
```

**Step 1、Step 3、Step 4 在 R4 与 R11 之间逐字相同**——R4 的 HLSL 核心代码可以**原封不动搬到 R11 的 PTG 材质**，唯一变动的是 Step 2 的 c0/c1/c2 来源：

| 阶段 | (c0, c1, c2) 数据来源 |
| --- | --- |
| **R4**（IsoSphere 调试 mesh） | cpp 端 per-vertex 写入 UV1/UV2，PS 直接 8-bit 拆分还原（每 fragment 0 次额外 Load） |
| **R11**（PTG 生产 mesh） | GPU `FindNearestCell(dir)` 现场查询（每 fragment 1 次四叉树下降） |

两者输出相同的 (c0, c1, c2) → 后续 Step 3/4/5 完全共用。

**直接推论**：
- R4 **不应**使用任何"per-vertex 的 V_A/V_B/V_C 顶点流"——R11 的 PS 输入根本没有 per-vertex 数据，只有 `WorldPosition` 和全局纹理。V_A/V_B/V_C 必须从纹理 Load，这是被 R11 的渲染管线**强制约束**的，不是 R4 的可选优化。
- R4 的 `CellDirLUT` 在 R11 是**同一份纹理资产**，不需要重建，只需要在 PTG 材质里复用。
- 后续 R5（Sharpen 软边）、R7（Triplanar 真纹理）、R8（WorldGen LayerIndex 接入）、R9（Decor/Owner/Fog 多 LUT）的所有 HLSL 设计都遵循同一原则：**任何 cell 级数据只能通过 1×NumCells 纹理 LUT 获取，绝不通过顶点流**。

### 3.3 GPU 成本

相对 R3 仅增加：
- 3 次 `Texture2D.Load`（FP32，按整数索引零滤波）
- 3 次 dot
- 1 次 normalize
- 1 次减法（WorldPos - PlanetCenter）

`CellDirLUT` 在 sub=3 时只 642×16 = ~10 KB，常驻 L2 cache。相对 R3 的整体每像素成本几乎无差。

---

## 4. cpp 端改动

### 4.1 新增 `BuildCellDirLUT_(NumCells)` 方法

参照 R3 的 `RebuildCellAttrLUT_(NumCells)` 同款结构（[PlanetTopologyDebugMesh.cpp](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp)），新增一份创建 `CellDirLUT` 纹理的逻辑：

- 格式：`PF_A32B32G32R32F`（R16F 精度对方向夹角不够，FP32 与 cell 中心方向真实精度匹配）
- Filter：`TF_Nearest`（按 CellId 整数索引采样，绝不允许双线性插值）
- SRGB：false（线性数据，不是颜色）
- 像素：`(R, G, B, A) = (UnitCenter.x, UnitCenter.y, UnitCenter.z, isPentagon ? 1.0 : 0.0)`
- A 通道 isPentagon 标志预留给 R5+（未来软边可能需要 pent 特判）

写入步骤：

```cpp
CellDirLUT = UTexture2D::CreateTransient(NumCells, 1, PF_A32B32G32R32F);
CellDirLUT->Filter        = TF_Nearest;
CellDirLUT->SRGB          = false;
CellDirLUT->AddressX      = TA_Clamp;
CellDirLUT->AddressY      = TA_Clamp;
CellDirLUT->LODGroup      = TEXTUREGROUP_ColorLookupTable;
CellDirLUT->CompressionSettings = TC_HDR;

FTexturePlatformData* Plat = CellDirLUT->GetPlatformData();
FByteBulkData& Bulk = Plat->Mips[0].BulkData;
float* Dst = static_cast<float*>(Bulk.Lock(LOCK_READ_WRITE));
for (int32 Cid = 0; Cid < NumCells; ++Cid)
{
    const FVector& U = Topology.Cells[Cid].UnitCenter;
    Dst[Cid * 4 + 0] = (float)U.X;
    Dst[Cid * 4 + 1] = (float)U.Y;
    Dst[Cid * 4 + 2] = (float)U.Z;
    Dst[Cid * 4 + 3] = Topology.Cells[Cid].bIsPentagon ? 1.0f : 0.0f;
}
Bulk.Unlock();
CellDirLUT->UpdateResource();
```

### 4.2 顶点结构 100% 不变

UV0/UV1/UV2/UV3 + VertexColor 全部维持 R3 的写入逻辑，**不需要任何额外字段**。R4 视觉效果完全靠材质端切换判别准则实现。

### 4.3 MID 注入清单

`Rebuild()` 末尾 MID 创建后，按表逐一注入：

```cpp
MID->SetTextureParameterValue(TEXT("CellAttrLUT"),  CellAttrLUT);
MID->SetTextureParameterValue(TEXT("CellDirLUT"),   CellDirLUT);     // R4 新增
MID->SetVectorParameterValue (TEXT("PlanetCenter"), FLinearColor(GetActorLocation())); // R4 新增
MID->SetScalarParameterValue (TEXT("NumLayersHint"),(float)NumLayersHint);
MID->SetScalarParameterValue (TEXT("NumCells"),     (float)NumCells);
```

每次 Rebuild 必须重新调一次（即使 PlanetCenter 没变，因为 MID 在 Rebuild 时被重建）。

### 4.4 Output Log 验收字符串

把 R3 末尾的 `Rebuilt (R3: ...)` 改写为：

```text
Rebuilt (R4: argmax(dot(dir,V_i)) + dual LUT). SubdivisionLevel=N Cells=N Corners=N Verts=N Tris=N Radius=R Smooth=B CellAttrLUT=OK CellDirLUT=OK PlanetCenter=(x,y,z) NumLayersHint=N
```

任何 LUT 缺失就打 `MISSING`，便于 §11.2.3 反射诊断时快速定位。

---

## 5. 材质搭建（M_TopologyDebug_R4）

### 5.1 复制 R3 材质

复制 `M_TopologyDebug_R3.uasset` 重命名为 `M_TopologyDebug_R4.uasset`，保留所有 R3 现有节点（Texture Object Parameter `CellAttrLUT`、Scalar Parameter `NumLayersHint`/`NumCells` 等）。

### 5.2 新增 Material 参数

| 类型 | 名字 | 说明 |
| --- | --- | --- |
| Texture Object Parameter | `CellDirLUT` | Sampler Type = `Linear Color`；不要 Texture Sample（会经 Mip） |
| Vector Parameter | `PlanetCenter` | RGB = Actor 世界坐标，A 不用 |

### 5.3 改写 Custom 节点

#### 5.3.1 Inputs（在 R3 基础上新增 2 个）

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

> ⚠ **Inputs 顺序必须与 Code 形参一一对应**——UE 按 Inputs 顺序生成 HLSL 形参。

#### 5.3.2 Code（直接复制粘贴）

```hlsl
// ---- 沿用 R3 的 8-bit 拆分还原 c0/c1/c2 ----
//      cpp 端写入：UV0=(HiC,LoC)、UV1=(HiA,LoA)、UV2=(HiB,LoB)
int c0 = (int)(UV1.x + 0.5) * 256 + (int)(UV1.y + 0.5);
int c1 = (int)(UV2.x + 0.5) * 256 + (int)(UV2.y + 0.5);
int c2 = (int)(UV0.x + 0.5) * 256 + (int)(UV0.y + 0.5);

// ---- R4 新增：从 CellDirLUT 取三 cell 单位中心方向（Load = 整数索引、零滤波）----
float3 V_A = CellDirLUT.Load(int3(c0, 0, 0)).rgb;
float3 V_B = CellDirLUT.Load(int3(c1, 0, 0)).rgb;
float3 V_C = CellDirLUT.Load(int3(c2, 0, 0)).rgb;

// ---- R4 新增：球面 Voronoi 判别（dot 距离 argmax）----
//   等位线 dir·V_i = dir·V_j 是平面 (V_i - V_j)·x = 0 与单位球的交大圆 = 测地线
//   ⇒ hex/pent 边在所有 mesh 边的中点处都没有折角
float3 dir = normalize(WorldPos - PlanetCenter);
float dA = dot(dir, V_A);
float dB = dot(dir, V_B);
float dC = dot(dir, V_C);
int chosen = (dA >= dB && dA >= dC) ? c0 : (dB >= dC ? c1 : c2);

// ---- 沿用 R3：LayerIndex LUT + hash placeholder（R7 起替换为真纹理采样）----
int layer = (int)(CellAttrLUT.Load(int3(chosen, 0, 0)).r * 255.0 + 0.5);
float hue = frac((layer + 1) * 0.6180339887);
float3 col = float3(frac(hue * 1.0), frac(hue * 7.123), frac(hue * 13.456));
return saturate(col + 0.25);
```

### 5.4 输出与材质赋值

1. Custom 节点输出 → Material 的 **Emissive Color**
2. Apply + **Save**（必须 Save，否则 .uasset 不更新；详见 [SDF §11.2.3](SphericalSDFTerrainDesign.md#1123-uasset-二进制-dump-工具对-ue-5x-材质不可用)）
3. 编辑器场景里选中 `APlanetTopologyDebugMesh` 实例 → Details
4. ⚠ 把材质挂到 **`PlanetTopology > Material`** 槽位，**不是** `渲染 > 材质 > 元素 0`（详见 [SDF §11.2.2](SphericalSDFTerrainDesign.md#1122-材质必须挂在-actor-的-planettopology--material-槽位极易踩坑)）
5. OnConstruction 自动 Rebuild → cpp 端创建 LUT、注入 MID → 立即看到 R4 效果

---

## 6. 验收清单

| 项 | 期望 |
| --- | --- |
| **A. hex/pent 边几何** | **每条 cell 边视觉上是连续光滑的大圆弧**，相机移到任意位置（尤其垂直俯视 mesh 边中点附近）看不到折角 |
| **B. layer 合并** | 调小 `NumLayersHint` 时大片相邻 hex 颜色合并（R3 LUT 路径保持有效） |
| **C. pent 几何对称** | 12 个 pent 几何对称、可以肉眼数出来 |
| **D. 边性质** | 同 layer 相邻 hex/pent 之间完全融合、不同 layer 之间是平滑测地线硬边（不再是折线硬边） |
| **E. Output Log** | `Rebuilt (R4: argmax(dot(dir,V_i)) + dual LUT) ... CellAttrLUT=OK CellDirLUT=OK ...` |
| **F. 反射诊断** | §11.2.3 描述的 cpp 反射枚举材质 Custom 节点时，能看到 8 个 Inputs 全部 `Connected=YES` |

### 6.1 R3 → R4 视觉前后对比

| 观察位置 | R3 | R4 |
| --- | --- | --- |
| hex 中央向外的边数 | 看起来 12 边（每边在 mesh 边中点折一下） | **6 边**（pent: 5 边） |
| 跨过 mesh 边时 | 一段直/弧线在中点突然转向 | **大圆弧自然延续，不可见 mesh 边的存在** |
| pent 与相邻 hex 的边 | 折线 | 平滑测地线 |

---

## 7. Material 参数命名规范（R4 起锁定，R11 沿用）

所有 cell 级几何 / 属性数据均通过 MID 参数注入：

| 参数名 | 类型 | 内容 | 引入阶段 |
| --- | --- | --- | --- |
| `CellAttrLUT` | Texture2D Object（1×NumCells，R8G8B8A8） | `(LayerBase, LayerDecor, Variant, Mask)` | R3 |
| `CellDirLUT` | Texture2D Object（1×NumCells，R32G32B32A32_FLOAT） | `(UnitCenter.xyz, isPentagon)` | **R4** |
| `PlanetCenter` | Vector4 Parameter | `(x, y, z, _)` = Actor 世界坐标 | **R4** |
| `NumLayersHint` | Scalar Parameter | LayerIndex 哈希分桶上限（仅 placeholder 阶段用） | R3 |
| `NumCells` | Scalar Parameter | LUT 有效宽度（== `Topology.Cells.Num()`） | R3 |
| `CellHeightLUT` | Texture2D Object（1×NumCells，R16_FLOAT） | Elevation | R12（生产路线） |
| `CellHighlightLUT` | Texture2D Object（1×NumCells，R8G8B8A8） | `(R,G,B,bSelected)` | R13（生产路线） |

**约束**：
- 所有 LUT 纹理 **`Filter=Nearest`、`SRGB=false`、`Width=NumCells`、`Height=1`**
- cpp 端在 [`APlanetTopologyDebugMesh::Rebuild()`](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp) 末尾按表逐一注入；任何参数缺失都会被材质 fallback 到 `GBlackTexture` / 0，导致 [§11.2.2](SphericalSDFTerrainDesign.md#1122-材质必须挂在-actor-的-planettopology--material-槽位极易踩坑) 描述的"挂错槽位"同类静默失败
- Output Log 必须打印每个参数的注入状态（`OK` / `MISSING`），便于 [§11.2.3](SphericalSDFTerrainDesign.md#1123-uasset-二进制-dump-工具对-ue-5x-材质不可用) 描述的反射诊断

---

## 8. 排错表（R4 常见症状 → 根因 → 修复）

| 症状 | 根因 | 修复 |
| --- | --- | --- |
| 整球粉色，hex 边折角依然存在 | 用了旧版 R3 材质 / Custom 节点 Code 没保存 | 检查材质 .uasset 修改时间；用 cpp 反射打印 Code 字段（[§11.2.3](SphericalSDFTerrainDesign.md#1123-uasset-二进制-dump-工具对-ue-5x-材质不可用)） |
| 整球纯黑 / 全 layer 0 | `CellDirLUT` 是 `GBlackTexture` fallback（参数没注入或材质挂错槽位） | 检查 Output Log 是否有 `CellDirLUT=OK`；材质必须挂在 `PlanetTopology > Material`（[§11.2.2](SphericalSDFTerrainDesign.md#1122-材质必须挂在-actor-的-planettopology--material-槽位极易踩坑)） |
| 颜色乱码 / 闪烁 | `CellDirLUT` 用了 R16F 精度 → cell 中心方向被截断 | 必须 `PF_A32B32G32R32F`，不能用 R16F |
| Custom 节点编译报错 `cannot convert from 'Texture2D' to 'float'` 等 | Inputs 顺序错乱，`CellDirLUT` 被识别成 Float2 | 严格按 §5.3.1 顺序添加 Inputs；UE 按顺序生成形参 |
| 球面 hex 颜色对，但 hex 边折角和 R3 完全一样 | PS 仍走 R3 的 argmax(λ) 路径（OneHot UV3） | 检查 Code 里有没有 `UV3.xy` 用作判别；R4 必须用 `dot(dir, V_i)` |
| dir 不对 / 球面只有半边显示 | `WorldPos` 节点连成了 Local Position；或 `PlanetCenter` 写错了 | 必须连 **Absolute World Position**；PlanetCenter 走 cpp `GetActorLocation()` |

---

## 9. 后续路径预告（R5+）

| 阶段 | 接续动作 |
| --- | --- |
| **R5** | Sharpen(λ) 软边：在 `argmax(dot)` 硬边与"λ 加权"软边之间连续切换；R4 的 dot argmax 与 §14.7 的 λ argmax 严格同区，可以共用 .ush 函数 |
| **R6** | 在判别面上叠 3D 噪声扰动：边界变蜿蜒、山脚参差不齐 |
| **R7** | `hash(layer)` 哈希色换成 `Triplanar(TerrainAlbedoArray, layer, WorldPos, Normal)` 真实地形纹理 |
| **R8** | 接入 WorldGen 的 `FCellGeoData → LayerIndex`，跑出第一张可玩星球 |
| **R11** | 渲染从 IsoSphere 调试 mesh 切到 PTG 高细分球皮；R4 的 PS 核心 HLSL 原封不动复用，仅 c0/c1/c2 来源换成 GPU `FindNearestCell` |

---

## 附录 A：与 §14.7 SphericalBarycentric 的关系

R4 的 `argmax(dot(dir, V_i))` 与 §14.7 的 `argmax(λ_i)` **严格同区**——在球面三角形内，两者划分出的 cell 区域**完全重合**（都满足 §14.7.4 的三个不变量：cell 中心 onehot、共享边 0.5/0.5/0、外心 1/3/1/3）。

但两者用途不同：

| 路径 | 输出 | 用途 |
| --- | --- | --- |
| R4 `argmax(dot)` | 整数 `chosen`（0/1/2 → c0/c1/c2） | 硬边切分（决定纹理硬切、不可微） |
| §14.7 λ | 连续权重 (λ_A, λ_B, λ_C) ∈ [0,1]³ | 软边/可微（Sharpen、Triplanar 加权、WPO 位移、§15 高亮 gap） |

**R11 PTG PS 端会同时跑两条**：硬边判别用 dot argmax（决定纹理硬切分），软边/高亮/位移用 λ（决定连续过渡量）。`CellDirLUT` 同时服务这两条路径——dot 路径直接读 V_i；λ 路径在 §14.7 的三重积公式里也需要 V_A/V_B/V_C。
