#include "Render/PlanetCameraComponent.h"

#include "Render/PlanetTessellatedMesh.h"
#include "TerraGameplayContainer.h"
#include "TerraPiecePresentationManager.h"
#include "FSphereTopology.h"
#include "FCell.h"

#include "Animation/AnimationAsset.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/PlanetInteractionController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlanetCamera, Log, All);

UPlanetCameraComponent::UPlanetCameraComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UPlanetCameraComponent::BeginPlay()
{
    Super::BeginPlay();
}

APlanetTessellatedMesh* UPlanetCameraComponent::GetHost() const
{
    return Cast<APlanetTessellatedMesh>(GetOwner());
}

void UPlanetCameraComponent::TickCamera(float DeltaSeconds)
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return;
    }

    (void)DeltaSeconds;
    if (bEnableG2_5CameraAssist
        && Host->GetWorld()
        && Host->GetWorld()->IsGameWorld()
        && Host->GameplayContainer.IsValid()
        && Host->GameplayContainer->IsInitialized()
        && LastFocusedTurnIndex != Host->GameplayContainer->GetTurnIndex())
    {
        if (!IsDelayedTurnStartFocusTimerActive())
        {
            UE_LOG(LogPlanetCamera, Log,
                TEXT("[Tess][C6.5] Tick fallback turn focus is not delayed. LastFocusedTurn=%d CurrentTurn=%d CurrentFaction=%d"),
                LastFocusedTurnIndex,
                Host->GameplayContainer->GetTurnIndex(),
                Host->GameplayContainer->GetCurrentFactionId());
            FocusCameraOnCurrentFactionBase();
            LastFocusedTurnIndex = Host->GameplayContainer->GetTurnIndex();
        }
        else
        {
            UE_LOG(LogPlanetCamera, Verbose,
                TEXT("[Tess][C6.5] Tick fallback turn focus suppressed by active delay timer. LastFocusedTurn=%d CurrentTurn=%d CurrentFaction=%d"),
                LastFocusedTurnIndex,
                Host->GameplayContainer->GetTurnIndex(),
                Host->GameplayContainer->GetCurrentFactionId());
        }
    }
}

void UPlanetCameraComponent::ResetCameraState()
{
    ClearDelayedTurnStartFocusTimer();
    LastFocusedTurnIndex = INDEX_NONE;
    bGameStartCameraApplied = false;
}

void UPlanetCameraComponent::ClearDelayedTurnStartFocusTimer()
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return;
    }

    if (UWorld* World = Host->GetWorld())
    {
        World->GetTimerManager().ClearTimer(DelayedTurnStartFocusTimerHandle);
    }
}

bool UPlanetCameraComponent::IsDelayedTurnStartFocusTimerActive() const
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return false;
    }

    if (UWorld* World = Host->GetWorld())
    {
        return World->GetTimerManager().IsTimerActive(DelayedTurnStartFocusTimerHandle);
    }
    return false;
}

void UPlanetCameraComponent::FocusCameraOnCell(int32 CellId, bool bMoveCamera)
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return;
    }

    if (!bEnableG2_5CameraAssist)
    {
        return;
    }

    UWorld* World = Host->GetWorld();
    if (!World || !World->IsGameWorld())
    {
        return;
    }

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
    if (!PlayerController)
    {
        return;
    }

    FVector TargetWorldPosition;
    if (!Host->GetCellSurfaceWorldPosition_(CellId, 0.0f, TargetWorldPosition))
    {
        return;
    }

    AActor* ViewTarget = PlayerController->GetViewTarget();
    FVector CameraWorldPosition = FVector::ZeroVector;
    if (bMoveCamera)
    {
        if (!Host->GetCellSurfaceWorldPosition_(CellId, FMath::Max(0.0f, G2_5TurnStartCameraHeightCM), CameraWorldPosition))
        {
            return;
        }
    }
    else if (ViewTarget)
    {
        CameraWorldPosition = ViewTarget->GetActorLocation();
    }
    else if (PlayerController->PlayerCameraManager)
    {
        CameraWorldPosition = PlayerController->PlayerCameraManager->GetCameraLocation();
    }
    else
    {
        return;
    }

    const FVector LookDirection = TargetWorldPosition - CameraWorldPosition;
    if (LookDirection.IsNearlyZero())
    {
        return;
    }

    const FRotator LookRotation = LookDirection.Rotation();
    if (ViewTarget)
    {
        if (bMoveCamera)
        {
            ViewTarget->SetActorLocation(CameraWorldPosition);
        }
        ViewTarget->SetActorRotation(LookRotation);
    }
    PlayerController->SetControlRotation(LookRotation);

    UE_LOG(LogPlanetCamera, Log,
        TEXT("[Tess][G2.5] Focus camera on Cell=%d Move=%d Camera=(%.1f, %.1f, %.1f) Target=(%.1f, %.1f, %.1f)"),
        CellId,
        bMoveCamera ? 1 : 0,
        CameraWorldPosition.X,
        CameraWorldPosition.Y,
        CameraWorldPosition.Z,
        TargetWorldPosition.X,
        TargetWorldPosition.Y,
        TargetWorldPosition.Z);
}

bool UPlanetCameraComponent::IsCellInC4ComfortView(int32 CellId) const
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return false;
    }

    if (!Host->CellTopology.IsValid() || !Host->CellTopology->Cells.IsValidIndex(CellId))
    {
        return false;
    }

    UWorld* World = Host->GetWorld();
    if (!World || !World->IsGameWorld())
    {
        return false;
    }
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
    if (!PlayerController)
    {
        return false;
    }

    FVector CameraWorldPosition = FVector::ZeroVector;
    FRotator CameraWorldRotation = FRotator::ZeroRotator;
    if (PlayerController->PlayerCameraManager)
    {
        CameraWorldPosition = PlayerController->PlayerCameraManager->GetCameraLocation();
        CameraWorldRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
    }
    else if (AActor* ViewTarget = PlayerController->GetViewTarget())
    {
        CameraWorldPosition = ViewTarget->GetActorLocation();
        CameraWorldRotation = ViewTarget->GetActorRotation();
    }
    else
    {
        return false;
    }

    FVector CurrentFocusUnitDir = FVector::ZeroVector;
    float CurrentDistanceToFocusCM = 0.0f;
    float CurrentTiltDeg = 0.0f;
    float CurrentYawAroundFocusDeg = 0.0f;
    if (!SyncFocusCameraStateFromView(
        CameraWorldPosition,
        CameraWorldRotation,
        CurrentFocusUnitDir,
        CurrentDistanceToFocusCM,
        CurrentTiltDeg,
        CurrentYawAroundFocusDeg))
    {
        return false;
    }

    const FVector TargetFocusUnitDir = Host->CellTopology->Cells[CellId].UnitCenter.GetSafeNormal();
    CurrentFocusUnitDir = CurrentFocusUnitDir.GetSafeNormal();
    if (TargetFocusUnitDir.IsNearlyZero() || CurrentFocusUnitDir.IsNearlyZero())
    {
        return false;
    }

    const float Dot = FMath::Clamp(static_cast<float>(FVector::DotProduct(CurrentFocusUnitDir, TargetFocusUnitDir)), -1.0f, 1.0f);
    const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(Dot));
    const float ComfortAngleDeg = FMath::Clamp(C4ComfortFocusAngleDeg, 0.0f, 180.0f);
    return AngleDeg <= ComfortAngleDeg;
}

