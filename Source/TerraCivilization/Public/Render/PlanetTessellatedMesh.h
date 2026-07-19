// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Render/PlanetHISMTileRenderer.h"
#include "TerraGameplayContainer.h"
#include "TerraPiecePresentationTypes.h"
#include "TerrainVisualTypes.h"
#include "Templates/UniquePtr.h"
#include "WorldGenSettings.h"   // T4：UPROPERTY 直接持有 FWorldGenSettings → 完整类型可见
#include "PlanetTessellatedMesh.generated.h"

class UTexture2D;
class UMaterialInstanceDynamic;
class UAnimationAsset;
class UAnimInstance;
class UAnimMontage;
class FSphereTopology;
class FWorldGenerator;   // T4：TUniquePtr<FWorldGenerator>，避免在头文件 include "WorldGenerator.h"
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class USkeletalMesh;
class UPlanetCameraComponent;
class UPlanetGameplayComponent;
class UPlanetHISMInteractionComponent;
class UPlanetPiecePresentationComponent;
class UTerrainVisualSurfaceComponent;
class FTerrainVisualCoordinator;
struct FTerraGameplayPieceState;
struct FTerraGameplayCaptureEntry;
struct FHitResult;

/**
 * APlanetTessellatedMesh
 *
 * SimpleGameplay planet actor.
 * Owns gameplay/worldgen orchestration and delegates tile rendering/highlight
 * state to FPlanetHISMTileRenderer. The runtime sphere surface is assembled
 * from HISM tile instances; legacy ProceduralMesh/SDF surface paths are not
 * part of this actor anymore.
 */
UCLASS()
class TERRACIVILIZATION_API APlanetTessellatedMesh : public AActor
{
    GENERATED_BODY()
    friend class UPlanetCameraComponent;
    friend class UPlanetGameplayComponent;
    friend class UPlanetHISMInteractionComponent;
    friend class UPlanetPiecePresentationComponent;

public:
    APlanetTessellatedMesh();

    /** FVTableHelper constructor + destructor stay in .cpp because this class owns incomplete TUniquePtr types. */
    APlanetTessellatedMesh(FVTableHelper& Helper);
    virtual ~APlanetTessellatedMesh();

    //----------------------------------------------------------
    // §2.1 / §1.3 拍板参数（编辑器可调）
    //----------------------------------------------------------

