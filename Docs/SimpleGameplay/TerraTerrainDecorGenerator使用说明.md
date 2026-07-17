# TerraTerrainDecorGenerator 使用说明

> 编码：UTF-8，简体中文。本文面向插件使用者，只说明实际操作。架构、算法和排错原理见 [TerraTerrainDecorGeneratorDesign.md](TerraTerrainDecorGeneratorDesign.md)。

## 1. 打开工具

1. 编译项目并重启 Unreal Editor。
2. 在主菜单选择 `Tools -> Terra Terrain Decor Generator`。
3. 将面板停靠到合适位置。参数会自动保存到当前项目的编辑器用户设置中。

首版只有 Ridge 山脊生成器可用。山峰、断崖和岩块将在后续版本加入。

## 2. 准备纹理和父材质

建议导入同一套岩石 PBR 纹理，例如 `Rock038_4K-PNG`：

| 用途 | 推荐文件 | UE 导入要求 |
| --- | --- | --- |
| Base Color | `Rock038_*_Color` | `sRGB=true`。 |
| Normal | `Rock038_*_NormalDX` | 必须是 NormalDX；`Compression=Normalmap`，`sRGB=false`。 |
| Roughness | `Rock038_*_Roughness` | `sRGB=false`。 |
| Height | `Rock038_*_Displacement/Height` | 建议 `sRGB=false`，必须保留 Source 数据。 |

准备一个岩石父材质，至少暴露以下 Texture 参数：

```text
BaseColorTexture
NormalTexture
RoughnessTexture
HeightTexture（可以不接材质输出，但会被 MI 保存）
```

连接建议：

```text
BaseColorTexture -> Base Color
NormalTexture    -> Normal
RoughnessTexture -> Roughness
```

`NormalTexture` 节点的 `Sampler Type` 必须设为 `Normal`。生成器只创建/更新材质实例，不会替你创建完整父材质。

## 3. 首次试生成

在面板中设置：

```text
Material.BaseColorTexture  = Rock038 Color
Material.NormalTexture     = Rock038 NormalDX
Material.RoughnessTexture  = Rock038 Roughness
Material.HeightTexture     = Rock038 Height/Displacement（可选）
Material.ParentMaterial    = 你的岩石父材质
Output.OutputDirectory     = /Game/Generated/TerrainDecor/Ridges
Output.StaticMeshNamePrefix= SM_Ridge
Output.FirstVariantIndex   = 1
```

先点击 `Generate First Variant`。成功后应得到：

```text
/Game/Generated/TerrainDecor/Ridges/SM_Ridge_01
/Game/Generated/TerrainDecor/Ridges/MI_Ridge_Rock
```

打开 `SM_Ridge_01`，检查：

- 长方向沿局部 `+X`。
- 高度沿局部 `+Z`。
- Pivot 位于山脊中部地表高度附近。
- 底部延伸到 `Z<0`，低角度看不到空壳。
- Nanite 已启用，碰撞默认关闭。
- 材质槽 0 使用共享的 `MI_Ridge_Rock`。
- 法线受光方向正常。

## 4. 推荐起始参数

先用以下参数建立中型山脊：

```text
LengthCM                  = 1800
WidthCM                   = 480
HeightCM                  = 520
SkirtDepthCM              = 80
CrossSectionSharpness     = 2.4
CrestOffsetNormalized     = 0.08
CrestOffsetVariation      = 0.14
EndTaperFraction          = 0.16
CenterlineWavinessCM      = 130
CenterlineWavelengthCM    = 700
WidthVariation            = 0.20
HeightVariation           = 0.25
ErosionStrengthCM         = 42
ErosionFrequency          = 4
ErosionOctaves            = 4
HeightTextureDisplacementCM = 18
LengthSegments            = 192
WidthSegments             = 48
TextureTiling             = 6
```

这组参数用于验证流程，不代表最终资产尺度。应结合游戏中的 Cell 直径和连续山脊宽度调整。

## 5. 调整形态

想让山脊更尖锐：提高 `CrossSectionSharpness` 或 `HeightCM`。如果像薄刀片，优先增加 `WidthCM`，不要继续提高高度。

