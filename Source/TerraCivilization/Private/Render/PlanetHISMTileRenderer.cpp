// Fill out your copyright notice in the Description page of Project Settings.

#include "Render/PlanetHISMTileRenderer.h"

#include "CellGeoData.h"
#include "FCell.h"
#include "FSphereTopology.h"
#include "TerraGameplayContainer.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlanetHISMTileRenderer, Log, All);

void FPlanetHISMTileRenderer::Initialize(
    UHierarchicalInstancedStaticMeshComponent* InPlainComp,
    UHierarchicalInstancedStaticMeshComponent* InForestComp,
    UHierarchicalInstancedStaticMeshComponent* InMountainComp)
{
    PlainComp = InPlainComp;
    ForestComp = InForestComp;
    MountainComp = InMountainComp;
}

bool FPlanetHISMTileRenderer::HasComponents() const
{
    return PlainComp && ForestComp && MountainComp;
}

bool FPlanetHISMTileRenderer::IsHISMComponent(const UPrimitiveComponent* Component) const
{
    return Component && (Component == PlainComp || Component == ForestComp || Component == MountainComp);
}

void FPlanetHISMTileRenderer::ResetRuntimeState_()
{
    PlainInstanceToCellId.Reset();
    ForestInstanceToCellId.Reset();
    MountainInstanceToCellId.Reset();
    CellIdToInstance.Reset();
    CurrentHoverCellId = INDEX_NONE;
    PendingHoverCellId = INDEX_NONE;
    HoverFadeTimer = 0.0f;
    LastPickedCellId = INDEX_NONE;
    LastClickedCellId = INDEX_NONE;
}

TArray<int32>* FPlanetHISMTileRenderer::FindInstanceMap_(const UPrimitiveComponent* Component)
{
    if (Component == PlainComp)
    {
        return &PlainInstanceToCellId;
    }
    if (Component == ForestComp)
    {
        return &ForestInstanceToCellId;
    }
    if (Component == MountainComp)
    {
        return &MountainInstanceToCellId;
    }
    return nullptr;
}

const TArray<int32>* FPlanetHISMTileRenderer::FindInstanceMap_(const UPrimitiveComponent* Component) const
{
    if (Component == PlainComp)
    {
        return &PlainInstanceToCellId;
    }
    if (Component == ForestComp)
    {
        return &ForestInstanceToCellId;
    }
    if (Component == MountainComp)
    {
        return &MountainInstanceToCellId;
    }
    return nullptr;
}

void FPlanetHISMTileRenderer::PrepareHighlightComponents(const FPlanetHISMHighlightConfig& Config)
{
    auto PrepareOne = [&Config](UHierarchicalInstancedStaticMeshComponent* Comp)
    {
        if (!Comp)
        {
            return;
        }

        Comp->SetNumCustomDataFloats(4);

        const int32 MaterialCount = Comp->GetNumMaterials();
        for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
        {
            if (UMaterialInterface* Material = Comp->GetMaterial(MaterialIndex))
            {
                UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Material);
                if (!MID)
                {
                    MID = Comp->CreateDynamicMaterialInstance(MaterialIndex, Material);
                }
                if (MID)
                {
                    MID->SetScalarParameterValue(TEXT("HighlightStrength"), Config.HighlightStrength);
                    MID->SetScalarParameterValue(TEXT("HighlightInnerRadius"), Config.HighlightInnerRadius);
                    MID->SetScalarParameterValue(TEXT("HighlightOuterRadius"), Config.HighlightOuterRadius);
                }
            }
        }
    };

    PrepareOne(PlainComp);
    PrepareOne(ForestComp);
    PrepareOne(MountainComp);
}

