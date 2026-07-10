# W4：生物群系分类（TerrainTags 模块 + DataAsset 驱动评分器）

> 上游主稿：[WorldGenDesign.md §5.6 / §6 / §11](WorldGenDesign.md)
>
> 平行/历史详稿：[W1_ModuleSkeleton.md](W1_ModuleSkeleton.md) / [W2_PlatesAndLandSea.md](W2_PlatesAndLandSea.md) / [W3_ScalarFields.md](W3_ScalarFields.md)
>
> 协同纪律：[AgentWorkflow.md](AgentWorkflow.md)（§1.3 8 章 + 附录、§1.4.0 字段名以源码为准、§3.6 法线、§3.7 东向公式）
>
> 关键参考：[TechnicalDesign.md §0.2 数据驱动原则](TechnicalDesign.md)、[TechnicalDesign.md §3](TechnicalDesign.md)（TerrainTags 模块草稿）

---

## 0. W4 一句话目标

> 在 [`Source/`](../Source) 下新建 `TerrainTags` 模块（`UTerrainDefinition` + `UTerrainSet` + `FTerrainClimateRule`），把"E/M/T → TerrainTag"的分类规则**完全落到 DataAsset**；17 个 `Terrain.*` GameplayTag 通过 [`Config/Tags/Terrain.ini`](../Config/Tags/Terrain.ini) 集中注册（**不**走 cpp `FNativeGameplayTag` 静态绑定——既然规则全靠 DataAsset 驱动，cpp 端无需任何强类型 Tag 句柄做语义判断）；`FWorldGenerator::Step_ClassifyBiomes()` 仅作为评分器（`argmax_{Def} Def->ScoreFor(Sample)`），不知道任何具体生物群系语义；`RebuildCellAttrLUT_()` 直接写 `Best->LayerIndex` 入 LUT.R，正式接 R8。

### 0.1 状态对比矩阵

| 维度 | W3 完成时 | W4 完成时 |
| --- | --- | --- |
| `Source/TerrainTags/` 模块 | ❌ 不存在 | ✅ 3 个文件（`UTerrainDefinition.h/cpp` + `UTerrainSet.h/cpp` + `TerrainTags.Build.cs`/`.h`/`.cpp`），**无** `TerrainTagsNative.*`（Tag 走 ini 注册） |
| 17 个 `Terrain.*` GameplayTag | ❌ 仅 [TechnicalDesign.md §3.1](TechnicalDesign.md) 草稿 | ✅ [`Config/Tags/Terrain.ini`](../Config/Tags/Terrain.ini) 集中注册（DataAsset 端 `EditAnywhere FGameplayTag TerrainTag` 自动从 ini 拉下拉框） |
| 17 个 `UTerrainDefinition` DataAsset | ❌ 不存在 | ✅ `Content/Data/Terrains/DA_Terrain_*.uasset` × 17（LayerIdx 0-15+18，16/17/19 为 R9 Decor 与 Sentinel 预留）|
| `DA_TerrainSet_Default` | ❌ 不存在 | ✅ 一份 `UTerrainSet`，`TerrainDefs` 数组装 17 个 Def（LayerIndex 不连续允许）|
| `FWorldGenSettings.TerrainSet` 字段 | `TSoftObjectPtr<UDataAsset> BiomeTable`（占位） | `TSoftObjectPtr<UTerrainSet> TerrainSet`（指向 `DA_TerrainSet_Default`） |
| `Step_ClassifyBiomes()` | 空体 | 评分器：双层 for 循环 × `Def->ScoreFor(Sample)` × argmax |
| `FCellGeoData.TerrainTag` | None | 19 个 `Terrain.*` Tag 之一 |
| `RebuildCellAttrLUT_()` R 通道 | `bIsLand ? 4 : 0`（W2 占位） | `Best->LayerIndex`（DataAsset 真实数据） |
| `EWorldGenDebugView` | None/PlateId/LandSea/Elevation/Moisture/Temperature/Mountain | 追加 `Biome`（默认值） |
| 反射诊断 R7 Inputs | 14 项 | **仍 14 项**（材质零改动） |
| 球面视觉 | W3 测试材质（红蓝热图 / 5 种 W2-W3 单色 LUT） | **19 layer 真实生物群系分布**（首次接 R7 Triplanar） |

### 0.2 W4 不做什么（明确边界）

- ❌ 不引入 `UBiomeTable`（原 W7 计划）—— `ClimateRules` 直接挂在 `UTerrainDefinition` 自身，无中间 BiomeTable 资产
- ❌ 不在 cpp 内硬编码 5×5 Whittaker 表的任何分支
- ❌ 不在 `FCellGeoData` 中持 `LayerIndex` 字段（GridRender 渲染期直接 `Def->LayerIndex` 查；`FCellGeoData` 只持 `TerrainTag`）
- ❌ 不引入 `UPlateProfile`（板块外观差异化推迟到 W6 视情况）
- ❌ 不实现 W7 计划中的"双线性 + 邻域多数表决"边界平滑——若 W4 实测出现马赛克再单独迭代；硬区间 `ScoreFor` 起步
- ❌ 不动 R7 材质资产、不动反射诊断 14 项 Inputs（跨期同构性硬承诺）
- ❌ 不实现"Volcano"（Layer 19）—— Layer 16/17/19 在 W4 阶段保留为"空资产 / 未挂"的占位，不进 `DA_TerrainSet_Default`

---

## 目录

