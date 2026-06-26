# R3 验收：3 次 Load(CellAttrLUT) + λᵢ 加权混合

> 关联代码：
> - C++：[PlanetTopologyDebugMesh.cpp](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp) `RebuildCellAttrLUT_()`
> - 设计稿：[SphericalSDFTerrainDesign.md §6.2 像素阶段：3 次 Load + 加权混合](SphericalSDFTerrainDesign.md#62-像素阶段3-次-load--加权混合)
>
> 关联前置：[R2_TopologyDebugMaterial.md](R2_TopologyDebugMaterial.md)

> ⚠️ **如果你看到效果不对（比如 hex/pent 之间的边/Corner 附近呈现纯黑），请优先看 §3 "质材搭建步骤"**——本项目推荐的 §3.A 净顶点色方案几乎不可能出错。原本的 Custom HLSL 方案仅作为 R6+ 接入真实地形采样的预热路径。

> 📐 **拓扑前置（极重要）**：球面上**只有一种几何元素**——primal 三角形（即 `FCorner`）。每个三角形的 3 个顶点是 3 个 Cell 中心；每个三角形被外心-边中点连线划分为 3 个 1/3 区域，分别归属其 3 个顶点 Cell 的 hex/pent。**不存在所谓 "hex 内部三角形" 与 "hex 之间的过渡三角形" 之分**——R2 看到的色块密铺只是着色函数选择性点亮 Role 0 角块的视觉假象。R3 把 3 个 1/3 区域都用各自顶点 Cell 的颜色填上，整个 hex/pent 就被自然拼出。详见 [SphericalSDFTerrainDesign §2.1.1](SphericalSDFTerrainDesign.md#211-关键拓扑澄清所有三角形地位完全等价极重要)。

## 1. R3 vs R2 的差异

| 阶段 | PS 着色公式 | 视觉效果 |
| --- | --- | --- |
| **R2** | `albedo = λ₀ × hash(c₀)` | 六边形/五边形色块 + “三角形空隙”的逐三角形渐变密铺（“空隙”是视觉假象，实际仅为未点亮的 Role 1/2 角块，详见 [R2 §4](R2_TopologyDebugMaterial.md#4-实际预览效果已验证)） |
| **R3** ⭐ | `albedo = Σᵢ λᵢ × LayerColor(layerᵢ)`，其中 `layerᵢ = LUT.Load(cᵢ).r * 255` | 所有 primal 三角形被 3 个顶点 Cell 的颜色均匀填满；hex/pent 内部颜色完全均匀；Cell 之间共享边上 50/50 软过渡；三 Cell 共享 Corner 上 1/3 加权；不再存在任何黑色区域 |

R3 引入了三个关键升级：

1. **`CellAttrLUT` 资源**：1×N 的 `R8G8B8A8` 动态纹理，R 通道每像素 = 一个 Cell 的 LayerIndex。**C++ 端在 `Rebuild()` 时构建并通过 MID 传给材质，无需手工绑定**。
2. **三方加权而非单方着色**：每个像素读取 3 次 LUT 拿到 layer₀/layer₁/layer₂，用 (λ₀, λ₁, λ₂) 做凸组合。
3. **均匀 hex 内部**：Cell A 中心处某个 λ_role = 1，且对应 cellId = A → 着 LayerColor(A) 的色；周围 6 个三角形里 A 担任不同 Role 但**始终对应同一个 layer A**——所以 hex 内部 6 个 1/3 区域拼接后颜色完全均匀。

## 2. C++ 端已完成的工作（无需手动操作）

`APlanetTopologyDebugMesh::Rebuild()` 现在会自动：

1. 调用 `RebuildCellAttrLUT_(NumCells)` 创建 `1×NumCells` 的 `PF_B8G8R8A8` `UTexture2D`，名为 `CellAttrLUT_Transient`
2. 用 Knuth 整数哈希 `LayerIndex = (CellId × 2654435761u) % NumLayersHint` 填充 R 通道
3. 包装外部 `Material` 为 `UMaterialInstanceDynamic`（MID）
4. 调用 `MID->SetTextureParameterValue("CellAttrLUT", LUT)`
5. 把 MID 设为 PMC 的 Section 0 材质

调试参数：

| 属性 | 默认 | 作用 |
| --- | --- | --- |
| `NumLayersHint` | 32 | 调小（如 8）→ 更多相邻 Cell 共用同一 LayerIndex → 看到"邻居 hex 颜色相同导致边界融合"的效果，更接近真实地形分布 |

## 3. 材质资产搭建步骤

本项目提供两条路径：

| 路径 | 适用阶段 | 优势 | 劣势 |
| --- | --- | --- | --- |
| **§3.A 顶点色净路径（推荐）** | R3 验收 / 开发调试阶段 | 两个节点就完成，几乎不可能出错；光栅化器自动重心插值 | 颜色是 C++ 端预烘的，GPU 里不能进一步采样真实地形纹理 |
| **§3.B Custom HLSL 路径** | R6+ 期需要在 GPU 里 Triplanar 采样时 | 可以拓展到真实地形纹理采样 | 很多连线点，人工实现容易出错 |

### 3.A 顶点色净路径（推荐 - 2 个节点就验收通过）

**原理**：C++ 端在 Rebuild 时已经为每个顶点写入了 `VertexColor = LayerColor[该顶点对应的 Cell]`。三角形的 3 个顶点分别是 3 个 Cell 的 LayerColor；光栅化器自动按重心坐标插值 → 插值结果就是 `Σ λᵢ × LayerColorᵢ`，与 R3 三方混合公式完全等价。**不需要任何 HLSL、不需要 LUT 参数、不需要 UV1/UV2/UV3**。

**步骤**：

1. 新建材质 `M_TopologyDebug_R3`，Shading Model = `Unlit`
2. 拖一个 `VertexColor` 节点，取 RGB（Component Mask R、G、B 拼出一个 Float3；或者直接用 RGB 输出引脚，本身就是 Float3）
3. 连到 **Emissive Color**
4. 编译保存。
5. 把该材质赋给 `APlanetTopologyDebugMesh` 的 Material 属性。

**就这么多**。C++ 端 Rebuild 时会自动按 `NumLayersHint` 重新计算 LayerColor 并赋给顶点 → 立即看到效果。

**预期看到**：
- 每个 hex/pent 内部颜色完全均匀
- 相邻两个 hex 共享边上采取两色 50/50 软过渡（**不是黑色**）
- 三 hex 共享的 Corner 位置（三角形重心）是三色 1/3 加权混合（中等亮度，避免了黑色问题因为 C++ 端 LayerColor 计算中加了 0.25 的最低亮度保护）
- 12 个五边形位置清晰可见
- 调小 `NumLayersHint` 能看到大片相邻 hex 颜色合并

### 3.B Custom HLSL + LUT 路径（为 R6+ Triplanar 预热，当前阶段可跳过）

这是原始 R3 设计。当前阶段与 §3.A 效果完全一致，但材质搭建起来复杂很多。本路径仅在 R6+ 需要在 GPU 里采样真实地形纹理时才需要使用。

#### 3.B.1 新建材质 `M_TopologyDebug_R3_LUT`

1. Content Browser → Material → 命名 `M_TopologyDebug_R3`
2. 打开材质编辑器；根 MaterialNode 设置：
   - **Shading Model** = `Unlit`
3. 把 R3 用到的两个参数加进材质：
   - **Texture Object Parameter**（注意是 Texture **Object**，不是 Texture Sample）→ 改名 `CellAttrLUT`，**Sampler Type** 设 `Linear Color`（R 通道存原始数据，不能被 sRGB 解码）
   - 不需要再加任何 Scalar/Vector 参数

#### 3.B.2 新建材质（同上）

#### 3.B.3 贴 Custom 节点

添加 **Custom** 节点，配置：

- **Output Type**：`CMOT Float3`
- **Inputs**（依次添加）：
  1. `UV1` (Float2) ← 来自 `TextureCoordinate(CoordinateIndex=1)`
  2. `UV2x` (Float1) ← `TextureCoordinate(CoordinateIndex=2)` 的 R 分量（Component Mask R）
  3. `UV3` (Float2) ← 来自 `TextureCoordinate(CoordinateIndex=3)`
  4. `LUT` (Texture2D) ← 直接从 Custom 节点的 Inputs 添加 `Texture2D` 类型；连线到 `CellAttrLUT` Texture Object 参数

- **Code**（本节代码与“调试与故障排查”中的调试版互为备选）：

```hlsl
// 还原三个 CellId（三顶点写同样值，插值后 round 即可）
int c0 = (int)(UV1.x + 0.5);
int c1 = (int)(UV1.y + 0.5);
int c2 = (int)(UV2x  + 0.5);

// 还原三方权重 / 重心坐标
float l0 = saturate(UV3.x);
float l1 = saturate(UV3.y);
float l2 = saturate(1.0 - l0 - l1);

// 3 次 Load(CellAttrLUT)：取每个 Cell 的 LayerIndex
// LUT 格式 PF_B8G8R8A8、SRGB=false、Filter=Nearest，
// shader 里 .r 直接返回 [0, 1] 归一化的 R 通道值（即 LayerIndex / 255）
float layer0 = LUT.Load(int3(c0, 0, 0)).r * 255.0;
float layer1 = LUT.Load(int3(c1, 0, 0)).r * 255.0;
float layer2 = LUT.Load(int3(c2, 0, 0)).r * 255.0;

// 把 LayerIndex 哈希成颜色（R3 placeholder：R7+ 会换成 Texture2DArray 真实地形采样）
// 重要：+ 1.0 使 layer = 0 不会被 hash 成黑色。sin(0)=0 → frac(0)=0 → 黑色 【bug 修复】
float h0 = frac(sin((layer0 + 1.0) * 12.9898) * 43758.5453);
float h1 = frac(sin((layer1 + 1.0) * 12.9898) * 43758.5453);
float h2 = frac(sin((layer2 + 1.0) * 12.9898) * 43758.5453);

// 让颜色饱和最低为 0.4，避免所有颜色三分量同时接近 0
float3 col0 = saturate(float3(frac(h0 * 1.0), frac(h0 * 7.123), frac(h0 * 13.456)) + 0.2);
float3 col1 = saturate(float3(frac(h1 * 1.0), frac(h1 * 7.123), frac(h1 * 13.456)) + 0.2);
float3 col2 = saturate(float3(frac(h2 * 1.0), frac(h2 * 7.123), frac(h2 * 13.456)) + 0.2);

// λ 加权混合（这就是 SDF 设计稿 §6.2 的核心公式）
return col0 * l0 + col1 * l1 + col2 * l2;
```

- 输出 → Material 的 **Emissive Color**

> ⚠໻8️ **Custom 节点 Inputs 顺序必须与上表一致**。UE 会按 Inputs 顺序生成 HLSL 函数签名，顺序错误会导致参数错位。连线时请仔细检查：
> - `UV1` 输入连 `TextureCoordinate` (Index=1) 的输出
> - `UV2x` 输入连 `TextureCoordinate` (Index=2) 输出的 **ComponentMask R** 节点
> - `UV3` 输入连 `TextureCoordinate` (Index=3) 的输出
> - `LUT` 输入连 **Texture Object Parameter** (名为 `CellAttrLUT`) 的输出

#### 3.B.4 把 LUT 参数连到 Custom 节点

材质编辑器中：

1. 拖出一个 **Texture Object Parameter** 节点，命名 `CellAttrLUT`
2. 把它连到 Custom 节点的 `LUT` Input

> **不要**用 Texture Sample 节点——Texture Sample 内部固定走 SamplerState，会自动选 Wrap/Linear，破坏 LUT 的 Nearest/Clamp 语义。**必须用 Texture Object** + `Texture2D.Load(int3)` 这种"裸索引"路径。

> Custom 节点的 Texture Object 输入会自动用一个**默认 SamplerState** 被绑到 GPU；但因为我们用 `.Load(int3)` 而不是 `.Sample(samp, uv)`，**采样器实际上不参与寻址**——所以 LUT 的 Filter/Address 设置完全由 C++ 端 UTexture2D 的属性控制，与材质的 Sampler Type 无关。

#### 3.B.5 把材质指给 Actor

1. 编辑器场景里选中 `APlanetTopologyDebugMesh` 实例
2. Details → Material = `M_TopologyDebug_R3`
3. **OnConstruction 自动触发 Rebuild** → C++ 端创建 LUT 并通过 MID 注入材质 → 立即看效果

## 4. 实际预览预期效果

| 现象 | 含义 | R2 对比 |
| --- | --- | --- |
| **所有 primal 三角形都被三顶点 Cell 的颜色填满，hex/pent 内部颜色完全均匀** | LUT.Load 还原 layer 正确；λ 加权混合在 Cell 中心 = (1,0,0) 时纯色；周围三角形同时贡献同一个 layer A 的 1/3 区域 | R2 只点亮中心 1/3 区域、剩下 2/3 是暗的 → R3 是均匀的整 Cell 色块 |
| **没有任何黑色区域** | 所有三角形都参与混合，三个顶点 Cell 的颜色都以正顶点权重价贡献 | R2 调不亮的那 2/3 输免为黑三角状 |
| **hex 共享边上呈现两 layer 的 50/50 混合** | λ_A = λ_B = 0.5 时混合一半一半 | R2 没有这个效果 |
| **Corner（hex 三角点）呈现三 layer 的 1/3 加权** | (λ₀,λ₁,λ₂) = (1/3,1/3,1/3) | R2 没有 |
| **`NumLayersHint` 改小后看到相邻 hex 颜色相同时无可见边界** | 验证 layer 哈希驱动而非 CellId 哈希 | R2 是直接用 CellId 哈希，每 Cell 必然不同色 |

> **重要验证点**：把 `NumLayersHint` 改成 4 或 8——你应该看到大片相邻 hex **变成同一种颜色**（因为它们的 layer 哈希 mod 8 重合了），相邻 hex 之间**完全无边界**。这证明 R3 的"按 LayerIndex 着色"路径走通了，为 R7（接入 WorldGen 真实地形分布）打好基础。

## 5. 排错提示

| 现象 | 原因 / 排查 |
| --- | --- |
| 整个球纯黑 | 1) Material 未指定；2) Shading Model 不是 Unlit；3) 节点没连到 Emissive |
| 球面所有 hex 都是同一颜色 | `CellAttrLUT` 参数没绑定成功 → Custom 节点的 LUT Input 没连到 Texture Object Parameter；或者材质里参数名不叫 `CellAttrLUT`（C++ 端用的是这个名字） |
| Cell 内部不再均匀，退化成 R2 那种“周围未点亮”效果 | `c0/c1/c2` round 出错；检查 Custom 节点里是否做了 `+ 0.5` |
| **预期看到的"hex 内部均匀"** | 根本原因：HLSL 实际执行的不是三方混合 `Σ λᵢ·colᵢ`，而是退化为单方 `λ₀·col₀`（这是 R2 公式，只点亮每个三角形的 Role 0 角块）。可能原因：<br>1) Custom 节点的 Inputs 顺序与 Code 中名字顺序对不上（上节 ⚠ 红警阁中的顺序是必须的）<br>2) Code 多行复制时丢了重要行<br>3) 某个 Input 没连上 → Custom 节点会报 `Custom material missing input` 错误。检查材质编辑器左下角 Stats 面板。<br>**诊断步骤见 §7** | hex/pent 内部均匀，但 hex 之间的边 / Corner 区域出现黑色 |
| 看到非常奇怪的渐变色（条纹/锯齿）| LUT 的 Filter 不是 Nearest，被双线性插值了。检查 C++ 端 `CellAttrLUT->Filter = TF_Nearest`，或者材质里用了 Texture Sample 而不是 Texture Object + Load |
| 颜色严重偏暗 | LUT 的 SRGB 没关 → R 通道被当颜色做了 sRGB→Linear 解码。检查 C++ 端 `CellAttrLUT->SRGB = false` |
| 边界处有可见缝 / Z-fighting | 顶点装填错了，几何位置不连续——这与 R3 着色无关，回去检查 R1 |

