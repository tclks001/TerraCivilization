# TerraCivilization 球面地块 StaticMesh 资产生成插件设计稿

> 本稿属于 `Docs/SimpleGameplay` 下的简易玩法迭代文档，用于指导本次基于 `GeometryScript` / `DynamicMesh` 的编辑器资产生成插件实现。
>
> 目标：把 ambientCG 等来源的 PBR 纹理组生成适合 `Static Mesh Instance / HISM` 摆放的球面圆形地块资产，用自然遮挡隐藏地块边缘毛边。

---

## 1. 设计目标

当前简易玩法尝试把球面地表显示交给大量地块 `StaticMesh` 实例：

- 不区分五边形和六边形资产。
- 单个地块资产生成得比逻辑 Cell 略大。
- 地块边缘向球内压低，让相邻实例靠自然遮挡覆盖边缘毛边。
- PBR 纹理继续使用原始 UV，不随球面变形重新展开。
- 高度图只影响局部起伏，不受中心到边缘遮挡衰减限制。

插件应优先服务编辑器内批量生成资产，而不是运行时生成。

---

## 2. 插件定位

插件名称：`TerraSphericalTileGenerator`

模块类型：`Editor`

主要能力：

- 暴露一个编辑器蓝图可调用函数库。
- 输入 PBR 纹理和参数。
- 构建一个带 UV 的高细分四边形网格。
- 将平面网格按 cube-sphere 思路映射为球面补丁。
- 按 UV 边缘遮挡项和高度图采样项调整顶点半径。
- 创建 `StaticMesh` 资产。
- 可选创建材质实例，把颜色、法线、粗糙、高度纹理打包引用到资产附近。

---

## 3. 暴露给编辑器的参数

```cpp
USTRUCT(BlueprintType)
struct FTSTGSphericalTileAssetBuildSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBR")
    UTexture2D* BaseColorTexture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBR")
    UTexture2D* NormalTexture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBR")
    UTexture2D* HeightTexture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBR")
    UTexture2D* RoughnessTexture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
    int32 SubdivisionsPerSide = 64;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
    float BaseRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
    float SphereExtensionAmplitude = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
    float HeightmapExtensionAmplitude = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
    float PatchAngularSizeDegrees = 18.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
    FString StaticMeshAssetPathAndName = "/Game/Generated/SphericalTiles/SM_SphericalTile";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    bool bCreateMaterialInstance = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    UMaterialInterface* ParentMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    FString MaterialInstanceAssetPathAndName = "/Game/Generated/SphericalTiles/MI_SphericalTile";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    FTSTGTextureParameterNames TextureParameterNames;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Mesh")
    bool bEnableCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Mesh")
    bool bEnableNanite = false;
};
```

说明：

- `BaseColorTexture`：颜色贴图，通常使用 ambientCG 的 `Color` / `Albedo` / `BaseColor` 贴图。
- `NormalTexture`：只接受 ambientCG 的 `NormalDX` 切线空间法线贴图。导入设置必须为 `NormalMap` 压缩、`sRGB=false`；插件和父材质不支持 `NormalGL`，也不会自动翻转绿色通道。
- `HeightTexture`：高度图，插件会读取该贴图的顶层 mip 并按 UV 双线性采样，用于真实修改网格顶点高度。
- `RoughnessTexture`：粗糙度贴图，用于写入材质实例参数，不参与几何生成。
- `SubdivisionsPerSide`：四边形网格每边细分程度，生成 `(N+1)*(N+1)` 个顶点、`N*N*2` 个三角形。当前代码 clamp 到 `1..512`。
- `BaseRadius`：地块基础球面半径。
- `SphereExtensionAmplitude`：边缘向内压低 / 中心向外鼓起的遮挡幅度。
- `HeightmapExtensionAmplitude`：高度图起伏幅度。
- `PatchAngularSizeDegrees`：地块在单位球面上的角宽度，用于从平面 patch 映射到球面方向。当前代码 clamp 到 `0.1..179.0`。
- `StaticMeshAssetPathAndName`：最终 `StaticMesh` 资产对象路径，例如 `/Game/Generated/SphericalTiles/SM_Plain_01`。
- `bCreateMaterialInstance`：是否尝试创建材质实例。
- `ParentMaterial`：材质实例的父材质。为空时仍会生成 `StaticMesh`，但不会创建或绑定材质实例。
- `MaterialInstanceAssetPathAndName`：材质实例资产对象路径，例如 `/Game/Generated/SphericalTiles/MI_Plain_01`。
- `TextureParameterNames`：父材质中的纹理参数名，默认是 `BaseColorTexture`、`NormalTexture`、`RoughnessTexture`、`HeightTexture`。
- `bEnableCollision`：是否为生成的 `StaticMesh` 启用碰撞；启用时使用复杂碰撞作为简单碰撞。
- `bEnableNanite`：是否在创建 `StaticMesh` 时启用 Nanite。

