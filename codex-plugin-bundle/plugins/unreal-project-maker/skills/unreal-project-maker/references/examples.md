# Examples

## Scope

本文件给出这份 skill 的典型使用样例。遇到相似用户请求时，先找最接近的例子，再按它建议的 reference 组合去展开，不要每次从零组织流程。

## Example 1: 新建 Runtime 模块

**用户可能会这样说**

- “帮我在项目里新建一个 `WorldGen` 模块，用来承载程序化地理生成。”
- “请把生成逻辑从当前 Actor 类里拆到独立 Runtime 模块。”

**先读**

- [workflow.md](workflow.md)
- [cpp-and-lifecycle.md](cpp-and-lifecycle.md)
- [validation-and-debugging.md](validation-and-debugging.md)

**建议动作**

1. 先读 `.uproject`、现有 `Build.cs`、`Source/` 模块结构。
2. 列出新模块职责、上下游依赖、数据输入输出。
3. 先写主设计稿摘要，再写独立详稿。
4. 创建模块骨架、Public/Private 文件落点、导出宏、日志类、基础 settings/DTO。
5. 让调用方只依赖稳定接口或 POD 输出。
6. 编译验证模块依赖无环、头文件无污染、最小示例能跑通。

**期望交付**

- 新模块目录与 Build.cs
- 设计稿里的模块依赖图
- 最小编译可过的公共接口
- 用户下一步在编辑器或调用方里如何接入

## Example 2: 新增一阶段材质通路

**用户可能会这样说**

- “给现有球面地表材质增加一层新的 triplanar 纹理混合。”
- “我要把现在的纯色调试材质升级成带多层贴图和 LUT 的版本。”

**先读**

- [workflow.md](workflow.md)
- [materials-and-hlsl.md](materials-and-hlsl.md)
- [validation-and-debugging.md](validation-and-debugging.md)

**建议动作**

1. 先确认这是“阶段升级”还是“小修补”。
2. 明确新增材质输入、纹理类型、Sampler Type、运行时注入参数。
3. 在详稿里给出完整 Custom 节点 Inputs 表、节点级搭建步骤、可直接粘贴 HLSL。
4. 在 C++ 里同步新增 UPROPERTY、MID 注入、诊断期望表、摘要日志。
5. 给用户一份编辑器内严格顺序的材质搭建清单。
6. 以 Lit / Unlit / Stats / Output Log 四件套做验收。

**期望交付**

- C++ 字段与注入逻辑
- 详细材质搭建文档
- 完整 HLSL 代码
- 反射诊断规则
- 视觉与日志验收清单

## Example 3: 诊断“材质看起来挂上了，但完全不响应参数”

**用户可能会这样说**

- “我改了参数但画面一点反应都没有。”
- “材质好像生效了，但 C++ 注入和日志都没动静。”

**先读**

- [materials-and-hlsl.md](materials-and-hlsl.md)
- [validation-and-debugging.md](validation-and-debugging.md)

**建议动作**

1. 先检查材质是否挂在受代码管理的槽位。
2. 检查诊断日志是否真的跑到了目标 Actor / Component。
3. 检查 Texture Object Parameter 的节点默认纹理是否为空、类型是否匹配。
4. 检查 Custom 节点 Inputs 名称、顺序、连接状态。
5. 要求用户给 Lit / Unlit / Stats / Output Log 截图。

**期望交付**

- 明确根因
- 修复步骤
- 如有必要，新增或升级运行时诊断代码

## Example 4: 程序化网格出现三角面分块 / 阴影锯齿

**用户可能会这样说**

- “网格表面按三角形分块，看起来像 flat shading。”
- “主表面还行，但阴影边界全是三角锯齿。”
- “切到新的网格路径后，同一材质在每个 cell 内部破碎了。”

**先读**

- [procedural-mesh.md](procedural-mesh.md)
- [materials-and-hlsl.md](materials-and-hlsl.md)
- [validation-and-debugging.md](validation-and-debugging.md)

**建议动作**

