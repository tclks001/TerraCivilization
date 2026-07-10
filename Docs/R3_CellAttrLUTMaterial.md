# R3 验收：argmax(λ) + Load(CellAttrLUT) → LayerIndex → 颜色

> 关联代码：
> - C++：[PlanetTopologyDebugMesh.cpp](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp) `RebuildCellAttrLUT_()` / `Rebuild()` 末尾 MID 注入
> - 设计稿：[SphericalSDFTerrainDesign.md §11.2 R3 推进要点](SphericalSDFTerrainDesign.md#112-r3-推进要点下一步)
>
> 关联前置：[R2_TopologyDebugMaterial.md](R2_TopologyDebugMaterial.md)（R2 已选定 PS 端 argmax 硬边作为验收路径，R3 在它基础上只多一次 `LUT.Load`）

> 📐 **拓扑前置（与 R2 同）**：球面上**只有一种几何元素**——primal 三角形（即 `FCorner`）。每个三角形的 3 个顶点是 3 个 Cell 中心；每个三角形被外心-边中点连线划分为 3 个 1/3 角块，分别归属其 3 个顶点 Cell 的 hex/pent。**不存在所谓"hex 内部三角形"与"hex 之间过渡三角形"之分**。详见 [SphericalSDFTerrainDesign §2.1.1](SphericalSDFTerrainDesign.md#211-关键拓扑澄清所有三角形地位完全等价极重要)、[§2.1.2](SphericalSDFTerrainDesign.md#212-视觉错觉防御13-角块的判别准则用于-r2r3-验收)。

---

## 0. 两条 R3 验收路径（先看这里！）

R3 阶段提供**两条等价**的验收路径，**LayerIndex 来源完全一致**（都是 `(CellId * 2654435761u) % NumLayersHint`）；区别仅在硬边 vs 软边、以及 LUT 是否真正被 GPU 读取：

| 路径 | 材质复杂度 | 边界 | 何时使用 |
| --- | --- | --- | --- |
| **A. VertexColor 软边路径**（推荐先做） | 2 个节点 | 软边（线性插值） | **无脑零配置**，先用它确认 cpp 端 LayerIndex 与 NumLayersHint 计算正确；任何材质 Custom 节点配置错误都不会影响这条路径 |
| **B. argmax + LUT.Load 硬边路径** | Custom HLSL + 4 个输入 | 硬边（PS 端 argmax） | 主推路径，验证 GPU LUT 通路，为 R6 Triplanar / R7 WorldGen 接入铺路 |

### 推荐流程

1. **第一步**：用路径 A 确认基本视觉正确（同 layer 融合、调小 NumLayersHint 见大片合并）
2. **第二步**：再切到路径 B 验证 LUT 真实从 GPU 读取
3. **如果 B 异常但 A 正常** → 100% 是 Custom HLSL 节点配置错（Inputs 顺序、Code 内容、Texture Object Parameter 命名等），看 §5 排错

### 路径 A 详细步骤（最简 R3 验收）

**C++ 端已自动完成**：每次 `Rebuild()`（包括编辑器中改 NumLayersHint 触发的）都会重新计算每个 Cell 的 LayerIndex，将其哈希后的颜色写入该 Cell 所有顶点的 VertexColor 通道。

**材质（仅 2 个节点）**：

1. Content Browser → Material → 命名 `M_TopologyDebug_R3_Simple`
2. 打开材质编辑器，根节点 **Shading Model** = `Unlit`
3. 拖一个 `VertexColor` 节点，把它的 RGB 输出连到 **Emissive Color**
4. 编译保存
5. 把该材质赋给场景中 `APlanetTopologyDebugMesh` 的 **Material** 属性

**预期效果（此路径独有的"软边"特征）**：

- ✅ 球面被**至多 NumLayersHint 种颜色**密铺
- ✅ 同色相邻 hex 之间**完全无可见边界**（因为 VertexColor 也相同）
- ✅ 不同色 hex 之间是**软渐变**（光栅化器线性插值，与硬边路径区别于此）
- ✅ 调小 `NumLayersHint`（如 4 / 8）→ 大片相邻 hex 立刻合并
- ✅ 编辑器 Details 面板里改 `NumLayersHint` 立即生效（C++ 端 `PostEditChangeProperty` 强制 Rebuild）

**这条路径"看不到"的**：

- ✗ argmax 硬边（这是路径 B 才有的；路径 A 是 λ 加权软边）
- ✗ GPU 端 LUT 实际是否走通（路径 A 完全跑 cpu 端预算，旁路了 LUT）

如果 `NumLayersHint=32` 默认下你看到的是大量同一种粉色 + 少数彩色 hex，且改 NumLayersHint 完全没反应——一定是 cpp 没编译进新版本，重新编译后再试。

> **路径 B 的细节请继续往下看 §1~§5**。

---

## 1. R3 vs R2 的差异

R2 的 PS 公式（argmax 硬边）：

```hlsl
int chosen = (l0 >= l1 && l0 >= l2) ? c0 : ((l1 >= l2) ? c1 : c2);
return saturate(hash(chosen + 1) + 0.25);   // 颜色 = CellId 哈希
```

R3 在它末尾**只多一次 LUT.Load**：

```hlsl
int chosen = (l0 >= l1 && l0 >= l2) ? c0 : ((l1 >= l2) ? c1 : c2);
int layer  = (int)(LUT.Load(int3(chosen, 0, 0)).r * 255.0 + 0.5);
return saturate(hash(layer + 1) + 0.25);    // 颜色 = LayerIndex 哈希
```

| 阶段 | PS 着色公式 | 验收要点 |
| --- | --- | --- |
| **R2** | `hash(argmax_cell + 1)` | 642 个独立色块、每色一格、12 个 pent 可见 |
| **R3** ⭐ | `hash(LUT.Load(argmax_cell).r * 255 + 1)` | 至多 `NumLayersHint` 种颜色；多 Cell 共用同一 LayerIndex 时合并为同色大区域；调小 `NumLayersHint` 时大片相邻 hex 颜色融合 |

### 为什么这才是 R3 的"正确"形态

- **保留 argmax 硬边**：R2 已经验证过，argmax 域恰好 = 外心-边中点连线划出的 1/3 角块。这是数学上的拓扑事实，不应在 R3 抹掉。
- **LUT 是逻辑层**：`hash(CellId)` 是一个调试 placeholder——每 Cell 一种颜色，与"地形分类"无关。`hash(LayerIndex)` 才是 R7 接入 WorldGen 的真实路径——CellId 只是"我是 642 个格子里的哪一个"，而 LayerIndex 才是"我是草原 / 沙漠 / 海洋"。R3 把这一层抽象正式引入。
- **未来扩展的入口**：R6（Triplanar 真实地形纹理）只需把 `hash(layer)` 换成 `Texture2DArray.Sample(albedoArr, ..., layer)`；R8（Decor / Owner / Fog）只需在 LUT.Load 之后再读 G/B/A 三个通道——所有扩展都在这一行内完成。

> **R3 不引入"λ 加权三方混合"**。该路径会**冲销** R2 验证过的 1/3 角块切分（参见 [SphericalSDFTerrainDesign §2.1.2](SphericalSDFTerrainDesign.md#212-视觉错觉防御13-角块的判别准则用于-r2r3-验收) 关于"argmax vs 线性混合"的视觉差异说明）。

---

## 2. C++ 端已完成的工作（无需手动操作）

[`APlanetTopologyDebugMesh::Rebuild()`](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp) 现在会自动：

1. 调用 `RebuildCellAttrLUT_(NumCells)` —— 创建 `1×NumCells` 的 `PF_B8G8R8A8` `UTexture2D`，名为 `CellAttrLUT_Transient`
2. 用 Knuth 整数哈希 `LayerIndex = (CellId * 2654435761u) % NumLayersHint` 填到 R 通道
3. 把外部 `Material` 包装为 `UMaterialInstanceDynamic`（MID）
4. 调用 `MID->SetTextureParameterValue("CellAttrLUT", LUT)` + `SetScalarParameterValue("NumLayersHint", ...)` + `SetScalarParameterValue("NumCells", ...)`
5. 把 MID 设为 PMC Section 0 的材质

**纹理设置（关键，否则视觉会崩）**：

| 属性 | 值 | 原因 |
| --- | --- | --- |
| `Filter` | `TF_Nearest` | 禁止双线性插值，否则两个邻居 Cell 的 layer 会被混合 |
| `SRGB` | `false` | R 通道是离散整数（0~255），不能被 sRGB→Linear 解码 |
| `AddressX` / `AddressY` | `TA_Clamp` | Cell 编号必须在 `[0, NumCells)` 内，越界视为最后一格 |
| `CompressionSettings` | `TC_VectorDisplacementmap` | 强制 BGRA8 不压缩；UE 默认压缩纹理会损坏整数语义 |
| `LODGroup` | `TEXTUREGROUP_ColorLookupTable` | UE 引擎专为 LUT 设计的组：默认 Filter=Nearest、强制 SRGB=false、强制 NoMipmaps、强制不压缩；比 `Pixels2D` 更可靠 |

调试参数（Actor Details 面板可改）：

| 属性 | 默认 | 作用 |
| --- | --- | --- |
| `NumLayersHint` | 32 | 调小（如 8 / 4）→ 多个相邻 Cell 共用同一 LayerIndex → 看到"邻居 hex 颜色相同导致边界融合"的效果，验证 LUT 路径生效 |

---

## 3. 材质资产搭建步骤

R3 验收推荐用一条 `Custom` HLSL 节点路径——它直接复用 R2 的材质，仅在末尾加一次 `LUT.Load`。

### 3.1 创建材质 `M_TopologyDebug_R3`

1. Content Browser → Material → 命名 `M_TopologyDebug_R3`
2. 打开材质编辑器；根 MaterialNode 设置：
   - **Shading Model** = `Unlit`
   - **Two Sided** = false（球面只看正面）
3. 添加纹理参数：
   - 拖出一个 **Texture Object Parameter** 节点，命名 `CellAttrLUT`
   - **Sampler Type** = `Linear Color`（R 通道是离散整数，不要 sRGB 解码）
   - 默认纹理可以留空——C++ 端会在 OnConstruction 时把 `CellAttrLUT_Transient` 通过 MID 注入
   - ⚠ **不要**用 Texture Sample 节点（它会自动走 SamplerState 与 Mip）；R3 用 `Texture2D.Load(int3)` 直读

> 仅本节用 `CellAttrLUT` 这一个 Texture Object Parameter；不需要任何 Scalar/Vector 参数（`NumLayersHint` / `NumCells` 只是给 MID 留的诊断槽，shader 内不读）。

### 3.2 添加 Custom 节点

新建 **Custom** 节点：

- **Output Type**：`CMOT Float3`
- **Inputs**（顺序与 Code 中的形参一一对应，**顺序错就会编译错位**）：

| # | 名字 | 类型 | 连源 |
| --- | --- | --- | --- |
| 1 | `UV0` | Float2 | `TextureCoordinate(CoordinateIndex=0)` |
| 2 | `UV1` | Float2 | `TextureCoordinate(CoordinateIndex=1)` |
| 3 | `UV2` | Float2 | `TextureCoordinate(CoordinateIndex=2)` |
| 4 | `UV3` | Float2 | `TextureCoordinate(CoordinateIndex=3)` |
| 5 | `LUT` | Texture2D | `CellAttrLUT` Texture Object Parameter |

> ⚠ **极重要**：Inputs 必须**按上表顺序**添加。UE 按 Inputs 顺序生成 HLSL 形参，顺序错乱会导致 `UV1` 形参实际拿到 UV0 的值，所有解码都崩。

### 3.3 Code（直接复制粘贴）

```hlsl
// ---- 8-bit 拆分解码 3 个 CellId（避开 fp16 精度问题，详见 cpp 注释）----
//      cpp 端写入：UV0=(HiC,LoC)、UV1=(HiA,LoA)、UV2=(HiB,LoB)
//      解码：CellId = round(Hi)*256 + round(Lo)
int c0 = (int)(UV1.x + 0.5) * 256 + (int)(UV1.y + 0.5);
int c1 = (int)(UV2.x + 0.5) * 256 + (int)(UV2.y + 0.5);
int c2 = (int)(UV0.x + 0.5) * 256 + (int)(UV0.y + 0.5);

// ---- 还原重心坐标 / 三方权重 ----
float l0 = saturate(UV3.x);
float l1 = saturate(UV3.y);
float l2 = saturate(1.0 - l0 - l1);

// ---- argmax(λ)：选三个权重中最大的那个对应的 CellId ----
//      argmax 域 = 外心-边中点连线划出的 1/3 角块（拓扑事实）
int chosen;
if (l0 >= l1 && l0 >= l2)      chosen = c0;
else if (l1 >= l2)             chosen = c1;
else                           chosen = c2;

// ---- 一次 LUT.Load 拿到该 Cell 的 LayerIndex ----
//      LUT 是 1×NumCells 的 BGRA8 整数纹理；.r 返回 [0,1]，乘 255 还原为 layer
int layer = (int)(LUT.Load(int3(chosen, 0, 0)).r * 255.0 + 0.5);

// ---- LayerIndex 哈希成颜色（R3 placeholder；R6+ 会换成 Texture2DArray 真实采样）----
//      +1 避免 layer=0 → sin(0)=0 → 黑色
float hue = frac((layer + 1) * 0.6180339887);
float3 col = float3(frac(hue * 1.0), frac(hue * 7.123), frac(hue * 13.456));
return saturate(col + 0.25);
```

### 3.4 输出与材质赋值

1. Custom 节点输出 → Material 的 **Emissive Color**
2. 编译保存（**Apply 按钮 + Save 按钮都要按**，Apply 只更新内存，Save 才写入 .uasset）
3. 编辑器场景里选中 `APlanetTopologyDebugMesh` 实例 → Details 面板
4. ⚠ **极易踩坑（必须看清楚槽位）**：把 `M_TopologyDebug_R3` 挂到 **`PlanetTopology > Material`** 槽位（即 Actor 自己暴露的 UPROPERTY），**不是** **`渲染 > 材质 > 元素 0`** 槽位。这两个槽位在 Details 面板里都是"材质"字样，非常容易混淆。详见下表：

   | 槽位位置 | 对应属性 | 是否走 LUT 注入 |
   | --- | --- | --- |
   | **PlanetTopology > Material** ✅ | `APlanetTopologyDebugMesh::Material` | ✅ Rebuild 时包装 MID + 注入 `CellAttrLUT` |
   | 渲染 > 材质 > 元素 0 ❌ | `MeshComp->OverrideMaterials[0]` | ❌ 直接覆盖渲染槽，绕过 cpp 的 MID 包装；CellAttrLUT 永远是黑色 fallback；改 NumLayersHint 无反应 |

   挂错槽位的视觉症状：**整球只有一种粉色 hex + 极少数彩色，改 NumLayersHint 完全无反应**。详见 [SDF 设计稿 §11.2.2](SphericalSDFTerrainDesign.md#1122-材质必须挂在-actor-的-planettopology--material-槽位极易踩坑)。

5. **OnConstruction 自动 Rebuild** → C++ 端创建 LUT 并通过 MID 注入材质 → 立即看到效果

---

## 4. 实际预览预期效果

| 现象 | 含义 |
| --- | --- |
| 球面被**至多 `NumLayersHint` 种颜色**密铺（默认 32 种） | LUT.Load 还原 layer 正确；多 Cell 共用同一 layer → 同色 |
| 同色相邻 hex 之间**没有任何可见边界**（直接融合） | argmax 域是 1/3 角块；同 layer 时该角块两侧 hash 色相同 → 视觉融合，与几何边界无关 |
| 不同色 hex 之间是**硬边**（非渐变） | argmax 在角块边界处突变 → R3 与 R2 一样保留硬边 |
| **每个色块内部完全均匀**（无任何渐变伪影） | LUT 用 Filter=Nearest，禁止插值；hash(layer) 在每个角块内是常量 |
| **12 个 pent 位置仍清晰可见**（如果它们被分配到唯一 layer） | pent 几何特性；如果 pent 与某个 hex 共 layer 也会融合，调高 `NumLayersHint` 即可让 pent 单独成色 |

### 关键验证操作

把 `NumLayersHint` 改成 4 或 8（Actor Details 面板里直接拖）→ **大片相邻 hex 立刻变成同一种颜色**（因为它们的 `hash * 2654435761 mod 8` 重合了），相邻 hex 之间**完全无边界**。这证明：

1. ✅ LUT 路径走通（R 通道被正确读到、Filter=Nearest 没被双线性破坏）
2. ✅ argmax 选出的是真实 CellId（不是退化到 Role 0 / 顶点固定值）
3. ✅ MID 注入成功（材质里的 `CellAttrLUT` 参数确实拿到了动态纹理）
4. ✅ 可以平滑过渡到 R7：把 placeholder LayerIndex 替换为 `WorldGen.FCellGeoData[CellId].TerrainTag.LayerIndex` 即可看到真实地形分布

---

## 5. 排错提示

| 现象 | 原因 / 排查 |
| --- | --- |
| 整个球纯黑 | 1) Material 未指定；2) Shading Model 不是 Unlit；3) Custom 节点输出没连到 Emissive Color |
| 整个球**只有一种颜色**（不论 NumLayersHint 怎么调） | LUT 没被注入：检查材质里参数名是否准确叫 `CellAttrLUT`（C++ 端用的就是这个名字，区分大小写）；检查 Custom 节点的 `LUT` Input 是否连到了 Texture Object Parameter 而不是 Texture Sample |
| 球面颜色看起来"和 R2 一样有 642 种" | LUT.Load 没生效，layer 实际拿到的是 0（未注入）或者哈希被旁路了。打开 Output Log 看 `Rebuilt (R3: ...)` 行的 `LUT=OK` 还是 `LUT=NULL` |
| 颜色严重偏暗 / 偏色 | LUT 的 `SRGB=false` 没生效。检查 [PlanetTopologyDebugMesh.cpp](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp) `RebuildCellAttrLUT_` 中是否设置了 `NewLUT->SRGB = false` |
| hex 内部出现**条纹 / 渐变伪影** | LUT 的 `Filter` 不是 `TF_Nearest`，被双线性采样了。检查 C++ 端 `NewLUT->Filter = TF_Nearest` |
| 同色 hex 之间仍有边界（没有融合） | 颜色 hash 函数对 layer 不稳定 / 包含了 CellId 自身。检查 Code 中 `hash` 的输入是 `layer + 1` 而不是 `chosen + 1` |
| 不同色 hex 之间不是硬边而是渐变 | 用错了路径（走了 §6 附录的 λ 加权混合）。R3 主路径必须保留 `argmax`；如果想要软边请等 R4 |
| 出现可见缝 / Z-fighting | 顶点装填错了，与 R3 着色无关，回去检查 R1/R2 |

材质编辑器左下角 **Stats** 面板会显示 Custom 节点的输入连线问题：

```
Custom material missing input 1 (UV1)
Custom material missing input 4 (LUT)
```

按提示补全连线。

---

## 6. 验收完成的标志

- ✅ 球面被多种色块密铺，每个色块内部颜色完全均匀（**无任何渐变伪影**）
- ✅ 不同色 hex 之间是硬边（与 R2 一致）
- ✅ 同色相邻 hex 之间**没有可见边界**（融合为大区域）→ 这是 R3 与 R2 的关键区别
- ✅ 调小 `NumLayersHint` 看到大片相邻 hex 颜色合并
- ✅ Output Log 中 `LogPlanetTopologyDebugMesh` 输出 `Rebuilt (R3: ...)` 字样且 `LUT=OK`、`NumLayersHint` 与 Details 面板一致

---

## 7. 后续路径预告

| 步骤 | 接续动作 |
| --- | --- |
| **R4** | 把 R3 的 `argmax(λ)` 折线硬边升级为球面 Voronoi `argmax(dot(dir, V_i))` 测地线大圆弧硬边——消除 hex 边在 mesh 边中点处的可见折角（详见 [R4_VoronoiBoundary.md](R4_VoronoiBoundary.md)） |
| **R5** | 在 R4 球面 Voronoi 距离空间加 `Sharpen(δ, EdgeWidth)` 软边：过渡带几何 = 测地线大圆弧两侧的等距弧度带，`EdgeWidth = 0` 退化为 R4（详见 [R5_SharpenSoftEdge.md](R5_SharpenSoftEdge.md)） |
| **R6** | 在 δ 上叠 3D 噪声扰动：海岸线/山脚不规则 |
| **R7** | 把 `hash(layer)` 哈希色换成 `Triplanar(TerrainAlbedoArray, layer, WorldPos, Normal)` 真实地形纹理 |
| **R8** | 接入 WorldGen 的 `FCellGeoData → LayerIndex`，跑出第一张可玩星球；`RebuildCellAttrLUT_` 中的 Knuth 哈希 placeholder 被真实数据替换 |

---

## 附录 A：λ 加权三方混合（**对照参考**，**不用于 R3 验收**）

> ⚠ **本附录仅作为对比/教学保留**。不要在 R3 验收时使用——它会冲销 R2 验证过的 1/3 角块切分（参见 [SphericalSDFTerrainDesign §2.1.2](SphericalSDFTerrainDesign.md#212-视觉错觉防御13-角块的判别准则用于-r2r3-验收) 关于"argmax vs 线性混合"的视觉差异说明）。

如果你想看 SDF 设计稿 §6.2 描述的"3 次 Load + λᵢ 加权"软边混合效果，可以把 §3.3 的 Code 替换为：

```hlsl
// 8-bit 拆分解码（与主路径一致）
int c0 = (int)(UV1.x + 0.5) * 256 + (int)(UV1.y + 0.5);
int c1 = (int)(UV2.x + 0.5) * 256 + (int)(UV2.y + 0.5);
int c2 = (int)(UV0.x + 0.5) * 256 + (int)(UV0.y + 0.5);
float l0 = saturate(UV3.x);
float l1 = saturate(UV3.y);
float l2 = saturate(1.0 - l0 - l1);

float layer0 = LUT.Load(int3(c0, 0, 0)).r * 255.0;
float layer1 = LUT.Load(int3(c1, 0, 0)).r * 255.0;
float layer2 = LUT.Load(int3(c2, 0, 0)).r * 255.0;

float h0 = frac((layer0 + 1) * 0.6180339887);
float h1 = frac((layer1 + 1) * 0.6180339887);
float h2 = frac((layer2 + 1) * 0.6180339887);
float3 col0 = saturate(float3(frac(h0), frac(h0*7.123), frac(h0*13.456)) + 0.25);
float3 col1 = saturate(float3(frac(h1), frac(h1*7.123), frac(h1*13.456)) + 0.25);
float3 col2 = saturate(float3(frac(h2), frac(h2*7.123), frac(h2*13.456)) + 0.25);

// λ 加权混合：每个三角形内部 3 个顶点 Cell 的颜色按重心坐标连续过渡
return col0 * l0 + col1 * l1 + col2 * l2;
```

**视觉效果对比**：

- argmax 路径（R3 主路径）：球面被纯色硬边色块密铺；相邻不同 layer 之间是硬边；同 layer 直接融合
- λ 加权路径（本附录）：每个三角形被填成三色软渐变；hex 中心是该 Cell 颜色，hex 边界变成两 Cell 各 50%；三 Cell 共享 Corner 是 1/3 加权——视觉上"hex/pent"的几何形状被重心插值糊化，**不再是清晰的多边形色块**

R4 阶段会用一个 `Sharpen(λ, edgeWidth)` 函数让两条路径成为同一公式的连续插值：`edgeWidth=0` → argmax 硬边，`edgeWidth=0.5` → λ 加权软边。届时本附录的写法会被吸收为 R4 的一种特例。

## 附录 B：VertexColor 直显（**已废弃**）

R2 阶段曾用过"C++ 端把 LayerColor 写入 VertexColor、材质里 VertexColor → Emissive 两节点直显"的最简路径。**该路径在 R3 不再使用**，原因有二：

1. R3 的核心是验证 GPU 端 `LUT.Load` 走通——VertexColor 直显完全旁路 LUT，无法验证 `NumLayersHint` 调整后颜色融合
2. VertexColor 是**线性插值**路径，与 R3 主路径的 argmax 硬边视觉效果不一致

C++ 端 `VertexColor = CellHashColor[CellId]` 仍然写入（用于 R2 验收已经过了，留作 fallback），但 R3 验收材质应当**忽略 VertexColor 通道**，只走 `Custom` 节点 + LUT 路径。