---

## 4. 网格生成流程

### 4.1 平面四边形网格

先在 UV 空间生成规则网格：

```text
u = x / N
v = y / N
```

每个四边形拆成两个三角形，并保留原始 UV。

### 4.2 cube-sphere 式球面映射

把 UV 先映射到局部方形 patch：

```text
sx = (u - 0.5) * tan(PatchAngularSize / 2) * 2
sy = (v - 0.5) * tan(PatchAngularSize / 2) * 2
RawDir = normalize(float3(sx, sy, 1))
```

该方向可视为 cube face 上的一个点投影到单位球面。

### 4.3 半径计算

每个顶点的半径由三部分组成：

```text
radius = BaseRadius + EdgeOcclusionOffset + HeightOffset
```

遮挡项按 UV 到中心的径向 dot 距离控制，越靠内越向上，越靠外越向下：

```text
CenterVector = float2(0.5, 0.5)
CenterDelta = float2(u, v) - CenterVector
CenterDistance01 = saturate(sqrt(dot(CenterDelta, CenterDelta)) / 0.5)
EdgeOcclusionOffset = SphereExtensionAmplitude * (1 - 2 * CenterDistance01)
```

说明：

- 中心处 `CenterDistance01=0`，遮挡项为 `+SphereExtensionAmplitude`。
- 距离中心 0.5 UV 单位处 `CenterDistance01=1`，遮挡项为 `-SphereExtensionAmplitude`。
- 四角会被 clamp 到同样的最低边缘高度，确保四周边界都压进球内，适合依靠相邻实例自然遮挡。

高度图项：

```text
HeightOffset = HeightmapExtensionAmplitude * HeightSample(u, v)
```

其中 `HeightSample` 初版从 `HeightTexture` 顶层 mip 做双线性采样，取线性灰度值；缺失高度图时按 `0` 处理。

最终坐标：

```text
Position = RawDir * radius
```

---

## 5. 纹理和材质打包策略

`StaticMesh` 资产本身只保存几何、UV 和材质槽。PBR 纹理不会真正内嵌进网格文件，而是通过材质/材质实例引用。

初版采用：

- 创建或复用一个动态生成的 `MaterialInstanceConstant`。
- 将 `BaseColorTexture`、`NormalTexture`、`RoughnessTexture`、`HeightTexture` 写入常用参数名。
- 把材质实例赋给生成的 `StaticMesh` 第 0 个材质槽。

默认参数名：

| 参数 | 名称 |
| --- | --- |
| 颜色 | `BaseColorTexture` |
| 法线 | `NormalTexture` |
| 粗糙 | `RoughnessTexture` |
| 高度 | `HeightTexture` |

> 若没有提供父材质，插件仍会生成网格资产，但材质实例创建会跳过。

---

## 6. 编辑器入口

使用 `UBlueprintFunctionLibrary` 暴露：

```cpp
UFUNCTION(BlueprintCallable, CallInEditor, Category = "Terra|Spherical Tile Generator")
static UStaticMesh* GenerateSphericalTileStaticMeshAsset(
    const FTSTGSphericalTileAssetBuildSettings& Settings,
    FString& OutErrorMessage);
```

这样后续可以通过：

- Editor Utility Widget
- Blutility
- Python/蓝图自动化
- 未来自定义 Details 面板

调用生成。

---

## 7. 详细具体用法

### 7.1 启用插件并重启编辑器

工程文件 [TerraCivilization.uproject](../../TerraCivilization.uproject) 已启用：

- `GeometryScripting`
- `TerraSphericalTileGenerator`

如果编辑器已经打开，第一次加入插件后需要重启编辑器，让 `TerraSphericalTileGenerator` 的 Editor 模块加载。

验证方式：

1. 打开 UE 编辑器。
2. 进入 `Edit -> Plugins`。
3. 搜索 `Terra Spherical Tile Generator`。
4. 确认插件处于启用状态。

### 7.2 准备 ambientCG PBR 纹理

