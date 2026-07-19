# HISM + SDF 大世界动态地形 Demo 设计稿

> 编码：UTF-8，简体中文。
>
> 定位：新项目的独立技术验证 Demo，不继承 TerraCivilization 的球面、Gameplay、棋子、WorldGen 或现有资产约束。本文只复用已验证的 HISM + SDF 方法论和 Unreal 渲染能力。

## 1. 目标

验证一种以 Nanite HISM Static Mesh 为全部可见地形几何、以共享 SDF/PBR 材质为连续视觉场、以稀疏运行时编辑场为权威数据的动态大世界地形方案。

玩家可以在有限范围内持续进行堆积、挖掘、切削和洞穴开凿。系统将编辑转换为局部 SDF Stamp，更新附近少量 HISM 地形贴片或过渡模块，并更新相同范围的材质信息。目标不是首版实现通用体素引擎，而是验证下列体验可同时成立：

```text
玩家笔刷编辑
-> 地表轮廓、洞壁或洞顶产生可读的局部变化
-> 材质层连续：土、岩石、湿润、裂缝、积水、植被恢复
-> HISM 仍享受 Nanite、批量渲染和稳定远景
-> 射线命中、角色落脚和可选导航与可见几何一致
-> 编辑仅使局部 Chunk 重算，不重建整片世界
```

## 2. 当前已验证事实

TerraCivilization 的 SV6-B 已验证了以下事实。这些结论可以迁移到新 Demo，但不等于洞穴和自由编辑已被验证。

| 已验证事实 | 证据与意义 |
| --- | --- |
| HISM Static Mesh 能以高面数、Nanite 几何拼成连续可读的球面表面。 | 每个 Tile 保留独立网格和实例 Transform，仍能形成整体地表。 |
| 几何不严格焊接时，重叠遮挡可以隐藏大量边缘问题。 | 当前球面 Tile 的重叠层在外部观察方向下形成自然的前后遮挡。 |
| 一份共享 SDF 材质可跨 HISM 实例绘制连续颜色、三平面纹理、粗糙度、水体和高亮。 | SV6-B 使用世界方向/规范球面投影，避免实例本地 UV 和高度导致的纹理相位跳变。 |
| HISM 可通过 Per Instance Custom Data 携带局部 Cell/Chunk 上下文。 | Owner Cell 与一环邻居使材质不必每像素遍历所有逻辑单元。 |
| 运行时 LUT/MID 可为多个 HISM Component 注入共同的动态状态。 | Terrain、Highlight、River LUT 与 PBR 参数可以同步驱动 Plain/Forest/Mountain 实例。 |
| 材质连续不自动保证几何连续。 | 几何法线、深度、阴影、AO、碰撞与轮廓仍可能暴露实例边界。 |

球面 Demo 还有一个不可直接迁移的有利条件：它存在全局“向球外”的遮挡优先级。大世界洞穴没有该全局方向，因此新 Demo 必须验证基于局部 SDF 梯度的贴片朝向与埋入规则。

## 3. 非目标与技术边界

首版不承诺以下能力：

- 对任意 SDF 布尔结果都生成严格无缝的唯一网格。
- 每帧重排大量 HISM 实例。
- 无限世界、多人同步、完整 AI NavMesh、液体模拟或真实地质分层。
- 用材质 WPO 替代碰撞与可见轮廓。

方案的基本立场是：SDF 编辑场是地形的权威描述；HISM 不是权威数据，也不是任意几何布尔运算器，而是对局部零等值面的高质量离散表面代理。

当编辑产生难以由贴片稳定覆盖的拓扑变化时，系统允许进入专用过渡模块路径；若后续仍无法覆盖，可局部退化为运行时 Chunk Mesh。首版不实现该退化路径，但数据模型不得排斥它。

## 4. 预期视觉与交互效果

Demo 场景是一片约 `128m x 128m x 48m` 的可编辑岩土区域，分为地表、浅层洞穴和一段可贯通隧道。玩家持有球形笔刷，可切换 Add、Subtract、Flatten 三种操作。

预期效果：

1. 在平地加土后形成隆起、土石边缘和可恢复的植被遮罩。
2. 在斜坡减法后形成凹槽、壕沟、岩层暴露和局部湿润。
3. 从山侧挖入，形成可进入的洞口、洞壁和洞顶；从洞内外观察时不出现大面积 Tile 材质接缝。
4. 挖穿薄墙或连接两处洞室时，局部替换为专用过渡模块，保持洞道可见与可通行。
5. 远景继续由大量 Nanite HISM 实例稳定呈现；连续材质不因实例类型变化而突变。

