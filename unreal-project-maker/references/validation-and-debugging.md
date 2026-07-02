# Validation and Debugging

## Scope

本文件记录编译、运行时日志、截图诊断、视觉验收、性能预算和“假绿灯”识别方法。

## Minimum Validation Loop

- 改完 C++ 立即编译
- 能 clean 就尽量 clean：`Succeeded`、0 warnings、0 errors
- 输出一条简洁摘要日志，带关键参数、计数、模式、阶段信息

## Three-Layer Debugging

按这个顺序排查：

1. C++ / 数据层
   - 参数是否注入
   - LUT / 资源是否创建
   - 计数、范围、摘要日志是否合理
2. 材质 / 资产层
   - Stats 是否报错
   - Custom 节点 Inputs 是否齐全
   - 采样器、默认纹理、输出 pin 是否接对
3. 运行时 / 视图层
   - Lit / Unlit
   - Buffer Visualization
   - World Normal / BaseColor / Shadow 等专门视图

## Detect False Green Lights

以下都正常，也不代表视觉一定正确：

- 编译通过
- Custom 节点输入齐全
- 反射诊断通过
- 材质 Stats 通过

视觉仍不对时，优先怀疑：

- 材质挂载路径不对
- Texture Object 节点默认纹理为空或类型不对
- Sampler Type 与纹理不匹配
- 某个 pass 不允许当前 HLSL 写法
- 顶点法线或三角形级契约被破坏

## Always Inspect Images for Render Bugs

处理视觉 bug 时，不要只根据文字日志下结论。

优先要求并检查：

- Lit 截图
- Unlit 截图
- 必要时 Buffer Visualization 截图
- 必要时材质 Stats / Output Log 截图

很多“纯白”“全黑”“不反射”“分块”“边缘锯齿”问题，只看日志会误判。

## Acceptance Checklist

对中等以上任务，至少给出：

- 功能验收项
- 视觉验收项
- 日志验收项
- 编辑器验收项
- 回归项
- 性能预算

性能预算至少估：

- GPU ms 增量
- 显存或纹理占用
- sampler 数量
- 构建 / Rebuild 耗时