## 6. 验收完成的标志

- ✅ 球面被多种色块（≤ NumLayersHint 种）密铺，每个色块内部颜色完全均匀
- ✅ 相邻不同色 hex 的共享边上呈现两色 50/50 软过渡
- ✅ 三 hex 共享 Corner 处呈现三色 1/3 软过渡
- ✅ 12 个五边形位置仍清晰可见
- ✅ 调小 `NumLayersHint` 能看到大片相邻 hex 颜色合并
- ✅ Output Log 中 `LogPlanetTopologyDebugMesh` 输出 `Rebuilt (R3: ...)` 字样

## 7. 调试与故障排查（有效验证 R3 全链路是否走通）

如果你看到的效果与预期不符（最常见的不符：**hex/pent 内部均匀，但 hex 之间的边 / Corner 区域出现纯黑色**），说明 R3 三方混合公式没被正确执行（退化成了 R2 的“只点亮 Role 0 角块”状态）。请按下面顺序诊断。

### 7.1 步骤 A：验证 λ 三一可见且加和=1

新建一个调试材质 `M_TopologyDebug_R3_DebugLambda`：
- Shading Model = Unlit
- Custom 节点的 **Output Type = CMOT Float3**
- **Inputs 只加一项**：`UV3` (Float2)
- **Code**（只这 3 行）：
  ```hlsl
  float l0 = saturate(UV3.x);
  float l1 = saturate(UV3.y);
  return float3(l0, l1, saturate(1.0 - l0 - l1));
  ```