## 5. 总体架构

```text
                  Player Brush
                       |
                       v
             TerrainEditField (authority)
             sparse chunked signed-distance field
                       |
             dirty chunk / dirty cell set
                       |
        +--------------+--------------+
        |                             |
        v                             v
 Terrain Surface Resolver       Terrain Material Field
 topology + local gradient      stamps + material masks
        |                             |
        v                             v
 HISM Patch/Transition Builder  Shared HISM SDF Master Material
        |                             |
        +--------------+--------------+
                       |
                       v
    Nanite HISM visible terrain + collision/query proxy
```

### 5.1 TerrainEditField：唯一真相

世界按固定尺寸 Chunk 切分。每个 Chunk 保存稀疏 SDF/密度样本与材质层数据，而非保存“当前有哪些 HISM 实例”。建议首版：

| 参数 | 建议初值 |
| --- | ---: |
| Chunk 边长 | `16m` |
| 基础格尺寸 | `1m` |
| 每 Chunk SDF 样本 | `17 x 17 x 17` 个共享格点 |
| 编辑影响半径 | `1m - 6m` |
| SDF 正值 | 空气侧 |
| SDF 负值 | 实体侧 |

相邻 Chunk 共享边界样本。笔刷先写入权威 SDF，再标记受影响 Chunk 及相邻一圈 Chunk 脏。这样保存、撤销、重放和未来网络同步只处理编辑命令或稀疏样本，不依赖瞬态 HISM 索引。

### 5.2 HISM Surface Patches：主可见几何

常规地形不把每个体素格做成完整封闭方块，而是只在 SDF 零等值面附近放置 Surface Patch。一个 Patch 是带底部裙边的局部曲面 Static Mesh：

```text
可见核心区：靠近零等值面，承担当下画面轮廓
覆盖裙边：沿实体内部方向延伸，被邻居核心区或实体内部遮挡
Local +Z：局部 SDF 向外方向
裙边方向：Local -Z，始终埋入 SDF 负侧
```

Patch 的 Mesh 本身可有平面、单凸、单凹、缓坡、陡坡、椭圆凹槽、半球隆起、低频鞍面等多个形态变体。运行时只允许在不改变核心边界覆盖范围的前提下选择变体。

### 5.3 Transition Modules：拓扑变化区

以下情况不依赖普通重叠贴片，而使用小数量专用 Transition Module：

- 洞口环：开放地表到垂直/倾斜洞壁的过渡。
- 洞道贯通和分叉。
- 薄墙被穿透。
- 局部桥、孔洞、洞顶与洞底距离过近。
- 曲率/梯度不稳定的鞍点。

Transition Module 不是全量 3D Wang Tile 库。它只覆盖普通 Patch 无法靠“可见核心 + 向实体内部埋入裙边”稳定遮挡的拓扑事件。首版可以从洞口和直线洞道两个族开始。

### 5.4 Shared HISM SDF Master Material：连续视觉场

所有地形 Patch 与 Transition Module 使用一份共同主材质，通过全局/Chunk 参数读取：

- 世界坐标或局部规范投影坐标。
- TerrainEditField 导出的局部 SDF 距离、材质层、湿润度、恢复时间。
- ChunkId、Patch 类型、随机 Seed、底部融合遮罩等 PICD。
- 三平面 PBR 纹理、岩土 Tint、积水、裂缝、雪、苔藓和植被密度。

材质负责“看起来被挖过”，几何负责“真的有洞/坑/墙/洞顶”。两者读取同一编辑场或同一批局部 Stamp，避免几何更新后材质延迟或反向错位。

## 6. 局部 SDF 梯度贴片策略

当前球面 Demo 使用球心径向方向定义外侧。大世界中改用局部梯度：

```text
SurfacePoint = SDF(x)=0 的近似位置
Outward = normalize(gradient(SDF, SurfacePoint))
PatchTransform.Z = Outward
PatchTransform.X/Y = 由稳定切线基构造
PatchLocation = SurfacePoint - Outward * EmbedDepth
```

`EmbedDepth` 使裙边进入实体内部。相邻 Patch 不需要顶点焊接，只要求：各自可见核心覆盖零等值面，裙边不露出实体侧。共享 SDF 材质保证它们在重叠区的颜色、纹理尺度、湿润与材质层一致。