- [0. W4 一句话目标](#0-w4-一句话目标)
- [1. 设计依据（W4 架构拍板回顾）](#1-设计依据w4-架构拍板回顾)
- [2. TerrainTags 模块设计](#2-terraintags-模块设计)
  - [2.1 UTerrainDefinition / UTerrainSet / FTerrainClimateRule](#21-uterraindefinition--uterrainset--fterrainclimaterule)
  - [2.2 ScoreFor 评估算法](#22-scorefor-评估算法)
  - [2.3 17 个 Terrain.* GameplayTag（ini 注册）](#23-17-个-terrain-gameplaytagini-注册)
- [2.4 17 个 UTerrainDefinition DataAsset 完整初始值表](#24-17-个-uterraindefinition-dataasset-完整初始值表)
- [3. WorldGen 端改动](#3-worldgen-端改动)
  - [3.1 FWorldGenSettings.TerrainSet 字段切换](#31-fworldgensettingsterrainset-字段切换)
  - [3.2 Step_ClassifyBiomes 评分器实现](#32-step_classifybiomes-评分器实现)
  - [3.3 LUT 写入 + EWorldGenDebugView::Biome](#33-lut-写入--eworldgendebugviewbiome)
- [4. 实施步骤（cpp 落地清单）](#4-实施步骤cpp-落地清单)
- [5. 验收清单](#5-验收清单)
- [6. 排错表](#6-排错表)
- [7. 路径预告与衔接](#7-路径预告与衔接)
- [附录 A：可粘贴 cpp 全文](#附录-a可粘贴-cpp-全文)
- [附录 B：跨阶段同构性（W7 取消、与 R8/R9/R11 关系）](#附录-b跨阶段同构性w7-取消与-r8r9r11-关系)

---

## 1. 设计依据（W4 架构拍板回顾）

### 1.1 用户在 W3 验收后提出的关键质疑（2026-06-28）

> "用 Elevation/Moisture/Temperature/bIsMountain/bIsLand/bIsCoast 等生成对应的 Terrain，这个功能应该放在哪里？如果这个功能放在 WorldGen，但 TerrainTags 的 GameplayTags 是外部引入的，如果用硬编码的方式，直接把 Elevation 等地形参数映射到 GameplayTags，那样就失去了 Tags 的意义。因此这个功能必须放在 TerrainTags，即每种地形应该匹配哪些 Elevation/Moisture/Temperature 等参数也属于它的一个属性，可以通过 GameplayTags 在编辑器里无需编译源代码即可调整。"

这条质疑直接触及 [TechnicalDesign.md §0.2 数据驱动原则](TechnicalDesign.md)：**玩法不写死在 C++ 代码里。所有规则要素通过 GameplayTag + DataAsset 拆出来，由设计师编辑。** 原 [WorldGenDesign §5.6](WorldGenDesign.md) 的"cpp 硬编码 5×5 Whittaker 表 + 海陆/海岸/山脉短路链 + W7 后再 DataAsset 化"方案是直接违背原则的——W4 → W7 之间 cpp 算法本身要被推翻重写一次，工作量翻倍且窗口期内 TerrainTag 沦为字符串别名。

### 1.2 拍板结果（5 个权衡点）

| # | 问题 | 拍板 | 理由 |
| --- | --- | --- | --- |
| Q1 | 分类规则归属哪里？ | **方案 A**：规则属于 `UTerrainDefinition` 自身（`ClimateRules[]` 字段） | 每个地形是"自描述"的；新增地形 = 新建 DataAsset，零代码 |
| ⚠ Q1' | 几何短路（海/陆/山）是否硬编码？ | **不硬编码**。`Placement` 字段表达 Land/Ocean/Coast/Mountain | "海洋深度阈值/海岸 1-ring 范围/山脉阈值"虽然 W2/W3 已经写死，但"什么叫海洋"是设计语义；统一交给 `ClimateRules.Placement` |
| Q2 | TerrainSet 怎么暴露？ | `FWorldGenSettings::TerrainSet : TSoftObjectPtr<UTerrainSet>` 显式指向 | 关卡可携带不同地形集；避免全局副作用 DataAsset |
| Q3 | ScoreFor 初版如何实现？ | **硬区间命中 (1.0/0.0) + Priority 排序**起步；后期可升平滑评分 | 与原 §12 风险表"邻域多数表决"对齐 |
| Q4 | 是否合并 W4 和 W7？ | **合并**。原 W7 不再独立 | 既然 W4 一步到位 DataAsset 驱动，W7 没什么留下来的事 |
| Q5 | LayerIndex 在哪里中转？ | **GridRender 端直接读 `Def->LayerIndex`**，不在 `FCellGeoData` 中转 | 与主稿 §10.1 既有契约一致；FCellGeoData 只持 `TerrainTag` |

### 1.3 字段名核对表（W4 cpp 落地前 grep / 读 .h 已核对，遵循 [AgentWorkflow §1.4.0](AgentWorkflow.md)）

| 调用点 | 错误猜测 | 真实字段（源码） |
| --- | --- | --- |
| `FCellGeoData.bIsLand` | `IsLand` / `IsLandFlag` | `bIsLand`（uint8 : 1，[CellGeoData.h:36](../Source/WorldGen/Public/CellGeoData.h)） |
| `FCellGeoData.bIsCoast` | `IsCoast` | `bIsCoast`（同上） |
| `FCellGeoData.bIsMountain` | `IsMountain` | `bIsMountain`（同上） |
| `FCellGeoData.TerrainTag` | `Terrain` / `Tag` | `TerrainTag`（[CellGeoData.h:43](../Source/WorldGen/Public/CellGeoData.h)） |
| `FCellGeoData.Resources` | `ResourceTags` | `Resources`（同上 `FGameplayTagContainer`） |
| `FWorldGenSettings.TerrainSet`（W4 新增） | `BiomeTable` / `TerrainTable` | 本稿统一为 `TerrainSet`（替换原 `BiomeTable` 占位） |
| `FWorldGenerator::CellData` 访问 | `GetCells()` / `Cells` | `GetCellData()` 返回 `const TArray<FCellGeoData>&`（[WorldGenerator.h:32](../Source/WorldGen/Public/WorldGenerator.h)） |

---

## 2. TerrainTags 模块设计

### 2.1 UTerrainDefinition / UTerrainSet / FTerrainClimateRule

```cpp
// Source/TerrainTags/Public/TerrainPlacementMask.h
UENUM(BlueprintType)
enum class ETerrainPlacementMask : uint8
{
    Any       UMETA(DisplayName = "任意（无几何要求）"),
    Land      UMETA(DisplayName = "陆地（bIsLand && !bIsCoast && !bIsMountain）"),
    Ocean     UMETA(DisplayName = "海洋（!bIsLand）"),
    Coast     UMETA(DisplayName = "海岸（bIsCoast）"),
    Mountain  UMETA(DisplayName = "山脉（bIsMountain）"),
};
```

```cpp
// Source/TerrainTags/Public/TerrainClimateRule.h
USTRUCT(BlueprintType)
struct TERRAINTAGS_API FTerrainClimateRule
{
    GENERATED_BODY()

    /** 几何/Placement 要求；不匹配直接得 0 分。 */
    UPROPERTY(EditAnywhere, Category = "Placement")
    ETerrainPlacementMask Placement = ETerrainPlacementMask::Land;

    /** 温度区间 [Min, Max]（FCellGeoData::Temperature ∈ [-1, 1]）。 */
    UPROPERTY(EditAnywhere, Category = "Climate")
    FFloatInterval Temperature {-1.f, 1.f};

    /** 湿度区间 [Min, Max]（FCellGeoData::Moisture ∈ [0, 1]）。 */
    UPROPERTY(EditAnywhere, Category = "Climate")
    FFloatInterval Moisture {0.f, 1.f};

    /** 高程区间 [Min, Max]（FCellGeoData::Elevation ∈ [-1, 1]）。 */
    UPROPERTY(EditAnywhere, Category = "Climate")
    FFloatInterval Elevation {-1.f, 1.f};

    /** 命中后的得分；用于消歧（多 Def 同时命中时取最高 Priority）。 */
    UPROPERTY(EditAnywhere, Category = "Score", meta = (ClampMin = "0.01"))
    float Priority = 1.f;
};
```

```cpp
// Source/TerrainTags/Public/TerrainDefinition.h
class UTerrainDefinition;  // forward

USTRUCT()
struct FClimateSample
{
    GENERATED_BODY()

    float Elevation   = 0.f;
    float Moisture    = 0.5f;
    float Temperature = 0.f;
    bool  bIsLand     = true;
    bool  bIsCoast    = false;
    bool  bIsMountain = false;
};

UCLASS(BlueprintType)
class TERRAINTAGS_API UTerrainDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** 关联的 Terrain.* GameplayTag（与 Config/Tags/Terrain.ini 中注册的 Tag 名严格一致；编辑器内通过下拉框选择）。 */
    UPROPERTY(EditAnywhere, Category = "Identity")
    FGameplayTag TerrainTag;

    /**
     * 渲染层 LayerIndex（CellAttrLUT.R 写入值，对应 Texture2DArray slice）。
     * 范围 [0, 19]；预留 [20, 255] 给 R9 Decor/Owner/Fog。
     */
    UPROPERTY(EditAnywhere, Category = "Identity", meta = (ClampMin = "0", ClampMax = "255"))
    int32 LayerIndex = 0;

    /**
     * 适用条件；OR 语义——任一条规则命中即"此地形适用于该 cell"。
     * Score = max_{Rule} (匹配 ? Rule.Priority : 0)。
     */
    UPROPERTY(EditAnywhere, Category = "Placement")
    TArray<FTerrainClimateRule> ClimateRules;

    /** 命中此地形时拷贝到 FCellGeoData::Resources（W4 仅拷贝、不读取）。 */
    UPROPERTY(EditAnywhere, Category = "Gameplay")
    FGameplayTagContainer DefaultResources;

    // ───── W4 不消费、为 Gameplay/GridRender 预留的字段 ─────
    UPROPERTY(EditAnywhere, Category = "Gameplay") float MoveCostBase = 1.f;
    UPROPERTY(EditAnywhere, Category = "Gameplay") float DefenseBonus = 0.f;
    UPROPERTY(EditAnywhere, Category = "Gameplay") bool  bAllowBuilding = true;
    UPROPERTY(EditAnywhere, Category = "Gameplay") FGameplayTagContainer ImpassableFor;

    /** 评估匹配分数。0 = 不匹配；越大越优先。详见 §2.2。 */
    float ScoreFor(const FClimateSample& Sample) const;
};
```

```cpp
// Source/TerrainTags/Public/TerrainSet.h
UCLASS(BlueprintType)
class TERRAINTAGS_API UTerrainSet : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /**
     * 持有的 UTerrainDefinition 引用。FWorldGenerator::Step_ClassifyBiomes 遍历此数组。
     * 顺序 = LayerIndex 顺序（约定：第 i 项 LayerIndex = i），方便编辑器一眼对照。
     * 不强制；运行时按 Def->LayerIndex 字段为准。
     */
    UPROPERTY(EditAnywhere, Category = "Set")
    TArray<TSoftObjectPtr<UTerrainDefinition>> TerrainDefs;

    /** 查询期一次性 Resolve 全部软引用，Step_ClassifyBiomes 调用前必须先 Load 完成。 */
    void LoadSynchronous();
    const TArray<UTerrainDefinition*>& GetLoadedDefs() const { return LoadedDefsCache; }

private:
    UPROPERTY(Transient) TArray<TObjectPtr<UTerrainDefinition>> LoadedDefsCache;
};
```

> **为什么 `TSoftObjectPtr` 而非 `TObjectPtr`**：`UTerrainSet` 本身是 DataAsset，被 `FWorldGenSettings::TerrainSet`（也是 SoftObjectPtr）持有；如果 `UTerrainSet` 强引用 19 个 Def，编辑器打开 SettingsActor 时会瀑布式硬加载所有 Def → 不必要的启动开销。`LoadSynchronous()` 在 `Step_ClassifyBiomes` 入口处被调用，一次性把全部 Def 加载并 cache 到 `LoadedDefsCache`（运行时只 GC 持有，避免再次 Load）。

### 2.2 ScoreFor 评估算法

```cpp
// Source/TerrainTags/Private/TerrainDefinition.cpp
float UTerrainDefinition::ScoreFor(const FClimateSample& S) const
{
    float Best = 0.f;
    for (const FTerrainClimateRule& Rule : ClimateRules)
    {
        // 1) Placement 几何要求
        bool bPlacementOK = false;
        switch (Rule.Placement)
        {
            case ETerrainPlacementMask::Any:      bPlacementOK = true; break;
            case ETerrainPlacementMask::Land:     bPlacementOK =  S.bIsLand && !S.bIsCoast && !S.bIsMountain; break;
            case ETerrainPlacementMask::Ocean:    bPlacementOK = !S.bIsLand; break;
            case ETerrainPlacementMask::Coast:    bPlacementOK =  S.bIsLand &&  S.bIsCoast; break;
            case ETerrainPlacementMask::Mountain: bPlacementOK =  S.bIsLand &&  S.bIsMountain; break;
            default: bPlacementOK = false; break;
        }
        if (!bPlacementOK) continue;

        // 2) 三轴硬区间命中（W4 起步：1.0/0.0 不平滑；W4.5+ 可升 smoothstep）
        const bool bTOk = (S.Temperature >= Rule.Temperature.Min) && (S.Temperature <= Rule.Temperature.Max);
        const bool bMOk = (S.Moisture    >= Rule.Moisture.Min   ) && (S.Moisture    <= Rule.Moisture.Max);
        const bool bEOk = (S.Elevation   >= Rule.Elevation.Min  ) && (S.Elevation   <= Rule.Elevation.Max);
        if (!(bTOk && bMOk && bEOk)) continue;

        // 3) 命中：返回该规则的 Priority；同 Def 多条规则取最大
        Best = FMath::Max(Best, Rule.Priority);
    }
    return Best;
}
```

**评分语义（关键）**：

- 单条规则命中 → 返回 `Priority`；不命中 → 0
- 同 Def 多条规则之间 OR 语义 → 取 max
- WorldGen 端 argmax 跨 Def → 取分数最高的 Def
- **冲突消歧靠 Priority**：例如 `Forest.Tropical` 与 `Wetland` 都覆盖 `T=0.9, M=0.95` 时，让 `Wetland.ClimateRules[0].Priority = 1.5 > Forest.Tropical.Priority = 1.0`，则 Wetland 胜
- **覆盖盲区**：如果某个 (E,M,T,Placement) 没有任何 Def 命中（最高分仍为 0），WorldGen 端 fallback 到一个固定 Tag（约定 `Terrain.Plain.Grass`，见 [§3.2](#32-step_classifybiomes-评分器实现) Sentinel 处理）

### 2.3 17 个 Terrain.* GameplayTag（ini 注册）

> ⚡ **关键决策**：Tag 注册走 ini 而**不**走 cpp `FNativeGameplayTag`。理由——既然 TerrainTags 模块所有规则要素都通过 DataAsset 拆出来（[TechnicalDesign §0.2](TechnicalDesign.md) 数据驱动原则），cpp 端**根本不需要拿到强类型 Tag 句柄**：`Step_ClassifyBiomes` 只比较 `Def->TerrainTag` 与 `Def->TerrainTag`（Tag 之间的 `==`），从不拿"`Terrain.Plain.Grass` 这个具体语义"做语义分支。如果改 ini 注册，cpp 0 行变更即可新增/删除/重命名 Tag，与"规则在 DataAsset、不在 cpp"完全同构；用 `FNativeGameplayTag` 反而把 17 个具体 Tag 名钉在 cpp 里，等于半数据驱动半硬编码。

#### 注册方式

创建 [`Config/Tags/Terrain.ini`](../Config/Tags/Terrain.ini)：

```ini
[/Script/GameplayTags.GameplayTagsList]
+GameplayTagList=(Tag="Terrain.Ocean.Deep",        DevComment="深海，Elevation 远低于 SeaLevel")
+GameplayTagList=(Tag="Terrain.Ocean.Shallow",     DevComment="浅海")
+GameplayTagList=(Tag="Terrain.Coast.Beach",       DevComment="沙滩海岸")
+GameplayTagList=(Tag="Terrain.Coast.Rocky",       DevComment="岩石海岸")
+GameplayTagList=(Tag="Terrain.Plain.Grass",       DevComment="温带草原")
+GameplayTagList=(Tag="Terrain.Plain.Savanna",     DevComment="热带草原")
+GameplayTagList=(Tag="Terrain.Forest.Temperate",  DevComment="温带森林")
+GameplayTagList=(Tag="Terrain.Forest.Tropical",   DevComment="热带雨林")
+GameplayTagList=(Tag="Terrain.Forest.Taiga",      DevComment="针叶林")
+GameplayTagList=(Tag="Terrain.Wetland",           DevComment="湿地")
+GameplayTagList=(Tag="Terrain.Desert.Sand",       DevComment="沙漠")
+GameplayTagList=(Tag="Terrain.Desert.Rocky",      DevComment="戈壁")
+GameplayTagList=(Tag="Terrain.Mountain.Hill",     DevComment="山丘")
+GameplayTagList=(Tag="Terrain.Mountain.Peak",     DevComment="山峰")
+GameplayTagList=(Tag="Terrain.Mountain.Snow",     DevComment="雪山")
+GameplayTagList=(Tag="Terrain.Tundra",            DevComment="苔原")
+GameplayTagList=(Tag="Terrain.Glacier",           DevComment="冰川")
```

并在 [`Config/DefaultGameplayTags.ini`](../Config/DefaultGameplayTags.ini) 末尾追加（让引擎扫描这份 ini）：

```ini
[/Script/GameplayTags.GameplayTagsSettings]
+GameplayTagTableList=/Game/...   ; 已有的（如有）
+GameplayTagSource=Terrain.ini
```

> 实操中也可不写 `GameplayTagSource`——`UGameplayTagsManager` 默认会扫描 `Config/Tags/*.ini`；以工程实测为准。

#### cpp 端的零句柄消费

- `UTerrainDefinition::TerrainTag` 是 `FGameplayTag`（Editor 下拉框，从 ini 表自动拉取）
- `Step_ClassifyBiomes` 中**不**通过名字访问任何 Tag——Sentinel fallback 通过 "`Settings.SentinelTerrainTag` 字段（也是 `FGameplayTag`）"指向（详见 §3.2），由设计师在 `FWorldGenSettings` 编辑器面板内挂上 `Terrain.Plain.Grass`，cpp 仍不出现该字符串
- `RebuildCellAttrLUT_` 通过 `TMap<FGameplayTag, UTerrainDefinition*>` 反查 `LayerIndex`，也不依赖具体 Tag 名

这条原则使得**新增/删除/重命名一个 Terrain.* Tag 完全不需要重编 cpp**——只改 ini + 改 DataAsset，与「W4 完整数据驱动」的目标自洽。

### 2.4 17 个 UTerrainDefinition DataAsset 完整初始值表

> **核心提示**：本表是"DataAsset 资产首次创建时的建议初始值"。一旦写入 `.uasset`，后续完全靠编辑器调整——cpp 0 行变更。[WorldGenDesign §6 19-Layer 表](WorldGenDesign.md) 与本表一一对应。

#### 2.4.1 表 A：身份 + ClimateRules（W4 评分器消费）

> **字段对照**（与 [`TerrainDefinition.h`](../Source/TerrainTags/Public/TerrainDefinition.h) 严格一致）：`TerrainTag`（FGameplayTag）/ `LayerIndex`（int32）/ `ClimateRules[]`（TArray\<FTerrainClimateRule\>）。<br>
> **ClimateRules 缩写**：`Plc=Placement`（Land/Ocean/Coast/Mountain/Any）、`T=Temperature ∈ [-1,1]`、`M=Moisture ∈ [0,1]`、`E=Elevation ∈ [-1,1]`、`Pri=Priority`（默认 1.0）。区间留空 = 全域 [Min,Max]（不约束）。

| LayerIdx | DataAsset 资产名 | TerrainTag | ClimateRules（OR 语义；Score = max Pri） |
| ---: | --- | --- | --- |
| 0 | `DA_Terrain_OceanDeep` | `Terrain.Ocean.Deep` | `{Plc=Ocean, E=[-1.0,-0.2], Pri=1.0}` |
| 1 | `DA_Terrain_OceanShallow` | `Terrain.Ocean.Shallow` | `{Plc=Ocean, E=[-0.2, 0.0], Pri=1.0}` |
| 2 | `DA_Terrain_CoastBeach` | `Terrain.Coast.Beach` | `{Plc=Coast, E=[ 0.0, 0.1], Pri=1.0}` |
| 3 | `DA_Terrain_CoastRocky` | `Terrain.Coast.Rocky` | `{Plc=Coast, E=[ 0.1, 1.0], Pri=1.5}` <br>（高海拔海岸优先 Rocky 胜出 Beach） |
| 4 | `DA_Terrain_PlainGrass` | `Terrain.Plain.Grass` | `{Plc=Land, T=[ 0.2, 0.7], M=[0.3, 0.7], Pri=1.0}` <br>（**Sentinel 候选**——见 §2.4.4） |
| 5 | `DA_Terrain_PlainSavanna` | `Terrain.Plain.Savanna` | `{Plc=Land, T=[ 0.6, 1.0], M=[0.2, 0.4], Pri=1.0}` |
| 6 | `DA_Terrain_ForestTemperate` | `Terrain.Forest.Temperate` | `{Plc=Land, T=[ 0.3, 0.7], M=[0.6, 0.85], Pri=1.1}` |
| 7 | `DA_Terrain_ForestTropical` | `Terrain.Forest.Tropical` | `{Plc=Land, T=[ 0.7, 1.0], M=[0.7, 1.0], Pri=1.1}` |
| 8 | `DA_Terrain_Wetland` | `Terrain.Wetland` | `{Plc=Land, T=[ 0.4, 1.0], M=[0.95, 1.0], E=[-0.1, 0.2], Pri=1.5}` <br>（高湿 + 近海拔，盖森林） |
| 9 | `DA_Terrain_DesertSand` | `Terrain.Desert.Sand` | `{Plc=Land, T=[ 0.6, 1.0], M=[0.0, 0.2], Pri=1.0}` |
| 10 | `DA_Terrain_DesertRocky` | `Terrain.Desert.Rocky` | `{Plc=Land, T=[ 0.2, 0.6], M=[0.0, 0.2], Pri=1.0}` |
| 11 | `DA_Terrain_MountainHill` | `Terrain.Mountain.Hill` | `{Plc=Mountain, E=[ 0.5, 0.7], Pri=1.0}` |
| 12 | `DA_Terrain_MountainPeak` | `Terrain.Mountain.Peak` | `{Plc=Mountain, E=[ 0.7, 1.0], T=[ 0.3, 1.0], Pri=1.0}` |
| 13 | `DA_Terrain_MountainSnow` | `Terrain.Mountain.Snow` | `{Plc=Mountain, E=[ 0.7, 1.0], T=[-1.0, 0.3], Pri=1.0}` |
| 14 | `DA_Terrain_ForestTaiga` | `Terrain.Forest.Taiga` | `{Plc=Land, T=[ 0.0, 0.3], M=[0.4, 0.85], Pri=1.0}` |
| 15 | `DA_Terrain_Tundra` | `Terrain.Tundra` | `{Plc=Land, T=[ 0.0, 0.2], M=[0.0, 0.4], Pri=1.0}` |
| 18 | `DA_Terrain_Glacier` | `Terrain.Glacier` | `{Plc=Land, T=[-1.0, 0.0], Pri=1.5}` <br>+ `{Plc=Mountain, T=[-1.0, 0.0], Pri=1.5}` <br>（极寒平地 / 极寒山地都判 Glacier） |

> **共 17 行**——与 §2.3 的 17 个 Terrain.* GameplayTag 一一对应。

#### 2.4.2 LayerIndex 跳号说明（16 / 17 / 19 预留）

| LayerIdx | 状态 | 用途 |
| ---: | --- | --- |
| 16 | **预留（W4 不创建）** | 留给 R9 Decor 层（如 `Terrain.Forest.Bamboo`）扩展 |
| 17 | **预留（W4 不创建）** | 留给 R9 Decor 层（如 `Terrain.Plain.Flower`）扩展 |
| 19 | **预留（W4 不创建）** | 占位"未分类"哨兵；R8 Triplanar 默认 Albedo[19] = 紫色 magenta，肉眼可识别"漏填"（详见 [WorldGenDesign §6](WorldGenDesign.md)） |

> **关键**：`DA_TerrainSet_Default.TerrainDefs` 数组只装 17 项 Def（不是 19 项）。渲染端只关心 Def 自身的 `LayerIndex` 字段，与数组索引无关；TerrainDefs 数组**允许 LayerIndex 不连续**。

#### 2.4.3 表 B：Gameplay 默认值（W4 不消费、W7+ 接入）

> 这些字段 W4 评分器不读，仅作为 DataAsset 的**完整初始值**给设计师 / Gameplay 模块未来扩展提供 baseline。`MoveCostBase=1.0` 表示中性；`DefenseBonus=0.0` 表示无加成。

| LayerIdx | DataAsset | MoveCostBase | DefenseBonus | bAllowBuilding | DefaultResources（GameplayTag） | ImpassableFor |
| ---: | --- | ---: | ---: | :---: | --- | --- |
| 0 | `OceanDeep`     | 99.0 | 0.0 | ❌ | `Resource.Sea.Fish, Resource.Sea.DeepFish`            | `Unit.Movement.Land` |
| 1 | `OceanShallow`  |  3.0 | 0.0 | ❌ | `Resource.Sea.Fish`                                    | `Unit.Movement.Land` |
| 2 | `CoastBeach`    |  1.0 | 0.0 | ✅ | `Resource.Sea.Fish`                                    | （空） |
| 3 | `CoastRocky`    |  1.5 | 0.2 | ✅ | `Resource.Stone`                                       | （空） |
| 4 | `PlainGrass`    |  1.0 | 0.0 | ✅ | `Resource.Wheat, Resource.Cattle`                      | （空） |
| 5 | `PlainSavanna`  |  1.0 | 0.0 | ✅ | `Resource.Cattle, Resource.Ivory`                      | （空） |
| 6 | `ForestTemperate` | 1.5 | 0.3 | ✅ | `Resource.Wood, Resource.Fur`                          | （空） |
| 7 | `ForestTropical`  | 1.8 | 0.3 | ✅ | `Resource.Wood, Resource.Spice, Resource.Banana`       | （空） |
| 8 | `Wetland`       |  2.5 | 0.0 | ❌ | `Resource.Reed, Resource.Fish`                         | `Unit.Movement.Heavy` |
| 9 | `DesertSand`    |  1.5 | 0.0 | ✅ | `Resource.Salt, Resource.Oil`                          | （空） |
| 10 | `DesertRocky`  |  1.5 | 0.1 | ✅ | `Resource.Stone, Resource.Iron`                        | （空） |
| 11 | `MountainHill` |  2.0 | 0.5 | ✅ | `Resource.Stone, Resource.Iron, Resource.Coal`         | `Unit.Movement.Heavy` |
| 12 | `MountainPeak` |  4.0 | 0.7 | ❌ | `Resource.Stone, Resource.Gem`                         | `Unit.Movement.Heavy` |
| 13 | `MountainSnow` |  5.0 | 0.6 | ❌ | `Resource.Gem, Resource.Silver`                        | `Unit.Movement.Heavy, Unit.Movement.Wheel` |
| 14 | `ForestTaiga`  |  1.6 | 0.3 | ✅ | `Resource.Wood, Resource.Fur, Resource.Deer`           | （空） |
| 15 | `Tundra`       |  1.4 | 0.0 | ✅ | `Resource.Fur, Resource.Deer`                          | （空） |
| 18 | `Glacier`      |  3.5 | 0.0 | ❌ | `Resource.Ice`                                         | `Unit.Movement.Heavy, Unit.Movement.Wheel` |

> **Resource.* / Unit.Movement.* Tag 注册**：W4 仅写入 DataAsset 字段（FGameplayTagContainer 允许保存未注册 Tag）；这些 Tag 的实际 ini 注册在 W7+ Gameplay 模块上线时补；W4 的反射诊断不会校验它们。

#### 2.4.4 Sentinel 与覆盖盲区核对

**Sentinel 约定**：`FWorldGenSettings::SentinelTerrainTag` 默认指向 `Terrain.Plain.Grass`（设计师在编辑器面板挂值；详见 [§3.2](#32-step_classifybiomes-评分器实现)）。当某 cell 在所有 17 个 Def 上 ScoreFor 全部为 0 时，WorldGen 端写入 Sentinel Tag。

**(T, M) 平面盲区核对**（仅 `Plc=Land && !Coast && !Mountain` 范围；E 不参与盲区分析，因为 E∈[-1,0] 已被 Ocean/Coast 吃掉，E≥0.5 被 Mountain 吃掉，Land 实际只覆盖 E∈[0, 0.5]）：

```
        M=0.0  0.2  0.3  0.4   0.6   0.7  0.85  0.95  1.0
T=-1.0 ┌────────────────── Glacier (T<0) ──────────────────┐
T= 0.0 ├──────────────────────────────────────────────────┤
T= 0.2 │ Tundra (T<0.2,M<0.4)│Tundra│ Taiga (T<0.3, M<0.85)│
T= 0.3 │ DesRocky │           │ ForestTemp │  ?   │ ?(裂)  │
T= 0.6 │ DesRocky │ ?(裂)     │ Plain Grass│ ?(裂)│ ?(裂)  │
T= 0.7 │ DesSand  │ Savanna   │ ForestTemp │ ?(裂)│ ?(裂)  │
T= 1.0 │ DesSand  │ Savanna   │ ?(裂)      │ForestTrop│Wet(M=0.95+)│
```

> 上图\"?\(裂\)\" 处即覆盖盲区——例如 `T=0.55, M=0.5` 不命中任何 Def → 走 Sentinel = `Terrain.Plain.Grass`。

**已确认覆盖盲区**（W4 接受、由 Sentinel 兜底；后续设计师在 PIE 后通过 W8 Debug 视图找到这些缺口再扩区间即可，**不在 W4 落地范围内**）：

1. `T ∈ [0.3, 0.6], M ∈ [0.2, 0.3]` —— 半干旱带（DesertRocky 上沿）
2. `T ∈ [0.7, 1.0], M ∈ [0.4, 0.7]` —— 热带半湿润（Savanna 与 ForestTropical 之间）
3. `T ∈ [0.0, 0.2], M ∈ [0.4, 0.85]` —— 寒带湿润（Tundra 与 Taiga 之间）
4. `T ∈ [0.7, 1.0], M ∈ [0.7, 0.95]` —— 热带森林次饱和带（兜底落到 ForestTropical 或 Sentinel，按 T 上限决定）

> **设计取舍**：W4 故意保留这些盲区给 Sentinel 兜底——避免起步表项区间互相重叠后 Priority 大战；后续 W4.5（"软命中" smoothstep 升级）会把这些盲区天然消化。

#### 2.4.5 关键 Priority 排序（消歧规则一览）

| 场景 | Priority 排序 | 解释 |
| --- | --- | --- |
| 高海拔海岸 | `Coast.Rocky (1.5) > Coast.Beach (1.0)` | Coast 上 E≥0.1 走 Rocky |
| 高湿暖带 | `Wetland (1.5) > Forest.Tropical (1.1) > Plain.Grass (1.0)` | M=0.95+ 强制 Wetland |
| 高湿密林 | `Forest.Tropical (1.1) > Plain.Grass (1.0)` | M=0.7-0.95 暖带走 Tropical |
| 极寒平地 | `Glacier (1.5) > Tundra (1.0)` | T<0 走 Glacier |
| 极寒山地 | `Glacier (1.5) > Mountain.Snow (1.0)` | 山区 + T<0 也强制 Glacier，覆盖 Mountain.Snow（T∈[-1,0.3]）的低温段 |
| 温带湿润 | `Forest.Temperate (1.1) > Plain.Grass (1.0)` | M≥0.6 走森林 |

> **Priority 全部 ≤ 2.0**——避免数值膨胀；新增地形时建议沿用此约定。

#### 2.4.6 创建 DataAsset 资产时的字段填充顺序（编辑器内）

按 §4.6 用户操作清单步骤 3-5 创建 17 个 `.uasset`，每个资产打开后填的顺序：

```
1. Identity 分类
   ├ TerrainTag      ← 表 A 列 3（下拉框选择）
   └ LayerIndex      ← 表 A 列 1
2. Placement 分类
   └ ClimateRules[]  ← 表 A 列 4（点 + 添加 N 条规则）
3. Gameplay 分类
   ├ DefaultResources ← 表 B 列 5
   ├ MoveCostBase    ← 表 B 列 2
   ├ DefenseBonus    ← 表 B 列 3
   ├ bAllowBuilding  ← 表 B 列 4
   └ ImpassableFor   ← 表 B 列 6
```

**典型耗时**：每 Def 60 秒 × 17 = **17 分钟**（首次建表期）。后续调参靠编辑器 PIE 即时反馈，无需重编译。

---

## 3. WorldGen 端改动

### 3.1 `FWorldGenSettings.TerrainSet` 字段切换

把 [WorldGenSettings.h:121](../Source/WorldGen/Public/WorldGenSettings.h) 的占位字段：

```cpp
TSoftObjectPtr<UDataAsset> BiomeTable;
```

替换为（已在主稿 §4.1 同步）：

```cpp
// 前向声明 UTerrainSet（cpp 端 #include "TerrainSet.h"）
class UTerrainSet;
TSoftObjectPtr<UTerrainSet> TerrainSet;
```

**字段重命名注意**：因为字段名变了（`BiomeTable → TerrainSet`），所有 `WorldGenSettings.h` 的 grep 命中（`AgentWorkflow §1.4.0` 字段名纪律）必须同步——读取这个字段的代码目前为 0 处（W1~W3 都不消费），但**编辑器中已挂的 Settings.uasset 会丢失字段值**。W4 落地附带"PIE 重启 + Settings 重挂 `DA_TerrainSet_Default`"用户操作清单（§4 末）。

### 3.2 `Step_ClassifyBiomes` 评分器实现

```cpp
// Source/WorldGen/Private/WorldGenerator.cpp
void FWorldGenerator::Step_ClassifyBiomes()
{
    // 0) 解析 TerrainSet（首次访问触发 LoadSynchronous）
    UTerrainSet* Set = Settings.TerrainSet.LoadSynchronous();
    if (!Set)
    {
        UE_LOG(LogWorldGen, Warning,
            TEXT("[WorldGen] Step_ClassifyBiomes: TerrainSet 未指定；所有 cell 写默认 Tag (None)"));
        return;
    }
    Set->LoadSynchronous();
    const TArray<UTerrainDefinition*>& Defs = Set->GetLoadedDefs();
    if (Defs.Num() == 0)
    {
        UE_LOG(LogWorldGen, Warning,
            TEXT("[WorldGen] Step_ClassifyBiomes: TerrainSet 内含 0 个 Def；跳过分类"));
        return;
    }

    // 1) Sentinel：覆盖盲区 fallback
    // Sentinel：覆盖盲区 fallback。SentinelTag 由 FWorldGenSettings 持有（FGameplayTag 字段，
    // 设计师在编辑器面板挂 Terrain.Plain.Grass）。cpp 不出现具体 Tag 字符串。
    UTerrainDefinition* SentinelDef = nullptr;
    if (Settings.SentinelTerrainTag.IsValid())
    {
        for (UTerrainDefinition* Def : Defs)
        {
            if (Def && Def->TerrainTag == Settings.SentinelTerrainTag)
            {
                SentinelDef = Def;
                break;
            }
        }
    }

    // 2) 双层 for 循环 × ScoreFor × argmax
    const int32 N = CellData.Num();
    int32 SentinelCount = 0;
    for (int32 i = 0; i < N; ++i)
    {
        FCellGeoData& CD = CellData[i];
        FClimateSample S;
        S.Elevation   = ElevationField[i];
        S.Moisture    = MoistureField[i];
        S.Temperature = TemperatureField[i];
        S.bIsLand     = (CD.bIsLand     != 0);
        S.bIsCoast    = (CD.bIsCoast    != 0);
        S.bIsMountain = (CD.bIsMountain != 0);

        UTerrainDefinition* Best = nullptr;
        float BestScore = 0.f;
        for (UTerrainDefinition* Def : Defs)
        {
            if (!Def) continue;
            const float Score = Def->ScoreFor(S);
            if (Score > BestScore) { BestScore = Score; Best = Def; }
        }

        if (Best)
        {
            CD.TerrainTag = Best->TerrainTag;
            CD.Resources  = Best->DefaultResources;
        }
        else
        {
            ++SentinelCount;
            if (SentinelDef)
            {
                CD.TerrainTag = SentinelDef->TerrainTag;
                CD.Resources  = SentinelDef->DefaultResources;
            }
            // else：留 None Tag，渲染端 fallback 到 LayerIndex 0
        }
    }

    UE_LOG(LogWorldGen, Log,
        TEXT("[WorldGen] W4 OK, %d cells classified (sentinel-fallback=%d)"),
        N, SentinelCount);
}
```

### 3.3 LUT 写入 + `EWorldGenDebugView::Biome`

```cpp
// 在 EWorldGenDebugView 末尾追加（PlanetTopologyDebugMesh.h:36 之后）：
Biome  UMETA(DisplayName = "Biome 真实分类 (W4，默认值)"),
```

```cpp
// RebuildCellAttrLUT_() 的两个 switch 中各加一行：
case EWorldGenDebugView::Biome:
{
    // ★ W4：从 CellData[CellId].TerrainTag 通过 UTerrainDefinition 查 LayerIndex
    UTerrainDefinition* Def = ResolveDefForTag(CD.TerrainTag);  // 详见 §A.5 辅助函数
    Layer = Def ? (uint8)FMath::Clamp(Def->LayerIndex, 0, 255) : (uint8)0;
    break;
}
```

把"默认视图"也指向 Biome（W4 完成后的预期表现）：

```cpp
case EWorldGenDebugView::None:
case EWorldGenDebugView::LandSea:
default:
    // ★ W4：默认视图升级为 Biome，与 R7 19-Layer 真实地表对齐；
    // LandSea 仍保留为单独选项给回归测试用。
    if (DebugView == EWorldGenDebugView::LandSea)
    {
        Layer = CD.bIsLand ? (uint8)4 : (uint8)0;
    }
    else
    {
        UTerrainDefinition* Def = ResolveDefForTag(CD.TerrainTag);
        Layer = Def ? (uint8)FMath::Clamp(Def->LayerIndex, 0, 255) : (uint8)0;
    }
    break;
```

`ResolveDefForTag(Tag)` 由 `RebuildCellAttrLUT_()` 在循环外预构建一次的 `TMap<FGameplayTag, UTerrainDefinition*>` 提供（避免每 cell × O(NumDefs) 查找）。

---

## 4. 实施步骤（cpp 落地清单）

> 下列步骤按"模块 → DataAsset → WorldGen → 渲染端 → 编辑器操作"五段式执行；每段后立即编译验证（[AgentWorkflow §1.4](AgentWorkflow.md)）。

### 4.1 段 ①：新建 `Source/TerrainTags/` 模块

| 文件 | 用途 |
| --- | --- |
| `TerrainTags.Build.cs` | 依赖 `Core/CoreUObject/Engine/GameplayTags` |
| `Public/TerrainTags.h` | 模块入口（IModuleInterface 默认实现） |
| `Private/TerrainTags.cpp` | `IMPLEMENT_MODULE(FDefaultModuleImpl, TerrainTags)` |
| `Public/TerrainPlacementMask.h` | `ETerrainPlacementMask` |
| `Public/TerrainClimateRule.h` | `FTerrainClimateRule` |
| `Public/TerrainDefinition.h` + `Private/TerrainDefinition.cpp` | `UTerrainDefinition` + `ScoreFor` 实现 |
| `Public/TerrainSet.h` + `Private/TerrainSet.cpp` | `UTerrainSet` + `LoadSynchronous` |
| ~~`TerrainTagsNative.*`~~ | **不创建**——17 个 Tag 走 [`Config/Tags/Terrain.ini`](../Config/Tags/Terrain.ini)（详见 §2.3） |

`.uproject` 的 `Modules` 字段追加 `TerrainTags`（Type=Runtime, LoadingPhase=Default）。

### 4.2 段 ②：把 `WorldGen` 与 `TerraCivilization` 主模块依赖 `TerrainTags`

```diff
// Source/WorldGen/WorldGen.Build.cs
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "Grid",
-   "GameplayTags"
+   "GameplayTags",
+   "TerrainTags"
});
```

```diff
// Source/TerraCivilization/TerraCivilization.Build.cs
-PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Grid", "ProceduralTerrainGenerator", "ProceduralMeshComponent", "WorldGen", "GameplayTags" });
+PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Grid", "ProceduralTerrainGenerator", "ProceduralMeshComponent", "WorldGen", "GameplayTags", "TerrainTags" });
```

### 4.3 段 ③：`FWorldGenSettings.TerrainSet` 字段替换

把 [WorldGenSettings.h:113~125](../Source/WorldGen/Public/WorldGenSettings.h) 的注释 + `BiomeTable` 字段替换为 `TerrainSet`（见 §3.1 / §A.4）。

### 4.4 段 ④：在 `Source/WorldGen/Private/WorldGenerator.cpp` 实现 `Step_ClassifyBiomes`

按 §3.2 / §A.6 全文粘贴；`Generate()` 末尾日志改为追加 `, Biome=W4` 状态串。

### 4.5 段 ⑤：渲染端 `RebuildCellAttrLUT_()` 改造（PlanetTopologyDebugMesh.cpp）

- `EWorldGenDebugView` 末尾追加 `Biome` 项（[PlanetTopologyDebugMesh.h:36](../Source/TerraCivilization/Public/Render/PlanetTopologyDebugMesh.h)）
- `DebugView` 默认值由 `None` 改为 `Biome`（首次拖入 Actor 即看到真实生物群系）
- 在 `RebuildCellAttrLUT_()` 函数顶部预构建 `TagToDefMap`；两个 switch 中各加 `case EWorldGenDebugView::Biome:` 与默认 fallback 的 Biome 路径
- 反射诊断不变（仍 14 项 Inputs）

### 4.6 段 ⑥：编辑器内创建 18 个 DataAsset 资产（17 Def + 1 Set；用户操作清单）

> ⚠ **前置必读**：直接手放 `Config/Tags/Terrain.ini` 文件 + 重启编辑器**不会**让 Tag 出现在 DA 下拉框——必须先经编辑器 "Add New Gameplay Tag Source" 一次显式注册该 ini。原理见 [AgentWorkflow.md §3.8](AgentWorkflow.md)。

| 步骤 | 操作 | 文件位置 |
| --- | --- | --- |
| 1 | **首次启动编辑器**（编译完成后）→ `Project Settings → GameplayTags → Manage Gameplay Tags` → 顶部 `Add New Gameplay Tag` → `Source` 下拉框中输入 `Terrain.ini` → `Tag` 字段填 `Terrain.Plain.Grass`（占位）→ 点 `Add New Tag` | 编辑器自动写盘并注册 `Config/Tags/Terrain.ini` |
| 2 | **关闭编辑器** → 用文本编辑器把详稿 [§A.6](#a6-configtagsterrainini) 的完整 17 行 `+GameplayTagList=` 内容**覆盖** `Config/Tags/Terrain.ini`（编辑器自动生成的版本只含一个占位 Tag）→ **重启编辑器** | `Config/Tags/Terrain.ini` |
| 3 | Content Browser → 右键 → Miscellaneous → Data Asset → 选 `UTerrainDefinition` 类 → 命名 `DA_Terrain_OceanDeep` | `Content/Data/Terrains/DA_Terrain_OceanDeep.uasset` |
| 4 | 双击打开 → `Terrain Tag` 下拉框选 `Terrain.Ocean.Deep`（17 个 Tag 全部可见）→ `LayerIndex` 填 `0` → `ClimateRules` 加一项按 §2.4 表填 | 同上 |
| 5 | 重复创建剩 16 项（共 17 个 Def），一一对照 §2.4 | `Content/Data/Terrains/DA_Terrain_*.uasset` |
| 6 | 创建 `UTerrainSet` 资产 → 命名 `DA_TerrainSet_Default` → `TerrainDefs` 数组按 LayerIndex 顺序填 17 项 | `Content/Data/Terrains/DA_TerrainSet_Default.uasset` |
| 7 | 关卡里选中 `BP_PlanetTopologyDebugMesh` Actor → `WorldGen Settings → Terrain Set` 指向 `DA_TerrainSet_Default`、`Sentinel Terrain Tag` 选 `Terrain.Plain.Grass` | 关卡 |
| 8 | 切 `DebugView` 为 `Biome` 或 `None`（默认）→ Save → PIE 验收 | 关卡 |

---

## 5. 验收清单

### 5.1 必通过项

| # | 检查项 | 通过标准 |
| --- | --- | --- |
| A | 编译 0 警 0 错 | `Result: Succeeded` 三模块（TerrainTags / WorldGen / TerraCivilization） |
| B | `Output Log` 命中 `[WorldGen] W4 OK, 642 cells classified (sentinel-fallback=N)` | N 占比 < 10%；高于 10% 提示存在覆盖盲区，按 §6 排错 |
| C | LUT 自检日志：前 16 cell 的 `R=` 值不再是 Knuth 哈希，而是 `Def->LayerIndex` 真实值 | `expected layer` 与 `R=` 一致；范围 ∈ [0, 18] |
| D | 反射诊断 R7 Inputs 仍是 14 项；`R7-compliant ✓` | 不变 |
| E | PIE 默认视图 Lit 模式 | 球面看到 19 种纹理色块（不是 R3 单色 placeholder）；12 五边形可见 |
| F | 赤道带（z≈0）≈ Desert/Tropical Forest | 视觉粗看：低纬度低湿度沙漠、低纬度高湿度热带林 |
| G | 极区（\|z\|>0.8）≈ Tundra / Glacier | 视觉粗看：极区蓝白色 |
| H | 大陆东岸森林、西岸沙漠（W3.5 上风 SSSP 效果继承） | 视觉粗看：东岸偏 Forest，西岸偏 Desert/Savanna |
| I | 山脉沿板块汇聚边界（W3 效果继承） | 视觉粗看：山脉是连续的链而非散点 |
| J | 编辑器调参验收（**DataAsset 驱动核心证据**） | 改 `DA_Terrain_ForestTropical.ClimateRules[0].Temperature` 区间从 [0.7,1] 到 [0.6,1]，重 PIE 看到热带雨林范围扩大；**无需重编 cpp** |

### 5.2 临时自检（仅 Editor 编译时启用）

```cpp
#if WITH_EDITOR
// Step_ClassifyBiomes 末尾：
ensureMsgf(SentinelCount * 10 < N,
    TEXT("[WorldGen] W4: sentinel-fallback ratio %d/%d > 10%%；存在显著覆盖盲区"),
    SentinelCount, N);
#endif
```

---

## 6. 排错表

| 症状 | 根因 | 修复 |
| --- | --- | --- |
| `Output Log` 报 `TerrainSet 未指定` | 关卡 Actor 的 `WorldGenSettings.TerrainSet` 没挂 | Editor 中选 Actor → `WorldGen Settings → Terrain Set` 指向 `DA_TerrainSet_Default` |
| `Output Log` 报 `TerrainSet 内含 0 个 Def` | `DA_TerrainSet_Default.TerrainDefs` 数组为空 / 引用无效 | 双击 DA_TerrainSet_Default → 检查 17 项 Def 引用未失效 |
| `sentinel-fallback ratio > 30%` | `ClimateRules[]` 区间留太多空白；某 (T,M) 没人覆盖 | W8 Debug 视图开 `Sentinel`（W8 落地，临时手段：把 `EWorldGenDebugView::Biome` 切到 `None` fallback 路径里查 LayerIndex==SentinelDef.LayerIndex 的 cell） |
| 球面全 LayerIndex 0（深海） | `ResolveDefForTag(None)` 返回 nullptr → fallback 0 | 检查 `Step_ClassifyBiomes` 是否在 `RebuildCellAttrLUT_` 之前跑完；检查 `CD.TerrainTag.IsValid()` |
| Lit 漆黑、Unlit 正常 | **不应该出现**（W3 已修复法线，W4 仅改 LUT） | 若复现：按 [AgentWorkflow §3.6](AgentWorkflow.md) 检查 R7 法线写法是否被改坏 |
| 视觉看到大片单色块占 90% | 某个 Def 的 `ClimateRules` 区间过宽 + Priority 过高，吃掉了其它 Def | 调小该 Def 的 Priority；或缩窄区间 |
| 编辑器中 `UTerrainDefinition` 资产不能创建（Class Picker 找不到） | 模块 `TerrainTags` LoadingPhase 不对 / 没加进 `.uproject.Modules` | 修改 `.uproject`，把 `TerrainTags` 加入 Modules 数组 |
| Tag 注册冲突（重复 Tag）| 同一 Tag 在多份 `.ini` 都有 `+GameplayTagList=` 行 | grep `Config/Tags/*.ini` + `Config/DefaultGameplayTags.ini`，确保 `Terrain.*` 仅在 `Config/Tags/Terrain.ini` 一处声明 |
| 编辑器 DataAsset 的 `TerrainTag` 下拉框为空（即使把 `Config/Tags/Terrain.ini` 文件已经放好、且重启过编辑器，依然为空）| **直接手放 ini 文件 + 重启编辑器并不会让它被注册成合法 Source**。引擎在启动期 `AddTagIniSearchPath(FPaths::ProjectConfigDir()/"Tags")`（[`GameplayTagsManager.cpp:683`](../../Program%20Files/Epic%20Games/UE_5.8/Engine/Source/Runtime/GameplayTags/Private/GameplayTagsManager.cpp)）只把目录加入扫描列表，但目录下的"陌生" ini 不会自动晋级为 GameplayTagsSettings 认可的 Tag Source；必须经编辑器内一次"Add New Gameplay Tag Source"动作显式注册。 | **正解（W4 用户 2026-06-28 实测有效）**：① 启动编辑器 → `Project Settings → GameplayTags → Manage Gameplay Tags → Add New Gameplay Tag` → `Source` 下拉框中输入 `Terrain.ini`（输入新文件名即"创建并注册"），`Tag` 字段填一个占位（如 `Terrain.Plain.Grass`），点 `Add New Tag` → 编辑器**写盘并注册** `Config/Tags/Terrain.ini`；② 关闭编辑器 → 用文本编辑器把详稿 §A.6 的完整 17 行 `+GameplayTagList=` 内容**覆盖**该 ini（编辑器自动生成的版本只含一个占位 Tag）；③ 重启编辑器 → 17 个 Tag 全部出现在 DA 字段下拉框。<br>**兜底方案**：把 17 行直接内联到 `Config/DefaultGameplayTags.ini` 的 `[/Script/GameplayTags.GameplayTagsList]` section（该文件由 [`GameplayTagsManager.cpp:278`](../../Program%20Files/Epic%20Games/UE_5.8/Engine/Source/Runtime/GameplayTags/Private/GameplayTagsManager.cpp) 硬编码强制读取，无需注册）。详细原理见 [AgentWorkflow.md §3.8](AgentWorkflow.md)。 |
| `DA_Terrain_*` 改 `LayerIndex` 后 PIE 没变化 | LUT 缓存未刷新 | 关卡里 `BP_PlanetTopologyDebugMesh` 的 `Rebuild()` 重跑（拖一下 Actor / 改 `SubdivisionLevel` / Save 关卡）；R7 LUT 是 transient 纹理，每次 Rebuild 重生 |
| Wetland 完全没出现 | `Wetland` 区间 `M=[0.95,1]` 太严格 + 全球 Moisture max 不到 0.95 | PIE 时打 `EWorldGenDebugView::Moisture` 查全球 Moisture 上限；调 `MoistureScale` 或拓宽 Wetland 区间 |

---

## 7. 路径预告与衔接

### 7.1 W4 与 W5/W6 的衔接

- **W5 河流追踪**：W5 在 W4 Tag 已写好的基础上，对 `bIsRiver=true` 的 cell 把 LUT.B 通道写 1（不动 R 通道）；GridRender 看 LUT.B 决定是否叠加河流软边样式。`UTerrainDefinition` 不参与 W5。
- **W6 势力基地分配**：12 五边形如果在 W4 被分类为 `Mountain.Peak` 或 `Desert.Sand`，W6 阶段会**回写** `CD.TerrainTag = Plain.Grass`（强制陆地化 + 缓冲带友好地形，详见 [WorldGenDesign §5.8](WorldGenDesign.md)）；W4 不为 W6 留任何钩子。

### 7.2 W4 与 R8（SDF 渲染端）的关系

- 主稿 §10.1：GridRender 读 `Def->LayerIndex` 写 LUT.R（W4 阶段已经做完）
- 主稿 §10.2：Gameplay 读 `FCellGeoData[].TerrainTag`（W4 写好）+ 通过 `UTerrainDefinition` 查 `MoveCostBase / DefenseBonus`
- 主稿 §10.3：SaveLoad 仅序列化 `(Sub, Seed, FWorldGenSettings)` 三元组——其中 `FWorldGenSettings::TerrainSet` 是 SoftObjectPtr，序列化为路径字符串，加载时按路径 LoadSynchronous → 完全可重现

> **R7 Inputs 与 W4 的关系**：W4 不动 R7 材质资产；CellAttrLUT.R 通道含义不变（仍是 `LayerIndex`），只是写入数据源从 Knuth 哈希改为 `Def->LayerIndex`。R7 反射诊断 14 项 Inputs 全部保持。这是 [SphericalSDFTerrainDesign §10.1](SphericalSDFTerrainDesign.md) 解耦承诺的兑现：**WorldGen 改了，渲染端零改动**。

### 7.3 与 [SphereTopologyReference §8.1](SphereTopologyReference.md) 契约一致

W4 不写任何拓扑字段；只读 `FCell.bIsPentagon`（已在 W1 拷入 `FCellGeoData.bIsPentagon`）和 `FCellGeoData[]`。Topology 端零改动。

### 7.4 与 [AgentWorkflow §1.4.0](AgentWorkflow.md) 字段名纪律

本稿 §1.3 已列字段名核对表；落地 cpp 时再次 grep 验证。

### 7.5 W7 取消的影响

原 W7 计划的 3 件事在 W4 都已落地或推迟：

| W7 原计划 | W4 落实情况 |
| --- | --- |
| `UTerrainDefinition` DataAsset 化 | ✅ W4 已做 |
| `UBiomeTable` DataAsset | ❌ 取消（规则属于 `UTerrainDefinition` 自身，无需中间资产） |
| `UPlateProfile` DataAsset | ⏸ 推迟到 W6（如势力基地需要按板块差异化外观/资源时再做） |

---

## 附录 A：可粘贴 cpp 全文

> 完整可粘贴代码；落地时按 §4 顺序逐段验证编译。

### A.1 `Source/TerrainTags/TerrainTags.Build.cs`

```csharp
using UnrealBuildTool;

public class TerrainTags : ModuleRules
{
    public TerrainTags(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags"
        });
    }
}
```

### A.2 `Source/TerrainTags/Public/TerrainPlacementMask.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "TerrainPlacementMask.generated.h"

UENUM(BlueprintType)
enum class ETerrainPlacementMask : uint8
{
    Any       UMETA(DisplayName = "任意"),
    Land      UMETA(DisplayName = "陆地（非海岸非山）"),
    Ocean     UMETA(DisplayName = "海洋"),
    Coast     UMETA(DisplayName = "海岸"),
    Mountain  UMETA(DisplayName = "山脉"),
};
```

### A.3 `Source/TerrainTags/Public/TerrainClimateRule.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Math/Interval.h"
#include "TerrainPlacementMask.h"
#include "TerrainClimateRule.generated.h"

USTRUCT(BlueprintType)
struct TERRAINTAGS_API FTerrainClimateRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Placement")
    ETerrainPlacementMask Placement = ETerrainPlacementMask::Land;

    UPROPERTY(EditAnywhere, Category = "Climate")
    FFloatInterval Temperature {-1.f, 1.f};

    UPROPERTY(EditAnywhere, Category = "Climate")
    FFloatInterval Moisture {0.f, 1.f};

    UPROPERTY(EditAnywhere, Category = "Climate")
    FFloatInterval Elevation {-1.f, 1.f};

    UPROPERTY(EditAnywhere, Category = "Score", meta = (ClampMin = "0.01"))
    float Priority = 1.f;
};
```

### A.4 `Source/TerrainTags/Public/TerrainDefinition.h` 与 `Private/TerrainDefinition.cpp`

```cpp
// TerrainDefinition.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TerrainClimateRule.h"
#include "TerrainDefinition.generated.h"

USTRUCT()
struct FClimateSample
{
    GENERATED_BODY()
    float Elevation   = 0.f;
    float Moisture    = 0.5f;
    float Temperature = 0.f;
    bool  bIsLand     = true;
    bool  bIsCoast    = false;
    bool  bIsMountain = false;
};

UCLASS(BlueprintType)
class TERRAINTAGS_API UTerrainDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Identity")
    FGameplayTag TerrainTag;

    UPROPERTY(EditAnywhere, Category = "Identity", meta = (ClampMin = "0", ClampMax = "255"))
    int32 LayerIndex = 0;

    UPROPERTY(EditAnywhere, Category = "Placement")
    TArray<FTerrainClimateRule> ClimateRules;

    UPROPERTY(EditAnywhere, Category = "Gameplay")
    FGameplayTagContainer DefaultResources;

    UPROPERTY(EditAnywhere, Category = "Gameplay") float MoveCostBase  = 1.f;
    UPROPERTY(EditAnywhere, Category = "Gameplay") float DefenseBonus  = 0.f;
    UPROPERTY(EditAnywhere, Category = "Gameplay") bool  bAllowBuilding = true;
    UPROPERTY(EditAnywhere, Category = "Gameplay") FGameplayTagContainer ImpassableFor;

    float ScoreFor(const FClimateSample& Sample) const;
};
```

```cpp
// TerrainDefinition.cpp
#include "TerrainDefinition.h"

float UTerrainDefinition::ScoreFor(const FClimateSample& S) const
{
    float Best = 0.f;
    for (const FTerrainClimateRule& Rule : ClimateRules)
    {
        bool bPlacementOK = false;
        switch (Rule.Placement)
        {
            case ETerrainPlacementMask::Any:      bPlacementOK = true; break;
            case ETerrainPlacementMask::Land:     bPlacementOK =  S.bIsLand && !S.bIsCoast && !S.bIsMountain; break;
            case ETerrainPlacementMask::Ocean:    bPlacementOK = !S.bIsLand; break;
            case ETerrainPlacementMask::Coast:    bPlacementOK =  S.bIsLand &&  S.bIsCoast; break;
            case ETerrainPlacementMask::Mountain: bPlacementOK =  S.bIsLand &&  S.bIsMountain; break;
            default: break;
        }
        if (!bPlacementOK) continue;

        const bool bTOk = (S.Temperature >= Rule.Temperature.Min) && (S.Temperature <= Rule.Temperature.Max);
        const bool bMOk = (S.Moisture    >= Rule.Moisture.Min)    && (S.Moisture    <= Rule.Moisture.Max);
        const bool bEOk = (S.Elevation   >= Rule.Elevation.Min)   && (S.Elevation   <= Rule.Elevation.Max);
        if (!(bTOk && bMOk && bEOk)) continue;

        Best = FMath::Max(Best, Rule.Priority);
    }
    return Best;
}
```

### A.5 `Source/TerrainTags/Public/TerrainSet.h` 与 `Private/TerrainSet.cpp`

```cpp
// TerrainSet.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TerrainDefinition.h"
#include "TerrainSet.generated.h"

UCLASS(BlueprintType)
class TERRAINTAGS_API UTerrainSet : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Set")
    TArray<TSoftObjectPtr<UTerrainDefinition>> TerrainDefs;

    void LoadSynchronous();
    const TArray<UTerrainDefinition*>& GetLoadedDefs() const { return LoadedDefsCache; }

private:
    UPROPERTY(Transient)
    TArray<TObjectPtr<UTerrainDefinition>> LoadedDefsCache;
};
```

```cpp
// TerrainSet.cpp
#include "TerrainSet.h"

void UTerrainSet::LoadSynchronous()
{
    LoadedDefsCache.Reset(TerrainDefs.Num());
    for (const TSoftObjectPtr<UTerrainDefinition>& SoftDef : TerrainDefs)
    {
        if (UTerrainDefinition* Def = SoftDef.LoadSynchronous())
        {
            LoadedDefsCache.Add(Def);
        }
    }
}
```

### A.6 `Config/Tags/Terrain.ini`（替代原 `TerrainTagsNative.*`）

> ⚠ 已从原 cpp 静态注册改为 ini 注册（详见 §2.3 设计依据）。下述代码块为 `Config/Tags/Terrain.ini` 完整内容；W4 落地时新建该文件即可，不再有 `TerrainTagsNative.h/cpp`。

```ini
[/Script/GameplayTags.GameplayTagsList]
+GameplayTagList=(Tag="Terrain.Ocean.Deep",        DevComment="深海")
+GameplayTagList=(Tag="Terrain.Ocean.Shallow",     DevComment="浅海")
+GameplayTagList=(Tag="Terrain.Coast.Beach",       DevComment="沙滩")
+GameplayTagList=(Tag="Terrain.Coast.Rocky",       DevComment="岩石海岸")
+GameplayTagList=(Tag="Terrain.Plain.Grass",       DevComment="温带草原")
+GameplayTagList=(Tag="Terrain.Plain.Savanna",     DevComment="热带草原")
+GameplayTagList=(Tag="Terrain.Forest.Temperate",  DevComment="温带森林")
+GameplayTagList=(Tag="Terrain.Forest.Tropical",   DevComment="热带雨林")
+GameplayTagList=(Tag="Terrain.Forest.Taiga",      DevComment="针叶林")
+GameplayTagList=(Tag="Terrain.Wetland",           DevComment="湿地")
+GameplayTagList=(Tag="Terrain.Desert.Sand",       DevComment="沙漠")
+GameplayTagList=(Tag="Terrain.Desert.Rocky",      DevComment="戈壁")
+GameplayTagList=(Tag="Terrain.Mountain.Hill",     DevComment="山丘")
+GameplayTagList=(Tag="Terrain.Mountain.Peak",     DevComment="山峰")
+GameplayTagList=(Tag="Terrain.Mountain.Snow",     DevComment="雪山")
+GameplayTagList=(Tag="Terrain.Tundra",            DevComment="苔原")
+GameplayTagList=(Tag="Terrain.Glacier",           DevComment="冰川")
```

附加：在 `FWorldGenSettings` 中新增一个 `SentinelTerrainTag` 字段（用于覆盖盲区 fallback；设计师在编辑器内挂 `Terrain.Plain.Grass`）：

```cpp
// FWorldGenSettings 内追加：
/** 覆盖盲区 fallback 时使用的 Tag（一般指向 Terrain.Plain.Grass）。 */
UPROPERTY(EditAnywhere, Category = "WorldGen|Biomes")
FGameplayTag SentinelTerrainTag;
```

#### A.6 备份（已废弃）：原 cpp 静态注册方案

> 以下为历史方案（W4 第一版）

```cpp
~~（已废弃，仅留作历史对照——W4 落地不要写这部分代码）~~

```
// 原计划：TerrainTagsNative.h / .cpp 通过 FNativeGameplayTag 静态注册 17 项 Tag。
// 改为 ini 注册的原因详见 §2.3：既然规则全在 DataAsset、cpp 不消费具体 Tag 名，
// 静态绑定反而把 17 个名字钉死在 cpp 里，违反"完全数据驱动"原则。
```

### A.7 `WorldGenSettings.h` 字段切换 diff

```diff
-// W7 阶段会引入具体的 UBiomeTable : public UDataAsset；W1~W6 阶段 BiomeTable 字段类型先用基类 UDataAsset 占位，
-// 避免提前创建 UBiomeTable 文件污染骨架。W7 落地时把 TSoftObjectPtr<UDataAsset> 升级为 TSoftObjectPtr<UBiomeTable>。
+// W4 起：原 W7 计划取消，改为 UTerrainSet（DataAsset，持一组 UTerrainDefinition*）。
+// UTerrainDefinition 自身携带 ClimateRules[]——分类规则属于地形定义本身，
+// 设计师可在编辑器内调；详见 Docs/W4_BiomeClassification.md §2.1。

// ───────────── Biomes (W4) ─────────────

-/** 生物群系查表（W7 启用 DataAsset 化）；W4 阶段先用 cpp 硬编码默认 5x5 表。 */
-UPROPERTY(EditAnywhere, Category="WorldGen|Biomes")
-TSoftObjectPtr<UDataAsset> BiomeTable;
+/**
+ * W4 起：UTerrainSet（DataAsset）持有 17~19 个 UTerrainDefinition* 引用；
+ * 每个 Def 自带 ClimateRules[] 规则、LayerIndex、玩法属性。Step_ClassifyBiomes
+ * 不知道 Tag/Layer 语义，仅遍历调用 Def->ScoreFor(Sample) 取 argmax。
+ * 详见 Docs/W4_BiomeClassification.md §2.1。
+ */
+UPROPERTY(EditAnywhere, Category = "WorldGen|Biomes")
+TSoftObjectPtr<class UTerrainSet> TerrainSet;
```

### A.8 `Step_ClassifyBiomes` 与 Generate() 末尾日志（WorldGenerator.cpp 内）

```cpp
#include "TerrainSet.h"
#include "TerrainDefinition.h"

void FWorldGenerator::Step_ClassifyBiomes()
{
    UTerrainSet* Set = Settings.TerrainSet.LoadSynchronous();
    if (!Set)
    {
        UE_LOG(LogWorldGen, Warning, TEXT("[WorldGen] Step_ClassifyBiomes: TerrainSet 未指定"));
        return;
    }
    Set->LoadSynchronous();
    const TArray<UTerrainDefinition*>& Defs = Set->GetLoadedDefs();
    if (Defs.Num() == 0)
    {
        UE_LOG(LogWorldGen, Warning, TEXT("[WorldGen] Step_ClassifyBiomes: TerrainSet 内 0 个 Def"));
        return;
    }

    UTerrainDefinition* SentinelDef = nullptr;
    if (Settings.SentinelTerrainTag.IsValid())
    {
        for (UTerrainDefinition* Def : Defs)
        {
            if (Def && Def->TerrainTag == Settings.SentinelTerrainTag) { SentinelDef = Def; break; }
        }
    }

    const int32 N = CellData.Num();
    int32 SentinelCount = 0;
    for (int32 i = 0; i < N; ++i)
    {
        FCellGeoData& CD = CellData[i];
        FClimateSample S;
        S.Elevation   = ElevationField[i];
        S.Moisture    = MoistureField[i];
        S.Temperature = TemperatureField[i];
        S.bIsLand     = (CD.bIsLand     != 0);
        S.bIsCoast    = (CD.bIsCoast    != 0);
        S.bIsMountain = (CD.bIsMountain != 0);

        UTerrainDefinition* Best = nullptr;
        float BestScore = 0.f;
        for (UTerrainDefinition* Def : Defs)
        {
            if (!Def) continue;
            const float Score = Def->ScoreFor(S);
            if (Score > BestScore) { BestScore = Score; Best = Def; }
        }

        if (Best)
        {
            CD.TerrainTag = Best->TerrainTag;
            CD.Resources  = Best->DefaultResources;
        }
        else
        {
            ++SentinelCount;
            if (SentinelDef)
            {
                CD.TerrainTag = SentinelDef->TerrainTag;
                CD.Resources  = SentinelDef->DefaultResources;
            }
        }
    }

    LastBiomeSentinelCount = SentinelCount;
    UE_LOG(LogWorldGen, Log,
        TEXT("[WorldGen] W4 OK, %d cells classified (sentinel-fallback=%d)"),
        N, SentinelCount);
}
```

### A.9 `EWorldGenDebugView::Biome` 枚举扩展（PlanetTopologyDebugMesh.h）

```diff
 UENUM(BlueprintType)
 enum class EWorldGenDebugView : uint8
 {
     None         UMETA(DisplayName = "None (默认 LayerIndex / 与 Biome 等价)"),
     PlateId      UMETA(DisplayName = "PlateId 染色"),
     LandSea      UMETA(DisplayName = "海陆两色"),
     Elevation    UMETA(DisplayName = "Elevation 热图 (W3)"),
     Moisture     UMETA(DisplayName = "Moisture 热图 (W3)"),
     Temperature  UMETA(DisplayName = "Temperature 热图 (W3)"),
     Mountain     UMETA(DisplayName = "Mountain 高亮 (W3)"),
+    Biome        UMETA(DisplayName = "Biome 真实分类 (W4，默认值)"),
 };
```

### A.10 `RebuildCellAttrLUT_()` Biome 分支（PlanetTopologyDebugMesh.cpp）

```cpp
// 在 RebuildCellAttrLUT_ 函数顶部、for-loop 之前预构建 TagToDefMap：
TMap<FGameplayTag, UTerrainDefinition*> TagToDefMap;
if (UTerrainSet* TSet = WorldGenSettings.TerrainSet.LoadSynchronous())
{
    TSet->LoadSynchronous();
    for (UTerrainDefinition* Def : TSet->GetLoadedDefs())
    {
        if (Def) TagToDefMap.Add(Def->TerrainTag, Def);
    }
}

// 然后两个 switch 各加：
case EWorldGenDebugView::Biome:
{
    UTerrainDefinition** Found = TagToDefMap.Find(CD.TerrainTag);
    Layer = (Found && *Found) ? (uint8)FMath::Clamp((*Found)->LayerIndex, 0, 255) : (uint8)0;
    break;
}

// 默认 fallback 由 LandSea 改为 Biome：
case EWorldGenDebugView::None:
default:
{
    UTerrainDefinition** Found = TagToDefMap.Find(CD.TerrainTag);
    Layer = (Found && *Found) ? (uint8)FMath::Clamp((*Found)->LayerIndex, 0, 255) : (uint8)0;
    break;
}
case EWorldGenDebugView::LandSea:
{
    Layer = CD.bIsLand ? (uint8)4 : (uint8)0;
    break;
}
```

---

## 附录 B：跨阶段同构性（W7 取消、与 R8/R9/R11 关系）

### B.1 W4 完成 = R8 接入

R8 阶段（[SphericalSDFTerrainDesign.md §11.x](SphericalSDFTerrainDesign.md)）的工作量：

| R8 任务 | W4 完成情况 |
| --- | --- |
| `RebuildCellAttrLUT_` R 通道改为按 `Def->LayerIndex` 写 | ✅ 已做 |
| 反射诊断不变（14 项 Inputs） | ✅ 已确认 |
| R7 材质 / Triplanar / WPO 零改动 | ✅ 已确认 |
| 球面看到 19 layer 真实地表 | ✅ 已实现 |

**R8 阶段实际只需 0 行改动**——这是 [WorldGenDesign.md §10.1](WorldGenDesign.md) "WorldGen 改了，渲染端零改动"承诺的兑现。

> ⚠ **路线调整（2026-06-29 / 2026-06-30 命名修订）**：上面"R8 阶段随 W4 完成而完成"的描述**已不准确**。新路线是 R8（参数化 Tint，仍用 Knuth placeholder）→ T 阶段（自研球面网格 / TessellatedMesh）→ **W4 联调验收**——W4 cpp 已完成，但实际把 BaseTexIdx 从 placeholder 切换为 `Def->LayerIndex` 的动作发生在 T 阶段联调期。详见 [SphericalSDFTerrainDesign.md §11 Roadmap](SphericalSDFTerrainDesign.md#11-实施-roadmapm-step) 与 [TessellatedMeshDesign.md](TessellatedMeshDesign.md)。

### B.2 R9 多 LUT、T 阶段自研球面网格、R10 LOD、R11 高亮均不影响 W4

| 阶段 | SDF 端动作 | W4 是否需要适配 |
| --- | --- | --- |
| R8（参数化 Tint）| 加 4 通道 LUT + 17 种 tint 配方（仍用 Knuth placeholder）| ❌ 不需要——R8 不消费 W4 |
| T 阶段（TessellatedMesh）| mesh 切到独立 `MeshTopology = FSphereTopology(MeshSubdivisionLevel)`（默认 sub=4）+ cpp 端径向位移 + 把 BaseTexIdx 从 placeholder 切到 `Def->LayerIndex` | ⚠ 仅消费 `Elevation`（W3 已就绪）+ `LayerIndex`（W4 已就绪），无需 W4 改动 |
| R9 Decor/Owner/Fog 多 LUT | 加独立 LUT 通道 | ❌ 不需要 |
| R10 自研网格 LOD | 远 sub+0 / 近 sub+2 / 超近 sub+3 | ❌ 不需要 |
| R11 高亮描边 | 接入 §15 高亮 LUT | ❌ 不需要 |
| ~~原 R11 PTG 路线~~ | ❌ 已废弃（被 T 阶段取代）| — |
| ~~原 R12 WPO~~ | ❌ 已废弃（被 T 阶段 cpp 端位移取代）| — |

### B.3 W5/W6 在 W4 之上的增量

| 阶段 | 写哪些字段 | 是否会回写 W4 写过的 TerrainTag |
| --- | --- | --- |
| W5 河流 | `bIsRiver/bIsLake/FlowTo` + LUT.B | ❌ 不动 TerrainTag |
| W6 基地 | `BaseFactionId` + 12 五边形强制陆地化 | ✅ 会回写（缓冲带：Mountain.Peak → Mountain.Hill；Desert → Plain.Savanna） |

### B.4 设计师调参验收路径（数据驱动核心证据）

W4 完成后，设计师在编辑器内可单独完成以下任一调整、无需重编 cpp：

1. 修改 `DA_Terrain_ForestTropical.ClimateRules[0].Temperature.Min` 从 0.7 → 0.6 → 重 PIE 看到热带雨林范围扩大
2. 修改 `DA_Terrain_Wetland.ClimateRules[0].Priority` 从 1.5 → 0.5 → Wetland 让位给 Forest.Tropical
3. 在 `DA_TerrainSet_Default` 中新增一个 `DA_Terrain_Mangrove`（红树林，新建 DataAsset、Tag 走 `.ini`）→ 重 PIE 立即生效
4. 调整 `MoveCostBase`、`DefenseBonus`（W4 不消费、为 Gameplay 预留）

**这就是 [TechnicalDesign.md §0.2](TechnicalDesign.md) "数据驱动原则"在 W4 阶段的实质落地。**

---

> **结语**：W4 的真正价值不仅是"球面终于看到 19 layer 真实地表"，更是**第一次把 GameplayTag 从字符串别名升级为携带规则的 DataAsset**。从 W4 起，所有"X 是什么"（X = 地形 / 单位 / 建筑 / 势力）都遵循同一范式：`U*Definition` DataAsset 自带规则、World/Gameplay 模块作为评估器、设计师在编辑器内调。这是 [TechnicalDesign §0.2](TechnicalDesign.md) 数据驱动原则的第一次完整落地——后续 W6 / Gameplay / GameplaySystem / AI 都可复用此范式。