- 输出 → Emissive Color
- `UV3` 输入连 `TextureCoordinate(CoordinateIndex=3)`

**预期看到**：球面面貌是 红-绿-蓝 三色渐变。每个三角形都会被渲染，三个顶点处分别是纯红/绿/蓝，重心处是发白的黄。任何地方都不会是黑色（因为 l0+l1+l2 ≡ 1）。

- ✅ **如果看到 RGB 三色渐变**，且没有黑色 → PMC 的 UV3 通道传过来了，重心坐标正确。跳到 7.2。
- ❌ **如果看到纯黑 / 纯红 / 不是预期的 RGB 渐变** → UV3 通道被丢失，R3 达不到上游。检查：
  - C++ 端是否调用了 `CreateMeshSection_LinearColor` 的 4-UV 重载（本项目已调）
  - PMC 版本是否为 UE 5.x（内部固定 `InitFromDynamicVertex(..., 4)`）
  - 是否使用了 NaniteOverride / Nanite 路径（PMC 不走 Nanite，理论上不会，但需仔细检查实例设置）

### 7.2 步骤 B：验证 LUT.Load 走通

复制上面的材质起名 `M_TopologyDebug_R3_DebugLayer`。修改：
- Custom 节点 Inputs 加三项：
  1. `UV1` (Float2) ← `TextureCoordinate(Index=1)`
  2. `UV2x` (Float1) ← `TextureCoordinate(Index=2)` 过 ComponentMask R
  3. `LUT` (Texture2D) ← `CellAttrLUT` Texture Object Parameter