1. 先判断顶点是共享还是独立展开。
2. 检查是否错误依赖 `CalculateTangentsForMesh` 生成平滑法线。
3. 球面情况下优先验证是否应直接写 `+UnitCenter`。
4. 若 shader 依赖三角形级常量，检查是否被共享顶点破坏。
5. 通过 World Normal、Lit、Shadow 或区域分块截图做二分定位。

**期望交付**

- 几何路径根因分析
- 法线 / 切线修复方式
- 是否需要切到独立顶点编码
- 验收截图建议

## Example 5: 新增 GameplayTag 与数据驱动定义

**用户可能会这样说**

- “帮我接一套新的 `Terrain.*` GameplayTag 和 DataAsset。”
- “为什么 Tag ini 写好了，但编辑器下拉框还是空的？”

**先读**

- [gameplay-tags-and-data.md](gameplay-tags-and-data.md)
- [cpp-and-lifecycle.md](cpp-and-lifecycle.md)
- [validation-and-debugging.md](validation-and-debugging.md)

**建议动作**

1. 先检查现有 Tag 命名空间与配置路径。
2. 明确哪些 Tag 是启动期硬依赖，哪些是普通数据驱动项。
3. 用更保守的流程处理新 Tag Source：编辑器注册、再填 ini、再验证下拉框。
4. 检查 `meta=(Categories="...")` 是否与命名空间匹配。
5. 同步设计稿、DataAsset 字段、查询逻辑、验收文档。

**期望交付**

- Tag Source / ini 方案
- DataAsset 字段与命名空间约定
- 编辑器验证步骤
- 常见“下拉框为空”排查路径

## Example 6: 把一次开发沉淀成可复用设计文档

**用户可能会这样说**

- “把这次实现过程整理成后续阶段的基线文档。”
- “请把项目里已经踩过的 UE5 通用坑抽出来，做成 skill/reference。”

**先读**

- [workflow.md](workflow.md)
- [cpp-and-lifecycle.md](cpp-and-lifecycle.md)
- [materials-and-hlsl.md](materials-and-hlsl.md)
- [procedural-mesh.md](procedural-mesh.md)
- [gameplay-tags-and-data.md](gameplay-tags-and-data.md)
- [validation-and-debugging.md](validation-and-debugging.md)

**建议动作**

1. 先把项目专属契约和 UE5 通用经验拆开。
2. 保留可迁移的方法论、排错法、生命周期规律、材质规则。
3. 把太长的正文拆进 `references/`，主文件只保留导航和触发说明。
4. 用“用户可能会这样说”的方式反推 skill 的触发场景是否足够明确。

**期望交付**

- 精简后的主 `SKILL.md`
- 按专题拆好的 references
- 可被别的 agent 直接复用的例子集合

## Example 7: 典型最终回复应该长什么样

**建议输出骨架**

1. 先说这次采用了哪条方案，以及为什么。
2. 再说改了哪些文件、核心改动是什么。
3. 再说已经验证了什么、还没验证什么。
4. 最后给用户下一步编辑器操作清单。

**示意**

```text
我按“新增材质阶段 + 运行时诊断同步升级”这条路落地了，避免把逻辑只堆进材质图里。

这次主要改了 3 类东西：C++ 侧新增参数与 MID 注入、材质契约诊断升级、详稿里补了完整 HLSL 和编辑器步骤。编译已通过；运行时结构自检也通了。当前还差你在编辑器里把新 Texture Object Parameter 和 Custom Inputs 按文档顺序接好，然后用 Lit / Unlit 各看一遍。

下一步请按这个顺序操作：1. 复制旧材质；2. 新增两个标量参数；3. 给 Custom 节点补第 N~M 个 Inputs；4. 粘贴新版 HLSL；5. 把材质挂回代码拥有的 Material 槽；6. 确认日志里出现新的阶段摘要行和输入合规提示。
```

## Rule of Thumb

如果用户请求看起来同时涉及“方案选择 + C++ + 材质 + 编辑器手工步骤 + 验收”，就把它当成一个完整阶段任务来处理，而不是当成单点修 bug。