    /** Cell 拓扑细分层级（玩法 / WorldGen 用）。默认 sub=3 → 642 cells。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess",
              meta = (ClampMin = "1", ClampMax = "5"))
    int32 CellSubdivisionLevel = 3;

    /** 球半径（cm）。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess",
              meta = (ClampMin = "100.0"))
    float GlobeRadiusCM = 15000.0f;

    //----------------------------------------------------------
    // SimpleGameplay：TerrainVisual SV1 连续基础表面
    //----------------------------------------------------------

    /** TerrainVisual 独立视觉路径；默认保留 Legacy HISM Debug 行为。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual")
    ETerrainVisualMode TerrainVisualMode = ETerrainVisualMode::LegacyHISMDebug;

    /** SV1 连续基础球面的独立渲染细分层级；不影响 Gameplay CellTopology。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual", meta = (ClampMin = "1", ClampMax = "7"))
    int32 TerrainVisualSurfaceSubdivisionLevel = 7;

    /** SV1 连续基础表面材质；为空时使用引擎默认材质作流程验收。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual")
    TObjectPtr<UMaterialInterface> TerrainVisualBaseMaterial;

    /** SV2 连续表面 Hex/Pent 高亮材质；为空时保留 SV1 基础表面 Section 0。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV2 Highlight")
    TObjectPtr<UMaterialInterface> TerrainVisualHighlightMaterial;

    /** SV3 连续表面地形材质；为空时继续使用 SV2 高亮材质。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV3 Surface")
    TObjectPtr<UMaterialInterface> TerrainVisualSurfaceMaterial;

    /** SV6-B：只在 HISM + SDF 实验模式覆盖三种旧 Tile HISM 的共享径向投影材质。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV6-B HISM SDF Experiment")
    TObjectPtr<UMaterialInterface> TerrainVisualHISMSDFMaterial;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV6-A Surface Enhancement") TObjectPtr<UTexture2D> TerrainVisualGravelColor;
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV6-A Surface Enhancement") TObjectPtr<UTexture2D> TerrainVisualGravelNormal;
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV6-A Surface Enhancement") TObjectPtr<UTexture2D> TerrainVisualGravelRoughness;
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV6-A Surface Enhancement") TObjectPtr<UTexture2D> TerrainVisualMossColor;
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV6-A Surface Enhancement") TObjectPtr<UTexture2D> TerrainVisualMossNormal;
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV6-A Surface Enhancement") TObjectPtr<UTexture2D> TerrainVisualMossRoughness;
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV6-A Surface Enhancement") TObjectPtr<UTexture2D> TerrainVisualRockColor;
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV6-A Surface Enhancement") TObjectPtr<UTexture2D> TerrainVisualRockNormal;
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV6-A Surface Enhancement") TObjectPtr<UTexture2D> TerrainVisualRockRoughness;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV6-A Surface Enhancement") FLinearColor TerrainVisualPlainTint = FLinearColor(0.72f, 0.82f, 0.48f, 1.0f);
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV6-A Surface Enhancement") FLinearColor TerrainVisualForestTint = FLinearColor(0.18f, 0.42f, 0.20f, 1.0f);
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV6-A Surface Enhancement") FLinearColor TerrainVisualMountainTint = FLinearColor(0.48f, 0.42f, 0.34f, 1.0f);
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV6-A Surface Enhancement", meta = (ClampMin = "50.0", ClampMax = "5000.0")) float TerrainVisualTileScaleCM = 450.0f;
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV6-A Surface Enhancement", meta = (ClampMin = "1.0", ClampMax = "16.0")) float TerrainVisualTriplanarSharpness = 4.0f;
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV6-A Surface Enhancement", meta = (ClampMin = "0.0", ClampMax = "2.0")) float TerrainVisualNormalStrength = 0.55f;

    /** SV4：山地 SDF 距离场的最大宏观径向高度，只影响视觉表现。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV4 Macro Height", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
    float TerrainVisualMountainHeightCM = 2400.0f;

    /** SV4：值越大，山脊越尖；建议保持大于 1。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV4 Macro Height", meta = (ClampMin = "1.0", ClampMax = "8.0"))
    float TerrainVisualMountainFalloffExponent = 3.0f;

    /** SV4：森林 SDF 距离场的最大宏观径向高度，只影响视觉表现。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV4 Macro Height", meta = (ClampMin = "0.0", ClampMax = "5000.0"))
    float TerrainVisualForestHeightCM = 500.0f;

    /** SV4：森林高度 sigmoid 的陡峭度；值越低，森林边缘到丘陵中心的过渡越平缓。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV4 Macro Height", meta = (ClampMin = "1.0", ClampMax = "20.0"))
    float TerrainVisualForestSigmoidSteepness = 8.0f;

    /** SV4.5：启用山脊起伏、山坡侵蚀沟槽和低地缓起伏。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV4.5 Medium Frequency")
    bool bEnableTerrainVisualMediumFrequencyErosion = true;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV4.5 Medium Frequency", meta = (ClampMin = "0.0", ClampMax = "2000.0"))
    float TerrainVisualCrestNoiseAmplitudeCM = 420.0f;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV4.5 Medium Frequency", meta = (ClampMin = "0.0", ClampMax = "2000.0"))
    float TerrainVisualErosionAmplitudeCM = 260.0f;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV4.5 Medium Frequency", meta = (ClampMin = "0.0", ClampMax = "500.0"))
    float TerrainVisualLowlandNoiseAmplitudeCM = 55.0f;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV4.5 Medium Frequency", meta = (ClampMin = "0.1", ClampMax = "30.0"))
    float TerrainVisualMediumFrequencyNoiseFrequency = 5.5f;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV4.5 Medium Frequency", meta = (ClampMin = "0.0", ClampMax = "3.0"))
    float TerrainVisualErosionDomainWarpAmplitude = 0.42f;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV4.5 Medium Frequency", meta = (ClampMin = "0.5", ClampMax = "8.0"))
    float TerrainVisualErosionValleySharpness = 2.2f;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV5 Decorative Rivers")
    bool bEnableTerrainVisualDecorativeRivers = true;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV5 Decorative Rivers", meta = (ClampMin = "0", ClampMax = "16"))
    int32 TerrainVisualRiverSourceCount = 10;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV5 Decorative Rivers", meta = (ClampMin = "1", ClampMax = "4"))
    int32 TerrainVisualRiverTerminalBasinCount = 2;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV5 Decorative Rivers", meta = (ClampMin = "2", ClampMax = "32"))
    int32 TerrainVisualRiverMinPathCells = 6;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV5 Decorative Rivers", meta = (ClampMin = "2", ClampMax = "64"))
    int32 TerrainVisualRiverMaxPathCells = 48;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV5 Decorative Rivers", meta = (ClampMin = "1", ClampMax = "16"))
    int32 TerrainVisualRiverTerminalLakeMinDischarge = 2;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV5 Decorative Rivers", meta = (ClampMin = "1.0", ClampMax = "500.0"))
    float TerrainVisualRiverMinWidthCM = 70.0f;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV5 Decorative Rivers", meta = (ClampMin = "0.0", ClampMax = "300.0"))
    float TerrainVisualRiverWidthScaleCM = 45.0f;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV5 Decorative Rivers", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float TerrainVisualRiverLengthWidthGrowthCM = 8.0f;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV5 Decorative Rivers", meta = (ClampMin = "1.0", ClampMax = "8.0"))
    float TerrainVisualRiverLakeLengthMultiplier = 3.2f;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV5 Decorative Rivers", meta = (ClampMin = "1.0", ClampMax = "8.0"))
    float TerrainVisualRiverLakeWidthMultiplier = 2.4f;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV5 Decorative Rivers", meta = (ClampMin = "0.05", ClampMax = "0.25"))
    float TerrainVisualRiverMaxLakeRadiusFraction = 0.25f;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV2 Highlight")
    FLinearColor TerrainVisualBaseGroundColor = FLinearColor(0.08f, 0.10f, 0.06f, 1.0f);

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV2 Highlight",
              meta = (ClampMin = "0.001", ClampMax = "0.15"))
    float TerrainVisualHighlightPaddingRad = 0.03f;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|SV2 Highlight",
              meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float TerrainVisualHighlightStrength = 1.50f;

    /** 连续模式下是否显示旧 HISM 瓦片，仅用于与基础表面对照。默认隐藏。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Terrain Visual|Debug")
    bool bShowLegacyHISMDebugInContinuousSurfaceMode = false;

    /** Deprecated compatibility hook. HISM highlight now uses per-instance custom data. */
    void SetHighlightLUT(class UTexture2D* InLUT);