bool UPlanetCameraComponent::RequestC4SelectionFocus(int32 CellId) const
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return false;
    }

    if (!Host->CellTopology.IsValid() || !Host->CellTopology->Cells.IsValidIndex(CellId))
    {
        return false;
    }

    UWorld* World = Host->GetWorld();
    if (!World || !World->IsGameWorld())
    {
        return false;
    }

    APlanetInteractionController* InteractionController = Cast<APlanetInteractionController>(
        UGameplayStatics::GetPlayerController(World, 0));
    if (!InteractionController)
    {
        return false;
    }

    const FVector TargetFocusUnitDir = Host->CellTopology->Cells[CellId].UnitCenter.GetSafeNormal();
    return InteractionController->RequestC4FocusOnUnitDir(
        TargetFocusUnitDir,
        FMath::Max(0.0f, C4SelectedPieceFocusBlendSeconds));
}

float UPlanetCameraComponent::GetC6ActionCameraBlendSeconds(ETerraPiecePresentationMoveType MoveType) const
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return 0.001f;
    }

    return MoveType == ETerraPiecePresentationMoveType::Jump
        ? FMath::Max(Host->P2JumpDurationSeconds, 0.001f)
        : FMath::Max(Host->P2MoveDurationSeconds, 0.001f);
}

void UPlanetCameraComponent::RequestC6ActionCameraTrackingForMoveEvents(const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents) const
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return;
    }

    if (!bEnableC6ActionCameraTracking || MoveEvents.Num() <= 0)
    {
        return;
    }

    APlanetInteractionController* InteractionController = Cast<APlanetInteractionController>(UGameplayStatics::GetPlayerController(Host, 0));
    if (!InteractionController || !Host->CellTopology.IsValid())
    {
        return;
    }

    for (const FTerraPiecePresentationMoveEvent& MoveEvent : MoveEvents)
    {
        if (!MoveEvent.IsValidMove() || !Host->CellTopology->Cells.IsValidIndex(MoveEvent.ToCellId))
        {
            continue;
        }

        if (IsCellInC4ComfortView(MoveEvent.ToCellId))
        {
            UE_LOG(LogPlanetCamera, Verbose,
                TEXT("[Tess][C6] Move target in comfort view. Piece=%d ToCell=%d"),
                MoveEvent.PieceId,
                MoveEvent.ToCellId);
            continue;
        }

        const FVector TargetFocusUnitDir = Host->CellTopology->Cells[MoveEvent.ToCellId].UnitCenter.GetSafeNormal();
        if (TargetFocusUnitDir.IsNearlyZero())
        {
            continue;
        }

        const float BlendSeconds = GetC6ActionCameraBlendSeconds(MoveEvent.MoveType);
        const bool bRequested = InteractionController->RequestC6FocusOnUnitDir(TargetFocusUnitDir, BlendSeconds);
        if (bRequested)
        {
            UE_LOG(LogPlanetCamera, Log,
                TEXT("[Tess][C6] Requested action camera tracking. Piece=%d FromCell=%d ToCell=%d MoveType=%d Blend=%.2f"),
                MoveEvent.PieceId,
                MoveEvent.FromCellId,
                MoveEvent.ToCellId,
                static_cast<int32>(MoveEvent.MoveType),
                BlendSeconds);
        }
    }
}

float UPlanetCameraComponent::GetC6_5AnimationLengthSeconds(UAnimationAsset* AnimationAsset, float FallbackSeconds) const
{
    return AnimationAsset
        ? FMath::Max(AnimationAsset->GetPlayLength(), 0.001f)
        : FMath::Max(FallbackSeconds, 0.001f);
}

float UPlanetCameraComponent::GetC6_5AttackAnimationDurationSeconds(
    ETerraGameplayPieceType PieceType,
    UAnimationAsset* AttackAnimation,
    float FallbackSeconds) const
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return 0.0f;
    }

    if (PieceType == ETerraGameplayPieceType::Commander)
    {
        return FMath::Max(Host->P35CommanderAttackDurationSeconds, 0.001f)
            / FMath::Max(Host->P35CommanderAttackPlayRateScale, 0.001f);
    }

    if (PieceType == ETerraGameplayPieceType::Archer)
    {
        return FMath::Max(Host->P35ArcherAttackDurationSeconds, 0.001f)
            / FMath::Max(Host->P35ArcherAttackPlayRateScale, 0.001f);
    }

    return Host->P3AttackAnimationStartOffsetSeconds
        + GetC6_5AnimationLengthSeconds(AttackAnimation, FallbackSeconds);
}