从 ambientCG 下载某个材质时，建议至少准备四张贴图：

| 用途 | ambientCG 常见命名 | 填入字段 |
| --- | --- | --- |
| 颜色 | `Color` / `Albedo` / `BaseColor` | `BaseColorTexture` |
| 法线 | `NormalDX` | `NormalTexture` |
| 高度 | `Displacement` / `Height` | `HeightTexture` |
| 粗糙 | `Roughness` | `RoughnessTexture` |

导入 UE 后建议检查：

- `BaseColorTexture`：通常保持 `sRGB=true`。
- `NormalTexture`：必须使用 `NormalDX`，压缩类型应为 `NormalMap`，且 `sRGB=false`。不得传入 `NormalGL`；插件不会自动翻转绿色通道。
- `HeightTexture`：建议关闭 `sRGB`，并保留源文件数据；插件会读取 `Texture->Source` 顶层 mip。
- `RoughnessTexture`：建议关闭 `sRGB`。

注意：

- `HeightTexture` 为空时仍可生成资产，只是没有高度图起伏，只有中心鼓起和边缘压低。
- 如果 `HeightTexture` 没有有效 `Source` 数据，函数会失败并在 `OutErrorMessage` 中返回原因。
- 当前高度采样支持常见源格式：`G8`、`G16`、`BGRA8`、`RGBA16`、`RGBA16F`；其他格式会按 `0` 高度处理。

### 7.3 准备父材质

插件本身不会自动创建完整主材质。若希望生成后自动绑定 PBR 纹理，需要先准备一个父材质，例如：

```text
/Game/Materials/M_TerraSphericalTile_Master
```

父材质至少建议包含四个 `TextureObject` / `TextureSampleParameter2D` 参数：

| 参数名 | 默认字段 | 连接建议 |
| --- | --- | --- |
| `BaseColorTexture` | `TextureParameterNames.BaseColor` | 连接到 `Base Color` |
| `NormalTexture` | `TextureParameterNames.Normal` | 连接到 `Normal` |
| `RoughnessTexture` | `TextureParameterNames.Roughness` | 连接到 `Roughness` |
| `HeightTexture` | `TextureParameterNames.Height` | 可不连接，或用于后续材质效果 |

最小父材质建议：

```text
TextureSampleParameter2D(BaseColorTexture) -> Base Color
TextureSampleParameter2D(NormalTexture)    -> Normal
TextureSampleParameter2D(RoughnessTexture) -> Roughness
```

> **❗ 必需的法线采样设置**（新建节点默认不对，必须手动改）：
>
> - 在父材质里，选中接到 `Material.Normal` 引脚的 `TextureSampleParameter2D(NormalTexture)` 节点，
>   在 Details 面板中将 **`Sampler Type` 显式设为 `Normal`**（不能保留新建节点的默认值 `Color`）。
> - 同时法线贴图本身的导入设置必须是 `Compression Settings = TC_Normalmap` + `sRGB = false`（参见 §7.2）。
>
> 若 `Sampler Type` 错设为 `Color`，UE **不会**自动执行 `2*rgb - 1` 的 unpack，采样出来的 `[0, 1]` 原始颜色会直接当作切线空间法线送入引脚，
> 经 TBN 变换后世界法线会被整体扳向 `(+X, +Y, 0)`，表现为地块在 **World Normal 可视化下整片黄色**、且 **XY 正方向光能照亮、XY 负方向光死黑**。
> 详细的排查与修复流程见 §7.9 常见问题排查表。

说明：

- 几何高度已经在资产生成时烘焙进顶点位置，材质里的 `HeightTexture` 不是必需连接项。
- 如果父材质的参数名不同，可以在 `TextureParameterNames` 中改成对应名称。
- 如果 `ParentMaterial=nullptr`，插件仍会生成 `StaticMesh`，但不会创建 `MaterialInstanceConstant`，也不会给网格自动绑定材质。

### 7.4 用 Editor Utility Blueprint 调用

推荐用 `Editor Utility Blueprint` 作为第一版手动生成入口。

操作步骤：

1. 在内容浏览器中新建一个 `Editor Utility Blueprint`。
2. 父类选择 `EditorUtilityObject`，命名示例：`EUO_GenerateSphericalTileAsset`。
3. 打开蓝图，创建一个可调用函数，例如 `GeneratePlainTile`。
4. 在函数中创建 `FTSTGSphericalTileAssetBuildSettings` 结构体变量。
5. 给结构体填入纹理、半径和输出路径。
6. 调用 `Generate Spherical Tile Static Mesh Asset` 节点。
7. 打印 `OutErrorMessage`。
8. 右键该 Utility Blueprint，执行对应的编辑器调用入口。