    //----------------------------------------------------------
    // SimpleGameplay：WorldGen 三地形参数
    //----------------------------------------------------------

    /**
     * SimpleGameplay WorldGen 参数（RandomSeed / 山脉数量与平均节点数 / 森林数量与平均节点数）。
     * 改动后 OnConstruction 自动重跑 Generator->Generate() 并刷新 HISM tile instances。
     * 与 APlanetTopologyDebugMesh 的 WorldGenSettings 字段语义完全一致（两 actor 各跑各自的实例）。
     */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|WorldGen")
    FWorldGenSettings WorldGenSettings;

    //----------------------------------------------------------
    // SimpleGameplay：HISM 静态网格瓦片渲染
    //----------------------------------------------------------

    /** true：主视觉使用每 Cell 一个 StaticMesh 实例的 HISM 瓦片渲染。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    bool bEnableHISMTileRendering = true;

    /** true：HISM 组件开启 QueryOnly 碰撞，可直接参与鼠标拾取。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    bool bEnableHISMTileCollision = true;

    /** 平原 Cell 使用的烘焙 StaticMesh 资产。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    TObjectPtr<UStaticMesh> PlainTileStaticMesh;

    /** 森林 Cell 使用的烘焙 StaticMesh 资产。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    TObjectPtr<UStaticMesh> ForestTileStaticMesh;

    /** 山脉 Cell 使用的烘焙 StaticMesh 资产。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    TObjectPtr<UStaticMesh> MountainTileStaticMesh;

    /** 生成瓦片资产时使用的 BaseRadius。默认 100，与 TerraSphericalTileGenerator 默认值一致。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles", meta = (ClampMin = "1.0"))
    float HISMTileSourceRadiusCM = 100.0f;

    /** HISM 瓦片相对 GlobeRadiusCM 的额外半径偏移。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    float HISMTileRadiusOffsetCM = 0.0f;

    /** HISM 瓦片额外统一缩放。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles", meta = (ClampMin = "0.001"))
    float HISMTileAdditionalUniformScale = 1.0f;

    //----------------------------------------------------------
    // 生命周期
    //----------------------------------------------------------
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
    virtual bool ShouldTickIfViewportsOnly() const override;

    /** 连续表面网格为运行时派生数据；保存时剥离，避免写入 External Actor 包。 */
    virtual void PreSave(FObjectPreSaveContext SaveContext) override;
    virtual void PostSaveRoot(FObjectPostSaveRootContext SaveContext) override;
#endif