- **Code** 换为：
  ```hlsl
  int c0 = (int)(UV1.x + 0.5);
  int c1 = (int)(UV1.y + 0.5);
  int c2 = (int)(UV2x  + 0.5);
  float l0 = saturate(UV3.x);
  float l1 = saturate(UV3.y);
  float l2 = saturate(1.0 - l0 - l1);
  // 把 3 个 layer 值（0-31）除以 32 归一化，当颜色输出
  float layer0 = LUT.Load(int3(c0, 0, 0)).r * 255.0 / 32.0;
  float layer1 = LUT.Load(int3(c1, 0, 0)).r * 255.0 / 32.0;
  float layer2 = LUT.Load(int3(c2, 0, 0)).r * 255.0 / 32.0;
  return float3(layer0, layer1, layer2);
  ```

**预期看到**：球面上每个 hex 内部均匀添加了三个颜色分量（分别来自三个邻居 hex 的 layer 值），**最低亮度为三个 layer 都是 0 的特例**。

- ✅ **如果 hex 内部均匀五颜六色 + 三角形里三个顶点 Cell 的颜色都可见的混合色**（不再是黑色）→ 三方混合走通。然后回到 §3.2 贴完整 Code。
- ❌ **如果 hex 之间仍是黑色**，但 hex 内部均匀 → 这验证了纯颜色输出下过渡区域（几何上还是同一个三角形）**本身被错误地填为黑色**。根本原因：HLSL 代码中只走了 `col0 * l0`，没走 `+ col1*l1 + col2*l2` 。重新检查你贴的 Code，确保最后一行是 三项加 。

