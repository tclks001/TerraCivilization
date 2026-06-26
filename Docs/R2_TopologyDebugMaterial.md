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

## 4. 验收标准

期望看到的效果（无需 PIE，编辑器视口直接可见）：

| 现象 | 含义 |
| --- | --- |
| **每个 hex Cell 一个独立颜色** | UV1/UV2 通过插值后能正确还原 c₀，CellId 哈希唯一 |
| **Cell 中心最亮，越靠近 Cell 边界越暗（趋近黑）** | λ₀ 在 Role 0 顶点 = 1，在该 Cell 之外 = 0；线性渐变正确 |
| **Cell 内部有 3 个不同的"亮度子区"以 Corner（hex 角）为分界** | 同一个 Cell 在不同三角形里担任不同的 Role（0/1/2）；每个三角形中 λ₀ 对应"三角形里 Role 0 顶点 = 该 Cell 中心方向" |
| **12 个五边形 Cell 形状清晰可见** | 拓扑构建正确 |

> **注意第 3 条**：因为 R2 我们只用 λ₀（不是按 CellId 真正的"我属于哪个 Cell"），所以同一个 hex Cell 在它周围 6 个三角形里**有时是 Role 0、有时是 Role 1/2**——λ₀=1 只在它担任 Role 0 的那 1/3 区域亮；担任 Role 1/2 时 λ₀ 是另一邻居 Cell 的 onehot。
>
> 这是预期现象，**用于验证 OneHot 的方向性正确**。R3 阶段我们会改成"先比 max(λ₀, λ₁, λ₂) 选出真正的归属 Cell ID（c₀ 或 c₁ 或 c₂），再哈希"——那时一个 hex Cell 才会变成完全均匀的颜色。

## 5. 排错提示

| 现象 | 原因 / 排查 |
| --- | --- |
| 整个球纯黑 | 1) Material 没设置 Shading Model = Unlit；2) 节点没连到 Emissive；3) PMC 没拿到 UV3（检查日志 Verts 数 > 0）|
| 球白茫茫一片，没有 Cell 形状 | UV3 全部相同（OneHot 没差异化）→ 检查 C++ 顶点循环里 OneHotA/B/C 是否区分写入 |
| 颜色有 Cell 形状但每个 Cell 内部颜色离散乱跳 | UV1.x（c₀）三个顶点没写一致；或者 PS 没做 Round，把插值后的中间值当成 CellId 用了 |
| 有些三角形有色、有些黑 | UV3 的 OneHot 没有覆盖第三个顶点 (0,0)；或者 `1 - x - y` 在 PS 端没 saturate |
| Cell 颜色有"接缝"（同一 Cell 内不同三角形颜色差异）| 这是预期的（见验收标准 §4 第 3 条说明）；R3 会修复 |