    // 注：不 override BeginDestroy()。
    // TUniquePtr fields are released by ~APlanetTessellatedMesh() in .cpp where
    // their complete types are visible; early Reset() from BeginDestroy risks
    // fighting UObject GC/editor teardown ordering.

    /**
     * 重建 CellTopology、WorldGen、HISM tile instances 与 SimpleGameplay 状态。
     * Editor 中拖入关卡或修改属性时（OnConstruction）会自动调用。
     */
    UFUNCTION(CallInEditor, Category = "PlanetTopology|Tess")
    void Rebuild();

    /** 尝试把鼠标命中的 HISM 实例解析成 CellId。 */
    UFUNCTION(BlueprintCallable, Category = "PlanetTopology|Tess|HISM Highlight")
    bool TryResolveHISMHitToCellId(const FHitResult& Hit, int32& OutCellId) const;

    /** 输入层 hover 命中 HISM 时调用；成功处理返回 true。 */
    bool HandleHISMHoverHit(const FHitResult& Hit);

    /** 输入层 click 命中 HISM 时调用；成功处理返回 true。 */
    bool HandleHISMClickHit(const FHitResult& Hit);

    /** SV1：连续基础表面命中后解析 CellId，并复用既有 Gameplay hover 入口。 */
    bool HandleContinuousSurfaceHoverHit(const FHitResult& Hit);

    /** SV1：连续基础表面命中后解析 CellId，并复用既有 Gameplay click 入口。 */
    bool HandleContinuousSurfaceClickHit(const FHitResult& Hit);

    bool IsContinuousTerrainVisualActive() const;

    /** 输入层 Tab / Shift+Tab 循环当前阵营可行动棋子时调用；成功处理返回 true。 */
    bool HandleC5NavigateCurrentFactionPiece(bool bReverse);

    /** 输入层右键撤销当前未提交行动时调用；成功处理返回 true。 */
    bool HandleHISMUndo();

    /** 鼠标离开 HISM 瓦片时调用，只清 hover，不清 select。 */
    void ClearHISMHover();

    /** 清空全部 HISM hover / select 高亮。 */
    UFUNCTION(BlueprintCallable, Category = "PlanetTopology|Tess|HISM Highlight")
    void ClearAllHISMHighlights();

    /** 获取当前 HISM hover CellId。 */
    UFUNCTION(BlueprintCallable, Category = "PlanetTopology|Tess|HISM Highlight")
    int32 GetLastHISMPickedCellId() const;

    /** 获取最近一次 HISM click CellId。 */
    UFUNCTION(BlueprintCallable, Category = "PlanetTopology|Tess|HISM Highlight")
    int32 GetLastHISMClickedCellId() const;

    UFUNCTION(BlueprintCallable, Category = "PlanetTopology|Tess|Camera")
    UPlanetCameraComponent* GetPlanetCameraComponent() const { return PlanetCameraComponent; }