void FPlanetHISMTileRenderer::RebuildInstances(
    const FSphereTopology& CellTopology,
    const TArray<FCellGeoData>& GeoCells,
    const FPlanetHISMTileRenderConfig& Config)
{
    if (!HasComponents())
    {
        return;
    }

    PlainComp->ClearInstances();
    ForestComp->ClearInstances();
    MountainComp->ClearInstances();
    ResetRuntimeState_();

    PlainComp->SetStaticMesh(Config.PlainTileStaticMesh);
    ForestComp->SetStaticMesh(Config.ForestTileStaticMesh);
    MountainComp->SetStaticMesh(Config.MountainTileStaticMesh);

    if (!Config.bEnableRendering)
    {
        return;
    }

    const int32 NumCells = CellTopology.Cells.Num();
    CellIdToInstance.SetNum(NumCells);
    if (GeoCells.Num() != NumCells)
    {
        UE_LOG(LogPlanetHISMTileRenderer, Warning,
            TEXT("[HISM Tiles] Skip rebuild: WorldGen cell count mismatch. Got=%d Expected=%d"),
            GeoCells.Num(), NumCells);
        return;
    }

    if (!Config.PlainTileStaticMesh || !Config.ForestTileStaticMesh || !Config.MountainTileStaticMesh)
    {
        UE_LOG(LogPlanetHISMTileRenderer, Warning,
            TEXT("[HISM Tiles] Need all three StaticMesh assets. Plain=%s Forest=%s Mountain=%s"),
            *GetNameSafe(Config.PlainTileStaticMesh),
            *GetNameSafe(Config.ForestTileStaticMesh),
            *GetNameSafe(Config.MountainTileStaticMesh));
    }

    const float SourceRadius = FMath::Max(Config.SourceRadiusCM, 1.0f);
    const float TargetRadius = FMath::Max(Config.GlobeRadiusCM + Config.RadiusOffsetCM, 1.0f);
    const float UniformScale = (TargetRadius / SourceRadius) * FMath::Max(Config.AdditionalUniformScale, 0.001f);
    const FVector Scale3D(UniformScale);

    int32 PlainCount = 0;
    int32 ForestCount = 0;
    int32 MountainCount = 0;
    int32 MissingMeshCount = 0;

    for (int32 CellId = 0; CellId < NumCells; ++CellId)
    {
        const FVector UnitCenter = CellTopology.Cells[CellId].UnitCenter.GetSafeNormal();
        if (UnitCenter.IsNearlyZero())
        {
            continue;
        }

        UHierarchicalInstancedStaticMeshComponent* TargetComp = nullptr;
        switch (GeoCells[CellId].SimpleTerrainType)
        {
        case ETerraSimpleTerrainType::Forest:
            TargetComp = ForestComp;
            ++ForestCount;
            break;
        case ETerraSimpleTerrainType::Mountain:
            TargetComp = MountainComp;
            ++MountainCount;
            break;
        case ETerraSimpleTerrainType::Plain:
        default:
            TargetComp = PlainComp;
            ++PlainCount;
            break;
        }

        if (!TargetComp || !TargetComp->GetStaticMesh())
        {
            ++MissingMeshCount;
            continue;
        }

        const FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, UnitCenter);
        const int32 InstanceIndex = TargetComp->AddInstance(FTransform(Rotation, FVector::ZeroVector, Scale3D), false);
        if (InstanceIndex == INDEX_NONE)
        {
            continue;
        }

        if (TArray<int32>* InstanceToCellId = FindInstanceMap_(TargetComp))
        {
            if (InstanceToCellId->Num() <= InstanceIndex)
            {
                InstanceToCellId->SetNum(InstanceIndex + 1);
            }
            (*InstanceToCellId)[InstanceIndex] = CellId;
        }

        if (CellIdToInstance.IsValidIndex(CellId))
        {
            CellIdToInstance[CellId].Component = TargetComp;
            CellIdToInstance[CellId].InstanceIndex = InstanceIndex;
        }

        if (Config.bEnableInstanceHighlight)
        {
            TargetComp->SetCustomDataValue(InstanceIndex, 0, 0.0f, false);
            TargetComp->SetCustomDataValue(InstanceIndex, 1, 0.0f, false);
            TargetComp->SetCustomDataValue(InstanceIndex, 2, 0.0f, false);
            TargetComp->SetCustomDataValue(InstanceIndex, 3, 0.0f, false);
        }
    }

    UE_LOG(LogPlanetHISMTileRenderer, Log,
        TEXT("[HISM Tiles] Rebuilt spherical tiles: Plain=%d Forest=%d Mountain=%d MissingMeshSkipped=%d Radius=%.1fcm SourceRadius=%.1fcm Scale=%.4f"),
        PlainCount, ForestCount, MountainCount, MissingMeshCount, TargetRadius, SourceRadius, UniformScale);
}

