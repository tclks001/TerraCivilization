#include "Render/PlanetPiecePresentationComponent.h"

#include "Render/PlanetGameplayComponent.h"
#include "Render/PlanetTessellatedMesh.h"
#include "TerraGameplayContainer.h"
#include "TerraPiecePresentationManager.h"
#include "FSphereTopology.h"
#include "FCell.h"

#include "Animation/AnimationAsset.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

namespace
{
    FTerraPiecePaletteTile MakeP7Tile(int32 Row, int32 Col)
    {
        FTerraPiecePaletteTile Tile;
        Tile.Row = Row;
        Tile.Col = Col;
        return Tile;
    }

    FTerraPiecePaletteMask MakeP7Mask(ETerraGameplayPieceType PieceType, int32 PrimaryRow, int32 PrimaryCol, int32 SecondaryRow, int32 SecondaryCol)
    {
        FTerraPiecePaletteMask Mask;
        Mask.PieceType = PieceType;
        Mask.PrimaryTiles.Add(MakeP7Tile(PrimaryRow, PrimaryCol));
        Mask.SecondaryTiles.Add(MakeP7Tile(SecondaryRow, SecondaryCol));
        Mask.MinSaturationToReplace = 0.05f;
        return Mask;
    }

    FTerraPieceFactionPalette MakeP7Palette(const FLinearColor& Primary, const FLinearColor& Secondary)
    {
        FTerraPieceFactionPalette Palette;
        Palette.PrimaryColor = Primary;
        Palette.SecondaryColor = Secondary;
        Palette.ColorStrength = 1.0f;
        return Palette;
    }

    void BuildDefaultP7FactionPalettes(TArray<FTerraPieceFactionPalette>& OutPalettes)
    {
        OutPalettes.Reset();
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.85f, 0.08f, 0.06f, 1.0f), FLinearColor(1.00f, 0.72f, 0.18f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.08f, 0.30f, 0.90f, 1.0f), FLinearColor(0.92f, 0.95f, 1.00f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.08f, 0.58f, 0.20f, 1.0f), FLinearColor(0.68f, 0.50f, 0.32f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.48f, 0.18f, 0.78f, 1.0f), FLinearColor(0.72f, 0.72f, 0.78f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.95f, 0.38f, 0.08f, 1.0f), FLinearColor(0.06f, 0.06f, 0.06f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.05f, 0.78f, 0.92f, 1.0f), FLinearColor(0.02f, 0.10f, 0.32f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.95f, 0.82f, 0.10f, 1.0f), FLinearColor(0.38f, 0.22f, 0.10f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.92f, 0.12f, 0.58f, 1.0f), FLinearColor(0.18f, 0.18f, 0.20f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.02f, 0.55f, 0.50f, 1.0f), FLinearColor(0.74f, 0.42f, 0.20f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.88f, 0.88f, 0.82f, 1.0f), FLinearColor(0.72f, 0.04f, 0.04f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.03f, 0.03f, 0.035f, 1.0f), FLinearColor(0.02f, 0.62f, 0.34f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.55f, 0.85f, 0.08f, 1.0f), FLinearColor(0.46f, 0.16f, 0.72f, 1.0f)));
    }

    void BuildDefaultP7PaletteMasks(TArray<FTerraPiecePaletteMask>& OutMasks)
    {
        OutMasks.Reset();
        OutMasks.Add(MakeP7Mask(ETerraGameplayPieceType::Commander, 1, 2, 1, 1));
        OutMasks.Add(MakeP7Mask(ETerraGameplayPieceType::Infantry, 1, 0, 1, 1));
        OutMasks.Add(MakeP7Mask(ETerraGameplayPieceType::Cavalry, 1, 0, 1, 1));
        OutMasks.Add(MakeP7Mask(ETerraGameplayPieceType::Archer, 1, 0, 1, 6));
    }
}

