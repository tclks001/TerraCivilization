// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WorldGenSettings.h"

class FTerraGameplayContainer;
class FSphereTopology;
class UHierarchicalInstancedStaticMeshComponent;
class UPrimitiveComponent;
class UStaticMesh;
struct FCellGeoData;
struct FHitResult;

struct FTerraHISMCellInstanceRef
{
    UHierarchicalInstancedStaticMeshComponent* Component = nullptr;
    int32 InstanceIndex = INDEX_NONE;

    bool IsValid() const { return Component != nullptr && InstanceIndex != INDEX_NONE; }
};

struct FPlanetHISMTileRenderConfig
{
    bool bEnableRendering = true;
    bool bEnableCollision = true;
    bool bEnableInstanceHighlight = true;

    UStaticMesh* PlainTileStaticMesh = nullptr;
    UStaticMesh* ForestTileStaticMesh = nullptr;
    UStaticMesh* MountainTileStaticMesh = nullptr;

    float GlobeRadiusCM = 15000.0f;
    float SourceRadiusCM = 100.0f;
    float RadiusOffsetCM = 0.0f;
    float AdditionalUniformScale = 1.0f;
};

struct FPlanetHISMHighlightConfig
{
    bool bEnableInstanceHighlight = true;
    float HighlightStrength = 1.5f;
    float PlainHighlightInnerRadius = 0.2f;
    float PlainHighlightOuterRadius = 0.5f;
    float ForestHighlightInnerRadius = 0.2f;
    float ForestHighlightOuterRadius = 0.5f;
    float MountainHighlightInnerRadius = 0.2f;
    float MountainHighlightOuterRadius = 0.5f;
    FLinearColor HoverColor = FLinearColor(1.0f, 0.85f, 0.10f, 1.0f);
    FLinearColor CurrentFactionPieceColor = FLinearColor(1.0f, 0.45f, 0.68f, 1.0f);
    FLinearColor CurrentFactionPieceHoverColor = FLinearColor(1.0f, 0.22f, 0.32f, 1.0f);
    FLinearColor ActionTargetHoverColor = FLinearColor(0.08f, 0.45f, 1.0f, 1.0f);
    FLinearColor CaptureTargetHoverColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
};

class TERRACIVILIZATION_API FPlanetHISMTileRenderer
{
public:
    static constexpr int32 HighlightCustomDataOffset = 0;
    static constexpr int32 CellContextCustomDataOffset = 4;
    static constexpr int32 CellContextCandidateCount = 7;
    static constexpr int32 CustomDataFloatCount = CellContextCustomDataOffset + CellContextCandidateCount;

    void Initialize(
        UHierarchicalInstancedStaticMeshComponent* InPlainComp,
        UHierarchicalInstancedStaticMeshComponent* InForestComp,
        UHierarchicalInstancedStaticMeshComponent* InMountainComp);

    void RebuildInstances(
        const FSphereTopology& CellTopology,
        const TArray<FCellGeoData>& GeoCells,
        const FPlanetHISMTileRenderConfig& Config);

    void ApplyVisibility(bool bEnableRendering, bool bEnableCollision);
    void PrepareHighlightComponents(const FPlanetHISMHighlightConfig& Config);

    bool TryResolveHitToCellId(const FHitResult& Hit, bool bRequireCollisionEnabled, int32& OutCellId) const;

    void TickHoverFade(
        float DeltaSeconds,
        float HoverFadeDuration,
        const FPlanetHISMHighlightConfig& Config,
        const FTerraGameplayContainer* GameplayContainer);

    void UpdateHoverCell(
        int32 NewCellId,
        float HoverFadeDuration,
        const FPlanetHISMHighlightConfig& Config,
        const FTerraGameplayContainer* GameplayContainer);

    void ClearHover(
        float HoverFadeDuration,
        const FPlanetHISMHighlightConfig& Config,
        const FTerraGameplayContainer* GameplayContainer);

    void ClearAllHighlights(const FPlanetHISMHighlightConfig& Config, const FTerraGameplayContainer* GameplayContainer);

    void WriteHighlightForCell(
        int32 CellId,
        const FPlanetHISMHighlightConfig& Config,
        const FTerraGameplayContainer* GameplayContainer,
        bool bMarkRenderStateDirty = true);

    int32 GetCurrentHoverCellId() const { return CurrentHoverCellId; }
    int32 GetLastPickedCellId() const { return LastPickedCellId; }
    int32 GetLastClickedCellId() const { return LastClickedCellId; }
    void SetLastClickedCellId(int32 CellId) { LastClickedCellId = CellId; }

    bool HasComponents() const;
    bool IsHISMComponent(const UPrimitiveComponent* Component) const;

private:
    void ResetRuntimeState_();
    TArray<int32>* FindInstanceMap_(const UPrimitiveComponent* Component);
    const TArray<int32>* FindInstanceMap_(const UPrimitiveComponent* Component) const;

    UHierarchicalInstancedStaticMeshComponent* PlainComp = nullptr;
    UHierarchicalInstancedStaticMeshComponent* ForestComp = nullptr;
    UHierarchicalInstancedStaticMeshComponent* MountainComp = nullptr;

    TArray<int32> PlainInstanceToCellId;
    TArray<int32> ForestInstanceToCellId;
    TArray<int32> MountainInstanceToCellId;
    TArray<FTerraHISMCellInstanceRef> CellIdToInstance;

    int32 CurrentHoverCellId = INDEX_NONE;
    int32 PendingHoverCellId = INDEX_NONE;
    float HoverFadeTimer = 0.0f;
    int32 LastPickedCellId = INDEX_NONE;
    int32 LastClickedCellId = INDEX_NONE;
};
