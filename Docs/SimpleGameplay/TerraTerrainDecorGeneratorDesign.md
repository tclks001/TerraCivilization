# TerraTerrainDecorGenerator 插件设计稿

> 编码：UTF-8，简体中文。
>
> 本稿维护插件职责、架构、算法、扩展约束和排错信息。面向使用者的逐步操作见 [TerraTerrainDecorGenerator使用说明.md](TerraTerrainDecorGenerator使用说明.md)。后续增加山峰、断崖、岩块等生成器时，应同步维护本文，而不是另建互相冲突的总设计稿。

## 1. 目标与边界

`TerraTerrainDecorGenerator` 是独立的 Editor-only 地形装饰资产生成插件。首版离线生成可供 HISM 使用的 Nanite `StaticMesh` 山脊变体，不参与运行时地形生成、Gameplay、点击、碰撞或连续表面高度计算。

插件可以参考并复制 `TerraSphericalTileGenerator` 的成熟资产管线，但两者在模块、头文件、类型、链接和运行时均无依赖：

- `TerraSphericalTileGenerator` 继续生成旧球面地块资产，保留作 Debug/历史工具。
- `TerraTerrainDecorGenerator` 在局部笛卡尔坐标生成山脊、山峰、断崖和岩块等 Decor。
- 两个插件各自实现 Height Source 读取、DynamicMesh 构建、StaticMesh 烘焙、Nanite 设置和材质实例创建。
- 禁止新插件 include 或链接旧插件模块。

首版只实现 Ridge。`Peak`、`Cliff`、`Boulder` 只保留资产类型与生成器扩展位置，不提供伪实现。

## 2. 坐标与资产契约

所有生成器统一遵守：

```text
Local +X = 资产主延伸方向；Ridge 对应山脊切线方向
Local +Y = 横跨山脊或主要立面的方向
Local +Z = 离开连续地表的局部上方
Pivot    = 资产包围盒的 XY 中心、基础地表 Z=0
```

SV6 HISM Decor 摆放时，将 `+Z` 对齐球面径向方向，将 `+X` 对齐山脊球面切线。生成网格的可见地表从 `Z=0` 向上生长，底部延伸到 `-SkirtDepthCM`，实例可以轻微埋入连续基础表面，隐藏接缝。

生成资产默认：

- 开启 Nanite。
- 关闭碰撞；连续基础表面继续承担点击和高度查询。
- 第 0 材质槽绑定一份批次共享材质实例。
- 由 UE StaticMesh Build 重算法线和 MikkTSpace 切线。
- 保留 UV0；首版沿长度平铺、横向映射 `0..1`。

## 3. 模块结构

```text
TerraTerrainDecorGenerator.uplugin
└─ Source/TerraTerrainDecorGenerator (Editor)
   ├─ TerraTerrainDecorGeneratorModule
   │  ├─ 注册 Nomad Tab
   │  └─ 注册 Tools 菜单入口
   ├─ STerraTerrainDecorGeneratorPanel
   │  ├─ IDetailsView 参数编辑
   │  ├─ Generate First Variant
   │  └─ Generate Ridge Batch
   ├─ UTerraTerrainDecorGeneratorSettings
   │  └─ EditorPerProjectUserSettings 参数持久化
   ├─ UTerraTerrainDecorGeneratorLibrary
   │  ├─ GenerateRidgeStaticMeshAsset
   │  └─ GenerateRidgeStaticMeshAssets
   └─ ITerrainDecorMeshGenerator
      ├─ FRidgeMeshGenerator (已实现)
      ├─ FPeakMeshGenerator (预留)
      ├─ FCliffMeshGenerator (预留)
      └─ FBoulderMeshGenerator (预留)
```

`ITerrainDecorMeshGenerator` 只负责向 `FDynamicMesh3` 写几何和属性。资产路径验证、材质实例、Nanite 与 StaticMesh 创建属于共享烘焙层，后续生成器不得复制整套资产写入流程。