局部梯度极小或变化过快时，普通 Patch 不生成或降低优先级，转交给 Transition Module。避免在鞍点任意选择一个不稳定外法线，导致模块翻转。

## 7. 模块选择，而非自由资产拟合

运行时不能只从“看起来相似”的自由 Mesh 中随机选一个。选择必须由局部 SDF 派生，至少包含：

| 局部特征 | 用途 |
| --- | --- |
| 零等值面是否穿过格子 | 决定是否生成表面实例。 |
| 梯度方向与大小 | 决定朝向、坡度族、是否稳定。 |
| 曲率/二阶差分 | 在平缓、凸起、凹槽、鞍面之间选型。 |
| 连通性/空实图样 | 判断普通 Patch、洞口、隧道、分叉或薄壁过渡。 |
| 材质层与深度 | 决定土、岩、矿层、湿润、积雪和植被。 |
| 稳定随机 Seed | 在同一局部条件下选择形态变体，保证重载后不跳变。 |

形态变体只改变 Patch 内部曲率、侵蚀和岩层细节；它不得让裙边翻到空气侧，也不得缩小核心覆盖范围。这样即使选用不同半球/椭圆/鞍面变体，邻居仍可依靠遮挡和共享材质保持视觉连续。

## 8. Runtime 更新流程

一次笔刷编辑的建议流程：

1. 根据笔刷形状和强度修改受影响 SDF 样本与材质层。
2. 对受影响 Chunk 加一格邻域标记 Dirty；边界样本同步到邻居 Chunk。
3. 合并短时间窗口内的连续笔刷输入，例如 `50-100ms`，避免每帧重排 HISM。
4. 为每个 Dirty 格子解析零等值面、梯度、曲率和连通性。
5. 移除失效 Patch；在对应 HISM Component 中新增或替换普通 Patch/Transition Module 实例。
6. 写入实例 Transform、ChunkId、PatchType、Seed、局部材质参数和融合遮罩 PICD。
7. 更新 Dirty Chunk 的材质 Stamp/LUT；在下一帧由共享材质显示切削痕、湿润、岩层和植被变化。
8. 刷新局部碰撞代理；导航作为后续可选异步任务。

实例类型变化通常意味着从一个 HISM Component 移除实例、向另一组件添加实例。应按 Mesh Family 分组维护 HISM Component，批量提交并只在 Chunk 完成一次更新后标记渲染状态 Dirty。

## 9. 碰撞、查询与游戏规则

材质 SDF 不参与物理。因此必须明确碰撞真相：

- 首版：HISM Surface Patch 使用 Query/Physics Collision，射线和角色落脚命中当前可见模块。
- 碰撞 Mesh 可低于 Nanite 可见网格精度，但必须覆盖可站立表面与洞壁。
- 重叠裙边必须放在实体内部，不应形成空气侧双层碰撞。
- 角色移动可先使用简单胶囊体 + 射线落脚；洞穴中不要求复杂攀爬。
- 若未来需要大量频繁修改物理，改为每个 Dirty Chunk 构建一个简化碰撞 Proxy，而不是频繁重建全部 HISM 碰撞树。

## 10. 性能与资源原则

Nanite 解决高三角形 Static Mesh 的几何预算和远距离细节，不会降低复杂 SDF 材质的像素成本，也不会自动解决实例重排成本。

首版性能策略：

- 只为零等值面附近创建 HISM，不为完整实体体积创建块。
- 以 Chunk 为更新批次；限制每次笔刷影响的最大 Dirty Chunk 数。
- 每个 Mesh Family 一个 HISM Component，避免“一种变体一个 Component”导致组件爆炸。
- 材质中只查询本 Chunk/邻域的数据；禁止每像素遍历大世界所有编辑 Stamp。
- 中远景可使用低频 Patch 族或 Chunk 合并代理；近景才使用高密度曲面变体。
- 保存编辑命令和压缩 SDF 样本，不保存 HISM 瞬态实例缓存。

建议从一开始记录以下数据：帧时间、可见实例数、Dirty Chunk 数、单次编辑的实例增删数量、材质 GPU 时间、碰撞更新时间、显存和磁盘编辑数据大小。

## 11. 最小闭环里程碑

### M0：编辑场与调试视图

- 创建 `128m x 128m x 48m` 静态测试场。
- 实现稀疏 Chunk SDF、球形 Add/Subtract Stamp、撤销/重放命令。
- 仅显示 SDF 截面、零等值面点和 Dirty Chunk；无 HISM、无材质。
- 验收：跨 Chunk 边界编辑后，SDF 样本和撤销结果一致。