float UPlanetCameraComponent::GetC6_5ActionPresentationDelaySeconds(
    const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents,
    const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents) const
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return 0.0f;
    }

    float MoveSeconds = 0.0f;
    int32 ValidMoveEventCount = 0;
    for (const FTerraPiecePresentationMoveEvent& MoveEvent : MoveEvents)
    {
        if (MoveEvent.IsValidMove())
        {
            ++ValidMoveEventCount;
            MoveSeconds = FMath::Max(MoveSeconds, GetC6ActionCameraBlendSeconds(MoveEvent.MoveType));
            UE_LOG(LogPlanetCamera, Log,
                TEXT("[Tess][C6.5] Delay estimate includes move. Piece=%d FromCell=%d ToCell=%d MoveType=%d MoveSecondsNow=%.3f"),
                MoveEvent.PieceId,
                MoveEvent.FromCellId,
                MoveEvent.ToCellId,
                static_cast<int32>(MoveEvent.MoveType),
                MoveSeconds);
        }
        else
        {
            UE_LOG(LogPlanetCamera, Log,
                TEXT("[Tess][C6.5] Delay estimate ignores invalid move event. Piece=%d FromCell=%d ToCell=%d MoveType=%d"),
                MoveEvent.PieceId,
                MoveEvent.FromCellId,
                MoveEvent.ToCellId,
                static_cast<int32>(MoveEvent.MoveType));
        }
    }

    float CaptureSeconds = 0.0f;
    int32 ValidCaptureEventCount = 0;
    const FTerraPieceVisualConfig VisualConfig = Host->BuildP1PieceVisualConfig_();
    for (const FTerraPiecePresentationCaptureEvent& CaptureEvent : CaptureEvents)
    {
        if (!CaptureEvent.IsValidCapture())
        {
            UE_LOG(LogPlanetCamera, Log,
                TEXT("[Tess][C6.5] Delay estimate ignores invalid capture event. Captured=%d Attacker=%d Vanguard=%d"),
                CaptureEvent.Captured.PieceId,
                CaptureEvent.Attacker.PieceId,
                CaptureEvent.Vanguard.PieceId);
            continue;
        }
        ++ValidCaptureEventCount;

        float MaxAttackToHitSeconds = FMath::Max(Host->P3HitReactDelaySeconds, 0.0f);
        auto IncludeAttackToHit = [&VisualConfig, &MaxAttackToHitSeconds](const FTerraPiecePresentationCaptureParticipant& Participant)
        {
            if (Participant.IsValid())
            {
                MaxAttackToHitSeconds = FMath::Max(
                    MaxAttackToHitSeconds,
                    FMath::Max(VisualConfig.ResolveAttackToHitSeconds(Participant.PieceType), 0.0f));
            }
        };
        IncludeAttackToHit(CaptureEvent.Attacker);
        if (CaptureEvent.Vanguard.PieceId != CaptureEvent.Attacker.PieceId)
        {
            IncludeAttackToHit(CaptureEvent.Vanguard);
        }

        float AttackStepSeconds = 0.0f;
        auto IncludeAttackLength = [this, &VisualConfig, MaxAttackToHitSeconds, &AttackStepSeconds](const FTerraPiecePresentationCaptureParticipant& Participant)
        {
            if (!Participant.IsValid())
            {
                return;
            }

            const float AttackToHitSeconds = FMath::Max(VisualConfig.ResolveAttackToHitSeconds(Participant.PieceType), 0.0f);
            const float AttackStartDelaySeconds = FMath::Max(MaxAttackToHitSeconds - AttackToHitSeconds, 0.0f);
            UAnimationAsset* AttackAnimation = VisualConfig.ResolveAttackAnimation(Participant.PieceType);
            AttackStepSeconds = FMath::Max(
                AttackStepSeconds,
                AttackStartDelaySeconds
                    + GetC6_5AttackAnimationDurationSeconds(Participant.PieceType, AttackAnimation, 0.5f));
        };

        IncludeAttackLength(CaptureEvent.Attacker);
        if (CaptureEvent.Vanguard.PieceId != CaptureEvent.Attacker.PieceId)
        {
            IncludeAttackLength(CaptureEvent.Vanguard);
        }

        const float HitSeconds = MaxAttackToHitSeconds
            + Host->P3HitAnimationStartOffsetSeconds
            + GetC6_5AnimationLengthSeconds(Host->P3HitAnimation, 0.35f);
        const float DeathSeconds = MaxAttackToHitSeconds
            + Host->P3DeathAfterHitDelaySeconds
            + Host->P3DeathAnimationStartOffsetSeconds
            + GetC6_5AnimationLengthSeconds(Host->P3DeathAnimation, 0.75f);
        AttackStepSeconds = FMath::Max(AttackStepSeconds, HitSeconds);
        AttackStepSeconds = FMath::Max(AttackStepSeconds, DeathSeconds);

        const float FinishSeconds = AttackStepSeconds
            + FMath::Max(Host->P3MeleeReturnSeconds, Host->P3CapturedFadeSeconds);
        const float CaptureEventSeconds = Host->P3FacingBlendSeconds
            + FMath::Max(Host->P3MeleeRunInSeconds, 0.001f)
            + FinishSeconds;
        CaptureSeconds += CaptureEventSeconds;

        UE_LOG(LogPlanetCamera, Log,
            TEXT("[Tess][C6.5] Delay estimate includes capture. Captured=%d Attacker=%d Vanguard=%d MaxAttackToHit=%.3f AttackStep=%.3f Hit=%.3f Death=%.3f EventSeconds=%.3f CaptureSecondsTotal=%.3f"),
            CaptureEvent.Captured.PieceId,
            CaptureEvent.Attacker.PieceId,
            CaptureEvent.Vanguard.PieceId,
            MaxAttackToHitSeconds,
            AttackStepSeconds,
            HitSeconds,
            DeathSeconds,
            CaptureEventSeconds,
            CaptureSeconds);
    }

    UE_LOG(LogPlanetCamera, Log,
        TEXT("[Tess][C6.5] Delay estimate summary. MoveEvents=%d ValidMoveEvents=%d CaptureEvents=%d ValidCaptureEvents=%d MoveSeconds=%.3f CaptureSeconds=%.3f Total=%.3f"),
        MoveEvents.Num(),
        ValidMoveEventCount,
        CaptureEvents.Num(),
        ValidCaptureEventCount,
        MoveSeconds,
        CaptureSeconds,
        MoveSeconds + CaptureSeconds);

    return MoveSeconds + CaptureSeconds;
}

bool UPlanetCameraComponent::TryRequestC6_5DelayedTurnStartFocus(
    int32 ExpectedTurnIndex,
    int32 ExpectedFactionId,
    const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents,
    const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents)
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return false;
    }

    if (!bEnableC6_5DelayTurnStartFocusUntilActionPresentationEnds)
    {
        UE_LOG(LogPlanetCamera, Log,
            TEXT("[Tess][C6.5] Delay request rejected: disabled. Faction=%d Turn=%d MoveEvents=%d CaptureEvents=%d"),
            ExpectedFactionId,
            ExpectedTurnIndex,
            MoveEvents.Num(),
            CaptureEvents.Num());
        return false;
    }

    UWorld* World = Host->GetWorld();
    if (!World || !World->IsGameWorld())
    {
        UE_LOG(LogPlanetCamera, Log,
            TEXT("[Tess][C6.5] Delay request rejected: no game world. Faction=%d Turn=%d MoveEvents=%d CaptureEvents=%d"),
            ExpectedFactionId,
            ExpectedTurnIndex,
            MoveEvents.Num(),
            CaptureEvents.Num());
        return false;
    }

    const float DelaySeconds = GetC6_5ActionPresentationDelaySeconds(MoveEvents, CaptureEvents);
    if (DelaySeconds <= KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogPlanetCamera, Log,
            TEXT("[Tess][C6.5] Delay request rejected: estimated delay is zero. Faction=%d Turn=%d Delay=%.6f MoveEvents=%d CaptureEvents=%d"),
            ExpectedFactionId,
            ExpectedTurnIndex,
            DelaySeconds,
            MoveEvents.Num(),
            CaptureEvents.Num());
        return false;
    }

    const bool bHadExistingTimer = World->GetTimerManager().IsTimerActive(DelayedTurnStartFocusTimerHandle);
    World->GetTimerManager().ClearTimer(DelayedTurnStartFocusTimerHandle);
    const float TotalDelaySeconds = DelaySeconds + FMath::Max(C6_5TurnStartFocusDelayPaddingSeconds, 0.0f);
    const FTimerDelegate Delegate = FTimerDelegate::CreateUObject(
        Host,
        &APlanetTessellatedMesh::ExecuteC6_5DelayedTurnStartFocus_,
        ExpectedTurnIndex,
        ExpectedFactionId);
    World->GetTimerManager().SetTimer(
        DelayedTurnStartFocusTimerHandle,
        Delegate,
        FMath::Max(TotalDelaySeconds, 0.001f),
        false);

    UE_LOG(LogPlanetCamera, Log,
        TEXT("[Tess][C6.5] Delayed turn start camera focus. Faction=%d Turn=%d Delay=%.3f RawDelay=%.3f Padding=%.3f MoveEvents=%d CaptureEvents=%d ReplacedExistingTimer=%d"),
        ExpectedFactionId,
        ExpectedTurnIndex,
        TotalDelaySeconds,
        DelaySeconds,
        FMath::Max(C6_5TurnStartFocusDelayPaddingSeconds, 0.0f),
        MoveEvents.Num(),
        CaptureEvents.Num(),
        bHadExistingTimer ? 1 : 0);
    return true;
}

