﻿# ProceduralTerrainGenerator (PTG) 使用说明

> 适用版本：**v2.0.4**（基于 Unreal Engine 5.7 / 5.8）
> 作者：Víctor Hernández Molpeceres (Rockam)
> 许可：商城插件
> 官方文档（PDF）：`Documentation/PTG_documentation_2-0.pdf`
> 教程视频：<https://www.youtube.com/playlist?list=PLjoiveKRERhWCVYPEfQ_oUEqjfjwAhZyR>

---

## 目录

- [1. 插件概述](#1-插件概述)
- [2. 模块结构与依赖](#2-模块结构与依赖)
- [3. 快速上手](#3-快速上手)
- [4. 核心 Actor：`APtgManager`](#4-核心-actorapTgmanager)
  - [4.1 通用属性](#41-通用属性)
  - [4.2 地形 (Terrain) 属性](#42-地形-terrain-属性)
  - [4.3 水面 (Water) 属性](#43-水面-water-属性)
  - [4.4 自然 (Nature) 属性](#44-自然-nature-属性)
  - [4.5 角色生成 (Actors) 属性](#45-角色生成-actors-属性)
  - [4.6 噪声 (Fast Noise Lite) 属性](#46-噪声-fast-noise-lite-属性)
  - [4.7 调用接口 (UFUNCTION)](#47-调用接口-ufunction)
  - [4.8 委托 (Delegates / 事件)](#48-委托-delegates--事件)
- [5. 修改器：`APtgModifier`](#5-修改器apTgmodifier)
- [6. 数据结构](#6-数据结构)
  - [6.1 `FPtgBiomaNature`](#61-fptgbiomanature)
  - [6.2 `FPtgBiomaActors`](#62-fptgbiomaactors)
  - [6.3 `FPtgProcMeshData` 与枚举 `EPtgProcMeshShapes`](#63-fptgprocmeshdata-与枚举-eptgprocmeshshapes)
- [7. 蓝图函数库](#7-蓝图函数库)
  - [7.1 `UPtgProcMeshDataHelper`](#71-uptgprocmeshdatahelper)
  - [7.2 `UPtgUtils`](#72-uptgutils)
- [8. 噪声封装：`UPtgFastNoiseLiteWrapper`](#8-噪声封装uptgfastnoiselitewrapper)
- [9. 编辑器扩展](#9-编辑器扩展)
- [10. 内置内容资产](#10-内置内容资产)
- [11. 常见使用流程示例](#11-常见使用流程示例)
- [12. 注意事项 & FAQ](#12-注意事项--faq)
- [13. 版本历史](#13-版本历史)

---

## 1. 插件概述

ProceduralTerrainGenerator（下文简称 **PTG**）是一个**轻量级、易上手**的程序化地形生成插件。它允许你在编辑器中或运行时，使用自有美术资产（材质、网格、Actor）快速生成多种形态的地形，并可叠加水面、自然装饰物（草、树、石头）以及随机 Actor。

主要功能：

| 功能 | 说明 |
| --- | --- |
| **多形状地形** | 支持 `Plane`（平面）/ `Cube`（立方体）/ `Sphere`（球体，行星）三种基础形态 |
| **基于 FastNoiseLite 的噪声** | 内置 OpenSimplex2 / Perlin / Cellular / Value 等多种噪声算法，支持 FBm、Ridged、PingPong 等分形与 Domain Warp |
| **Plane 地形 Tiling（拼接）** | 可让多个 PTG_Manager 在世界中无缝拼接 |
| **LOD 支持** | 最多 8 级 LOD，自定义屏幕大小衰减系数 |
| **水面生成** | 三种生成模式：随机百分比、固定百分比、固定高度 |
| **自然/Actor 生成** | 按 Bioma（地表/水下/全部）放置，支持密度、缩放、旋转、剔除距离、影响导航等 |
| **修改器（Modifiers）** | 使用 Box / Sphere 区域排除或限制自然&Actor 出现位置 |
| **高度过滤** | 按高度百分比范围放置植被或 Actor |
| **运行时网格 RuntimeMeshComponent v4.1.5** | 高效的程序化 Mesh 渲染 |
| **编辑器工具** | 一键将 PTG 地形转换为 StaticMesh 资产；Plane 地形可导出 PNG Heightmap |
| **委托事件** | 提供生成开始 / 完成 / 失败 / 进度委托，便于业务流程控制 |
| **进度反馈** | 编辑器中通过 `FScopedSlowTask` 显示生成进度 |

---

## 2. 模块结构与依赖

插件包含 **4 个模块**：

| 模块 | 类型 | 说明 |
| --- | --- | --- |
| `RuntimeMeshComponent` | Runtime（ThirdParty） | 高效运行时 Mesh 组件 |
| `FastNoiseLite` | Runtime（ThirdParty） | 噪声库 |
| `ProceduralTerrainGenerator` | Runtime | PTG 主功能模块（`APtgManager`、`UPtgFastNoiseLiteWrapper` 等） |
| `ProceduralTerrainGeneratorEditor` | Editor | 编辑器扩展（详情面板按钮、Heightmap 导出） |

支持平台：**Win64、Linux**。

C++ 项目中如需直接调用 PTG，请在 `.Build.cs` 中加入：

```csharp
PrivateDependencyModuleNames.AddRange(new string[]
{
    "ProceduralTerrainGenerator",
    "RuntimeMeshComponent",
    "FastNoiseLite"
});
```

---

## 3. 快速上手

1. **启用插件**：`Edit -> Plugins -> Code Plugins -> ProceduralTerrainGenerator`，勾选并重启编辑器。
2. **拖放 Manager 到关卡**：
   - 在 Content Browser 中打开 `Plugins/ProceduralTerrainGenerator Content/ProceduralTerrainGenerator/BP_PTG_Manager`
   - 直接拖入关卡（`APtgManager` 是 `Abstract` 抽象类，**必须使用其蓝图子类**，例如自带的 `BP_PTG_Manager`）
3. **配置地形材质**：在 Details 面板 `PTG 0 - Properties | Terrain generation` 中设置 `TerrainMaterial`
4. **生成**：在 Details 面板顶部，点击 `Generate Everything` 按钮（或调用其同名函数）即可。
5. **后续调整**：修改任意属性后，若启用 `bGenerateEverythingOnPropertyChange`，PTG 会自动重新生成；否则可手动点击对应分组的 `Generate*` 按钮。

---

## 4. 核心 Actor：`APtgManager`

```cpp
UCLASS(Abstract)
class PROCEDURALTERRAINGENERATOR_API APtgManager : public AActor
```

> 头文件：`Source/ProceduralTerrainGenerator/Public/PtgManager.h`
> 注意：`APtgManager` 是 **抽象类**，必须使用蓝图派生（如 `BP_PTG_Manager`）。

### 4.1 通用属性

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `Radius` | `float` | 15000.0 | 地形半径（cm） |
| `Resolution` | `int32` | 150 | 在 X/Y 上的细分网格数 |
| `NumberOfLODs` | `int32` | 8 | LOD 数（1 表示无 LOD，仅 LOD0） |
| `LODScreenSizeMultiplier` | `float` | 0.5 | 每级 LOD 屏幕大小衰减系数（公式：`Pow(mult, LODIndex)`） |
| `bGenerateEverythingOnPropertyChange` | `bool` | false | 修改属性后是否自动重生成 |

### 4.2 地形 (Terrain) 属性

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `Shape` | `EPtgProcMeshShapes` | `Plane` | 地形形状：Plane / Cube / Sphere |
| `bUseTerrainTiling` | `bool` | false | 仅 Plane 形态可用，启用后多个 Manager 间噪声会拼接 |
| `NoiseInputScale` | `float` | 1.0 | 越小则地形越被"拉伸" |
| `NoiseOutputScale` | `float` | 1000.0 | 高度缩放 |
| `TerrainMaterial` | `UMaterialInterface*` | null | 地形材质 |
| `bEnableTerrainCollision` | `bool` | true | 地形是否生成碰撞 |
| `LowestGeneratedHeight` (只读) | `float` | - | 生成地形最低高度 |
| `HighestGeneratedHeight` (只读) | `float` | - | 生成地形最高高度 |

### 4.3 水面 (Water) 属性

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `bGenerateWater` | `bool` | true | 是否生成水面 |
| `WaterHeightGenerationType` | `EPtgWaterHeightGenerationType` | `RandomPercentage` | `RandomPercentage` / `FixedPercentage` / `FixedHeight`（仅 Plane 支持 FixedHeight） |
| `WaterSeed` | `int32` | 1619 | 随机水高的种子 |
| `WaterRandomHeightRangePercentages` | `FVector2D` | (10, 50) | 随机模式下水位百分比范围 |
| `WaterFixedHeightPercentage` | `float` | 50.0 | 固定百分比模式下使用 |
| `WaterFixedHeightValue` | `float` | 0.0 | 固定高度（仅 Plane 形态） |
| `WaterMaterial` | `UMaterialInterface*` | null | 水面材质 |
| `bEnableWaterCollision` | `bool` | false | 水面是否产生碰撞 |
| `WaterHeight` (只读) | `float` | - | 实际生成的水面相对高度 |

### 4.4 自然 (Nature) 属性

| 属性 | 类型 | 说明 |
| --- | --- | --- |
| `bGenerateNature` | `bool` | 是否生成自然装饰 |
| `BiomaNature` | `TArray<FPtgBiomaNature>` | 每个 Bioma 的自然装饰项配置数组（详见 [6.1](#61-fptgbiomanature)） |

### 4.5 角色生成 (Actors) 属性

| 属性 | 类型 | 说明 |
| --- | --- | --- |
| `bGenerateActors` | `bool` | 是否生成 Actor |
| `BiomaActors` | `TArray<FPtgBiomaActors>` | 每个 Bioma 的 Actor 配置数组（详见 [6.2](#62-fptgbiomaactors)） |

### 4.6 噪声 (Fast Noise Lite) 属性

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `NoiseType` | `EPtgFastNoiseLiteWrapperNoiseType` | OpenSimplex2 | 噪声算法 |
| `RotationType3D` | `EPtgFastNoiseLiteWrapperRotationType3D` | None | 仅 Cube/Sphere 形态有效 |
| `Seed` | `int32` | 1619 | 噪声种子 |
| `Frequency` | `float` | 0.02 | 噪声频率 |
| `FractalType` | `EPtgFastNoiseLiteWrapperFractalType` | FBm | 分形组合方式 |
| `FractalOctaves` | `int32` | 5 | 分形层数 |
| `FractalLacunarity` | `float` | 2.0 | 各层间频率倍率 |
| `FractalGain` | `float` | 0.5 | 各层间强度衰减 |
| `FractalWeightedStrength` | `float` | 0.0 | 非 DomainWarp 分形的层加权 |
| `FractalPingPongStrength` | `float` | 2.0 | PingPong 分形强度 |
| `CellularDistanceFunction` | `EPtgFastNoiseLiteWrapperCellularDistanceFunction` | EuclideanSq | Cellular 距离函数 |
| `CellularReturnType` | `EPtgFastNoiseLiteWrapperCellularReturnType` | Distance | Cellular 返回类型 |
| `CellularJitter` | `float` | 1.0 | Cellular 抖动强度 |
| `DomainWarpType` | `EPtgFastNoiseLiteWrapperDomainWarpType` | None | Domain Warp 算法 |
| `DomainWarpAmp` | `float` | 30.0 | Domain Warp 最大偏移 |

### 4.7 调用接口 (UFUNCTION)

所有蓝图可调用 / 编辑器内可调用 (`CallInEditor`) 的接口：

#### 一. 总控（`PTG 1 - General actions`）

| 函数 | 描述 |
| --- | --- |
| `void GenerateEverything()` | 使用 Manager 当前种子重新生成地形 + 水 + 自然 + Actor |
| `void GenerateEverythingWithCustomSeed(int32 terrainSeed, int32 waterSeed, int32 natureSeed, int32 actorsSeed)` | 使用自定义种子全部重生成 |
| `void GenerateEverythingWithRandomSeed()` | 全部使用随机种子重生成 |
| `void ClearNatureAndActors()` | 清除所有自然装饰和 Actor |

#### 二. 地形（`PTG 2 - Terrain generation actions`）

| 函数 | 描述 |
| --- | --- |
| `void GenerateTerrainMesh()` | 使用当前种子生成地形 |
| `void GenerateTerrainMeshWithCustomSeed(int32 terrainSeed)` | 使用指定种子生成地形 |
| `void GenerateTerrainMeshWithRandomSeed()` | 使用随机种子生成地形 |
| `void ClearTerrainMesh()` | 清空地形网格 |

#### 三. 水面（`PTG 3 - Water generation actions`）

| 函数 | 描述 |
| --- | --- |
| `void GenerateWaterMesh()` | 使用当前种子生成水面 |
| `void GenerateWaterMeshWithCustomSeed(int32 waterSeed)` | 仅当 `bGenerateWater = true` 且为随机百分比模式时生效 |
| `void GenerateWaterMeshWithRandomSeed()` | 使用随机种子生成水面（条件同上） |
| `void ClearWaterMesh()` | 清空水面网格 |

#### 四. 自然（`PTG 4 - Nature generation actions`）

| 函数 | 描述 |
| --- | --- |
| `void GenerateNature()` | 生成自然装饰 |
| `void GenerateNatureWithCustomSeed(int32 natureSeed)` | 使用自定义种子生成 |
| `void GenerateNatureWithRandomSeed()` | 使用随机种子生成（仅 `bGenerateNature = true` 时） |
| `void ClearNature()` | 清空自然装饰 |

#### 五. Actor（`PTG 5 - Actors generation actions`）

| 函数 | 描述 |
| --- | --- |
| `void GenerateActors()` | 生成 Actor |
| `void GenerateActorsWithCustomSeed(int32 actorsSeed)` | 使用自定义种子生成 |
| `void GenerateActorsWithRandomSeed()` | 使用随机种子生成（仅 `bGenerateActors = true` 时） |
| `void ClearActors()` | 移除所有 Actor |

#### 六. Getters

| 函数 | 返回 | 描述 |
| --- | --- | --- |
| `EPtgProcMeshShapes GetShape()` | 形状枚举 | 获取地形形状 |
| `URuntimeMeshComponent* GetProcMeshTerrainComp()` | 地形 RMC 组件 | - |
| `URuntimeMeshComponent* GetProcMeshWaterComp()` | 水面 RMC 组件 | - |
| `const TArray<float>& GetVertexHeightData()` | 顶点高度数据 | 用于自定义后处理（如生成 Heightmap） |
| `float GetLowestGeneratedHeight()` | - | 已生成地形最低高度 |
| `float GetHighestGeneratedHeight()` | - | 已生成地形最高高度 |

### 4.8 委托 (Delegates / 事件)

可在蓝图中绑定的多播委托（`BlueprintAssignable`）：

| 委托 | 签名 | 触发时机 |
| --- | --- | --- |
| `OnGenerationStartedDelegate` | `(FString GenerationType)` | 一次生成开始时（GenerationType 例：`Terrain`、`Water`、`Nature`、`Actors`、`Everything`） |
| `OnGenerationCompletedDelegate` | `(FString GenerationType, int32 NumTriangles)` | 生成完成时 |
| `OnGenerationFailedDelegate` | `(FString GenerationType, FString Reason)` | 生成失败时 |
| `OnGenerationProgressDelegate` | `(FString Step, float Percentage)` | 生成进度（0~100） |

C++ 委托类型：

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPTGGenerationStartedDelegate, FString, GenerationType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPTGGenerationCompletedDelegate, FString, GenerationType, int32, NumTriangles);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPTGGenerationFailedDelegate, FString, GenerationType, FString, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPTGGenerationProgressDelegate, FString, Step, float, Percentage);
```

---

## 5. 修改器：`APtgModifier`

```cpp
UCLASS(Abstract)
class PROCEDURALTERRAINGENERATOR_API APtgModifier : public AActor
{
    UFUNCTION(BlueprintImplementableEvent, BlueprintPure, Category = "PTG Modifier")
    UShapeComponent* GetShapeComponent() const;
};
```

修改器是一个**带 ShapeComponent**（盒子或球体）的 Actor，用于在 PTG 生成自然/Actor 时**排除或限制**特定区域。

插件已内置：

- `BP_BoxModifier`：基于 `UBoxComponent` 的盒形修改器
- `BP_SphereModifier`：基于 `USphereComponent` 的球形修改器

使用流程：

1. 将 `BP_BoxModifier` 或 `BP_SphereModifier` 拖入关卡，调整位置和大小。
2. 在 `APtgManager` 的 `BiomaNature` / `BiomaActors` 中，将该 Actor 引用加入 `Modifiers` 数组。
3. 通过 `bUseLocationsOutsideModifiers` 控制：
   - `true`：仅在修改器**外**生成（默认，作为排除区）
   - `false`：仅在修改器**内**生成（作为限制区）

如需自定义形状，继承 `APtgModifier` 并实现 `GetShapeComponent()` 蓝图事件返回你的 ShapeComponent 即可。

可选地，可使用插件提供的 Volume 材质 `M_NatureGenerationModifierVolume` 与 MI 实例进行可视化。

---

## 6. 数据结构

### 6.1 `FPtgBiomaNature`

控制一组自然装饰物的生成配置。

| 字段 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `CorrespondingBioma` | `EPtgNatureBiomas` | Earth | 放置区域：地表 / 水下 / 全部 |
| `CullDistance` | `int32` | 150000 | 剔除距离（cm），0 表示无限 |
| `bEnableDensityScaling` | `bool` | false | 启用 ISMC 密度缩放（仅适合无碰撞小物体如草） |
| `bCastShadow` | `bool` | true | 是否投影 |
| `bCanAffectNavigation` | `bool` | false | **慎用**！开启会大幅拖慢生成 |
| `bGenerateOverlapEvents` | `bool` | false | 重叠事件 |
| `CollisionEnabled` | `ECollisionEnabled` | QueryAndPhysics | 碰撞模式 |
| `CollisionObjectType` | `ECollisionChannel` | WorldStatic | 碰撞通道 |
| `Meshes` | `TArray<UStaticMesh*>` | - | 候选 Mesh 池（运行时随机抽取） |
| `MinMeshesToSpawn` / `MaxMeshesToSpawn` | `int32` | 100 / 1000 | 数量范围 |
| `MinMaxScale` | `FVector2D` | (0.5, 3.0) | 随机缩放范围 |
| `RotationType` | `EPtgNatureRotationTypes` | TerrainShapeNormal | 朝向：随机 / 地形面法线 / Mesh 表面法线 |
| `Seed` | `int32` | 1619 | 此 Bioma 自然生成种子 |
| `Modifiers` | `TArray<APtgModifier*>` | - | 区域过滤器列表 |
| `bUseLocationsOutsideModifiers` | `bool` | true | true=排除，false=仅在内 |
| `HeightPercentageRangeToLocateNatureMeshes` | `FVector2D` | (0, 100) | 高度百分比范围 |
| `bUseLocationsOutsideHeightRange` | `bool` | false | 高度范围反转 |

### 6.2 `FPtgBiomaActors`

类似 `FPtgBiomaNature`，但生成的是 `AActor`：

| 字段 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `CorrespondingBioma` | `EPtgNatureBiomas` | Earth | 放置区域 |
| `CullDistance` | `int32` | 150000 | 剔除距离 |
| `ActorClass` | `TSubclassOf<AActor>` | null | 要生成的 Actor 类 |
| `MinActorsToSpawn` / `MaxActorsToSpawn` | `int32` | 5 / 30 | 数量范围 |
| `MinMaxScale` | `FVector2D` | (1.0, 1.0) | 缩放范围 |
| `RotationType` | `EPtgNatureRotationTypes` | TerrainShapeNormal | 朝向类型 |
| `Seed` | `int32` | 1619 | 种子 |
| `Modifiers` | `TArray<APtgModifier*>` | - | 修改器 |
| `bUseLocationsOutsideModifiers` | `bool` | true | 修改器使用方式 |
| `HeightPercentageRangeToLocateActors` | `FVector2D` | (0, 100) | 高度范围 |
| `bUseLocationsOutsideHeightRange` | `bool` | false | 高度范围反转 |

### 6.3 `FPtgProcMeshData` 与枚举 `EPtgProcMeshShapes`

```cpp
UENUM(BlueprintType)
enum class EPtgProcMeshShapes : uint8
{
    Plane,
    Cube,
    Sphere   // 实际为球形化立方体
};

USTRUCT(BlueprintType)
struct FPtgProcMeshData
{
    int32 SectionIndex;
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FLinearColor> VertexColors;
    TArray<FRuntimeMeshTangent> Tangents;
    bool bEnableCollision;
    // ...
};
```

其他常用枚举：

- `EPtgWaterHeightGenerationType { RandomPercentage, FixedPercentage, FixedHeight }`
- `EPtgNatureBiomas { Earth, Underwater, Both }`
- `EPtgNatureRotationTypes { Random, TerrainShapeNormal, MeshSurfaceNormal }`
- `EPtgDebugMessageTypes { Info, Warning, Error }`

---

## 7. 蓝图函数库

### 7.1 `UPtgProcMeshDataHelper`

> 头文件：`PtgProcMeshDataHelper.h`

```cpp
UFUNCTION(BlueprintCallable, Category = "PTG Procedural Mesh Data Helper")
static FPtgProcMeshData GenerateProcMeshData(
    float& lowestGeneratedHeight,
    float& highestGeneratedHeight,
    EPtgProcMeshShapes procMeshShape = EPtgProcMeshShapes::Plane,
    float radius = 15000.0f,
    int32 resolution = 150,
    bool bUseTerrainTiling = false,
    UPtgFastNoiseLiteWrapper* fastNoiseLiteWrapper = nullptr,
    float noiseInput = 1.0f,
    float noiseOutput = 1000.0f,
    const FVector& actorLocation = FVector::ZeroVector
);
```

按指定形状生成程序化 Mesh 数据，可选地基于传入的 `UPtgFastNoiseLiteWrapper` 应用噪声。

C++ 还提供更细粒度的：

- `GeneratePlaneData(...)`
- `GenerateCubeData(...)`
- `GenerateSphereData(...)`

### 7.2 `UPtgUtils`

```cpp
UFUNCTION(BlueprintCallable, Category = "PTG Utils")
static void PrintDebugMessage(
    const UObject* caller = nullptr,
    const FString& msg = TEXT(""),
    EPtgDebugMessageTypes type = EPtgDebugMessageTypes::Info,
    float timeOnScreen = 5.0f
);
```

向屏幕和日志输出带颜色的调试信息（Info=白，Warning=黄，Error=红）。

---

## 8. 噪声封装：`UPtgFastNoiseLiteWrapper`

`UPtgFastNoiseLiteWrapper` 是对第三方库 [FastNoiseLite](https://github.com/Auburn/FastNoiseLite) 的 UObject 封装，可在蓝图直接创建/配置后传给 `UPtgProcMeshDataHelper::GenerateProcMeshData`。

```cpp
UFUNCTION(BlueprintCallable, Category = "PTG Fast Noise Lite Wrapper")
void SetupFastNoiseLite(
    int seed = 1619,
    float frequency = 0.02f,
    EPtgFastNoiseLiteWrapperNoiseType noiseType = ...,
    EPtgFastNoiseLiteWrapperRotationType3D rotationType3D = ...,
    EPtgFastNoiseLiteWrapperFractalType fractalType = ...,
    int octaves = 5,
    float lacunarity = 2.0f,
    float gain = 0.5f,
    float weightedStrength = 0.0f,
    float pingPongStrength = 2.0f,
    EPtgFastNoiseLiteWrapperCellularDistanceFunction cellularDistanceFunction = ...,
    EPtgFastNoiseLiteWrapperCellularReturnType cellularReturnType = ...,
    float cellularJitter = 1.0f,
    EPtgFastNoiseLiteWrapperDomainWarpType domainWarpType = ...,
    float domainWarpAmp = 30.0f
);

bool IsInitialized() const;
float GetNoise2D(float x, float y);
float GetNoise3D(float x, float y, float z = 0.0f);
```

完整 Setter 列表（均带蓝图可调用）：

- 通用：`SetSeed`、`SetFrequency`、`SetNoiseType`、`SetRotationType3D`
- 分形：`SetFractalType`、`SetFractalOctaves`、`SetFractalLacunarity`、`SetFractalGain`、`SetFractalWeightedStrength`、`SetFractalPingPongStrength`
- Cellular：`SetCellularDistanceFunction`、`SetCellularReturnType`、`SetCellularJitter`
- DomainWarp：`SetDomainWarpType`、`SetDomainWarpAmp`

> 提示：`APtgManager` 内部已经持有一个 `UPtgFastNoiseLiteWrapper` 子对象，无需手动创建。仅当你想在 PTG 之外**单独使用噪声**时才需要自行 NewObject。

---

## 9. 编辑器扩展

模块 `ProceduralTerrainGeneratorEditor` 为 `APtgManager` 的 Details 面板注入两个按钮：

| 按钮 | 功能 |
| --- | --- |
| **Convert Terrain to Static Mesh** | 把当前 Procedural Terrain 网格转换为 `UStaticMesh` 资产（自动构建 BodySetup、`CTF_UseComplexAsSimple`），需选择保存路径 |
| **Create Terrain Heightmap** | 把当前地形顶点高度导出为 PNG 图片（**仅 `Plane` 形状可用**），默认文件名 `PTG_Heightmap.png` |

底层实现：`Source/ProceduralTerrainGeneratorEditor/Private/PTG_EditorDetails.cpp` 中的 `FPTG_EditorDetails::CustomizeDetails`。

按钮可用条件：

- 转换 StaticMesh：当前 Manager 已生成有效 RuntimeMesh
- 创建 Heightmap：地形形状为 `Plane`，且已生成顶点数据

---

## 10. 内置内容资产

路径：`Plugins/ProceduralTerrainGenerator Content/ProceduralTerrainGenerator/`

| 资产 | 说明 |
| --- | --- |
| `BP_PTG_Manager` | 推荐直接拖入关卡使用的 PTG 管理器蓝图 |
| `Modifiers/BP_BoxModifier` | 盒形修改器 |
| `Modifiers/BP_SphereModifier` | 球形修改器 |
| `Materials/M_NatureGenerationModifierVolume` | 修改器可视化材质 |
| `Materials/MI_NatureGenerationModifierVolume` | 同上的材质实例 |
| `Meshes/SM_Cube`、`Meshes/SM_Sphere` | 修改器使用的基础网格 |
| `Textures/T_PTG_ManagerLogo` | Manager 在编辑器中的图标 |

---

## 11. 常见使用流程示例

### 11.1 蓝图：在运行时随机生成一个新地形

```text
[BeginPlay]
   └─ Get All Actors Of Class (BP_PTG_Manager)
        └─ For Each Actor
             └─ Call "Generate Everything With Random Seed"
```

### 11.2 蓝图：监听生成完成事件

```text
[Event BeginPlay]
   └─ Bind Event to OnGenerationCompletedDelegate (Target = PtgManager)
       └─ Custom Event "OnPTGCompleted" (GenerationType: String, NumTriangles: Int)
            └─ Print String  "Type=", GenerationType, " Tris=", NumTriangles
```

### 11.3 C++：手动控制噪声生成数据

```cpp
#include "PtgFastNoiseLiteWrapper.h"
#include "PtgProcMeshDataHelper.h"

UPtgFastNoiseLiteWrapper* Noise = NewObject<UPtgFastNoiseLiteWrapper>(this);
Noise->SetupFastNoiseLite(/*seed*/ 42, /*frequency*/ 0.01f);

float MinH, MaxH;
FPtgProcMeshData Data = UPtgProcMeshDataHelper::GenerateProcMeshData(
    MinH, MaxH,
    EPtgProcMeshShapes::Plane,
    /*radius*/ 30000.f,
    /*resolution*/ 200,
    /*tiling*/ false,
    Noise,
    /*noiseInput*/ 1.0f,
    /*noiseOutput*/ 1500.f,
    GetActorLocation()
);
// 之后可用 Data 喂给你的 RuntimeMesh / ProceduralMeshComponent
```

### 11.4 仅生成水/自然/Actor

```cpp
APtgManager* Mgr = ...;
Mgr->GenerateTerrainMeshWithCustomSeed(1234);
Mgr->GenerateWaterMesh();
Mgr->GenerateNatureWithRandomSeed();
Mgr->GenerateActors();
```

---

## 12. 注意事项 & FAQ

- **`APtgManager` 是 Abstract**：必须使用蓝图子类（如 `BP_PTG_Manager`），直接拖 `APtgManager` 会被引擎拒绝。
- **Sphere 形状 ≠ 真球**：内部为"球形化立方体"，UV 分布与真球略有差异。
- **`FixedHeight` 水位仅 Plane**：Cube/Sphere 形态选择此模式不生效。
- **`bCanAffectNavigation = true` 慎用**：插件作者明确警告会显著拖慢自然生成。
- **Tiling 拼接**：仅 Plane 形态支持；同时要保持多个 Manager 的 `Radius`、`Resolution`、噪声参数完全一致。
- **LOD**：当 `NumberOfLODs = 1` 时不生成 LOD；最大值为 8。
- **生成进度**：在编辑器中通过 `FScopedSlowTask` 显示，运行时通过 `OnGenerationProgressDelegate` 监听。
- **Heightmap 导出**：仅 Plane 地形可用，结果是单通道高度的 PNG。
- **Standalone 崩溃修复**：v2.0.3 已修复 UE5.4.3 Standalone 模式崩溃问题。

---

## 13. 版本历史

> 完整记录见 `Documentation/Version_notes.txt`

| 版本 | 主要变更 |
| --- | --- |
| **2.0.4**（当前） | 修复弃用警告；新增委托；新增 `FScopedSlowTask` 显示进度；优化与改进 |
| 2.0.3 | 修复 UE5.4.3 Standalone 崩溃 |
| 2.0.2 | 自然生成新增更多自定义项 |
| 2.0.1 | 修复 Linux 下 `FastNoiseLite.h` 编译问题 |
| 2.0 | 大幅重构；新增 LOD；合并 Procedural Nature Generator 功能（修改器/高度选项）；启用 FastNoiseLite；RMC 升级到 4.1.5；新增大量编辑器/蓝图函数；移除 Custom Mesh |
| 1.4 | 支持从 Plane 地形导出 PNG Heightmap |
| 1.3 | RMC 升级 v4；修复 Mac/UE4.25 崩溃；自然 Cull Distance 可分别配置；支持 PN Triangles 镶嵌材质 |
| 1.2 | Plane 地形 Tiling；水面新增 Fixed Height 模式 |
| 1.1 | 自然/Actor 算法优化；3 种旋转类型；Cull Distance；导出 StaticMesh |

---

## 联系方式

- 作者主页：<https://www.unrealengine.com/marketplace/en-US/profile/Rockam>
- 商城页面：`com.epicgames.launcher://ue/marketplace/content/2e707e78b69540aeb7464b7c4e0e9c7b`
- 支持邮箱：norlun5@gmail.com
- 在线 PDF 文档：<https://drive.google.com/file/d/1Pvt5R2dNisqAcojoBX736JU7QWA6kjVi/view>

