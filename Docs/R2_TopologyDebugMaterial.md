# R2 验收：UV1/UV2/UV3 顶点属性 + λ₀ 按 CellId 哈希着色

> 关联代码：[PlanetTopologyDebugMesh.cpp](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp)
>
> 关联设计稿：[SphericalSDFTerrainDesign.md §4.3 方案 A](SphericalSDFTerrainDesign.md#43-顶点属性) + [§14.7 球面重心坐标](SphericalSDFTerrainDesign.md#147-球面重心坐标与外心折角修正核心几何不变量)

## 1. 顶点属性约定（C++ 端已完成）

`APlanetTopologyDebugMesh::Rebuild()` 装填的每个三角形 3 个独立顶点，UV 通道写入：

| 通道 | 内容 | Role 0 顶点 | Role 1 顶点 | Role 2 顶点 |
| --- | --- | --- | --- | --- |
| **UV0** | 预留细节纹理 fallback | (0, 0) | (0, 0) | (0, 0) |
| **UV1.xy** | (TriCellId₀, TriCellId₁) | (c₀, c₁) | (c₀, c₁) | (c₀, c₁) |
| **UV2.xy** | (TriCellId₂, _) | (c₂, 0) | (c₂, 0) | (c₂, 0) |
| **UV3.xy** | OneHot (λ₀, λ₁) | (1, 0) | (0, 1) | (0, 0) |

> **关键性质**：UV1/UV2 的"三个顶点写同样值"在光栅化器线性插值后**仍是同一个值**——PS 中 `round()` 还原即可拿到精确 int CellId。UV3 的 OneHot 经过插值得到 `(λ₀, λ₁)`，第三个权重用 `λ₂ = saturate(1 − λ₀ − λ₁)`。

## 2. 材质资产搭建步骤

### 2.1 新建材质

1. 在 Content Browser → 任意目录 → 右键 → Material → 命名 `M_TopologyDebug_R2`
2. 双击打开材质编辑器
3. 选中根 MaterialNode，Details 面板：
   - **Shading Model** = `Unlit`（R2 只验证拓扑，不需要光照）
   - 其它保持默认

### 2.2 节点连线（最小验证版本）

按从输入到输出的顺序，依次添加并连接：

```
TextureCoordinate (Coordinate Index = 3)
        │
        ▼  (R, G) = (λ₀, λ₁)
   [BreakFloat2]
        │  R = λ₀ ┐
                  │
                  ├──┐  这里把 λ₀ 单独取出来作为后续输出权重
                  │  │
TextureCoordinate (Coordinate Index = 1)        ← 取 (c₀, c₁)
        │
        ▼
   [BreakFloat2]
        │  R = c₀（float 形式）
        ▼
   [Round]                                       ← 还原成 int 形式
        │
        ▼
   ScalarParameter "ColorHashFreq" = 12.9898    ← 哈希常数
        │
        ▼
   [Multiply: c₀_rounded × ColorHashFreq]
        │
        ▼
   [Sine]    输出 ∈ [-1, 1]
        │
        ▼
   ScalarParameter "ColorHashScale" = 43758.5453
        │
        ▼
   [Multiply: Sine_out × ColorHashScale]
        │
        ▼
   [Frac]    输出 ∈ [0, 1)，作为基础哈希值 h
        │
        ├──→ R 通道：[Multiply h × 1.0]   → [Frac]
        ├──→ G 通道：[Multiply h × 7.123] → [Frac]
        └──→ B 通道：[Multiply h × 13.456]→ [Frac]
                          ↓
                  [MakeFloat3] = HashColor (RGB)
                          │
                          ▼
                  [Multiply HashColor × λ₀]      ← R2 step 要求"用 λ₀ 按 CellId 哈希填色"
                          │
                          ▼
                Material Output: Emissive Color
```

### 2.3 简化做法（推荐，更短的图）

如果嫌上面节点多，用一个 `Custom` HLSL 节点一气呵成：

1. 添加 Custom 节点
2. **Output Type** = `CMOT Float3`
3. **Inputs**：
   - `c0` (Float1)
   - `lambda0` (Float1)
4. **Code**：
   ```hlsl
   float h = frac(sin(c0 * 12.9898) * 43758.5453);
   float3 color = float3(
       frac(h * 1.0),
       frac(h * 7.123),
       frac(h * 13.456)
   );
   return color * lambda0;
   ```
5. 连线：
   - `c0`：从 `TextureCoordinate(Index=1)` 取出 R 分量再 `Round`
   - `lambda0`：从 `TextureCoordinate(Index=3)` 取出 R 分量
   - 输出 → Material 的 **Emissive Color**

### 2.4 为什么选 Unlit + Emissive

- R2 验证目的：让你**一眼看出 hex 形状**和**重心坐标 λ 是否正确**，光照会干扰判读
- Emissive 直接把数值搬到屏幕，所见即所得

## 3. 把材质挂到 Actor 上

1. 拖 `BP_PlanetTopologyDebugMesh`（或基类 `APlanetTopologyDebugMesh`）到关卡（其实 R1 step 已经摆好的实例直接用即可）
2. Details 面板：
   - **Material** = 刚才创建的 `M_TopologyDebug_R2`
   - **Subdivision Level** = 3（保持默认）
   - **Radius** = 15000（保持默认）
3. 编辑器视口立即看到效果（OnConstruction 自动 Rebuild）

## 4. 实际预览效果（已验证）

无需 PIE，编辑器视口直接可见。**球面是单一的 primal 三角形铺面**（每个 `FCorner` 一个三角形）；着色公式 `λ₀ × hash(c₀)` 在每个三角形里只**点亮** 1/3 角块——视觉上看起来像"hex/pent 色块 + 周围三角形空隙的密铺"，但**这只是一个着色函数选择性点亮的视觉假象，几何上并不存在两类三角形**。

### 4.1 精确解释：为什么会出现"hex 色块 + 三角形空隙"的错觉

#### 4.1.1 真实拓扑（参见 [SphericalSDFTerrainDesign §2.1.1](SphericalSDFTerrainDesign.md#211-关键拓扑澄清所有三角形地位完全等价极重要)）

整个测地线球面**只有一种几何元素：primal 三角形（`FCorner`）**。它们在拓扑上完全等价，从来没有"hex 内部三角形"和"过渡三角形"两类之分。每个三角形的 3 个顶点恰好是 3 个 Cell 中心；每个三角形被它的外心-边中点连线划分为 3 个 1/3 区域，每个区域归属其对应顶点 Cell 的 hex/pent。一个 hex Cell 的范围 = 它周围 6 个 primal 三角形各贡献的 1/3 区域之并；pent 则是 5 × 1/3。

#### 4.1.2 R2 的着色公式做了什么

`albedo = λ₀ × hash(c₀)` 中：
- `c₀` = `Corner.CellIds[0]`，即三角形被装填时**Role 0 顶点**对应的 Cell；
- `λ₀` = 重心坐标在 Role 0 顶点上的权重，从 1（在该顶点）线性降到 0（在对边上）。

所以**每个 primal 三角形单独显示为：以 Role 0 顶点对应 Cell 的颜色为基色，从该顶点向对边线性渐暗**。

#### 4.1.3 同一个 hex 周围 6 个三角形的"角色分布"

考察一个内部 hex Cell **A**。围绕 A 的 6 个 primal 三角形 $T_1, \dots, T_6$ 都把 A 作为它们的某一个顶点（Role 0、1 或 2 之一），具体是哪个 Role 取决于 [`FSphereTopology::BuildCorners`](../Source/Grid/Private/FSphereTopology.cpp) 装填 `Corner.CellIds[0..2]` 的顺序。

- **Role 0 = A 的三角形**（约 6 个里的 1/3，即 1~2 个）：λ₀ 从 A 中心向远端线性降低 → 该三角形的"靠近 A 的 1/3 区域"被着上 `hash(A)` 的色，且该区域和其它 Role 0=A 的三角形拼起来形成围绕 A 的若干扇形 → 这就是视觉上的 **hex 色块"内核"**；
- **Role 0 ≠ A 的三角形**：λ₀ 从 X (≠A) 那个顶点向远端降低；这些三角形也含 A 中心附近的一块，但 A 中心附近的 λ₀ 已经接近 0 → 几乎纯黑 → 视觉上是"贴着 hex 色块外面的渐变三角形"。

#### 4.1.4 视觉假象的来源

R2 看到的"六边形 + 五边形 + **三角形空隙**"密铺图案，本质是：

1. 真实拓扑：球面被 primal 三角形单一密铺（sub=3 时 1280 个三角形）；
2. R2 公式只点亮每个三角形的 Role 0 顶点那一角的 1/3 区域；
3. 同一 hex 周围的 6 个三角形里，有些"Role 0 是该 hex"（亮）、有些"Role 0 是邻居"（暗）；
4. 亮区聚成 hex 形状的色块、暗区拼成"三角形空隙"的形状——但所谓"空隙"也是 primal 三角形的一部分，只是被着色函数有意压暗了。

> 这是一个**渲染层的视觉解释**，不要据此误认为存在两类几何三角形。R3 改用三方加权后，所有三角形会被均匀点亮，"空隙"消失，hex 内部呈现完全均匀的色块。

### 4.2 几何性质验证

可以观察到：

- **每个 sub=3 细分顶点（即 `Cells[i].UnitCenter`）都是一个六边形 / 五边形的中心**——验证 hex/pent Cell 的拓扑构建正确。
- **五边形（pentagon）的位置就是 12 个 `bIsPentagon=true` 的 Cell**（原正二十面体的 12 个顶点位置）——拓扑构建正确。
- **每个 hex 由 6 个三角形的 1/3 区域拼成，每个 pent 由 5 个三角形的 1/3 区域拼成**——这是 primal/dual 对偶关系的直观体现，而不是"sub=2 把 hex 切成 4 子三角形"那种细分关系。

### 4.3 R2 验收结论

| 待验证的属性 | 验证结果 |
| --- | --- |
| UV1/UV2 三个顶点写同样值 → 插值后 `round(c₀)` 正确还原 | ✅ 每个均匀色块表明 c₀ 还原精确 |
| UV3 OneHot 三顶点差异化 → 插值后 λ₀ 平滑 | ✅ 1/3 角块区域内的平滑渐变证实 |
| 拓扑层正确（hex/pent 数量与位置） | ✅ 12 个五边形清晰可见 |
| 重心坐标的几何性质（顶点处=1、对边上=0） | ✅ 渐变方向与三角形顶点-对边的几何关系吻合 |

> **注意**：R2 的视觉效果**不是**"每个 hex Cell 内部均匀"——这点要等 R3 用三方加权混合后才能达成。R2 的"hex 内部某 1/3 区域亮、其它 2/3 是暗的"本质上是 `λ₀×hash(c₀)` 这个公式只点亮 Role 0 顶点角块的精确演示，所有该出现的现象都出现了，验收通过。

## 5. 排错提示

| 现象 | 原因 / 排查 |
| --- | --- |
| 整个球纯黑 | 1) Material 没设置 Shading Model = Unlit；2) 节点没连到 Emissive；3) PMC 没拿到 UV3（检查日志 Verts 数 > 0）|
| 球白茫茫一片，看不到 hex 形状 | UV3 全部相同（OneHot 没差异化）→ 检查 C++ 顶点循环里 OneHotA/B/C 是否区分写入 |
| 同一个色块内部颜色离散乱跳 | UV1.x（c₀）三个顶点没写一致；或者 PS 没做 Round，把插值后的中间值当成 CellId 用了 |
| 有些三角形有色、有些黑 | UV3 的 OneHot 没有覆盖第三个顶点 (0,0)；或者 `1 - x - y` 在 PS 端没 saturate |
| 看到的是"hex/pent + 三角形空隙"密铺 | ✅ 这是 R2 公式的正确视觉效果（详见 §4），无需修复，等待 R3 |