void UPlanetCameraComponent::ExecuteC6_5DelayedTurnStartFocus(int32 ExpectedTurnIndex, int32 ExpectedFactionId)
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return;
    }

    if (!Host->GameplayContainer.IsValid() || !Host->GameplayContainer->IsInitialized())
    {
        UE_LOG(LogPlanetCamera, Log,
            TEXT("[Tess][C6.5] Delayed turn start focus fired but gameplay is unavailable. ExpectedFaction=%d ExpectedTurn=%d"),
            ExpectedFactionId,
            ExpectedTurnIndex);
        return;
    }

    if (Host->GameplayContainer->GetTurnIndex() != ExpectedTurnIndex
        || Host->GameplayContainer->GetCurrentFactionId() != ExpectedFactionId)
    {
        UE_LOG(LogPlanetCamera, Verbose,
            TEXT("[Tess][C6.5] Skip stale delayed turn start focus. ExpectedFaction=%d ExpectedTurn=%d CurrentFaction=%d CurrentTurn=%d"),
            ExpectedFactionId,
            ExpectedTurnIndex,
            Host->GameplayContainer->GetCurrentFactionId(),
            Host->GameplayContainer->GetTurnIndex());
        return;
    }

    UE_LOG(LogPlanetCamera, Log,
        TEXT("[Tess][C6.5] Delayed turn start focus fired. Faction=%d Turn=%d"),
        ExpectedFactionId,
        ExpectedTurnIndex);
    FocusCameraOnCurrentFactionBase();
}

void UPlanetCameraComponent::FocusCameraOnSelectedCellSmart(int32 CellId)
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return;
    }

    if (!bEnableG2_5CameraAssist)
    {
        return;
    }

    if (!bEnableC4SmartSelectionFocus)
    {
        FocusCameraOnCell(CellId, false);
        return;
    }

    if (IsCellInC4ComfortView(CellId))
    {
        UE_LOG(LogPlanetCamera, Log, TEXT("[Tess][C4] Selected Cell=%d already in comfort view. Camera unchanged."), CellId);
        return;
    }

    if (!RequestC4SelectionFocus(CellId))
    {
        FocusCameraOnCell(CellId, false);
        return;
    }

    UE_LOG(LogPlanetCamera, Log, TEXT("[Tess][C4] Requested smart selection focus. Cell=%d Blend=%.2f"),
        CellId,
        C4SelectedPieceFocusBlendSeconds);
}

void UPlanetCameraComponent::FocusCameraOnCurrentFactionBase()
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return;
    }

    if (!Host->GameplayContainer.IsValid() || !Host->GameplayContainer->IsInitialized())
    {
        return;
    }

    if (!bGameStartCameraApplied)
    {
        bGameStartCameraApplied = true;
        if (bEnableC2GameStartWarZoneCamera && FocusCameraOnCurrentFactionWarZoneHard())
        {
            return;
        }
    }
    else if (bEnableC2_5TurnStartWarZoneFocusBlend && BlendCameraFocusToCurrentFactionWarZone())
    {
        return;
    }

    FocusCameraOnCell(Host->GameplayContainer->GetCurrentFactionBaseCellId(), true);
}

bool UPlanetCameraComponent::TryBuildCurrentFactionWarZoneDirection(FVector& OutLocalWarZoneDir) const
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return false;
    }

    if (!Host->GameplayContainer.IsValid() || !Host->GameplayContainer->IsInitialized() || !Host->CellTopology.IsValid())
    {
        return false;
    }

    const int32 CurrentFactionId = Host->GameplayContainer->GetCurrentFactionId();
    FVector LocalDirSum = FVector::ZeroVector;
    int32 AlivePieceCount = 0;

    for (const FTerraGameplayPieceState& Piece : Host->GameplayContainer->GetPieces())
    {
        if (!Piece.bAlive
            || Piece.OwnerFactionId != CurrentFactionId
            || !Host->CellTopology->Cells.IsValidIndex(Piece.CellId))
        {
            continue;
        }

        LocalDirSum += Host->CellTopology->Cells[Piece.CellId].UnitCenter.GetSafeNormal();
        ++AlivePieceCount;
    }

    if (AlivePieceCount <= 0 || LocalDirSum.IsNearlyZero())
    {
        return false;
    }

    OutLocalWarZoneDir = LocalDirSum.GetSafeNormal();
    return !OutLocalWarZoneDir.IsNearlyZero();
}

bool UPlanetCameraComponent::TryBuildCurrentFactionCommanderDirection(FVector& OutLocalCommanderDir) const
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return false;
    }

    if (!Host->GameplayContainer.IsValid() || !Host->GameplayContainer->IsInitialized() || !Host->CellTopology.IsValid())
    {
        return false;
    }

    const int32 CurrentFactionId = Host->GameplayContainer->GetCurrentFactionId();
    for (const FTerraGameplayPieceState& Piece : Host->GameplayContainer->GetPieces())
    {
        if (Piece.bAlive
            && Piece.OwnerFactionId == CurrentFactionId
            && Piece.PieceType == ETerraGameplayPieceType::Commander
            && Host->CellTopology->Cells.IsValidIndex(Piece.CellId))
        {
            OutLocalCommanderDir = Host->CellTopology->Cells[Piece.CellId].UnitCenter.GetSafeNormal();
            return !OutLocalCommanderDir.IsNearlyZero();
        }
    }

    const int32 BaseCellId = Host->GameplayContainer->GetCurrentFactionBaseCellId();
    if (Host->CellTopology->Cells.IsValidIndex(BaseCellId))
    {
        OutLocalCommanderDir = Host->CellTopology->Cells[BaseCellId].UnitCenter.GetSafeNormal();
        return !OutLocalCommanderDir.IsNearlyZero();
    }

    return false;
}

