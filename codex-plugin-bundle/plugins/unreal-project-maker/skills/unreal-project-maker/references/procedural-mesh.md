# Procedural Mesh

## Scope

本文件记录 ProceduralMesh、法线与切线、独立顶点/共享顶点选择、以及 shader 里的三角形级常量契约。

## Normals and Tangents Depend on Topology

先判断几何是不是“共享顶点”。

如果是“每三角形展开 3 个独立顶点”的程序化网格：

- 不要盲目调用 `CalculateTangentsForMesh` 期待得到平滑法线
- 它通常会退化成 flat shading

对于球面或近似球面：

- 顶点法线优先直接写 `+UnitCenter`
- 不需要切线空间法线贴图时，Tangents 可以留空

在 UE 的外部球面渲染约定下，光滑球面顶点法线取 `+UnitCenter`（朝外）。不要凭旧经验写成 `-UnitCenter`；先用 Lit/Unlit 和 World Normal 做 sign 验证。

重要认知：

- base pass 之外，阴影、背光裁剪、几何代理路径也会消费顶点法线
- 仅靠像素阶段反算法线，不能修复这些路径上的锯齿或阴影断层

若 BaseColor/像素 Normal 看起来平滑，但昼夜线、阴影或 Lumen 几何代理按三角形分块，直接检查 vertex buffer 法线。材质 Normal 引脚不能修复不执行该像素路径的 pass。

## Triangle-Constant Attribute Contracts

如果 shader 依赖“每个渲染三角形内部，一组离散属性或 ID 必须保持常量”，共享顶点会直接破坏契约。

典型信号：

- 大区域内部按三角形粒度破碎
- 同一三角形三个顶点需要写同一组离散 ID / 权重 / 编码值
- 顶点位置想共享，但属性不能共享

做法：

- 允许位置 / 法线语义共享
- 但把承载离散属性的顶点数据按三角形复制
- 用独立索引 `BaseIdx + 0/1/2` 组织三角形

不要为了省一点顶点内存，破坏 shader 的离散契约。

## Verify Coordinate-system Semantics

涉及东/西、旋转方向、cross product 或球面切向量时，先从坐标参数化推导，再用 Debug Arrow 目视验证。不要把右手系二维旋转直觉直接搬到 UE 左手系。

项目若约定 `+Z` 为北极、本初子午线经过 `+X`，则地理“向东”的具体符号必须由该项目的经度定义推导；将推导公式、手系和极区退化分支写入设计稿，不只写一个未经验证的向量常量。

## Practical Rule of Thumb

- **共享顶点**：适合真正连续的属性
- **独立顶点**：适合每三角形离散编码、候选 ID、one-hot 角色、按三角形固定权重
- **混合方案**：位置与法线语义共享，但离散 UV / 编码按三角形复制