    UFUNCTION(BlueprintCallable, Category = "PlanetTopology|Tess|Gameplay")
    UPlanetGameplayComponent* GetPlanetGameplayComponent() const { return PlanetGameplayComponent; }

    UFUNCTION(BlueprintCallable, Category = "PlanetTopology|Tess|HISM Highlight")
    UPlanetHISMInteractionComponent* GetPlanetHISMInteractionComponent() const { return PlanetHISMInteractionComponent; }

    UFUNCTION(BlueprintCallable, Category = "PlanetTopology|Tess|Piece Presentation")
    UPlanetPiecePresentationComponent* GetPlanetPiecePresentationComponent() const { return PlanetPiecePresentationComponent; }

private:
    /** 整体重建：Cell 拓扑 + WorldGen + HISM Tiles + Gameplay。 */
    void RebuildAll_();

    /** 私有：构造（或重建）玩法 / WorldGen 共用的 Cell 拓扑。 */
    void RebuildTopologies_();

    /** 私有：输出 Cell 拓扑统计到 Output Log。 */
    void LogTopologyStats_() const;

    /** SimpleGameplay：按 WorldGen 三地形输出重建平原 / 森林 / 山脉三套 HISM 实例。 */
    void RebuildHISMTileInstances_();
    void RebuildTerrainVisualSurface_();
    void ApplyHISMSDFExperimentMaterials_();

    FPlanetHISMTileRenderConfig BuildHISMTileRenderConfig_() const;
    FPlanetHISMHighlightConfig BuildHISMHighlightConfig_() const;

    /** SimpleGameplay：统一应用 HISM 显示 / 碰撞开关。 */
    void ApplyRenderModeVisibility_();
    void ApplyTerrainVisualMode_();
    bool UsesTerrainVisualHighlightLUT_() const;
    bool TryResolveContinuousSurfaceHitToCellId_(const FHitResult& Hit, int32& OutCellId) const;

    /** SimpleGameplay：把单个 Cell 当前 Gameplay/hover 逻辑合成为最终 RGB + Intensity 自定义数据。 */
    void WriteHISMHighlightForCell_(int32 CellId, bool bMarkRenderStateDirty = true);
    void RefreshG4CapturePreviewCellsForActionTarget_(int32 ActionTargetCellId, bool bMarkLastRenderStateDirty = true);
    void UpdateHISMHoverCell_(int32 NewCellId);
    void WriteTerrainVisualHighlightForCell_(int32 CellId);
    void RefreshTerrainVisualCapturePreviewCells_(int32 ActionTargetCellId);
    void RefreshAllTerrainVisualHighlights_();

    void RebuildGameplay_();
    void RefreshGameplayHighlights_(const TArray<int32>& DirtyCellIds);
    bool HandleGameplayCellClick_(int32 CellId, const TCHAR* SourceLabel, int32 InstanceIndex, const FString& ComponentName);
    bool TryExecuteNpcMcpValidatedAction_(int32 ExpectedTurnIndex, int32 ExpectedFactionId, int32 PieceId, int32 ToCellId, FTerraGameplayContainer::FValidatedActionExecutionResult& OutResult);
    void RefreshFactionPieceHighlights_(int32 FactionId);
    void RefreshCurrentFactionPieceHighlights_();

    /** SimpleGameplay G2.5：根据 CellId 计算球面 Cell 中心世界坐标。 */
    bool GetCellSurfaceWorldPosition_(int32 CellId, float RadiusOffsetCM, FVector& OutWorldPosition) const;
    bool QueryTerrainSurface_(const FVector& LocalUnitDirection, FTerrainSurfaceQueryResult& OutSurface) const;

    /** SimpleGameplay C6.5：timer 到点后校验仍是同一回合，再调用既有 C2/C2.5 回合镜头入口。 */
    void ExecuteC6_5DelayedTurnStartFocus_(int32 ExpectedTurnIndex, int32 ExpectedFactionId);

    /** SimpleGameplay P1/P2：根据当前 Gameplay 快照增量同步真实棋子 Actor，可选播放 P2 移动事件。 */
    void SyncP1PiecePresentation_(
        const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents = TArray<FTerraPiecePresentationMoveEvent>(),
        const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents = TArray<FTerraPiecePresentationCaptureEvent>());