void FPlanetHISMTileRenderer::ApplyVisibility(bool bEnableRendering, bool bEnableCollision)
{
    const ECollisionEnabled::Type Collision =
        (bEnableRendering && bEnableCollision) ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision;

    auto ApplyOne = [bEnableRendering, Collision](UHierarchicalInstancedStaticMeshComponent* Comp)
    {
        if (!Comp)
        {
            return;
        }
        Comp->SetVisibility(bEnableRendering, true);
        Comp->SetHiddenInGame(!bEnableRendering);
        Comp->SetCollisionEnabled(Collision);
        Comp->SetCollisionObjectType(ECC_WorldStatic);
        Comp->SetCollisionResponseToAllChannels(ECR_Block);
    };

    ApplyOne(PlainComp);
    ApplyOne(ForestComp);
    ApplyOne(MountainComp);
}

bool FPlanetHISMTileRenderer::TryResolveHitToCellId(
    const FHitResult& Hit,
    bool bRequireCollisionEnabled,
    int32& OutCellId) const
{
    OutCellId = INDEX_NONE;
    if (bRequireCollisionEnabled)
    {
        const UPrimitiveComponent* HitComp = Hit.GetComponent();
        if (!HitComp || HitComp->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
        {
            return false;
        }
    }

    const UPrimitiveComponent* HitComp = Hit.GetComponent();
    const int32 InstanceIndex = Hit.Item;
    if (!HitComp || InstanceIndex == INDEX_NONE)
    {
        return false;
    }

    const TArray<int32>* InstanceToCellId = FindInstanceMap_(HitComp);
    if (!InstanceToCellId || !InstanceToCellId->IsValidIndex(InstanceIndex))
    {
        return false;
    }

    const int32 CellId = (*InstanceToCellId)[InstanceIndex];
    if (!CellIdToInstance.IsValidIndex(CellId))
    {
        return false;
    }

    OutCellId = CellId;
    return true;
}

void FPlanetHISMTileRenderer::TickHoverFade(
    float DeltaSeconds,
    float HoverFadeDuration,
    const FPlanetHISMHighlightConfig& Config,
    const FTerraGameplayContainer* GameplayContainer)
{
    if (HoverFadeTimer <= 0.0f)
    {
        return;
    }

    HoverFadeTimer -= DeltaSeconds;
    if (HoverFadeTimer > 0.0f)
    {
        return;
    }

    HoverFadeTimer = 0.0f;
    const int32 OldHover = CurrentHoverCellId;
    CurrentHoverCellId = INDEX_NONE;
    LastPickedCellId = INDEX_NONE;
    if (OldHover != INDEX_NONE)
    {
        WriteHighlightForCell(OldHover, Config, GameplayContainer, false);
        TArray<int32> CaptureCellIds;
        if (GameplayContainer && GameplayContainer->CollectCapturePreviewCellIdsForActionTarget(OldHover, CaptureCellIds))
        {
            for (int32 I = 0; I < CaptureCellIds.Num(); ++I)
            {
                WriteHighlightForCell(CaptureCellIds[I], Config, GameplayContainer, I == CaptureCellIds.Num() - 1);
            }
        }
    }
}

void FPlanetHISMTileRenderer::UpdateHoverCell(
    int32 NewCellId,
    float HoverFadeDuration,
    const FPlanetHISMHighlightConfig& Config,
    const FTerraGameplayContainer* GameplayContainer)
{
    if (!Config.bEnableInstanceHighlight)
    {
        return;
    }

    if (NewCellId != INDEX_NONE && !CellIdToInstance.IsValidIndex(NewCellId))
    {
        return;
    }

    auto RefreshCapturePreview = [this, &Config, GameplayContainer](int32 ActionTargetCellId, bool bMarkLastRenderStateDirty)
    {
        if (!GameplayContainer || ActionTargetCellId == INDEX_NONE)
        {
            return;
        }

        TArray<int32> CaptureCellIds;
        if (!GameplayContainer->CollectCapturePreviewCellIdsForActionTarget(ActionTargetCellId, CaptureCellIds))
        {
            return;
        }

        for (int32 I = 0; I < CaptureCellIds.Num(); ++I)
        {
            WriteHighlightForCell(CaptureCellIds[I], Config, GameplayContainer, bMarkLastRenderStateDirty && I == CaptureCellIds.Num() - 1);
        }
    };

    if (NewCellId == INDEX_NONE)
    {
        LastPickedCellId = INDEX_NONE;
        if (CurrentHoverCellId == INDEX_NONE)
        {
            return;
        }
        if (PendingHoverCellId != INDEX_NONE)
        {
            PendingHoverCellId = INDEX_NONE;
            HoverFadeTimer = HoverFadeDuration;
        }
        return;
    }

    LastPickedCellId = NewCellId;

    if (CurrentHoverCellId == INDEX_NONE)
    {
        CurrentHoverCellId = NewCellId;
        PendingHoverCellId = NewCellId;
        HoverFadeTimer = 0.0f;
        WriteHighlightForCell(NewCellId, Config, GameplayContainer, false);
        RefreshCapturePreview(NewCellId, true);
        return;
    }

    if (NewCellId == CurrentHoverCellId)
    {
        PendingHoverCellId = NewCellId;
        HoverFadeTimer = 0.0f;
        return;
    }

    const int32 OldHover = CurrentHoverCellId;
    CurrentHoverCellId = NewCellId;
    PendingHoverCellId = NewCellId;
    HoverFadeTimer = 0.0f;
    WriteHighlightForCell(OldHover, Config, GameplayContainer, false);
    RefreshCapturePreview(OldHover, false);
    WriteHighlightForCell(NewCellId, Config, GameplayContainer, false);
    RefreshCapturePreview(NewCellId, true);
}

void FPlanetHISMTileRenderer::ClearHover(
    float HoverFadeDuration,
    const FPlanetHISMHighlightConfig& Config,
    const FTerraGameplayContainer* GameplayContainer)
{
    UpdateHoverCell(INDEX_NONE, HoverFadeDuration, Config, GameplayContainer);
}

void FPlanetHISMTileRenderer::ClearAllHighlights(
    const FPlanetHISMHighlightConfig& Config,
    const FTerraGameplayContainer* GameplayContainer)
{
    if (CurrentHoverCellId == INDEX_NONE)
    {
        return;
    }

    const int32 OldHover = CurrentHoverCellId;
    CurrentHoverCellId = INDEX_NONE;
    PendingHoverCellId = INDEX_NONE;
    HoverFadeTimer = 0.0f;
    LastPickedCellId = INDEX_NONE;
    WriteHighlightForCell(OldHover, Config, GameplayContainer, false);

    TArray<int32> CaptureCellIds;
    if (GameplayContainer && GameplayContainer->CollectCapturePreviewCellIdsForActionTarget(OldHover, CaptureCellIds))
    {
        for (int32 I = 0; I < CaptureCellIds.Num(); ++I)
        {
            WriteHighlightForCell(CaptureCellIds[I], Config, GameplayContainer, I == CaptureCellIds.Num() - 1);
        }
    }
}

void FPlanetHISMTileRenderer::WriteHighlightForCell(
    int32 CellId,
    const FPlanetHISMHighlightConfig& Config,
    const FTerraGameplayContainer* GameplayContainer,
    bool bMarkRenderStateDirty)
{
    if (!Config.bEnableInstanceHighlight || !CellIdToInstance.IsValidIndex(CellId))
    {
        return;
    }

    const FTerraHISMCellInstanceRef& Ref = CellIdToInstance[CellId];
    if (!Ref.IsValid())
    {
        return;
    }

    const float HoverIntensity = (CurrentHoverCellId == CellId) ? 1.0f : 0.0f;

    FLinearColor FinalHighlightColor = FLinearColor::Black;
    float FinalHighlightIntensity = 0.0f;

    FTerraGameplayCellHighlight GameplayHighlight;
    const bool bHasGameplayActionHighlight = GameplayContainer
        && GameplayContainer->GetHighlightForCell(CellId, GameplayHighlight);
    const bool bIsCurrentFactionPieceCell = GameplayContainer
        && GameplayContainer->IsCurrentFactionPieceCell(CellId);

    if (bHasGameplayActionHighlight)
    {
        const bool bIsG4CaptureTargetHover = GameplayContainer
            && CurrentHoverCellId != INDEX_NONE
            && GameplayContainer->IsCapturePreviewCellForActionTarget(CellId, CurrentHoverCellId);
        const bool bIsActionTargetHover = HoverIntensity > KINDA_SMALL_NUMBER
            && GameplayContainer
            && GameplayContainer->IsCurrentActionTargetCell(CellId);
        FinalHighlightColor = bIsG4CaptureTargetHover
            ? Config.CaptureTargetHoverColor
            : (bIsActionTargetHover ? Config.ActionTargetHoverColor : GameplayHighlight.Color);
        FinalHighlightIntensity = GameplayHighlight.Intensity;
    }
    else if (bIsCurrentFactionPieceCell && HoverIntensity > KINDA_SMALL_NUMBER)
    {
        FinalHighlightColor = Config.CurrentFactionPieceHoverColor;
        FinalHighlightIntensity = 1.0f;
    }
    else if (bIsCurrentFactionPieceCell)
    {
        FinalHighlightColor = Config.CurrentFactionPieceColor;
        FinalHighlightIntensity = 1.0f;
    }
    else if (HoverIntensity > KINDA_SMALL_NUMBER)
    {
        FinalHighlightColor = Config.HoverColor;
        FinalHighlightIntensity = HoverIntensity;
    }

    FinalHighlightIntensity = FMath::Clamp(FinalHighlightIntensity, 0.0f, 1.0f);

    const bool bRedWritten = Ref.Component->SetCustomDataValue(Ref.InstanceIndex, 0, FMath::Clamp(FinalHighlightColor.R, 0.0f, 1.0f), false);
    const bool bGreenWritten = Ref.Component->SetCustomDataValue(Ref.InstanceIndex, 1, FMath::Clamp(FinalHighlightColor.G, 0.0f, 1.0f), false);
    const bool bBlueWritten = Ref.Component->SetCustomDataValue(Ref.InstanceIndex, 2, FMath::Clamp(FinalHighlightColor.B, 0.0f, 1.0f), false);
    const bool bIntensityWritten = Ref.Component->SetCustomDataValue(Ref.InstanceIndex, 3, FinalHighlightIntensity, bMarkRenderStateDirty);

    if (bMarkRenderStateDirty)
    {
        Ref.Component->MarkRenderInstancesDirty();
    }

    if (!bRedWritten || !bGreenWritten || !bBlueWritten || !bIntensityWritten)
    {
        UE_LOG(LogPlanetHISMTileRenderer, Warning,
            TEXT("[HISM Tiles] Failed to write highlight custom data. Cell=%d Instance=%d Component=%s RWritten=%d GWritten=%d BWritten=%d IntensityWritten=%d Hover=%.1f Gameplay=%.1f Final=(%.3f, %.3f, %.3f, %.3f)"),
            CellId,
            Ref.InstanceIndex,
            *GetNameSafe(Ref.Component),
            bRedWritten ? 1 : 0,
            bGreenWritten ? 1 : 0,
            bBlueWritten ? 1 : 0,
            bIntensityWritten ? 1 : 0,
            HoverIntensity,
            GameplayHighlight.Intensity,
            FinalHighlightColor.R,
            FinalHighlightColor.G,
            FinalHighlightColor.B,
            FinalHighlightIntensity);
    }
}