蓝图逻辑可按以下结构组织：

```text
Make FTSTGSphericalTileAssetBuildSettings
    BaseColorTexture = T_Ground_Color
    NormalTexture = T_Ground_Normal
    HeightTexture = T_Ground_Height
    RoughnessTexture = T_Ground_Roughness
    SubdivisionsPerSide = 64
    BaseRadius = 100.0
    SphereExtensionAmplitude = 8.0
    HeightmapExtensionAmplitude = 4.0
    PatchAngularSizeDegrees = 18.0
    StaticMeshAssetPathAndName = /Game/Generated/SphericalTiles/SM_Plain_01
    bCreateMaterialInstance = true
    ParentMaterial = /Game/Materials/M_TerraSphericalTile_Master
    MaterialInstanceAssetPathAndName = /Game/Generated/SphericalTiles/MI_Plain_01
    bEnableCollision = true
    bEnableNanite = false
        -> Generate Spherical Tile Static Mesh Asset
        -> Print OutErrorMessage
```

成功时：

- 返回值是新生成的 `StaticMesh`。
- `OutErrorMessage` 为 `OK`。
- 内容浏览器中会出现 `StaticMeshAssetPathAndName` 指向的网格资产。
- 如果提供了 `ParentMaterial`，还会出现 `MaterialInstanceAssetPathAndName` 指向的材质实例，并自动绑定到网格第 0 材质槽。

### 7.5 推荐参数起点

#### 平原资产

```text
SubdivisionsPerSide = 64
BaseRadius = 100.0
SphereExtensionAmplitude = 6.0 ~ 8.0
HeightmapExtensionAmplitude = 1.0 ~ 3.0
PatchAngularSizeDegrees = 16.0 ~ 20.0
bEnableCollision = true
bEnableNanite = false
```

平原高度起伏应较弱，重点是让边界压进球内，不要让高度图造成明显穿帮。

#### 森林资产

```text
SubdivisionsPerSide = 64
BaseRadius = 100.0
SphereExtensionAmplitude = 8.0 ~ 10.0
HeightmapExtensionAmplitude = 2.0 ~ 5.0
PatchAngularSizeDegrees = 16.0 ~ 20.0
bEnableCollision = true
bEnableNanite = false
```

森林可以稍微增加高度起伏，后续也可以在父材质或独立实例上叠加树冠、草丛等视觉层。

#### 山脉资产

```text
SubdivisionsPerSide = 128
BaseRadius = 100.0
SphereExtensionAmplitude = 10.0 ~ 14.0
HeightmapExtensionAmplitude = 8.0 ~ 20.0
PatchAngularSizeDegrees = 16.0 ~ 22.0
bEnableCollision = true
bEnableNanite = true
```

山脉建议更高细分，并开启 Nanite 做初步尝试。若 HISM 大量实例化后性能或内存压力过大，再降低 `SubdivisionsPerSide` 或关闭 Nanite 对比。

### 7.6 输出路径规范

`StaticMeshAssetPathAndName` 和 `MaterialInstanceAssetPathAndName` 必须是 UE 对象路径，而不是磁盘路径。

正确示例：

```text
/Game/Generated/SphericalTiles/SM_Plain_01
/Game/Generated/SphericalTiles/MI_Plain_01
```

错误示例：

```text
C:/workspace/TerraCivilization/Content/Generated/SphericalTiles/SM_Plain_01.uasset
Content/Generated/SphericalTiles/SM_Plain_01
/Game/Generated/SphericalTiles/
```

命名建议：

| 地形 | StaticMesh | MaterialInstance |
| --- | --- | --- |
| 平原 | `/Game/Generated/SphericalTiles/SM_Tile_Plain_01` | `/Game/Generated/SphericalTiles/MI_Tile_Plain_01` |
| 森林 | `/Game/Generated/SphericalTiles/SM_Tile_Forest_01` | `/Game/Generated/SphericalTiles/MI_Tile_Forest_01` |
| 山脉 | `/Game/Generated/SphericalTiles/SM_Tile_Mountain_01` | `/Game/Generated/SphericalTiles/MI_Tile_Mountain_01` |

### 7.7 生成后的检查

生成完成后建议逐项检查：