    /** SimpleGameplay P1：清空真实棋子 Actor 表现。 */
    void ClearP1PiecePresentation_();

    /** SimpleGameplay P1：把 CellId 转为棋子 Actor 的球面世界 Transform。 */
    bool BuildP1PieceWorldTransform_(int32 CellId, FTransform& OutWorldTransform) const;
    bool BuildP1PieceWorldTransform_(int32 CellId, ETerraGameplayPieceType PieceType, FTransform& OutWorldTransform) const;

    /** SimpleGameplay P2.5/P2.6：从采样方向向球心射线命中 HISM，以命中半径修正棋子中心高度。 */
    bool TryResolveP2_5PieceHeightFromHISM_(int32 CellId, const FVector& TraceWorldUp, const FVector& PlacementWorldUp, float TraceAngularOffsetDeg, FVector& OutWorldPosition) const;

    /** SimpleGameplay P1/P2：按"背向本阵营主将"规则计算初始棋子朝向。 */
    bool BuildP1PieceWorldTransformForPiece_(const FTerraGameplayPieceState& Piece, const TArray<FTerraGameplayPieceState>& Pieces, FTransform& OutWorldTransform) const;

    /** SimpleGameplay P3：把 Gameplay 待结算吃子条目转换成表现事件。 */
    void BuildP3CaptureEventsFromPendingEntries_(
        const TArray<FTerraGameplayCaptureEntry>& CaptureEntries,
        const TArray<FTerraGameplayPieceState>& PiecesBeforeResolution,
        TArray<FTerraPiecePresentationCaptureEvent>& OutCaptureEvents) const;

    /** SimpleGameplay P1：从 Details 面板资产槽生成表现配置。 */
    FTerraPieceVisualConfig BuildP1PieceVisualConfig_() const;

public:
    /** G8：取球心世界坐标。 */
    FVector GetPlanetCenterWorld_() const;

    /** G8：取球半径（cm）。 */
    float GetPlanetRadiusCM_() const { return GlobeRadiusCM; }

    /** G8：把某个世界位置同步成球面轨道参数。C3 后仅保留兼容旧路径。 */
    bool SyncOrbitCameraStateFromWorldPosition(const FVector& CameraWorldPosition, float& InOutLongitudeDeg, float& InOutLatitudeDeg, float& InOutHeightOffsetCM) const;

    /** G8：按球面轨道参数把视角应用到当前 PlayerController / ViewTarget。C3 后仅保留兼容旧路径。 */
    bool ApplyOrbitCameraState(float LongitudeDeg, float LatitudeDeg, float HeightOffsetCM);

    /**
     * C3：从当前相机位置和旋转反推焦点式手动相机状态。仅在进入手动模式的首帧被调用一次；
     * 后续帧不再调用（详见 C3FocusCameraManualControlDesign.md §4.2）。
     */
    bool SyncFocusCameraStateFromView(
        const FVector& CameraWorldPosition,
        const FRotator& CameraWorldRotation,
        FVector& OutFocusUnitDir,
        float& OutDistanceToFocusCM,
        float& OutTiltDeg,
        float& OutYawAroundFocusDeg) const;

    /** C3：按焦点式相机状态把视角应用到当前 PlayerController / ViewTarget。 */
    bool ApplyFocusCameraState(
        const FVector& FocusUnitDir,
        float DistanceToFocusCM,
        float TiltDeg,
        float YawAroundFocusDeg);

    /**
     * C3：在当前焦点局部切平面上沿测地线（大圆）推进焦点，同时用 Rodrigues 旋转把 Yaw
     * 平行运输到新焦点。这样连续按住 A/D 或 W/S 会走一整条大圆，不会退化成纬线。
     * 详见 C3FocusCameraManualControlDesign.md §5.1.3。
     */
    bool OffsetFocusCameraStateOnTangent(
        float RightDeltaDeg,
        float ForwardDeltaDeg,
        FVector& InOutFocusUnitDir,
        float& InOutYawAroundFocusDeg) const;

    void RebuildG1DebugPieces_();
    void DrawG1DebugPieces_() const;