想让两侧不对称：调整 `CrestOffsetNormalized`。正值把脊顶推向 `+Y`，负值推向 `-Y`；`CrestOffsetVariation` 控制这种偏移沿长度变化。

想让山脊不笔直：提高 `CenterlineWavinessCM`，再用 `CenterlineWavelengthCM` 控制弯曲尺度。幅度不宜超过宽度的一半，否则实例沿球面山脊线摆放时难以对齐。

想增加自然变化：逐步增加 `WidthVariation`、`HeightVariation` 和 `ErosionStrengthCM`。每次只改一类参数，以便判断重复感来自轮廓还是表面噪声。

想隐藏接缝：提高 `SkirtDepthCM`，保持 `EndTaperFraction` 在约 `0.12-0.25`，并在 HISM 摆放时把实例向连续表面下埋少量距离。

## 6. 批量生成变体

确认单个资产形态后设置：

```text
FirstVariantIndex = 1
VariantCount      = 8
BaseSeed          = 1337
SeedStride        = 977
```

点击 `Generate Ridge Batch`，生成 `SM_Ridge_01` 到 `SM_Ridge_08`。所有资产共享材质实例，但使用不同稳定 Seed 生成形态。

生成器不会覆盖已有资产。如果已经试生成了 `_01`，批量生成会报告 `_01` 失败、继续尝试其余编号。正式批量前可以：

- 删除确认不需要的试生成资产；或
- 将 `FirstVariantIndex` 改为尚未使用的编号。

不要通过频繁改变 Seed 盲目堆积资产。建议先生成 6 至 10 个，放到实际星球山脊上检查重复感，再针对缺少的长短、宽窄、弯曲和断裂形态补充新的参数批次。

## 7. 建议的资产批次

首轮可以分三批，每批使用不同名称前缀和尺寸：

| 批次 | 名称前缀 | 数量 | 重点 |
| --- | --- | ---: | --- |
| 短山脊 | `SM_Ridge_Short` | 4-6 | 山脊端部、转折、小段强化。 |
| 中山脊 | `SM_Ridge_Medium` | 6-8 | 最常用的山脊中段。 |
| 长山脊 | `SM_Ridge_Long` | 4-6 | 平缓连续段，弯曲幅度应较小。 |

每批调整 `LengthCM`、`WidthCM`、`HeightCM` 和 Seed。不要只靠运行时非均匀缩放把一个中型资产拉成所有尺寸，否则纹理尺度和侵蚀结构也会一起变形。

## 8. 生成后的 HISM 使用要求

插件只生成资产，不负责摆放。SV6 HISM Decor 应遵循：

```text
Local +Z -> 球面径向方向
Local +X -> 当前山脊测地线弧段的切线
Location -> 连续地表查询位置，并向地表内偏移少量距离
Collision -> NoCollision
```

资产不必覆盖整条山脊。优先放在高点、转折、汇合与视觉重点，连续基础表面负责剩余轮廓和材质过渡。

## 9. 常见问题

| 现象 | 处理 |
| --- | --- |
| Tools 菜单中没有插件 | 确认插件已启用并重新编译、重启编辑器。 |
| `Asset already exists` | 删除明确要替换的旧资产，或更改起始编号；工具不会自动覆盖。 |
| Height texture 无法读取 | 确认纹理保留 Source 数据且格式受 UE Source API 支持；也可先清空 Height 纹理验证。 |
| 山脊太规则 | 增加脊顶偏置、中心线弯曲和宽高变化。 |
| 表面噪声过碎 | 降低 Erosion Strength/Frequency 和 Height Texture Displacement。 |
| 底部或端面露出 | 增加 Skirt Depth、提高端部收束比例，并让实例下埋。 |
| 高光过强 | 在父材质/MI 中提高 Roughness 下限、降低 Specular 和 Normal Strength；保持和 SV6-A 连续表面一致。 |
| 4K 纹理显存过高 | 多资产共享同一 MI；小型碎石未来使用 1K/2K，不为每个 Mesh 复制纹理。 |
