# Validation and Debugging

## Scope

本文件记录编译、运行时日志、截图诊断、视觉验收、性能预算和“假绿灯”识别方法。

## Minimum Validation Loop

- 改完 C++ 立即编译
- 明确记录 `Succeeded`、warnings、errors；不要把“对象文件编译完成但 DLL 链接失败”写成构建成功
- 输出一条简洁摘要日志，带关键参数、计数、模式、阶段信息

## DLL Occupancy and Process Ownership

链接出现 `LNK1104` 或日志明确指出 Unreal DLL 被占用时：

1. 确认占用进程及命令行属于哪个项目。
2. 只结束本任务启动、且已确认属于当前项目的 Editor/Standalone/Game 进程。
3. 不按进程名批量结束用户已有 Unreal 会话。
4. 等目标进程退出后，用同一构建命令重试。
5. 仍失败时保留真实链接错误并报告，不推测成功。

## Fresh-process Runtime Validation

对不需要手工资产编辑的运行时任务，编译后继续验证：

- 使用 fresh standalone/game process，避免依赖 Editor 已加载的 CDO、动态资源或缓存。
- 使用 `-AbsLog=<absolute path>` 或严格限定本次运行的日志时间窗口。
- 等待目标地图/系统初始化完成后，再搜索唯一成功标识、关键状态和 Error/Warning。
- 第二次运行要读取第二次的新日志，不能沿用第一次输出。
- 最终报告实际命令、进程状态、关键日志证据和未验证项。

PIE 正常而 fresh Standalone/Packaged 异常时，优先打印并检查：

- `TUniquePtr` 拓扑、生成器、规则容器是否为空
- `CreateTransient` 纹理、MID、运行时缓存是否存在
- Gameplay/Presentation 容器是否初始化
- 这些对象是否只在 `OnConstruction` 或 Editor-only 路径中创建

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

对法线相关问题优先看 World Normal：

- 整体异常偏黄常指向 Normal 贴图按 Color 采样、未解包到 `[-1,+1]`。
- 表面颜色平滑但阴影/昼夜线按三角形锯齿，检查 vertex buffer 法线；像素材质 Normal 无法修复 Shadow/Lumen 等几何路径。
- 几何法线已平滑但非线性散射/吸收边界仍分块，检查着色参数尺度，不要先盲目加细分。

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

## Blueprint and Native-component Migration Validation

移除、改名或移动 Native Default Subobject 后，至少做：

- Editor 关闭状态下完整构建。
- 在 fresh Editor 中创建并打开一个直接继承目标 C++ 类的新 Blueprint。
- 检查 Components 面板和 Details，不得出现 PropertyEditor 递归/stack overflow。
- 迁移旧 Blueprint 和地图引用；fresh Standalone 加载目标地图。
- 搜索 `Could not find template object`、序列化错误和被移除的 subobject 名称。

重置 Editor Details 展开状态只能作为诊断控制，不能修复 C++ 反射递归或 stale Blueprint exports。
