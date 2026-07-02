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

重要认知：

- base pass 之外，阴影、背光裁剪、几何代理路径也会消费顶点法线
- 仅靠像素阶段反算法线，不能修复这些路径上的锯齿或阴影断层

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

## Practical Rule of Thumb

- **共享顶点**：适合真正连续的属性
- **独立顶点**：适合每三角形离散编码、候选 ID、one-hot 角色、按三角形固定权重
- **混合方案**：位置与法线语义共享，但离散 UV / 编码按三角形复制