    /** D15：PIE 退出后材质恢复钩子（详见 AgentWorkflow.md §3.11）。 */
    void OnPostWorldCleanup_(class UWorld* World, bool bSessionEnded, bool bCleanupResources);

    //----------------------------------------------------------
    // 运行时持有
    //----------------------------------------------------------

    /** 逻辑层 sub=CellSubdivisionLevel 拓扑实例（玩法 / WorldGen 共用）。 */
    TUniquePtr<FSphereTopology> CellTopology;

    /**
     * WorldGen 主类实例。Rebuild() 中按顺序：
     *   RebuildTopologies_ → MakeUnique<FWorldGenerator>(CellTopology, Settings) → Generate()
     *     → RebuildHISMTileInstances_ → RebuildGameplay_。
     */
    TUniquePtr<FWorldGenerator> Generator;

    /** SimpleGameplay：平原瓦片 HISM 组件。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlanetTopology|Tess|HISM Tiles",
              meta = (AllowPrivateAccess = "true", NoEditInline))
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PlainTileHISMComp;

    /** SimpleGameplay：森林瓦片 HISM 组件。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlanetTopology|Tess|HISM Tiles",
              meta = (AllowPrivateAccess = "true", NoEditInline))
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ForestTileHISMComp;

    /** SimpleGameplay：山脉瓦片 HISM 组件。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlanetTopology|Tess|HISM Tiles",
              meta = (AllowPrivateAccess = "true", NoEditInline))
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> MountainTileHISMComp;

    /** TerrainVisual SV1：连续基础表面，独立于旧 HISM 渲染组件。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlanetTopology|Terrain Visual",
              meta = (AllowPrivateAccess = "true", NoEditInline))
    TObjectPtr<UTerrainVisualSurfaceComponent> TerrainVisualSurfaceComp;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> PlainHISMSDFMID;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> ForestHISMSDFMID;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> MountainHISMSDFMID;

    /** TerrainVisual SV0/SV1：只读视觉场和 Cell 查询协调器。 */
    TUniquePtr<FTerrainVisualCoordinator> TerrainVisualCoordinator;

#if WITH_EDITOR
    /** 本轮保存前是否剥离了连续表面，需要在保存完成后恢复编辑器预览。 */
    bool bRestoreTerrainVisualAfterSave = false;
#endif

    /** HISM 瓦片渲染、实例索引、命中反查与 PerInstanceCustomData 高亮状态。 */
    FPlanetHISMTileRenderer HISMTileRenderer;

    /** SimpleGameplay Camera：独立相机组件，承载配置字段与镜头运行时状态。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlanetTopology|Tess|Camera",
              meta = (AllowPrivateAccess = "true", NoEditInline))
    TObjectPtr<UPlanetCameraComponent> PlanetCameraComponent;

    /** SimpleGameplay Gameplay：独立玩法编排组件，承载回合/点击/Undo/NPC 执行等运行时状态。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlanetTopology|Tess|Gameplay",
              meta = (AllowPrivateAccess = "true", NoEditInline))
    TObjectPtr<UPlanetGameplayComponent> PlanetGameplayComponent;

    /** SimpleGameplay HISM Interaction：独立 HISM 命中解析、高亮写入与 hover/click 交互组件。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlanetTopology|Tess|HISM Highlight",
              meta = (AllowPrivateAccess = "true", NoEditInline))
    TObjectPtr<UPlanetHISMInteractionComponent> PlanetHISMInteractionComponent;

    /** SimpleGameplay Piece Presentation：独立棋子表现组件，承载配置字段与表现运行时状态。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlanetTopology|Tess|Piece Presentation",
              meta = (AllowPrivateAccess = "true", NoEditInline))
    TObjectPtr<UPlanetPiecePresentationComponent> PlanetPiecePresentationComponent;

    //----------------------------------------------------------
    // D15：PIE 退出后材质恢复（详见 AgentWorkflow.md §3.11）
    //----------------------------------------------------------
    /** FWorldDelegates::OnPostWorldCleanup 委托句柄（构造函数末尾注册，析构中解绑）。 */
    FDelegateHandle PostWorldCleanupHandle;
};
