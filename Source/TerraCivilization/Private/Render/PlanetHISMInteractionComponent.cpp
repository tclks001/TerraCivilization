#include "Render/PlanetHISMInteractionComponent.h"

#include "Render/PlanetGameplayComponent.h"
#include "Render/PlanetTessellatedMesh.h"

#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Logging/LogMacros.h"

UPlanetHISMInteractionComponent::UPlanetHISMInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

namespace
{
DEFINE_LOG_CATEGORY_STATIC(LogPlanetHISMInteraction, Log, All);
}

APlanetTessellatedMesh* UPlanetHISMInteractionComponent::GetHost() const
{
    return Cast<APlanetTessellatedMesh>(GetOwner());
}

FPlanetHISMHighlightConfig UPlanetHISMInteractionComponent::BuildHighlightConfig() const
{
    APlanetTessellatedMesh* Host = GetHost();
    FPlanetHISMHighlightConfig Config;
    Config.bEnableInstanceHighlight = bEnableHISMInstanceHighlight;
    Config.HighlightStrength = HighlightStrength;
    Config.PlainHighlightInnerRadius = PlainHISMHighlightInnerRadius;
    Config.PlainHighlightOuterRadius = PlainHISMHighlightOuterRadius;
    Config.ForestHighlightInnerRadius = ForestHISMHighlightInnerRadius;
    Config.ForestHighlightOuterRadius = ForestHISMHighlightOuterRadius;
    Config.MountainHighlightInnerRadius = MountainHISMHighlightInnerRadius;
    Config.MountainHighlightOuterRadius = MountainHISMHighlightOuterRadius;
    Config.HoverColor = HighlightHoverColor;
    if (Host && Host->GetPlanetGameplayComponent())
    {
        Config.CurrentFactionPieceColor = Host->GetPlanetGameplayComponent()->G2_5CurrentFactionPieceColor;
        Config.CurrentFactionPieceHoverColor = Host->GetPlanetGameplayComponent()->G2_5CurrentFactionPieceHoverColor;
        Config.ActionTargetHoverColor = Host->GetPlanetGameplayComponent()->G3ActionTargetHoverColor;
        Config.CaptureTargetHoverColor = Host->GetPlanetGameplayComponent()->G4CaptureTargetHoverColor;
    }
    return Config;
}

FPlanetHISMHighlightConfig UPlanetHISMInteractionComponent::BuildRuntimeHighlightConfig_() const
{
    FPlanetHISMHighlightConfig Config = BuildHighlightConfig();
    if (const APlanetTessellatedMesh* Host = GetHost(); Host && Host->IsHISMSDFTerrainVisualActive())
    {
        // SV7 keeps the renderer's hover state machine but routes visible highlights through SurfaceHighlightLUT.
        Config.bEnableInstanceHighlight = false;
    }
    return Config;
}

void UPlanetHISMInteractionComponent::PrepareHighlightComponents()
{
    if (APlanetTessellatedMesh* Host = GetHost())
    {
        Host->HISMTileRenderer.PrepareHighlightComponents(BuildHighlightConfig());
    }
}

void UPlanetHISMInteractionComponent::SetHighlightLUT(UTexture2D* InLUT)
{
    UE_LOG(LogPlanetHISMInteraction, Verbose,
        TEXT("[PlanetHISM] Deprecated SetHighlightLUT ignored (%s). HISM highlight uses PerInstanceCustomData."),
        *GetNameSafe(InLUT));
}

int32 UPlanetHISMInteractionComponent::GetLastHISMPickedCellId() const
{
    const APlanetTessellatedMesh* Host = GetHost();
    return Host ? Host->HISMTileRenderer.GetLastPickedCellId() : INDEX_NONE;
}

int32 UPlanetHISMInteractionComponent::GetLastHISMClickedCellId() const
{
    const APlanetTessellatedMesh* Host = GetHost();
    return Host ? Host->HISMTileRenderer.GetLastClickedCellId() : INDEX_NONE;
}

void UPlanetHISMInteractionComponent::TickHoverFade(float DeltaSeconds)
{
    if (APlanetTessellatedMesh* Host = GetHost())
    {
        const int32 OldHoverCellId = Host->HISMTileRenderer.GetCurrentHoverCellId();
        Host->HISMTileRenderer.TickHoverFade(
            DeltaSeconds,
            HISMHoverFadeDuration,
            BuildRuntimeHighlightConfig_(),
            Host->GetPlanetGameplayComponent() ? Host->GetPlanetGameplayComponent()->GetGameplayContainer() : nullptr);

        if (Host->UsesTerrainVisualHighlightLUT_()
            && OldHoverCellId != Host->HISMTileRenderer.GetCurrentHoverCellId())
        {
            Host->WriteTerrainVisualHighlightForCell_(OldHoverCellId);
            Host->RefreshTerrainVisualCapturePreviewCells_(OldHoverCellId);
        }
    }
}

bool UPlanetHISMInteractionComponent::TryResolveHISMHitToCellId(const FHitResult& Hit, int32& OutCellId) const
{
    const APlanetTessellatedMesh* Host = GetHost();
    if (!Host || !bEnableHISMInstanceHighlight || !Host->bEnableHISMTileRendering || !Host->bEnableHISMTileCollision)
    {
        OutCellId = INDEX_NONE;
        return false;
    }

    return Host->HISMTileRenderer.TryResolveHitToCellId(Hit, Host->bEnableHISMTileCollision, OutCellId);
}