### 7.3 步骤 C：检查最后一行的加号

Custom 节点中多行 HLSL 代码多次复制粘贴后，可能出现：

```hlsl
// 错误示例（最后一行丢了，退化成 R2 公式）
return col0 * l0;   // ← 需要还是 col0*l0 + col1*l1 + col2*l2
```

这个错误会精准复现“hex/pent 内部均匀（因为 6 个相邻三角形的 Role 0 角块拼成了中央均匀区） + hex 之间的边 / Corner 区域黑色（因为那里是其它三角形的“非 Role 0 角块”，被量为 0 的 λ₀ 乘黑）”的现象。检查你的 Code 最后一行：

```hlsl
return col0 * l0 + col1 * l1 + col2 * l2;   // ✅ 正确
```

### 7.4 步骤 D：查看材质编辑器 Stats / Errors

材质编辑器左下角有个 Stats 面板（查看菜单 → Stats）。任何 Custom 节点的 Input 未连接，这里会显示：

```
Custom material  missing input 1 (UV1)
```

按提示补全连线。

R3 验收通过后，后续路径预告：

| 步骤 | 接续动作 |
| --- | --- |
| **R4** | 加 `Sharpen(λ)` 软边控制：从柔软（默认）到硬边（Civ 6 风格）的连续切换 |
| **R5** | 加 3D 噪声扰动 λ：海岸线变成蜿蜒、山脚变成参差不齐 |
| **R6** | 把 `hash(layer)` 哈希色换成 `Triplanar(TerrainAlbedoArray, layer, WorldPos, Normal)` 真实地形纹理 |
| **R7** | 接入 WorldGen 的 `FCellGeoData → LayerIndex`，跑出第一张可玩星球 |