## 4. 数据模型

### 4.1 公共资产类型

`ETTDGDecorAssetType` 首版包含 `Ridge/Peak/Cliff/Boulder`。只有 `Ridge` 可执行；其他值用于固定扩展方向和避免未来改变批处理协议。

### 4.2 山脊形态参数

| 参数 | 职责 |
| --- | --- |
| `LengthCM/WidthCM/HeightCM` | 山脊主体尺寸。 |
| `SkirtDepthCM` | 埋入地表的封闭底裙深度。 |
| `CrossSectionSharpness` | 横截面从山脚到脊顶的高次函数指数。 |
| `CrestOffsetNormalized` | 脊顶左右偏置，制造不对称陡坡。 |
| `CrestOffsetVariation` | 脊顶偏置沿长度的变化。 |
| `EndTaperFraction` | 两端下沉回地面的长度比例。 |
| `CenterlineWavinessCM/WavelengthCM` | 中心线横向摆动幅度与尺度。 |
| `WidthVariation/HeightVariation` | 沿线宽高变化。 |
| `ErosionStrengthCM/Frequency/Octaves` | 多频值噪声侵蚀。 |
| `HeightTextureDisplacementCM` | Height 纹理对顶点的次级位移。 |
| `LengthSegments/WidthSegments` | 顶部和底部参数网格分辨率。 |
| `TextureTiling` | UV 沿长度重复次数。 |

### 4.3 材质设置

输入 `BaseColor/NormalDX/Roughness/Height` 纹理和父材质，生成一份共享 `MaterialInstanceConstant`。默认参数名与旧插件一致，但类型完全独立。

批量变体不为每个 Mesh 创建重复 MI。调用生成器时，材质实例若不存在则创建，已存在则更新参数，并赋给每个 StaticMesh 的第 0 槽。4K Rock038 可由所有山脊资产共享，避免重复资源和材质状态。

### 4.4 输出与稳定性

资产命名：

```text
{OutputDirectory}/{StaticMeshNamePrefix}_{VariantIndex:00}
```

变体 Seed：

```text
Seed = BaseSeed + VariantIndex * SeedStride
```

相同参数、相同纹理源和相同 Seed 应生成相同形态。生成器拒绝覆盖已有 StaticMesh；需要覆盖时必须由使用者显式删除旧资产或更换索引，避免一次误点破坏人工调整过的资产。

## 5. Ridge 几何算法

### 5.1 参数域

顶部使用 `(LengthSegments+1) * (WidthSegments+1)` 的规则参数网格：

```text
u = ix / LengthSegments
v = iy / WidthSegments
x = (u - 0.5) * LengthCM
across = 2*v - 1
```

沿 `u` 计算低频噪声和正弦弯曲，得到中心线 `CenterY(u)`、局部半宽 `HalfWidth(u)`、高度系数和脊顶偏置：

```text
y = CenterY(u) + across * HalfWidth(u)
```

### 5.2 非对称横截面

以 `CrestOffset` 为脊顶，将左右两侧分别归一化到 `0..1`，再计算：

```text
Profile = pow(1 - CrestDistance, CrossSectionSharpness)
```

因此一侧可以较陡、另一侧较缓，不会形成左右对称的屋脊。端部以 `smoothstep` 在 `EndTaperFraction` 范围内降为零，便于和连续山体衔接。

### 5.3 形态变化和侵蚀

内置确定性多 octave Value Noise，分别服务于：

- 中心线低频偏移。
- 局部宽度变化。
- 局部高度变化。
- 顶部与坡面的侵蚀位移。

Height 纹理采样值以 `0.5` 为中性点，只在 Profile 与端部遮罩内施加，避免边缘被高频位移重新抬起。首版噪声追求稳定、轻量和可复现，不模拟真实水力侵蚀；宏观侵蚀仍由 SV4.5 连续地表负责。

### 5.4 封闭和接缝