bool UPlanetCameraComponent::FocusCameraOnCurrentFactionWarZoneHard()
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return false;
    }

    UWorld* World = Host->GetWorld();
    if (!World || !World->IsGameWorld())
    {
        return false;
    }

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
    if (!PlayerController)
    {
        return false;
    }

    FVector LocalWarZoneDir = FVector::ZeroVector;
    if (!TryBuildCurrentFactionWarZoneDirection(LocalWarZoneDir))
    {
        return false;
    }

    FVector LocalCommanderDir = FVector::ZeroVector;
    TryBuildCurrentFactionCommanderDirection(LocalCommanderDir);

    const FTransform ActorTransform = Host->GetActorTransform();
    const FVector TargetWorldPosition = ActorTransform.TransformPosition(LocalWarZoneDir * Host->GlobeRadiusCM);
    const FVector WorldUp = ActorTransform.TransformVectorNoScale(LocalWarZoneDir).GetSafeNormal();
    if (WorldUp.IsNearlyZero())
    {
        return false;
    }

    FVector LocalForwardHint = FVector::VectorPlaneProject(LocalWarZoneDir - LocalCommanderDir, LocalWarZoneDir).GetSafeNormal();
    if (LocalForwardHint.IsNearlyZero())
    {
        FVector CurrentCameraWorldPosition = FVector::ZeroVector;
        if (AActor* ViewTarget = PlayerController->GetViewTarget())
        {
            CurrentCameraWorldPosition = ViewTarget->GetActorLocation();
        }
        else if (PlayerController->PlayerCameraManager)
        {
            CurrentCameraWorldPosition = PlayerController->PlayerCameraManager->GetCameraLocation();
        }

        if (!CurrentCameraWorldPosition.IsNearlyZero())
        {
            const FVector LocalCameraDir = ActorTransform.InverseTransformPosition(CurrentCameraWorldPosition).GetSafeNormal();
            LocalForwardHint = FVector::VectorPlaneProject(LocalWarZoneDir - LocalCameraDir, LocalWarZoneDir).GetSafeNormal();
        }
    }

    if (LocalForwardHint.IsNearlyZero())
    {
        LocalForwardHint = FVector::VectorPlaneProject(FVector::ForwardVector, LocalWarZoneDir).GetSafeNormal();
    }
    if (LocalForwardHint.IsNearlyZero())
    {
        LocalForwardHint = FVector::VectorPlaneProject(FVector::RightVector, LocalWarZoneDir).GetSafeNormal();
    }
    if (LocalForwardHint.IsNearlyZero())
    {
        return false;
    }

    const FVector WorldForwardHint = ActorTransform.TransformVectorNoScale(LocalForwardHint).GetSafeNormal();
    if (WorldForwardHint.IsNearlyZero())
    {
        return false;
    }

    const float CameraDistance = FMath::Clamp(
        FMath::Max(1.0f, C3InitialDistanceToFocusCM),
        G8CameraMinHeightOffsetCM,
        FMath::Max(G8CameraMinHeightOffsetCM, G8CameraMaxHeightOffsetCM));
    const float TiltT = (C3AutoTiltMaxDistanceCM > C3AutoTiltMinDistanceCM)
        ? (CameraDistance - C3AutoTiltMinDistanceCM) / (C3AutoTiltMaxDistanceCM - C3AutoTiltMinDistanceCM)
        : 0.0f;
    const float CameraTiltDeg = FMath::Lerp(
        C3AutoTiltAtMinDistanceDeg,
        C3AutoTiltAtMaxDistanceDeg,
        FMath::Clamp(TiltT, 0.0f, 1.0f));
    const float TiltRad = FMath::DegreesToRadians(CameraTiltDeg);
    const float HorizontalDistance = CameraDistance * FMath::Cos(TiltRad);
    const float VerticalDistance = CameraDistance * FMath::Sin(TiltRad);
    const FVector CameraWorldPosition = TargetWorldPosition - WorldForwardHint * HorizontalDistance + WorldUp * VerticalDistance;

    const FVector LookDirection = TargetWorldPosition - CameraWorldPosition;
    if (LookDirection.IsNearlyZero())
    {
        return false;
    }

    const FVector CameraForward = LookDirection.GetSafeNormal();
    const FRotator LookRotation = FRotationMatrix::MakeFromXZ(CameraForward, WorldUp).Rotator();

    FVector LocalEast = FVector::CrossProduct(FVector::UpVector, LocalWarZoneDir).GetSafeNormal();
    if (LocalEast.IsNearlyZero())
    {
        LocalEast = FVector::CrossProduct(FVector::RightVector, LocalWarZoneDir).GetSafeNormal();
    }
    const FVector LocalNorth = FVector::CrossProduct(LocalWarZoneDir, LocalEast).GetSafeNormal();
    if (!LocalEast.IsNearlyZero() && !LocalNorth.IsNearlyZero())
    {
        const float C3YawAroundFocusDeg = FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(
            static_cast<float>(FVector::DotProduct(LocalForwardHint, LocalEast)),
            static_cast<float>(FVector::DotProduct(LocalForwardHint, LocalNorth)))));

        if (APlanetInteractionController* InteractionController = Cast<APlanetInteractionController>(PlayerController))
        {
            InteractionController->SetC3FocusCameraState(LocalWarZoneDir, CameraDistance, C3YawAroundFocusDeg);
            InteractionController->SetControlRotation(LookRotation);
            ApplyFocusCameraState(LocalWarZoneDir, CameraDistance, CameraTiltDeg, C3YawAroundFocusDeg);

            UE_LOG(LogPlanetCamera, Log,
                TEXT("[Tess][C2] Game start focus war zone camera via C3 state Faction=%d Turn=%d Camera=(%.1f, %.1f, %.1f) Target=(%.1f, %.1f, %.1f) Distance=%.1f Tilt=%.1f Yaw=%.1f"),
                Host->GameplayContainer.IsValid() ? Host->GameplayContainer->GetCurrentFactionId() : INDEX_NONE,
                Host->GameplayContainer.IsValid() ? Host->GameplayContainer->GetTurnIndex() : INDEX_NONE,
                CameraWorldPosition.X,
                CameraWorldPosition.Y,
                CameraWorldPosition.Z,
                TargetWorldPosition.X,
                TargetWorldPosition.Y,
                TargetWorldPosition.Z,
                CameraDistance,
                CameraTiltDeg,
                C3YawAroundFocusDeg);

            return true;
        }
    }

    if (AActor* ViewTarget = PlayerController->GetViewTarget())
    {
        ViewTarget->SetActorLocation(CameraWorldPosition);
        ViewTarget->SetActorRotation(LookRotation);
    }
    PlayerController->SetControlRotation(LookRotation);

    UE_LOG(LogPlanetCamera, Log,
        TEXT("[Tess][C2] Game start focus war zone camera fallback Faction=%d Turn=%d Camera=(%.1f, %.1f, %.1f) Target=(%.1f, %.1f, %.1f) Distance=%.1f Tilt=%.1f"),
        Host->GameplayContainer.IsValid() ? Host->GameplayContainer->GetCurrentFactionId() : INDEX_NONE,
        Host->GameplayContainer.IsValid() ? Host->GameplayContainer->GetTurnIndex() : INDEX_NONE,
        CameraWorldPosition.X,
        CameraWorldPosition.Y,
        CameraWorldPosition.Z,
        TargetWorldPosition.X,
        TargetWorldPosition.Y,
        TargetWorldPosition.Z,
        CameraDistance,
        CameraTiltDeg);

    return true;
}