### M1：外部地表 HISM Patch

- 实现平面、凸起、凹槽、缓坡四个 Patch Family 和向实体内部的裙边。
- 用局部 SDF 梯度放置 HISM；支持挖坑、堆土、壕沟。
- HISM 使用简单统一色，不做洞穴。
- 验收：连续 30 秒笔刷编辑只更新局部 Chunk；从任意地表观察角度没有明显空气侧裂缝。

### M2：共享 SDF/PBR 材质

- 接入世界/Chunk 坐标 SDF 材质、三平面岩土、湿润、切削边缘和植被密度。
- Patch 与 Patch 之间通过共享投影和同一编辑场连续绘制。
- 验收：改变 Patch 形态/类型后，颜色、纹理相位、Roughness 和材质层不突变。

### M3：碰撞与角色落脚

- 为 Patch 增加局部 Query Collision；实现角色射线落脚与挖掘射线命中。
- 裙边不在空气侧造成重复命中。
- 验收：角色可走过新堆土和坑边，不会站在已挖空的可见地形上。

### M4：洞口与浅层洞穴

- 实现洞口环、直线洞道、洞壁、洞顶三个 Transition Family。
- 支持从地表斜向挖入一段可进入洞道。
- 验收：从洞内外观察洞口，材质连续、没有大面积露出的裙边，洞顶与洞壁可被正确命中。

### M5：拓扑变化与压力测试

- 增加洞道贯通、薄墙穿透、简单 T 型分叉过渡模块。
- 同时进行连续编辑、撤销、重放和 Chunk 边界挖掘。
- 验收：记录无法由普通 Patch 处理的区域比例、Transition Module 覆盖率、帧时间和碰撞更新耗时，判断模块化路线是否足够扩展。

### M6：大世界方向验证

- 扩展为至少 `1km x 1km` 的流式 Chunk 场景。
- 加入近中远三档 Patch 密度和 Chunk 加载/卸载。
- 验收：远景 HISM/Nanite 稳定、局部编辑不触发全局重建、保存数据规模可控。

## 12. 成功标准与退出条件

本 Demo 成功，不是指“任何布尔体都完美无缝”，而是满足：

1. M1-M4 在目标硬件上视觉可信、交互正确、局部更新稳定。
2. 常规编辑区域主要依靠重叠 Patch，不要求严格焊接网格。
3. 洞口与浅层洞穴可由有限 Transition Family 处理。
4. SDF 编辑场、可见 HISM、碰撞代理和材质状态不会长期漂移。
5. 性能数据证明局部实例替换和复杂材质可接受。

以下情况应触发方案升级评估，而不是继续无止境扩充资产库：

- Transition Module 数量随普通编辑组合近似指数增长。
- 洞内频繁出现无法遮挡的空气侧裂缝、双层阴影或碰撞冲突。
- Chunk 更新大部分时间消耗在 HISM 移除/新增与物理更新。
- 薄壁、分叉、自由洞室等目标玩法占比很高。

届时应保留 Shared SDF Material、HISM Decor 和编辑场，把局部 Surface Patch 生成改为运行时 Chunk Mesh 或体积渲染，而不是推翻整套数据与材质系统。

## 13. 与 TerraCivilization SV6-B 的关系

SV6-B 是本 Demo 的材质与实例数据原型，不是其几何系统原型：

| SV6-B | 大世界 Demo 对应演进 |
| --- | --- |
| 球面方向 `Dir` | 世界局部 SDF 坐标与 Chunk 坐标。 |
| Owner Cell + 六邻居 PICD | ChunkId、PatchType、邻域 SDF 页、融合遮罩、Seed。 |
| 球心径向外侧 | `normalize(gradient(SDF))` 局部向外法线。 |
| 三种 Tile HISM | 多个 Patch/Transition Mesh Family。 |
| Terrain/River/Highlight LUT | Chunk SDF/材质/恢复状态图集或局部参数页。 |
| HISM 几何遮挡 | 可见核心区 + 向实体内部埋入的覆盖裙边。 |

SV6-B 已经证明“实例几何可不连续而材质场连续”。大世界 Demo 的核心未知项，是这个原则在局部 SDF 梯度、洞穴内观察、拓扑变化、频繁编辑和碰撞代理同时存在时，是否仍有足够大的稳定适用区间。