顶部、底面、两条长侧边和两个端面共同组成封闭网格。底面固定在 `-SkirtDepthCM`。封闭体的目的不是提供 Gameplay 碰撞，而是：

- 从低角度观察时不暴露空壳。
- 允许实例轻微下埋。
- 给 Nanite 和阴影生成稳定拓扑。

HISM 不要求完整覆盖 SV4 山脊线。连续基础表面负责宏观轮廓，生成资产只强化局部脊顶、转折和断层；端部收束、底裙下埋和材质一致性共同隐藏接缝。

## 6. 编辑器 UI

插件注册 `Tools -> Terra Terrain Decor Generator`，打开可停靠 Nomad 面板。面板以 `IDetailsView` 呈现同一份强类型设置对象，并提供：

- `Generate First Variant`：只生成 `FirstVariantIndex`，用于快速试形态。
- `Generate Ridge Batch`：从 `FirstVariantIndex` 起生成 `VariantCount` 个资产。
- 状态区：显示成功路径、Seed 或逐资产失败原因。

设置存储在 `EditorPerProjectUserSettings`，关闭面板或编辑器后保留。UI 不直接实现生成算法，只调用同一 Blueprint Function Library，因此未来可被命令行编辑器、自动化脚本或 Editor Utility 复用。

## 7. 后续生成器扩展规则

新增 Peak、Cliff、Boulder 时应：

1. 新增独立强类型 Shape Settings，不向 Ridge 结构塞入无关字段。
2. 实现 `ITerrainDecorMeshGenerator::BuildMesh`。
3. 复用共享 Height Sampler、材质实例和 StaticMesh 烘焙管线。
4. 遵守统一局部坐标、Pivot、封闭体、Nanite、无碰撞和命名契约。
5. 在面板增加生成器模式与对应参数视图；未实现模式必须明确禁用。
6. 更新本文的数据模型、算法、验收与排错章节，同时更新使用说明的实际操作部分。

## 8. 排错基线

| 现象 | 主要检查点 |
| --- | --- |
| 面板菜单不存在 | 插件是否启用、Editor 模块是否编译加载、编辑器是否已重启。 |
| 资产路径无效 | 必须使用 `/Game/.../AssetName` 对象路径；输出目录不能是磁盘路径。 |
| 提示资产已存在 | 生成器有意禁止覆盖；删除旧资产或调整 `FirstVariantIndex`。 |
| Height 纹理读取失败 | 纹理必须保留有效 Source 数据；检查导入格式和源文件。 |
| 岩石单向受光或 World Normal 异常 | 父材质 Normal 参数的 Sampler Type 必须是 `Normal`，纹理必须是 NormalDX、Normalmap 压缩、`sRGB=false`。 |
| 山脊像规则屋脊 | 增加 `CrestOffsetNormalized/Variation`、中心线弯曲和宽高变化。 |
| 山脊像锯齿或融化 | 降低 Erosion/Height Texture 位移，或提高网格细分后减小位移幅度。 |
| 与连续表面接缝明显 | 增大 `SkirtDepthCM`、减小端部高度、实例略下埋，并对齐连续表面与 Decor 材质参数。 |
| 生成/保存时间过长 | 先降低细分和变体数；4K 纹理不增加网格生成计算，但会影响导入、编译和显存。 |

## 9. 首版验收标准

- 插件与 `TerraSphericalTileGenerator` 无模块依赖。
- Editor 目标可编译，Tools 菜单可打开生成面板。
- 参数跨面板重开保持。
- 可生成单个和批量 Ridge StaticMesh。
- 相同 Seed 可复现，不同 Seed 具有可见但受约束的变化。
- 资产局部 `+X/+Z`、Pivot、UV、封闭底裙符合契约。
- 默认开启 Nanite、关闭碰撞，并共享材质实例。
- 已有资产不会被静默覆盖。
- Peak/Cliff/Boulder 的类型与内部生成器扩展位置已保留，但不可误调用为已实现功能。