UPlanetPiecePresentationComponent::UPlanetPiecePresentationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    PiecePresentationManager = CreateDefaultSubobject<UTerraPiecePresentationManager>(TEXT("PiecePresentationManager"));
}

APlanetTessellatedMesh* UPlanetPiecePresentationComponent::GetHost() const
{
    return Cast<APlanetTessellatedMesh>(GetOwner());
}

void UPlanetPiecePresentationComponent::SyncPresentation(
    const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents,
    const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents)
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return;
    }

    UWorld* World = Host->GetWorld();
    if (!World || !World->IsGameWorld() || !bEnableP1PiecePresentation)
    {
        ClearPresentation();
        return;
    }

    UPlanetGameplayComponent* GameplayComp = Host->GetPlanetGameplayComponent();
    const FTerraGameplayContainer* Gameplay = GameplayComp ? GameplayComp->GetGameplayContainer() : nullptr;
    if (!PiecePresentationManager || !Gameplay || !Gameplay->IsInitialized())
    {
        ClearPresentation();
        return;
    }

    const FTerraPieceVisualConfig VisualConfig = BuildVisualConfig();

    TArray<FTerraPiecePresentationSnapshot> Snapshots;
    const TArray<FTerraGameplayPieceState>& Pieces = Gameplay->GetPieces();
    Snapshots.Reserve(Pieces.Num());

    for (const FTerraGameplayPieceState& Piece : Pieces)
    {
        if (!Piece.bAlive)
        {
            continue;
        }

        FTransform PieceWorldTransform = FTransform::Identity;
        if (!BuildPieceWorldTransformForPiece(Piece, Pieces, PieceWorldTransform))
        {
            continue;
        }

        FTerraPiecePresentationSnapshot& Snapshot = Snapshots.AddDefaulted_GetRef();
        Snapshot.PieceId = Piece.PieceId;
        Snapshot.OwnerFactionId = Piece.OwnerFactionId;
        Snapshot.CellId = Piece.CellId;
        Snapshot.PieceType = Piece.PieceType;
        Snapshot.WorldTransform = PieceWorldTransform;
    }

    PiecePresentationManager->SyncPieces(Snapshots, VisualConfig, MoveEvents, CaptureEvents);
}

void UPlanetPiecePresentationComponent::ClearPresentation()
{
    if (PiecePresentationManager)
    {
        PiecePresentationManager->ClearPieces();
    }
}

bool UPlanetPiecePresentationComponent::BuildPieceWorldTransform(int32 CellId, FTransform& OutWorldTransform) const
{
    return BuildPieceWorldTransform(CellId, ETerraGameplayPieceType::Infantry, OutWorldTransform);
}