bool UPlanetHISMInteractionComponent::TryResolveInteractionHitToCellId_(const FHitResult& Hit, int32& OutCellId) const
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        OutCellId = INDEX_NONE;
        return false;
    }

    if (!Host->IsHISMSDFTerrainVisualActive())
    {
        return TryResolveHISMHitToCellId(Hit, OutCellId);
    }

    if (!Host->bEnableHISMTileCollision
        || Hit.GetActor() != Host
        || !Hit.GetComponent()
        || Hit.GetComponent()->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
    {
        OutCellId = INDEX_NONE;
        return false;
    }

    return Host->ResolveTerrainCellFromWorldPosition(Hit.ImpactPoint, OutCellId);
}

void UPlanetHISMInteractionComponent::WriteHISMHighlightForCell(int32 CellId, bool bMarkRenderStateDirty)
{
    if (APlanetTessellatedMesh* Host = GetHost())
    {
        Host->HISMTileRenderer.WriteHighlightForCell(
            CellId,
            BuildRuntimeHighlightConfig_(),
            Host->GetPlanetGameplayComponent() ? Host->GetPlanetGameplayComponent()->GetGameplayContainer() : nullptr,
            bMarkRenderStateDirty);
    }
}

void UPlanetHISMInteractionComponent::RefreshCapturePreviewCellsForActionTarget(int32 ActionTargetCellId, bool bMarkLastRenderStateDirty)
{
    APlanetTessellatedMesh* Host = GetHost();
    FTerraGameplayContainer* Gameplay = (Host && Host->GetPlanetGameplayComponent())
        ? Host->GetPlanetGameplayComponent()->GetGameplayContainer()
        : nullptr;
    if (!Gameplay || ActionTargetCellId == INDEX_NONE)
    {
        return;
    }

    TArray<int32> CaptureCellIds;
    if (!Gameplay->CollectCapturePreviewCellIdsForActionTarget(ActionTargetCellId, CaptureCellIds))
    {
        return;
    }

    for (int32 I = 0; I < CaptureCellIds.Num(); ++I)
    {
        WriteHISMHighlightForCell(CaptureCellIds[I], bMarkLastRenderStateDirty && I == CaptureCellIds.Num() - 1);
    }
}

void UPlanetHISMInteractionComponent::UpdateHISMHoverCell(int32 NewCellId)
{
    if (APlanetTessellatedMesh* Host = GetHost())
    {
        Host->HISMTileRenderer.UpdateHoverCell(
            NewCellId,
            HISMHoverFadeDuration,
            BuildRuntimeHighlightConfig_(),
            Host->GetPlanetGameplayComponent() ? Host->GetPlanetGameplayComponent()->GetGameplayContainer() : nullptr);
    }
}

bool UPlanetHISMInteractionComponent::HandleHISMHoverHit(const FHitResult& Hit)
{
    APlanetTessellatedMesh* Host = GetHost();
    int32 CellId = INDEX_NONE;
    if (!Host || !TryResolveInteractionHitToCellId_(Hit, CellId))
    {
        return false;
    }

    if (Host->IsHISMSDFTerrainVisualActive())
    {
        Host->UpdateHISMHoverCell_(CellId);
    }
    else
    {
        UpdateHISMHoverCell(CellId);
    }

    if (GEngine && CellId != INDEX_NONE && CellId != Host->HISMTileRenderer.GetLastClickedCellId())
    {
        const FString Msg = FString::Printf(TEXT("HISM Hover Cell #%d  Instance=%d  Component=%s"),
            CellId, Hit.Item, *GetNameSafe(Hit.GetComponent()));
        GEngine->AddOnScreenDebugMessage(2, 0.25f, FColor::Yellow, Msg);
    }

    return true;
}

bool UPlanetHISMInteractionComponent::HandleHISMClickHit(const FHitResult& Hit)
{
    APlanetTessellatedMesh* Host = GetHost();
    int32 CellId = INDEX_NONE;
    if (!Host || !TryResolveInteractionHitToCellId_(Hit, CellId))
    {
        return false;
    }

    Host->HISMTileRenderer.SetLastClickedCellId(CellId);
    return Host->HandleGameplayCellClick_(
        CellId,
        Host->IsHISMSDFTerrainVisualActive() ? TEXT("HISM Spatial Projection") : TEXT("HISM Click"),
        Hit.Item,
        GetNameSafe(Hit.GetComponent()));
}

void UPlanetHISMInteractionComponent::ClearHISMHover()
{
    UpdateHISMHoverCell(INDEX_NONE);
}

void UPlanetHISMInteractionComponent::ClearAllHISMHighlights()
{
    if (APlanetTessellatedMesh* Host = GetHost())
    {
        Host->HISMTileRenderer.ClearAllHighlights(
            BuildRuntimeHighlightConfig_(),
            Host->GetPlanetGameplayComponent() ? Host->GetPlanetGameplayComponent()->GetGameplayContainer() : nullptr);
    }
}