bool UPlanetCameraComponent::BlendCameraFocusToCurrentFactionWarZone()
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return false;
    }

    if (!Host->GameplayContainer.IsValid() || !Host->GameplayContainer->IsInitialized() || !Host->CellTopology.IsValid())
    {
        return false;
    }

    FVector LocalWarZoneDir = FVector::ZeroVector;
    if (!TryBuildCurrentFactionWarZoneDirection(LocalWarZoneDir))
    {
        return false;
    }

    UWorld* World = Host->GetWorld();
    if (!World || !World->IsGameWorld())
    {
        return false;
    }

    APlanetInteractionController* InteractionController = Cast<APlanetInteractionController>(
        UGameplayStatics::GetPlayerController(World, 0));
    if (!InteractionController)
    {
        return false;
    }

    const bool bRequested = InteractionController->RequestC2_5FocusOnUnitDir(
        LocalWarZoneDir,
        FMath::Max(0.0f, C2_5TurnStartFocusBlendSeconds));
    if (bRequested)
    {
        UE_LOG(LogPlanetCamera, Log,
            TEXT("[Tess][C2.5] Requested turn start war zone focus blend Faction=%d Turn=%d Blend=%.2f"),
            Host->GameplayContainer->GetCurrentFactionId(),
            Host->GameplayContainer->GetTurnIndex(),
            C2_5TurnStartFocusBlendSeconds);
    }
    return bRequested;
}


bool UPlanetCameraComponent::SyncOrbitCameraStateFromWorldPosition(const FVector& CameraWorldPosition, float& InOutLongitudeDeg, float& InOutLatitudeDeg, float& InOutHeightOffsetCM) const
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return false;
    }

    const FVector PlanetCenterWorld = Host->GetPlanetCenterWorld_();
    FVector LocalOffset = Host->GetActorTransform().InverseTransformPosition(CameraWorldPosition);
    const float DistanceFromCenter = LocalOffset.Length();
    if (DistanceFromCenter <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    const FVector UnitDir = LocalOffset / DistanceFromCenter;
    const float LatitudeRad = FMath::Asin(FMath::Clamp(static_cast<float>(UnitDir.Z), -1.0f, 1.0f));
    const float LongitudeRad = FMath::Atan2(static_cast<float>(UnitDir.Y), static_cast<float>(UnitDir.X));

    InOutLatitudeDeg = FMath::RadiansToDegrees(LatitudeRad);
    InOutLongitudeDeg = FMath::RadiansToDegrees(LongitudeRad);
    InOutHeightOffsetCM = DistanceFromCenter - Host->GlobeRadiusCM;
    return !PlanetCenterWorld.ContainsNaN();
}

bool UPlanetCameraComponent::ApplyOrbitCameraState(float LongitudeDeg, float LatitudeDeg, float HeightOffsetCM)
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return false;
    }

    UWorld* World = Host->GetWorld();
    if (!World || !World->IsGameWorld() || !bEnableG8ManualCameraControl)
    {
        return false;
    }

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
    if (!PlayerController)
    {
        return false;
    }

    const float LongitudeRad = FMath::DegreesToRadians(LongitudeDeg);
    const float LatitudeRad = FMath::DegreesToRadians(LatitudeDeg);
    const float Radius = Host->GlobeRadiusCM + HeightOffsetCM;

    const FVector LocalUnitDir(
        FMath::Cos(LatitudeRad) * FMath::Cos(LongitudeRad),
        FMath::Cos(LatitudeRad) * FMath::Sin(LongitudeRad),
        FMath::Sin(LatitudeRad));
    const FVector CameraWorldPosition = Host->GetActorTransform().TransformPosition(LocalUnitDir * Radius);
    const FVector PlanetCenterWorld = Host->GetPlanetCenterWorld_();
    const FVector LookDirection = PlanetCenterWorld - CameraWorldPosition;
    if (LookDirection.IsNearlyZero())
    {
        return false;
    }

    const FRotator LookRotation = LookDirection.Rotation();
    if (AActor* ViewTarget = PlayerController->GetViewTarget())
    {
        ViewTarget->SetActorLocation(CameraWorldPosition);
        ViewTarget->SetActorRotation(LookRotation);
    }
    PlayerController->SetControlRotation(LookRotation);
    return true;
}