bool UPlanetPiecePresentationComponent::BuildPieceWorldTransform(int32 CellId, ETerraGameplayPieceType PieceType, FTransform& OutWorldTransform) const
{
    const APlanetTessellatedMesh* Host = GetHost();
    if (!Host || !Host->CellTopology.IsValid() || !Host->CellTopology->Cells.IsValidIndex(CellId))
    {
        return false;
    }

    const FTransform ActorTransform = Host->GetActorTransform();
    const FVector LocalUp = Host->CellTopology->Cells[CellId].UnitCenter.GetSafeNormal();
    if (LocalUp.IsNearlyZero())
    {
        return false;
    }

    FVector LocalForward = FVector::VectorPlaneProject(FVector::ForwardVector, LocalUp).GetSafeNormal();
    if (LocalForward.IsNearlyZero())
    {
        LocalForward = FVector::VectorPlaneProject(FVector::RightVector, LocalUp).GetSafeNormal();
    }
    if (LocalForward.IsNearlyZero())
    {
        return false;
    }

    const FVector WorldUp = ActorTransform.TransformVectorNoScale(LocalUp).GetSafeNormal();
    const FVector WorldForward = ActorTransform.TransformVectorNoScale(LocalForward).GetSafeNormal();
    if (WorldUp.IsNearlyZero() || WorldForward.IsNearlyZero())
    {
        return false;
    }

    FVector TraceWorldUp = WorldUp;
    float TraceAngularOffsetDeg = 0.0f;
    if (PieceType == ETerraGameplayPieceType::Cavalry)
    {
        const float RequestedOffsetDeg = FMath::Clamp(P2_6CavalryHeightTraceAngularOffsetDeg, 0.0f, 15.0f);
        if (RequestedOffsetDeg > KINDA_SMALL_NUMBER)
        {
            const FVector RotationAxis = FVector::CrossProduct(WorldUp, WorldForward).GetSafeNormal();
            if (!RotationAxis.IsNearlyZero())
            {
                TraceWorldUp = WorldUp.RotateAngleAxis(RequestedOffsetDeg, RotationAxis).GetSafeNormal();
                TraceAngularOffsetDeg = RequestedOffsetDeg;
            }
        }
    }

    FVector WorldPosition = FVector::ZeroVector;
    if (!TryResolvePieceHeightFromHISM(CellId, TraceWorldUp, WorldUp, TraceAngularOffsetDeg, WorldPosition))
    {
        const FVector LocalPosition = LocalUp * (Host->GlobeRadiusCM + FMath::Max(0.0f, P1PieceRadiusOffsetCM));
        WorldPosition = ActorTransform.TransformPosition(LocalPosition);
    }

    OutWorldTransform = FTransform(FRotationMatrix::MakeFromXZ(WorldForward, WorldUp).ToQuat(), WorldPosition, FVector::OneVector);
    return true;
}

