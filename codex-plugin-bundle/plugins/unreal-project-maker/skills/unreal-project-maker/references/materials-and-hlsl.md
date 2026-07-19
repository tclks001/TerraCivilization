# Materials and HLSL

## Scope

本文件记录 UE5 材质、Texture 参数、Sampler、Custom 节点 HLSL、运行时注入与反射诊断的通用规则。

## Do Not Read `.uasset` as Text

`.uasset` 是二进制。看资产属性或引用时用：

- 编辑器
- asset-aware 工具
- 导出命令
- 运行时反射诊断

材质连线、Custom 节点 Inputs、参数类型匹配，通常更适合在运行时通过引擎反射验证。

## Build Contract Diagnostics for Hand-Wired Materials

只要存在“用户在材质编辑器手工搭图、C++ 运行时注入参数”的路径，就主动做诊断函数。

建议打印：

- 找到多少个 Custom 节点
- Description
- OutputType
- Code 长度
- Inputs 数量
- 每个 Input 的名字和是否连接
- 是否满足当前阶段契约

如果某阶段应有 N 个 Inputs，就维护一份期望表，并在阶段推进时同步升级。

## Respect the Code-Owned Material Slot

如果某个 Actor 通过自定义 `UPROPERTY` 持有材质，并在代码里包装 MID、注入参数、做诊断，那么材质应挂在那条受代码管理的路径上，而不是绕过它直接挂到底层组件别的槽位。

信号：

- 诊断日志根本没跑
- 视觉像材质生效了，但动态参数和重建完全没反应

## Texture Object Parameter Has Two Defaults

区分：

1. 材质图节点本身的默认纹理
2. 主材质 / MI / 参数面板的默认值展示

对 `Texture Object Parameter` 来说，节点默认纹理通常承担“编译期资源类型和占位”的角色。经验规则：

- 节点默认纹理不要留空
- 节点默认纹理类型必须与运行时注入类型一致
- 不要以为 MI 面板有值就等于节点默认值也对

## `SetTextureParameterValue` Requires Type Match

如果节点默认是 `Texture2D`，运行时注入 `Texture2DArray`，很可能静默失败或表现异常。

做法：

- 节点上先挂同类型占位资源
- 再在运行时用同类型资源覆盖

## Sampler Type Must Match Texture Data

至少检查：

- BaseColor / Albedo -> `Color`
- Normal -> `Normal`
- Mask / 灰度 -> `LinearGrayscale` 或对应灰度采样器
- LUT / 浮点 RGBA -> `Linear Color`

常见症状：

- 材质编译报红
- 回退默认材质
- 视觉纯白 / 棋盘格 / 全黑

### Normal sampled as Color has a stable visual fingerprint

即使 Normal 资产已经设置 `TC_Normalmap`、`sRGB=false`，材质节点的 `Sampler Type=Color` 仍会把 `[0,1]` RGB 原样送入 Normal，引起整体切线法线偏向正 X/Y。

优先诊断：

1. 打开 Buffer Visualization -> World Normal。
2. 正确朝上的地面应接近蓝色；整片明显偏黄时，先检查 Normal Sample/Parameter 的 `Sampler Type`。
3. 显式设为 `Normal`，并再次核对纹理 `Compression Settings=Normalmap`、`sRGB=false` 和 DX/GL 绿通道约定。

不要先归因于绕序、UV 或顶点法线；它们通常不会产生这种整片 `(+X,+Y)` 偏置。

## UE Custom Node HLSL Rules

UE Custom 节点不是独立 `.usf` 文件，它的 Code 会被拼进函数体。默认遵守：

- 不要在 Code 里定义顶层函数
- 不要指望在 Code 里直接 `#include`
- 特殊 shader stage 优先用 `SampleLevel(..., 0)`
- Inputs 顺序必须与代码形参一一对应
- 给用户可直接粘贴的最终代码，不要只给算法参考块

可复用逻辑优先写成宏或局部 block：

```hlsl
#define DO_SOMETHING(IN_UV, OUT_V)            \
{                                             \
    float2 _uv = (IN_UV);                     \
    OUT_V = frac(sin(dot(_uv, float2(12.9, 78.2))) * 43758.5); \
}

float n0;
DO_SOMETHING(uv0, n0);
```

不要：

- 从 ShaderToy 直接复制辅助函数
- 把算法参考块当作可粘贴代码
- 在多语句宏里不用块作用域
- 用 `.Sample(...)` 然后在某些 pass / shader model 里炸掉

## Prefer Single Layer Water for Reflective Water

想做“看起来像水”的对象时：

- 水体优先 `Single Layer Water`
- 不要默认指望 `Translucent + Default Lit` 获得真实反射

诊断顺序：

1. 验证噪声 / 法线 / 时间流动逻辑本身在跑
2. 验证反射路径是否工作
3. 若 `Opaque + Roughness=0` 下反射立刻正常，问题多半在透明材质通路本身

如果着色函数里有 `1 / cos(theta)`、指数衰减、强非线性吸收，边界处出现三角锯齿时，先怀疑参数尺度而不是先怀疑几何细分。

### Validate the reflection path before tuning waves

当水面“不动/不反射”时做两次二分：

1. 把噪声直接接 Emissive，确认 Time 与波动逻辑在运行。
2. 临时改 `Opaque + Roughness=0 + Specular=1`；反射立刻出现时，根因在 Translucent 反射路径，而不是噪声。

Single Layer Water 还依赖场景条件：Atmosphere Sun Light、可用 SkyLight capture，以及非 None 的 Reflection Method。材质正确但场景缺其中一项，仍可能没有可信反射。

### Scale nonlinear water parameters as a coupled set

若光滑几何法线下，SLW 在掠射角/昼夜边界仍按三角形分块，检查 Beer-Lambert 一类 `exp(-sigma*d/cos(theta))` 的尺度敏感性。可尝试：

```text
geometry thickness/offset *= N
scattering and absorption coefficients /= N
```

这样近似保持光学厚度 `sigma*d`，同时降低单位光程斜率。不要只改厚度或只改系数；那会分别导致过度不透明或失去水色。该方法是参数诊断，不替代正确顶点法线。
