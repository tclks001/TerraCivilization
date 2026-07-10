# R2 验收：UV1/UV2/UV3 顶点属性 + PS 端 argmax(λ) 硬边着色

> 关联代码：[PlanetTopologyDebugMesh.cpp](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp)
>
> 关联设计稿：
> - [SphericalSDFTerrainDesign §2.1.1 关键拓扑澄清](SphericalSDFTerrainDesign.md#211-关键拓扑澄清所有三角形地位完全等价极重要)
> - [SphericalSDFTerrainDesign §2.1.2 视觉错觉防御](SphericalSDFTerrainDesign.md#212-视觉错觉防御13-角块的判别准则用于-r2r3-验收)
> - [SphericalSDFTerrainDesign §14.7 球面重心坐标与外心修正](SphericalSDFTerrainDesign.md#147-球面重心坐标与外心折角修正核心几何不变量)

## 0. 拓扑前置（看材质前必读）

整个测地线球面 mesh 由且仅由一种几何元素铺成 —— **primal 三角形（=`FCorner`）**：

- **mesh 顶点 = `FSphereTopology::Cells`**（sub=3 时 642 个）；每个 Cell 同时是一个 hex / pent 的逻辑中心。
- **mesh 三角形 = `FSphereTopology::Corners`**（sub=3 时 1280 个）；每个 Corner 由 3 个 Cell 顶点张成，`Corner.UnitDir` 是这三个 Cell 中心的重心归一化（即三角形外心方向）。
- `FSphereTopology::Tris` 已废弃，本阶段不使用。

**五/六边形不存在于 mesh 几何上**——它们是 **5 或 6 个 primal 三角形各贡献 1/3 角块拼成**的逻辑面：

- 每个 primal 三角形被它的"外心-边中点连线"切成 **3 个 1/3 角块**，分别归属其 3 个顶点 Cell。
- 一个 hex Cell 范围 = 它周围 6 个三角形各取 1 个 1/3 角块的并集；pent 是 5 × 1/3。
- **每个三角形的 3 个 1/3 角块属于 3 个不同的 hex/pent**——三角形并非任何 hex/pent 的"专属内部"。
- 不存在所谓"hex/pent 内部三角形" vs "hex 之间过渡三角形"两类——所有三角形在拓扑上**完全等价**。

R2 视觉验收的核心目标 = **让 5/6 个 1/3 角块的拼合关系肉眼可见**，即**每个 Cell 在视觉上是一个清晰的 hex/pent 多边形**，三角形几何边界与 hex/pent 边界**完全分离**。这需要在 PS 端做 **argmax(λ) 硬边着色**——线性插值的 VertexColor 直显路径**做不到这个效果**，因为重心坐标平滑渐变会让 1/3 角块的边界在视觉上消失，反而聚拢成"5/6 个完整三角形围中心顶点"的错觉。

---

## 1. 顶点属性约定（C++ 端已完成）

`APlanetTopologyDebugMesh::Rebuild()` 装填的每个三角形 3 个独立顶点，UV 通道写入：

| 通道 | 内容 | Role 0 顶点 | Role 1 顶点 | Role 2 顶点 |
| --- | --- | --- | --- | --- |
| **UV0** | 预留细节纹理 fallback | (0, 0) | (0, 0) | (0, 0) |
| **UV1.xy** | (TriCellId₀, TriCellId₁) | (c₀, c₁) | (c₀, c₁) | (c₀, c₁) |
| **UV2.xy** | (TriCellId₂, _) | (c₂, 0) | (c₂, 0) | (c₂, 0) |
| **UV3.xy** | OneHot (λ₀, λ₁) | (1, 0) | (0, 1) | (0, 0) |
| **VertexColor** | 该顶点对应 Cell 的 hash 色（备用） | hash(c₀) | hash(c₁) | hash(c₂) |

> **关键性质**：UV1/UV2 的"三个顶点写同样值"在光栅化器线性插值后**仍是同一个值**——PS 中 `round()` 还原即可拿到精确 int CellId。UV3 的 OneHot 经过插值得到 `(λ₀, λ₁)`，第三个权重用 `λ₂ = saturate(1 − λ₀ − λ₁)`。VertexColor 在三个顶点不同（hash 色），仅作 fallback 路径用，**主验收路径不读它**。

---

## 2. 材质资产搭建步骤（argmax 硬边路径）

### 2.1 新建材质

1. Content Browser → 任意目录 → 右键 → Material → 命名 `M_TopologyDebug_R2`
2. 双击打开材质编辑器
3. 选中根 MaterialNode，Details 面板：
   - **Shading Model** = `Unlit`
   - 其它保持默认

### 2.2 用 Custom HLSL 节点一气呵成（推荐）

新建一个 `Custom` 节点，配置：

- **Output Type** = `CMOT Float3`
- **Inputs**（依次添加，顺序必须与 Code 中的形参一致）：
  1. `UV0` (Float2) ← `TextureCoordinate(Index=0)`
  2. `UV1` (Float2) ← `TextureCoordinate(Index=1)`
  3. `UV2` (Float2) ← `TextureCoordinate(Index=2)`
  4. `UV3` (Float2) ← `TextureCoordinate(Index=3)`
- **Code**：

```hlsl
// 1) 8-bit 拆分解码 3 个 CellId（避开 fp16 精度问题）
//    cpp 端写入：UV0=(HiC,LoC)、UV1=(HiA,LoA)、UV2=(HiB,LoB)
//    解码：CellId = round(Hi)*256 + round(Lo)
int c0 = (int)(UV1.x + 0.5) * 256 + (int)(UV1.y + 0.5);
int c1 = (int)(UV2.x + 0.5) * 256 + (int)(UV2.y + 0.5);
int c2 = (int)(UV0.x + 0.5) * 256 + (int)(UV0.y + 0.5);

// 2) 还原重心坐标 (λ₀, λ₁, λ₂)，硬件免费给我们的"三方权重"
float l0 = saturate(UV3.x);
float l1 = saturate(UV3.y);
float l2 = saturate(1.0 - l0 - l1);

// 3) argmax(λ) 选 CellId —— 这一步把三角形清晰切成 3 个 1/3 角块
int chosen = (l0 >= l1 && l0 >= l2) ? c0 : ((l1 >= l2) ? c1 : c2);

// 4) hash(chosen) → 颜色。+1 防止 chosen=0 时 sin(0)=0 → 黑色 bug；
//    黄金分割 (0.618) 让相邻 CellId 的色调相隔最远，肉眼分辨率最大化；
//    +0.25 偏置避免颜色三分量同时接近 0
float hue = frac((chosen + 1) * 0.6180339887498949);
float3 col = float3(
    frac(hue * 1.000),
    frac(hue * 7.123),
    frac(hue * 13.456)
);
return saturate(col + 0.25);
```

- **输出** → Material 的 **Emissive Color**

### 2.3 为什么选 Unlit + Emissive

- R2 验证目的：让你**一眼看出 hex/pent 形状**和**1/3 角块的清晰边界**，光照会干扰判读
- Emissive 直接把数值搬到屏幕，所见即所得

---

## 3. 把材质挂到 Actor 上

1. 拖 `BP_PlanetTopologyDebugMesh`（或基类 `APlanetTopologyDebugMesh`）到关卡（R1 阶段已经摆好的实例直接复用即可）
2. Details 面板：
   - **Material** = 刚才创建的 `M_TopologyDebug_R2`
   - **Subdivision Level** = 3（保持默认）
   - **Radius** = 15000（保持默认）
3. 编辑器视口立即看到效果（OnConstruction 自动 Rebuild）

---

## 4. 实际预览预期效果

**球面被 642 个 hex/pent 纯色多边形完整密铺**：

| 视觉元素 | 出现位置 | 颜色行为 |
| --- | --- | --- |
| **六边形纯色多边形** | 围绕每一个非 pentagon 的 mesh 顶点（共 630 个） | 内部完全纯色 = `hash(中心 Cell 的 CellId + 1)` |
| **五边形纯色多边形** | 围绕 12 个 pentagon mesh 顶点（原正二十面体顶点位置） | 内部完全纯色 |
| **hex/pent 之间的边界** | 两个相邻 Cell 的"分水线" = primal 三角形里两 λ 相等的轨迹 | 颜色突变（**没有渐变**），边界数学精确 |
| **3 hex/pent 共角点** | 每个 primal 三角形的外心 = `Corner.UnitDir` 在球面上的投影 | 三色"Y 字形"汇合点 |

**验收必看的几何事实**：

1. **三角形几何边界与 hex/pent 边界完全分离** —— 你将看到一些 hex 的边横跨某个三角形内部（穿过"外心 → 边中点"那条 1/3 角块分界线），而某些三角形内部呈现 3 种颜色（3 个 1/3 角块各占 1 色）；这正是"3 邻接 hex/pent 共享同一个三角形"的视觉证据。
2. **不存在"过渡三角形" / 不存在"5/6 个三角形被 hex 占用"的图案** —— 所有三角形完全等价，区分仅在于 3 个顶点 Cell 的 hash 色不同。
3. **每个 hex/pent 边数恰好** = 该中心 Cell 的 `NeighborCellIds` 数 = 5 或 6 —— 12 个 pent 在原正二十面体的 12 个顶点位置。
4. **hex/pent 边的中点** = primal 三角形 "Cell A-Cell B" 那条边的中点 = `M_AB`（由 [§14.7](SphericalSDFTerrainDesign.md#147-球面重心坐标与外心折角修正核心几何不变量) 推导）。

### 4.1 R2 验收结论检查表

| 待验证的属性 | 检查方法 | 通过标志 |
| --- | --- | --- |
| UV1/UV2 三个顶点写同样值 → 插值后 `round` 还原 int CellId 精确 | 任一 hex/pent 内部颜色完全均匀（无离散噪点） | ✅ |
| UV3 OneHot 三顶点差异化 → 插值后重心坐标正确 | hex/pent 边界恰好出现在外心-边中点连线上（沿三角形对称） | ✅ |
| `argmax(λ)` 选 Cell 路径走通 | hex/pent 之间是**硬边**而非渐变 | ✅ |
| 拓扑层正确（hex/pent 数量与位置） | 数得出 12 个五边形且分布于二十面体顶点 | ✅ |
| 重心坐标的几何性质（顶点 = 1、外心 = 1/3、边中点 = 1/2 / 1/2 / 0） | hex 角点处 3 色 Y 字形汇合、hex 边中点处 2 色对接 | ✅ |

---

## 5. 排错提示

| 现象 | 原因 / 排查 |
| --- | --- |
| 整个球纯黑 | 1) Material 未指定；2) Shading Model 不是 Unlit；3) 节点没连到 Emissive Color |
| 整个球纯白 / 单色 | Custom 节点 Inputs 顺序与 Code 形参不一致；UV1/UV2/UV3 没正确连到对应的 `TextureCoordinate(Index=1/2/3)` |
| **看到 5/6 个完整三角形围 hex + 过渡三角形**（即用户图片中的现象） | argmax 没走通——可能 Custom 节点的 Code 没有 `(l0 >= l1 && l0 >= l2) ? c0 : ...` 这段；或者 PS 实际拿到的 λ 全等（OneHot 没传过来） → 退化为线性插值的 VertexColor 直显路径，必然出现此错觉。**重新检查 Code 第 9~10 行的 argmax 选择**。 |
| 同一个 hex 内部颜色离散乱跳 | UV1.x（c₀）三个顶点没写同样值；或者 PS 没做 `+ 0.5` round，导致插值中间值被当 CellId |
| hex/pent 之间不是硬边而是渐变 | argmax 退化（同上）；或不小心用了"VertexColor → Emissive"的捷径路径——后者必然是渐变（线性插值），不是 R2 主验收路径 |
| 12 个 pentagon 看不出来 | hash 函数把相邻 Cell 哈希到了相近色——可调高 `+0.25` 偏置或换其他黄金分割种子 |
| 边界处有可见缝 / Z-fighting | 顶点装填错了，几何位置不连续——这与 R2 着色无关，回去检查 R1 |

---

## 6. 与 R3 的关系

R2 的 argmax 硬边视觉，等价于"每个像素显示其所属 Cell 的颜色"——这正是 **完整 SDF 的最终视觉效果**（最大权重 Cell 占主导）。

R3 引入的升级是把 hash(CellId) **替换为 `LayerColor[LUT.Load(CellId).r]`**：从"每 Cell 一种颜色"过渡到"每 Cell 一个 LayerIndex、相同 LayerIndex 共享颜色"，便于看到地形分布合并的视觉效果。argmax 硬边路径在 R3 仍可保留（叫"硬边渲染模式"），R3 的额外目标是验证 LUT 的 Load 路径走通；之后 R4 加 sharpening/softness 控制；R5 加噪声扰动；R6 接 Triplanar 真实地形纹理。

---

## 7. 可选：Fallback "VertexColor 直显" 路径（仅参考）

C++ 端的 `VertexColor = hash(对应 Cell 的 CellId)` 主要是给 fallback 路径准备的。如果你想**不用 Custom HLSL** 也能看到 hex/pent 大致结构，可以临时建一个 `M_TopologyDebug_R2_VColor`：

- Shading Model = Unlit
- `VertexColor` 节点 → Emissive Color

但**这条路径不是 R2 主验收**——它会得到平滑渐变的视觉效果（线性插值），让"5/6 个三角形围中心顶点"看起来像"hex 占完整 5/6 三角形"（即用户多次纠正过的视觉错觉）。**主验收必须用 §2.2 的 argmax 硬边路径**。