bool UPlanetPiecePresentationComponent::TryResolvePieceHeightFromHISM(
    int32 CellId,
    const FVector& TraceWorldUp,
    const FVector& PlacementWorldUp,
    float TraceAngularOffsetDeg,
    FVector& OutWorldPosition) const
{
    const APlanetTessellatedMesh* Host = GetHost();
    UWorld* World = Host ? Host->GetWorld() : nullptr;
    if (!Host || !World || !bEnableP2_5HISMPieceHeightTrace || !Host->bEnableHISMTileRendering || !Host->bEnableHISMTileCollision)
    {
        return false;
    }
    if (!Host->CellTopology.IsValid() || !Host->CellTopology->Cells.IsValidIndex(CellId))
    {
        return false;
    }
    if (TraceWorldUp.IsNearlyZero() || PlacementWorldUp.IsNearlyZero())
    {
        return false;
    }

    const FVector PlanetCenterWorld = Host->GetPlanetCenterWorld_();
    const FVector TraceDirection = TraceWorldUp.GetSafeNormal();
    const FVector PlacementDirection = PlacementWorldUp.GetSafeNormal();
    const FVector TraceStart = PlanetCenterWorld + TraceDirection * (Host->GlobeRadiusCM + FMath::Max(P2_5PieceHeightTraceStartOffsetCM, 0.0f));
    const FVector TraceEnd = PlanetCenterWorld - TraceDirection * FMath::Max(P2_5PieceHeightTracePastCenterOffsetCM, 0.0f);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TerraPieceHeightTrace), false);
    TArray<FHitResult> Hits;
    if (!World->LineTraceMultiByChannel(Hits, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
    {
        return false;
    }

    const FHitResult* FirstHISMHit = nullptr;
    const FHitResult* CurrentCellHit = nullptr;
    for (const FHitResult& Hit : Hits)
    {
        UPrimitiveComponent* HitComp = Hit.GetComponent();
        if (HitComp != Host->PlainTileHISMComp && HitComp != Host->ForestTileHISMComp && HitComp != Host->MountainTileHISMComp)
        {
            continue;
        }

        if (!FirstHISMHit)
        {
            FirstHISMHit = &Hit;
        }

        int32 HitCellId = INDEX_NONE;
        if (Host->TryResolveHISMHitToCellId(Hit, HitCellId) && HitCellId == CellId)
        {
            CurrentCellHit = &Hit;
            break;
        }
    }

    const FHitResult* SelectedHit = CurrentCellHit ? CurrentCellHit : FirstHISMHit;
    if (!SelectedHit)
    {
        return false;
    }

    const float ImpactRadius = FVector::Distance(SelectedHit->ImpactPoint, PlanetCenterWorld);
    const float FinalRadius = ImpactRadius + FMath::Max(P1PieceRadiusOffsetCM, 0.0f);
    OutWorldPosition = PlanetCenterWorld + PlacementDirection * FinalRadius;
    return true;
}

bool UPlanetPiecePresentationComponent::BuildPieceWorldTransformForPiece(
    const FTerraGameplayPieceState& Piece,
    const TArray<FTerraGameplayPieceState>& Pieces,
    FTransform& OutWorldTransform) const
{
    const APlanetTessellatedMesh* Host = GetHost();
    if (!Host || !BuildPieceWorldTransform(Piece.CellId, Piece.PieceType, OutWorldTransform))
    {
        return false;
    }

    if (Piece.PieceType == ETerraGameplayPieceType::Commander
        || Piece.OwnerFactionId == INDEX_NONE
        || Piece.CellId == INDEX_NONE
        || !Host->CellTopology.IsValid()
        || !Host->CellTopology->Cells.IsValidIndex(Piece.CellId))
    {
        return true;
    }

    const FTerraGameplayPieceState* CommanderPiece = nullptr;
    for (const FTerraGameplayPieceState& CandidatePiece : Pieces)
    {
        if (CandidatePiece.bAlive
            && CandidatePiece.OwnerFactionId == Piece.OwnerFactionId
            && CandidatePiece.PieceType == ETerraGameplayPieceType::Commander)
        {
            CommanderPiece = &CandidatePiece;
            break;
        }
    }

    if (!CommanderPiece
        || CommanderPiece->CellId == INDEX_NONE
        || CommanderPiece->CellId == Piece.CellId
        || !Host->CellTopology->Cells.IsValidIndex(CommanderPiece->CellId))
    {
        return true;
    }

    const FTransform ActorTransform = Host->GetActorTransform();
    const FVector LocalUp = Host->CellTopology->Cells[Piece.CellId].UnitCenter.GetSafeNormal();
    const FVector CommanderLocalDir = Host->CellTopology->Cells[CommanderPiece->CellId].UnitCenter.GetSafeNormal();
    const FVector PieceLocalDir = Host->CellTopology->Cells[Piece.CellId].UnitCenter.GetSafeNormal();
    const FVector LocalForward = FVector::VectorPlaneProject(PieceLocalDir - CommanderLocalDir, LocalUp).GetSafeNormal();
    if (LocalForward.IsNearlyZero())
    {
        return true;
    }

    const FVector WorldUp = ActorTransform.TransformVectorNoScale(LocalUp).GetSafeNormal();
    const FVector WorldForward = ActorTransform.TransformVectorNoScale(LocalForward).GetSafeNormal();
    if (WorldUp.IsNearlyZero() || WorldForward.IsNearlyZero())
    {
        return true;
    }

    OutWorldTransform.SetRotation(FRotationMatrix::MakeFromXZ(WorldForward, WorldUp).ToQuat());
    return true;
}

void UPlanetPiecePresentationComponent::BuildCaptureEventsFromPendingEntries(
    const TArray<FTerraGameplayCaptureEntry>& CaptureEntries,
    const TArray<FTerraGameplayPieceState>& PiecesBeforeResolution,
    TArray<FTerraPiecePresentationCaptureEvent>& OutCaptureEvents) const
{
    OutCaptureEvents.Reset();
    OutCaptureEvents.Reserve(CaptureEntries.Num());

    auto BuildParticipant = [this, &PiecesBeforeResolution](int32 PieceId, FTerraPiecePresentationCaptureParticipant& OutParticipant) -> bool
    {
        if (!PiecesBeforeResolution.IsValidIndex(PieceId))
        {
            return false;
        }

        const FTerraGameplayPieceState& Piece = PiecesBeforeResolution[PieceId];
        if (Piece.PieceId == INDEX_NONE || Piece.CellId == INDEX_NONE)
        {
            return false;
        }

        FTransform WorldTransform = FTransform::Identity;
        if (!BuildPieceWorldTransformForPiece(Piece, PiecesBeforeResolution, WorldTransform))
        {
            return false;
        }

        OutParticipant.PieceId = Piece.PieceId;
        OutParticipant.CellId = Piece.CellId;
        OutParticipant.PieceType = Piece.PieceType;
        OutParticipant.WorldTransform = WorldTransform;
        return true;
    };

    for (const FTerraGameplayCaptureEntry& CaptureEntry : CaptureEntries)
    {
        FTerraPiecePresentationCaptureEvent CaptureEvent;
        const bool bHasCaptured = BuildParticipant(CaptureEntry.CapturedPieceId, CaptureEvent.Captured);
        const bool bHasAttacker = BuildParticipant(CaptureEntry.AttackerPieceId, CaptureEvent.Attacker);
        const bool bHasVanguard = BuildParticipant(CaptureEntry.VanguardPieceId, CaptureEvent.Vanguard);
        if (bHasCaptured && (bHasAttacker || bHasVanguard))
        {
            OutCaptureEvents.Add(CaptureEvent);
        }
    }
}

FTerraPieceVisualConfig UPlanetPiecePresentationComponent::BuildVisualConfig() const
{
    FTerraPieceVisualConfig VisualConfig;
    VisualConfig.CommanderMesh = P1CommanderMesh.Get() ? P1CommanderMesh.Get() : LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Characters/Mage1.Mage1"));
    VisualConfig.InfantryMesh = P1InfantryMesh.Get() ? P1InfantryMesh.Get() : LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Characters/Knight1.Knight1"));
    VisualConfig.CavalryMesh = P1CavalryMesh.Get() ? P1CavalryMesh.Get() : LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Characters/Knight1.Knight1"));
    VisualConfig.ArcherMesh = P1ArcherMesh.Get() ? P1ArcherMesh.Get() : LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Characters/Ranger1.Ranger1"));
    VisualConfig.UniformScale = FMath::Max(P1PieceUniformScale, 0.001f);
    VisualConfig.MeshRelativeLocation = P1MeshRelativeLocation;
    VisualConfig.MeshRelativeRotation = P1MeshRelativeRotation;
    VisualConfig.HorseMesh = P4HorseMesh.Get() ? P4HorseMesh.Get() : LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Animations/Horse/Horse.Horse"));
    VisualConfig.HorseIdleAnimation = P4HorseIdleAnimation.Get() ? P4HorseIdleAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Horse/HorseIdle.HorseIdle"));
    VisualConfig.HorseMoveAnimation = P4HorseMoveAnimation.Get() ? P4HorseMoveAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Horse/HorseWalk.HorseWalk"));
    VisualConfig.HorseJumpAnimation = P4HorseJumpAnimation.Get() ? P4HorseJumpAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Horse/HorseGallop_Jump.HorseGallop_Jump"));
    VisualConfig.HorseJumpPlayRateScale = FMath::Max(P4HorseJumpPlayRateScale, 0.001f);
    VisualConfig.HorseDeathAnimation = P4HorseDeathAnimation.Get() ? P4HorseDeathAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Horse/HorseDeath.HorseDeath"));
    VisualConfig.RiderSittingAnimation = P4RiderSittingAnimation.Get() ? P4RiderSittingAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralSitting.Rig_Medium_GeneralSitting"));
    VisualConfig.RiderAnimInstanceClass = P4RiderAnimInstanceClass;
    VisualConfig.RiderUpperBodyAttackMontage = P4RiderUpperBodyAttackMontage;
    VisualConfig.RiderUpperBodyHitMontage = P4RiderUpperBodyHitMontage;
    VisualConfig.RiderSaddleAttachName = P4SaddleAttachName.IsNone() ? FName(TEXT("Torso")) : P4SaddleAttachName;
    VisualConfig.HorseRelativeLocation = P4HorseRelativeLocation;
    VisualConfig.HorseRelativeRotation = P4HorseRelativeRotation;
    VisualConfig.HorseUniformScale = FMath::Max(P4HorseUniformScale, 0.001f);
    VisualConfig.RiderRelativeLocation = P4RiderRelativeLocation;
    VisualConfig.RiderRelativeRotation = P4RiderRelativeRotation;
    VisualConfig.RiderUniformScale = FMath::Max(P4RiderUniformScale, 0.001f);
    VisualConfig.RiderDeathRelativeLocation = P4RiderDeathRelativeLocation;
    VisualConfig.RiderDeathRelativeRotation = P4RiderDeathRelativeRotation;
    VisualConfig.ArcherBowMesh = P5ArcherBowMesh.Get() ? P5ArcherBowMesh.Get() : LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/bow.bow"));
    VisualConfig.InfantrySwordMesh = P5InfantrySwordMesh.Get() ? P5InfantrySwordMesh.Get() : LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/sword_1handed.sword_1handed"));
    VisualConfig.InfantryShieldMesh = P5InfantryShieldMesh.Get() ? P5InfantryShieldMesh.Get() : LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/shield_round.shield_round"));
    VisualConfig.CavalryAxeMesh = P5CavalryAxeMesh.Get() ? P5CavalryAxeMesh.Get() : LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/axe_1handed.axe_1handed"));
    VisualConfig.ArcherBowAttachName = P5ArcherBowAttachName;
    VisualConfig.InfantrySwordAttachName = P5InfantrySwordAttachName;
    VisualConfig.InfantryShieldAttachName = P5InfantryShieldAttachName;
    VisualConfig.CavalryAxeAttachName = P5CavalryAxeAttachName;
    VisualConfig.ArcherBowRelativeLocation = P5ArcherBowRelativeLocation;
    VisualConfig.ArcherBowRelativeRotation = P5ArcherBowRelativeRotation;
    VisualConfig.ArcherBowUniformScale = FMath::Max(P5ArcherBowUniformScale, 0.001f);
    VisualConfig.InfantrySwordRelativeLocation = P5InfantrySwordRelativeLocation;
    VisualConfig.InfantrySwordRelativeRotation = P5InfantrySwordRelativeRotation;
    VisualConfig.InfantrySwordUniformScale = FMath::Max(P5InfantrySwordUniformScale, 0.001f);
    VisualConfig.InfantryShieldRelativeLocation = P5InfantryShieldRelativeLocation;
    VisualConfig.InfantryShieldRelativeRotation = P5InfantryShieldRelativeRotation;
    VisualConfig.InfantryShieldUniformScale = FMath::Max(P5InfantryShieldUniformScale, 0.001f);
    VisualConfig.CavalryAxeRelativeLocation = P5CavalryAxeRelativeLocation;
    VisualConfig.CavalryAxeRelativeRotation = P5CavalryAxeRelativeRotation;
    VisualConfig.CavalryAxeUniformScale = FMath::Max(P5CavalryAxeUniformScale, 0.001f);
    VisualConfig.P6ArrowProjectileMesh = P6ArrowProjectileMesh.Get() ? P6ArrowProjectileMesh.Get() : LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/arrow_bow.arrow_bow"));
    UClass* DefaultP6SpellProjectileClass = P6SpellProjectileActorClass.Get();
    if (!DefaultP6SpellProjectileClass)
    {
        DefaultP6SpellProjectileClass = LoadClass<AActor>(nullptr, TEXT("/Game/FXVarietyPack/Blueprints/BP_ky_fireBall.BP_ky_fireBall_C"));
    }
    VisualConfig.P6SpellProjectileActorClass = DefaultP6SpellProjectileClass;
    VisualConfig.P6ArrowAttachName = P6ArrowAttachName;
    VisualConfig.P6SpellAttachName = P6SpellAttachName;
    VisualConfig.P6ArrowRelativeLocation = P6ArrowRelativeLocation;
    VisualConfig.P6ArrowRelativeRotation = P6ArrowRelativeRotation;
    VisualConfig.P6ArrowUniformScale = FMath::Max(P6ArrowUniformScale, 0.001f);
    VisualConfig.P6ArrowTargetRelativeLocation = P6ArrowTargetRelativeLocation;
    VisualConfig.P6SpellRelativeLocation = P6SpellRelativeLocation;
    VisualConfig.P6SpellRelativeRotation = P6SpellRelativeRotation;
    VisualConfig.P6SpellUniformScale = FMath::Max(P6SpellUniformScale, 0.001f);
    VisualConfig.P6ArrowReleaseDelaySeconds = FMath::Max(P6ArrowReleaseDelaySeconds, 0.0f);
    VisualConfig.P6SpellReleaseDelaySeconds = FMath::Max(P6SpellReleaseDelaySeconds, 0.0f);
    VisualConfig.P6ArrowFlightSeconds = FMath::Max(P6ArrowFlightSeconds, 0.001f);
    VisualConfig.P6SpellFlightSeconds = FMath::Max(P6SpellFlightSeconds, 0.001f);
    VisualConfig.P6ArrowArcHeightCM = FMath::Max(P6ArrowArcHeightCM, 0.0f);
    VisualConfig.P7PaletteReplaceMaterial = P7PaletteReplaceMaterial.Get()
        ? P7PaletteReplaceMaterial.Get()
        : LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/PiecePresentation/Materials/M_TerraPiece_PaletteReplace.M_TerraPiece_PaletteReplace"));
    VisualConfig.P7CommanderBaseTexture = P7CommanderBaseTexture.Get()
        ? P7CommanderBaseTexture.Get()
        : LoadObject<UTexture2D>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/mage_texture.mage_texture"));
    VisualConfig.P7InfantryBaseTexture = P7InfantryBaseTexture.Get()
        ? P7InfantryBaseTexture.Get()
        : LoadObject<UTexture2D>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/knight_texture.knight_texture"));
    VisualConfig.P7CavalryRiderBaseTexture = P7CavalryRiderBaseTexture.Get()
        ? P7CavalryRiderBaseTexture.Get()
        : LoadObject<UTexture2D>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/knight_texture.knight_texture"));
    VisualConfig.P7ArcherBaseTexture = P7ArcherBaseTexture.Get()
        ? P7ArcherBaseTexture.Get()
        : LoadObject<UTexture2D>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/ranger_texture.ranger_texture"));
    VisualConfig.P7FactionPalettes = P7FactionPalettes;
    if (VisualConfig.P7FactionPalettes.Num() == 0)
    {
        BuildDefaultP7FactionPalettes(VisualConfig.P7FactionPalettes);
    }
    VisualConfig.P7PaletteMasksByPieceType = P7PaletteMasksByPieceType;
    if (VisualConfig.P7PaletteMasksByPieceType.Num() == 0)
    {
        BuildDefaultP7PaletteMasks(VisualConfig.P7PaletteMasksByPieceType);
    }
    VisualConfig.P35CommanderAttackDurationSeconds = FMath::Max(P35CommanderAttackDurationSeconds, 0.001f);
    VisualConfig.P35CommanderAttackPlayRateScale = FMath::Max(P35CommanderAttackPlayRateScale, 0.001f);
    VisualConfig.P35ArcherAttackDurationSeconds = FMath::Max(P35ArcherAttackDurationSeconds, 0.001f);
    VisualConfig.P35ArcherAttackPlayRateScale = FMath::Max(P35ArcherAttackPlayRateScale, 0.001f);
    VisualConfig.IdleAnimation = P2IdleAnimation.Get() ? P2IdleAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralIdle_A.Rig_Medium_GeneralIdle_A"));
    VisualConfig.MoveAnimation = P2MoveAnimation.Get() ? P2MoveAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_MovementBasicWalking_A.Rig_Medium_MovementBasicWalking_A"));
    VisualConfig.JumpAnimation = P2JumpAnimation.Get() ? P2JumpAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_MovementBasicJump_Full_Short.Rig_Medium_MovementBasicJump_Full_Short"));
    VisualConfig.CommanderMagicAttackAnimation = P3CommanderMagicAttackAnimation.Get() ? P3CommanderMagicAttackAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralMagic_Spell_Casting.Rig_Medium_GeneralMagic_Spell_Casting"));
    VisualConfig.ArcherRangedAttackAnimation = P3ArcherRangedAttackAnimation.Get() ? P3ArcherRangedAttackAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralShooting_Arrow.Rig_Medium_GeneralShooting_Arrow"));
    VisualConfig.InfantryMeleeAttackAnimation = P3InfantryMeleeAttackAnimation.Get() ? P3InfantryMeleeAttackAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralSword_And_Shield_Slash.Rig_Medium_GeneralSword_And_Shield_Slash"));
    VisualConfig.CavalryMeleeAttackAnimation = P3CavalryMeleeAttackAnimation.Get() ? P3CavalryMeleeAttackAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralUpward_Thrust.Rig_Medium_GeneralUpward_Thrust"));
    VisualConfig.HitAnimation = P3HitAnimation.Get() ? P3HitAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralHit_A.Rig_Medium_GeneralHit_A"));
    VisualConfig.DeathAnimation = P3DeathAnimation.Get() ? P3DeathAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralDeath_A.Rig_Medium_GeneralDeath_A"));
    VisualConfig.MoveDurationSeconds = FMath::Max(P2MoveDurationSeconds, 0.001f);
    VisualConfig.JumpDurationSeconds = FMath::Max(P2JumpDurationSeconds, 0.001f);
    VisualConfig.JumpHeightCM = FMath::Max(P2JumpHeightCM, 0.0f);
    VisualConfig.P3FacingBlendSeconds = FMath::Max(P3FacingBlendSeconds, 0.0f);
    VisualConfig.P3MeleeRunInSeconds = FMath::Max(P3MeleeRunInSeconds, 0.001f);
    VisualConfig.P3MeleeReturnSeconds = FMath::Max(P3MeleeReturnSeconds, 0.001f);
    VisualConfig.P3AttackAnimationStartOffsetSeconds = FMath::Max(P3AttackAnimationStartOffsetSeconds, 0.0f);
    VisualConfig.P3CommanderAttackToHitSeconds = FMath::Max(P3CommanderAttackToHitSeconds, 0.0f);
    VisualConfig.P3ArcherAttackToHitSeconds = FMath::Max(P3ArcherAttackToHitSeconds, 0.0f);
    VisualConfig.P3InfantryAttackToHitSeconds = FMath::Max(P3InfantryAttackToHitSeconds, 0.0f);
    VisualConfig.P3CavalryAttackToHitSeconds = FMath::Max(P3CavalryAttackToHitSeconds, 0.0f);
    VisualConfig.P3HitReactDelaySeconds = FMath::Max(P3HitReactDelaySeconds, 0.0f);
    VisualConfig.P3HitAnimationStartOffsetSeconds = FMath::Max(P3HitAnimationStartOffsetSeconds, 0.0f);
    VisualConfig.P3DeathAfterHitDelaySeconds = FMath::Max(P3DeathAfterHitDelaySeconds, 0.0f);
    VisualConfig.P3DeathAnimationStartOffsetSeconds = FMath::Max(P3DeathAnimationStartOffsetSeconds, 0.0f);
    VisualConfig.P3CapturedFadeSeconds = FMath::Max(P3CapturedFadeSeconds, 0.0f);
    return VisualConfig;
}
