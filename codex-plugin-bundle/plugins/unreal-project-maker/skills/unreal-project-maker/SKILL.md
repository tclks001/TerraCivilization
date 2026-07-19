---
name: unreal-project-maker
description: Plan, scaffold, implement, migrate, document, and validate Unreal Engine 5 project features across C++, modules, components, procedural meshes, materials, GameplayTags, Blueprint assets, editor/PIE/standalone/packaged lifecycles, and runtime debugging. Use when building or refactoring UE5 systems, creating phased design docs, wiring UPROPERTY-driven editor controls, integrating Custom-node HLSL, diagnosing visual or lifecycle failures, migrating native default subobjects and dependent Blueprints, validating fresh-process runtime behavior, or turning project-specific Unreal experience into a reusable development workflow.
---

# Unreal Project Maker

## Overview

把 UE5 功能开发当成一条完整生产链来处理，而不是只改几行代码。先读项目现状，再明确方案，再写分层设计，再做 C++ / 材质 / 编辑器落地，最后给出可复现的验收与排错路径。

默认同时覆盖以下场景：新模块、程序化生成、程序化网格、材质与 HLSL、GameplayTag / DataAsset、Actor/Component 编辑器参数、Blueprint 资产迁移、PIE/Standalone/Packaged 生命周期、视觉验收与调试。

## Quick Start

先做 4 件事：

1. 读 `.uproject`、`Source/`、`Config/`、`Docs/`，确认模块边界与现有命名。
2. 读真实 `.h/.cpp`，不要凭设计稿摘要猜字段名或生命周期。
3. 判断这次是“小修补”还是“新阶段/新系统”，决定是否需要主稿 + 详稿双文档。
4. 如果用户给的是方向性需求而不是唯一方案，先列 2~3 条路线与权衡，等用户拍板。
5. 识别验证类型：需要用户手工资产验收，还是可以在无资产编辑条件下完成编译、独立运行与日志验收。

## Core Workflow

### 1. Clarify

- 明确为什么做、这期做什么、不做什么。
- 给出功能目标、视觉目标、性能预算、风险点。
- 存在多条技术路线时，先列方案和代价，再进入实现。

### 2. Document in Layers

- 主稿只保留阶段摘要、关键约束、跳转和状态，不塞实现细节。
- 新阶段、新系统、新材质通路、新模块、需要手工搭资产时，拆独立详稿。
- 详稿至少覆盖：目标、实现、编辑器步骤、验收清单、排错表、上下游关系。

详见：
- [workflow.md](references/workflow.md)：阶段工作流、主稿/详稿分层、交付格式
- [validation-and-debugging.md](references/validation-and-debugging.md)：验收清单、三层诊断、假绿灯识别

### 3. Implement Conservatively

- 先对齐现有模块边界、命名风格、参数命名，再新增字段和逻辑。
- UPROPERTY 要同时设计成“编辑器面板 API”：分类、范围、默认值、注释、日志名一致。
- 写跨模块字段访问前，先读真实头文件核对字段名和类型。

详见：
- [cpp-and-lifecycle.md](references/cpp-and-lifecycle.md)：真实源码优先、UPROPERTY、`TUniquePtr`、`BeginDestroy`、PIE 生命周期

### 4. Validate End to End

- 改完代码立即编译。
- 对无资产编辑任务，在 fresh process 中运行目标并使用本次运行的独立日志取证；不要把旧日志当成成功证据。
- 若 DLL 被占用，只处理本任务启动且已确认属于当前项目的进程；不要批量结束用户已有 Editor 会话。
- 有手工材质 / 蓝图 / 资源步骤时，给用户严格顺序的操作清单。
- 涉及原生 Default Subobject 名称、层级或类型变化时，把它当作 Blueprint 资产迁移，并在 fresh Editor/Standalone 中验证。
- 完成视觉或功能验收后，把踩坑沉淀回详稿和主稿。

## Reference Map

按问题读取对应 reference，不要一次全读：

- **工作流 / 文档拆分 / 交付格式**：读 [workflow.md](references/workflow.md)
- **C++、模块边界、UPROPERTY、组件/Blueprint 迁移、Editor/PIE/Packaged 生命周期**：读 [cpp-and-lifecycle.md](references/cpp-and-lifecycle.md)
- **材质、Texture 参数、Sampler、Custom 节点 HLSL**：读 [materials-and-hlsl.md](references/materials-and-hlsl.md)
- **ProceduralMesh、法线、切线、三角形级常量契约**：读 [procedural-mesh.md](references/procedural-mesh.md)
- **GameplayTag、ini Source、数据驱动路径**：读 [gameplay-tags-and-data.md](references/gameplay-tags-and-data.md)
- **编译、DLL 占用、独立运行、日志证据、视觉验收、截图诊断、性能预算**：读 [validation-and-debugging.md](references/validation-and-debugging.md)
- **典型用户请求、参考阅读顺序、交付示例**：读 [examples.md](references/examples.md)

## Always Do

- 先读真实项目，再动手。
- 先读真实头文件，再写跨模块字段访问。
- 先让用户选路线，再做不可逆实现。
- 给出可复制粘贴的最终代码和编辑器步骤。
- 对手工材质 / 蓝图资产建立运行时诊断。
- 处理视觉 bug 时同时看日志和截图。
- 对 PIE 正常但 Standalone/Packaged 异常的问题，检查纯 C++ 运行态是否只在 Construction 路径创建。
- 对原生组件重构，检查 Blueprint 序列化兼容、Details 递归和 CDO owner 缓存。
- 让每次阶段推进都带验收和排错沉淀。
- 保留用户已有改动；不要主动删除文件或资产。需要清理时先验证无引用，再在最终回复列出待用户处理项。

## Never Do

- 没澄清方案就直接编码。
- 凭记忆猜字段名。
- 把实现细节全塞进主稿。
- 把 `.uasset` 当文本读。
- 在 UE Custom 节点里随手定义函数。
- 在独立顶点网格上盲信自动切线会给你平滑法线。
- 在 `BeginDestroy` 里手动 `Reset()` 一堆 `TUniquePtr`。
- 在 `OnConstruction` 里给纯视觉挂件到处 `SpawnActor`。
- 在原生 Component 构造期缓存 Actor 构造函数的 `this`；运行时 owner 从 `GetOwner()` 解析。
- 把另一个可见 `UActorComponent` 嵌套成 Component 的默认子对象，或让 Actor Details 递归 inline 展开复杂组件引用。
- 删除或移动 Native Default Subobject 后，把旧 Blueprint 当成天然兼容。
- 只因 PIE 成功就跳过 fresh Standalone/Packaged 验证。
- 只看“绿灯日志”就宣布视觉验收通过。