bool UPlanetCameraComponent::SyncFocusCameraStateFromView(
    const FVector& CameraWorldPosition,
    const FRotator& CameraWorldRotation,
    FVector& OutFocusUnitDir,
    float& OutDistanceToFocusCM,
    float& OutTiltDeg,
    float& OutYawAroundFocusDeg) const
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return false;
    }

    const FTransform ActorTransform = Host->GetActorTransform();
    const FVector LocalCameraPosition = ActorTransform.InverseTransformPosition(CameraWorldPosition);
    const FVector LocalCameraForward = ActorTransform.InverseTransformVectorNoScale(CameraWorldRotation.Vector()).GetSafeNormal();
    if (LocalCameraForward.IsNearlyZero())
    {
        return false;
    }

    const float Radius = FMath::Max(1.0f, Host->GlobeRadiusCM);
    const float B = FVector::DotProduct(LocalCameraPosition, LocalCameraForward);
    const float C = FVector::DotProduct(LocalCameraPosition, LocalCameraPosition) - Radius * Radius;
    const float Discriminant = B * B - C;

    FVector LocalFocusPoint = FVector::ZeroVector;
    if (Discriminant >= 0.0f)
    {
        const float SqrtDiscriminant = FMath::Sqrt(Discriminant);
        const float T0 = -B - SqrtDiscriminant;
        const float T1 = -B + SqrtDiscriminant;
        const float T = T0 >= 0.0f ? T0 : T1;
        if (T >= 0.0f)
        {
            LocalFocusPoint = LocalCameraPosition + LocalCameraForward * T;
        }
    }

    if (LocalFocusPoint.IsNearlyZero())
    {
        const FVector LocalCameraDir = LocalCameraPosition.GetSafeNormal();
        if (LocalCameraDir.IsNearlyZero())
        {
            return false;
        }
        LocalFocusPoint = LocalCameraDir * Radius;
    }

    const FVector LocalFocusDir = LocalFocusPoint.GetSafeNormal();
    if (LocalFocusDir.IsNearlyZero())
    {
        return false;
    }

    const FVector FocusWorldPosition = ActorTransform.TransformPosition(LocalFocusDir * Radius);
    const FVector WorldUp = ActorTransform.TransformVectorNoScale(LocalFocusDir).GetSafeNormal();
    const FVector WorldForward = CameraWorldRotation.Vector().GetSafeNormal();
    if (WorldUp.IsNearlyZero() || WorldForward.IsNearlyZero())
    {
        return false;
    }

    OutFocusUnitDir = LocalFocusDir;
    OutDistanceToFocusCM = FMath::Max(1.0f, FVector::Distance(CameraWorldPosition, FocusWorldPosition));

    const float DownDotNormal = FVector::DotProduct(-WorldForward, WorldUp);
    OutTiltDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(DownDotNormal, -1.0f, 1.0f)));
    OutTiltDeg = FMath::Clamp(OutTiltDeg, 5.0f, 85.0f);

    // 在焦点局部基 (East, North) 下反解 Yaw。
    // 与 ApplyFocusCameraState / OffsetFocusCameraStateOnTangent 保持同一约定：
    //     ForwardHint = cos(Yaw) * North + sin(Yaw) * East
    const FVector LocalWorldUp = ActorTransform.InverseTransformVectorNoScale(WorldUp).GetSafeNormal();
    FVector LocalEast = FVector::CrossProduct(FVector::UpVector, LocalWorldUp).GetSafeNormal();
    if (LocalEast.IsNearlyZero())
    {
        LocalEast = FVector::CrossProduct(FVector::RightVector, LocalWorldUp).GetSafeNormal();
    }
    if (LocalEast.IsNearlyZero())
    {
        return false;
    }
    const FVector LocalNorth = FVector::CrossProduct(LocalWorldUp, LocalEast).GetSafeNormal();
    if (LocalNorth.IsNearlyZero())
    {
        return false;
    }

    const FVector LocalCameraToFocus = (LocalFocusDir * Radius - LocalCameraPosition).GetSafeNormal();
    const FVector LocalForwardTangent = FVector::VectorPlaneProject(LocalCameraToFocus, LocalFocusDir).GetSafeNormal();
    if (!LocalForwardTangent.IsNearlyZero())
    {
        const float YawRad = FMath::Atan2(
            static_cast<float>(FVector::DotProduct(LocalForwardTangent, LocalEast)),
            static_cast<float>(FVector::DotProduct(LocalForwardTangent, LocalNorth)));
        OutYawAroundFocusDeg = FRotator::NormalizeAxis(FMath::RadiansToDegrees(YawRad));
    }
    else
    {
        OutYawAroundFocusDeg = 0.0f;
    }

    return true;
}

bool UPlanetCameraComponent::ApplyFocusCameraState(
    const FVector& FocusUnitDir,
    float DistanceToFocusCM,
    float TiltDeg,
    float YawAroundFocusDeg)
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return false;
    }

    UWorld* World = Host->GetWorld();
    if (!World)
    {
        return false;
    }
    if (!World->IsGameWorld())
    {
        return false;
    }
    if (!bEnableG8ManualCameraControl)
    {
        return false;
    }

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
    if (!PlayerController)
    {
        return false;
    }

    const FVector LocalFocusDir = FocusUnitDir.GetSafeNormal();
    if (LocalFocusDir.IsNearlyZero())
    {
        return false;
    }

    // 极区退化补救：|FocusDir × WorldZ| 太小时，改用 WorldY 作辅助基。详见设计稿 §3。
    FVector LocalEast = FVector::CrossProduct(FVector::UpVector, LocalFocusDir).GetSafeNormal();
    if (LocalEast.IsNearlyZero())
    {
        LocalEast = FVector::CrossProduct(FVector::RightVector, LocalFocusDir).GetSafeNormal();
    }
    if (LocalEast.IsNearlyZero())
    {
        return false;
    }

    const FVector LocalNorth = FVector::CrossProduct(LocalFocusDir, LocalEast).GetSafeNormal();
    if (LocalNorth.IsNearlyZero())
    {
        return false;
    }

    const float YawRad = FMath::DegreesToRadians(FRotator::NormalizeAxis(YawAroundFocusDeg));
    const FVector LocalForwardHint = (LocalNorth * FMath::Cos(YawRad) + LocalEast * FMath::Sin(YawRad)).GetSafeNormal();
    if (LocalForwardHint.IsNearlyZero())
    {
        return false;
    }

    const float Distance = FMath::Clamp(
        DistanceToFocusCM,
        G8CameraMinHeightOffsetCM,
        FMath::Max(G8CameraMinHeightOffsetCM, G8CameraMaxHeightOffsetCM));
    const float TiltRad = FMath::DegreesToRadians(FMath::Clamp(TiltDeg, 5.0f, 85.0f));
    const float HorizontalDistance = Distance * FMath::Cos(TiltRad);
    const float VerticalDistance = Distance * FMath::Sin(TiltRad);

    const FTransform ActorTransform = Host->GetActorTransform();
    const FVector FocusWorldPosition = ActorTransform.TransformPosition(LocalFocusDir * Host->GlobeRadiusCM);
    const FVector WorldUp = ActorTransform.TransformVectorNoScale(LocalFocusDir).GetSafeNormal();
    const FVector WorldForwardHint = ActorTransform.TransformVectorNoScale(LocalForwardHint).GetSafeNormal();
    if (WorldUp.IsNearlyZero() || WorldForwardHint.IsNearlyZero())
    {
        return false;
    }

    const FVector CameraWorldPosition = FocusWorldPosition - WorldForwardHint * HorizontalDistance + WorldUp * VerticalDistance;
    const FVector CameraForward = (FocusWorldPosition - CameraWorldPosition).GetSafeNormal();
    if (CameraForward.IsNearlyZero())
    {
        return false;
    }

    const FRotator LookRotation = FRotationMatrix::MakeFromXZ(CameraForward, WorldUp).Rotator();
    if (AActor* ViewTarget = PlayerController->GetViewTarget())
    {
        ViewTarget->SetActorLocation(CameraWorldPosition);
        ViewTarget->SetActorRotation(LookRotation);
    }
    PlayerController->SetControlRotation(LookRotation);
    return true;
}