1. 打开生成的 `StaticMesh`。
2. 确认网格形状是一个球面 patch，而不是平面。
3. 在 `UV` 预览中确认 UV 覆盖 `0..1`。
4. 查看边界是否明显低于中心。
5. 查看高度图起伏是否符合材质预期。
6. 若创建了材质实例，确认第 0 材质槽已绑定对应 `MI_*`。
7. 把多个实例放在相近球面方向上，观察边缘是否能自然互相遮挡。
8. 如果作为 HISM 使用，确认实例的朝向需要把资产本地 `+Z` 方向对齐到目标 Cell 的球面外法线。

### 7.8 和 HISM 摆放的关系

该插件只负责生成单个地块 `StaticMesh` 资产，不负责在棋盘上摆放实例。

后续 HISM 摆放时建议遵循：

```text
InstanceLocation = Cell.UnitCenter * PlacementRadius
InstanceRotation = RotationBetweenVectors(LocalUp = +Z, TargetUp = Cell.UnitCenter)
InstanceScale = 适配 Cell 尺寸的统一缩放，必要时略大于逻辑 Cell
```

关键点：

- 生成资产时，网格默认朝向局部 `+Z` 球面方向。
- 摆到全局球面时，需要把局部 `+Z` 旋到 `Cell.UnitCenter`。
- `PatchAngularSizeDegrees` 和实例缩放共同决定资产覆盖范围。
- 若边缘仍露毛边，优先增大 `SphereExtensionAmplitude` 或实例缩放，而不是盲目增加高度图幅度。

### 7.9 常见问题排查

| 现象 | 可能原因 | 处理方式 |
| --- | --- | --- |
| `OutErrorMessage` 提示对象路径无效 | 使用了磁盘路径或目录路径 | 改成 `/Game/.../AssetName` 格式 |
| 生成了网格但没有材质 | `ParentMaterial=nullptr` 或 `bCreateMaterialInstance=false` | 指定父材质并启用 `bCreateMaterialInstance` |
| 材质实例生成了但贴图没生效 | 父材质参数名与默认参数名不一致 | 修改 `TextureParameterNames` 或父材质参数名 |
| **地块在 World Normal 可视化下整片黄色（≈ `(+X, +Y, 0)`），且只有 XY 正方向入射的光能照亮、XY 负方向入射的光下地块死黑**（关闭高度图后仍然如此，且顶点绕序、UV 都已确认无误） | **父材质中 `TextureSampleParameter2D(NormalTexture)` 节点的 `Sampler Type` 错设为 `Color`（新建节点的默认值）**，导致采样出来的 `[0, 1]` RGB 未被 UE unpack 到 `[-1, +1]`，直接当作切线空间法线进入引脚 | 打开父材质→选中接到 `Material.Normal` 的 `TextureSampleParameter2D` 节点→Details 面板把 `Sampler Type` 改为 `Normal`；同时确认贴图导入设置 `Compression Settings = TC_Normalmap` 且 `sRGB = false`。修正后 World Normal 应回到蓝色，方向光绕 Z 轴旋转时照光应连续过渡 |
| 高度图没有起伏 | `HeightTexture=nullptr`、高度幅度太小或源格式未支持 | 指定高度图，提高 `HeightmapExtensionAmplitude`，检查纹理导入格式 |
| 边缘仍然露毛边 | 压边幅度或实例覆盖不足 | 提高 `SphereExtensionAmplitude`，或让 HISM 实例缩放略大 |
| 山脉太尖 / 穿插严重 | `HeightmapExtensionAmplitude` 过大 | 降低高度图幅度或换更平滑高度图 |
| 网格过重 | `SubdivisionsPerSide` 太高 | 平原/森林先用 `64`，山脉再尝试 `128` |
| 大量实例性能不稳定 | Nanite、碰撞、材质复杂度组合过重 | 分别测试关闭 `bEnableCollision`、关闭 `bEnableNanite`、降低细分 |

---

## 8. 验收标准

- 插件能在编辑器目标中编译。
- `TerraCivilization.uproject` 启用 `GeometryScripting` 和 `TerraSphericalTileGenerator`。
- 函数库能输入 ambientCG 导入的四张纹理。
- 能生成一个 `StaticMesh` 资产。
- 生成网格保留原始 `0..1` UV。
- 顶点按 cube-sphere 方向投影到球面 patch。
- 顶点半径包含遮挡项和高度图项。
- 相邻实例放大摆放时，边缘向内压低，有利于自然遮挡。