bool UPlanetCameraComponent::OffsetFocusCameraStateOnTangent(
    float RightDeltaDeg,
    float ForwardDeltaDeg,
    FVector& InOutFocusUnitDir,
    float& InOutYawAroundFocusDeg) const
{
    // 输入弧度增量。
    const float RightDeltaRad = FMath::DegreesToRadians(RightDeltaDeg);
    const float ForwardDeltaRad = FMath::DegreesToRadians(ForwardDeltaDeg);
    const float DeltaAngleRadSq = RightDeltaRad * RightDeltaRad + ForwardDeltaRad * ForwardDeltaRad;
    if (DeltaAngleRadSq <= KINDA_SMALL_NUMBER * KINDA_SMALL_NUMBER)
    {
        return true; // 本帧无输入，什么都不做。
    }

    const FVector FocusUnitDir = InOutFocusUnitDir.GetSafeNormal();
    if (FocusUnitDir.IsNearlyZero())
    {
        return false;
    }

    // 构造当前焦点的局部切平面基 (East, North)。
    // 极区退化补救：|FocusDir × WorldZ| 太小时改用 WorldY，见设计稿 §3。
    FVector LocalEast = FVector::CrossProduct(FVector::UpVector, FocusUnitDir).GetSafeNormal();
    if (LocalEast.IsNearlyZero())
    {
        LocalEast = FVector::CrossProduct(FVector::RightVector, FocusUnitDir).GetSafeNormal();
    }
    if (LocalEast.IsNearlyZero())
    {
        return false;
    }
    const FVector LocalNorth = FVector::CrossProduct(FocusUnitDir, LocalEast).GetSafeNormal();
    if (LocalNorth.IsNearlyZero())
    {
        return false;
    }

    // 由当前 Yaw 组当前 ForwardTangent 与 RightTangent。
    // 与 Apply/Sync 完全一致的约定：ForwardTangent = cos(Yaw)·North + sin(Yaw)·East。
    const float YawRad = FMath::DegreesToRadians(FRotator::NormalizeAxis(InOutYawAroundFocusDeg));
    const FVector LocalForwardTangent = (LocalNorth * FMath::Cos(YawRad) + LocalEast * FMath::Sin(YawRad)).GetSafeNormal();
    if (LocalForwardTangent.IsNearlyZero())
    {
        return false;
    }
    // 屏幕右方向切向：Cross(FocusDir, ForwardTangent)。
    //   在 Yaw=0 的赤道场景里手工推：Up=(1,0,0), North=(0,0,1), 得
    //     LocalRightTangent = Cross((1,0,0),(0,0,1)) = (0,-1,0) = -East
    //   同时 Apply 里 MakeFromXZ(CameraForward=+North, WorldUp=+Up) 摆出的相机
    //   Actor Y（左手系里的 RightVector = 屏幕右方向）也是 -East。二者一致。
    //   因此 "D 键 = +RightDeltaDeg = 焦点沿 +LocalRightTangent = -East 走 = 屏幕右滑"
    //   等价于用户口中的 "D = 视角向右走"。
    const FVector LocalRightTangent = FVector::CrossProduct(FocusUnitDir, LocalForwardTangent).GetSafeNormal();
    if (LocalRightTangent.IsNearlyZero())
    {
        return false;
    }

    // 组本帧的瞬时移动切向 T 与旋转弧长 θ。
    const FVector LocalTangentDelta = LocalRightTangent * RightDeltaRad + LocalForwardTangent * ForwardDeltaRad;
    const float DeltaAngleRad = LocalTangentDelta.Length();
    if (DeltaAngleRad <= KINDA_SMALL_NUMBER)
    {
        return true;
    }
    const FVector LocalMoveTangent = LocalTangentDelta / DeltaAngleRad;

    // 沿测地线推进焦点。等价于绕轴 A = Cross(FocusDir, T) 旋转 θ 弧度。
    // 因为 |FocusDir| = 1、|T| = 1、FocusDir ⊥ T，故 |A| = 1，可直接作为 Rodrigues 旋转轴。
    const FVector RotationAxis = FVector::CrossProduct(FocusUnitDir, LocalMoveTangent).GetSafeNormal();
    if (RotationAxis.IsNearlyZero())
    {
        return false;
    }

    const float CosTheta = FMath::Cos(DeltaAngleRad);
    const float SinTheta = FMath::Sin(DeltaAngleRad);

    auto Rodrigues = [&](const FVector& V) -> FVector
    {
        // R(k, θ)·v = v·cosθ + (k×v)·sinθ + k·(k·v)·(1 - cosθ)
        const FVector KCrossV = FVector::CrossProduct(RotationAxis, V);
        const float KDotV = FVector::DotProduct(RotationAxis, V);
        return V * CosTheta + KCrossV * SinTheta + RotationAxis * (KDotV * (1.0f - CosTheta));
    };

    const FVector NewFocusUnitDir = Rodrigues(FocusUnitDir).GetSafeNormal();
    if (NewFocusUnitDir.IsNearlyZero())
    {
        return false;
    }

    // 把 ForwardTangent 沿同一根旋转轴平行运输到新焦点。
    const FVector NewForwardTangent = Rodrigues(LocalForwardTangent).GetSafeNormal();
    if (NewForwardTangent.IsNearlyZero())
    {
        InOutFocusUnitDir = NewFocusUnitDir;
        return true; // 焦点更新成功，Yaw 保持原值。
    }

    // 在新焦点的局部基下反解新 Yaw。
    FVector NewLocalEast = FVector::CrossProduct(FVector::UpVector, NewFocusUnitDir).GetSafeNormal();
    if (NewLocalEast.IsNearlyZero())
    {
        NewLocalEast = FVector::CrossProduct(FVector::RightVector, NewFocusUnitDir).GetSafeNormal();
    }
    if (NewLocalEast.IsNearlyZero())
    {
        InOutFocusUnitDir = NewFocusUnitDir;
        return true;
    }
    const FVector NewLocalNorth = FVector::CrossProduct(NewFocusUnitDir, NewLocalEast).GetSafeNormal();
    if (NewLocalNorth.IsNearlyZero())
    {
        InOutFocusUnitDir = NewFocusUnitDir;
        return true;
    }

    // NewForwardTangent 理论上已在新切平面内（因为整个 Rodrigues 保持标架正交），
    // 但受浮点误差影响，先投影到新切平面再反解 atan2 更稳定。
    const FVector ProjectedForward = FVector::VectorPlaneProject(NewForwardTangent, NewFocusUnitDir).GetSafeNormal();
    if (!ProjectedForward.IsNearlyZero())
    {
        const float NewYawRad = FMath::Atan2(
            static_cast<float>(FVector::DotProduct(ProjectedForward, NewLocalEast)),
            static_cast<float>(FVector::DotProduct(ProjectedForward, NewLocalNorth)));
        InOutYawAroundFocusDeg = FRotator::NormalizeAxis(FMath::RadiansToDegrees(NewYawRad));
    }

    InOutFocusUnitDir = NewFocusUnitDir;
    return true;
}

