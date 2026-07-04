#include "TerraPieceActor.h"

#include "Animation/AnimationAsset.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "TerraPieceAnimInstance.h"

namespace
{
    FVector GetPiecePresentationCenter(const AActor* PieceActor)
    {
        const AActor* Owner = PieceActor ? PieceActor->GetOwner() : nullptr;
        return Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
    }

    FVector SphericalInterpPosition(const FVector& Center, const FVector& From, const FVector& To, float Alpha, float ExtraHeightCM)
    {
        const FVector FromOffset = From - Center;
        const FVector ToOffset = To - Center;
        const float FromRadius = FromOffset.Length();
        const float ToRadius = ToOffset.Length();

        const FVector FromDir = FromOffset.GetSafeNormal();
        const FVector ToDir = ToOffset.GetSafeNormal();
        if (FromDir.IsNearlyZero() || ToDir.IsNearlyZero())
        {
            const FVector LinearPosition = FMath::Lerp(From, To, Alpha);
            const FVector LinearUp = (LinearPosition - Center).GetSafeNormal();
            return LinearPosition + LinearUp * ExtraHeightCM;
        }

        const float Dot = FMath::Clamp(FVector::DotProduct(FromDir, ToDir), -1.0f, 1.0f);
        const float Angle = FMath::Acos(Dot);

        FVector Direction = FVector::ZeroVector;
        if (Angle <= KINDA_SMALL_NUMBER)
        {
            Direction = FMath::Lerp(FromDir, ToDir, Alpha).GetSafeNormal();
        }
        else
        {
            const float SinAngle = FMath::Sin(Angle);
            Direction = ((FMath::Sin((1.0f - Alpha) * Angle) / SinAngle) * FromDir
                + (FMath::Sin(Alpha * Angle) / SinAngle) * ToDir).GetSafeNormal();
        }

        if (Direction.IsNearlyZero())
        {
            Direction = FMath::Lerp(FromDir, ToDir, Alpha).GetSafeNormal();
        }

        const float Radius = FMath::Lerp(FromRadius, ToRadius, Alpha) + ExtraHeightCM;
        return Center + Direction * Radius;
    }
}

ATerraPieceActor::ATerraPieceActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
    SetRootComponent(RootScene);

    HumanMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HumanMesh"));
    HumanMesh->SetupAttachment(RootScene);
    HumanMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HumanMesh->SetGenerateOverlapEvents(false);
    HumanMesh->SetCanEverAffectNavigation(false);
    HumanMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
}

void ATerraPieceActor::ApplyPresentationSnapshot(const FTerraPiecePresentationSnapshot& Snapshot, const FTerraPieceVisualConfig& VisualConfig)
{
    if (bHasActiveMove
        && ActiveMoveEvent.PieceId == Snapshot.PieceId
        && ActiveMoveEvent.ToCellId == Snapshot.CellId)
    {
        PieceId = Snapshot.PieceId;
        OwnerFactionId = Snapshot.OwnerFactionId;
        CellId = Snapshot.CellId;
        PieceType = Snapshot.PieceType;
        CachedVisualConfig = VisualConfig;
        return;
    }

    bHasActiveMove = false;
    ActiveMoveElapsedSeconds = 0.0f;
    ActiveMoveDurationSeconds = 0.0f;
    ActiveMoveEvent = FTerraPiecePresentationMoveEvent();
    SetActorTickEnabled(false);

    const int32 PreviousCellId = CellId;
    const bool bShouldApplySnapshotFacing = Snapshot.bForceFacingFromSnapshot || PreviousCellId != Snapshot.CellId;

    PieceId = Snapshot.PieceId;
    OwnerFactionId = Snapshot.OwnerFactionId;
    CellId = Snapshot.CellId;
    PieceType = Snapshot.PieceType;
    FTransform AppliedTransform = Snapshot.WorldTransform;
    if (!bShouldApplySnapshotFacing)
    {
        AppliedTransform.SetRotation(GetActorQuat());
    }
    SetActorTransform(AppliedTransform);

    CachedVisualConfig = VisualConfig;
    ApplyVisualConfig_(VisualConfig);

    FTerraPieceAnimDriveState NewDriveState;
    NewDriveState.ActionState = ETerraPieceAnimActionState::Idle;
    NewDriveState.MoveSpeed = 0.0f;
    NewDriveState.NormalizedPhase = 0.0f;
    NewDriveState.FacingDirection = GetActorForwardVector();
    NewDriveState.bDead = false;
    SetDriveState_(NewDriveState);
    PlayConfiguredAnimation_(VisualConfig.IdleAnimation, true);
}

void ATerraPieceActor::PlayPresentationMove(
    const FTerraPiecePresentationSnapshot& TargetSnapshot,
    const FTerraPiecePresentationMoveEvent& MoveEvent,
    const FTerraPieceVisualConfig& VisualConfig)
{
    if (!MoveEvent.IsValidMove())
    {
        ApplyPresentationSnapshot(TargetSnapshot, VisualConfig);
        return;
    }

    const bool bContinueFromCurrentVisualTransform = bHasActiveMove;
    const FTransform CurrentVisualTransform = GetActorTransform();

    PieceId = TargetSnapshot.PieceId;
    OwnerFactionId = TargetSnapshot.OwnerFactionId;
    CellId = TargetSnapshot.CellId;
    PieceType = TargetSnapshot.PieceType;

    CachedVisualConfig = VisualConfig;
    ApplyVisualConfig_(VisualConfig);

    ActiveMoveEvent = MoveEvent;
    if (bContinueFromCurrentVisualTransform)
    {
        ActiveMoveEvent.FromWorldTransform = CurrentVisualTransform;
    }
    else
    {
        ActiveMoveEvent.FromWorldTransform.SetRotation(CurrentVisualTransform.GetRotation());
    }
    ActiveMoveEvent.ToWorldTransform = TargetSnapshot.WorldTransform;

    ActiveMoveElapsedSeconds = 0.0f;
    ActiveMoveDurationSeconds = MoveEvent.MoveType == ETerraPiecePresentationMoveType::Jump
        ? FMath::Max(VisualConfig.JumpDurationSeconds, 0.001f)
        : FMath::Max(VisualConfig.MoveDurationSeconds, 0.001f);
    bHasActiveMove = true;

    SetActorTransform(ActiveMoveEvent.FromWorldTransform);
    ActiveMoveDesiredFacingDirection = ActiveMoveEvent.ToWorldTransform.GetLocation() - ActiveMoveEvent.FromWorldTransform.GetLocation();
    if (ActiveMoveDesiredFacingDirection.IsNearlyZero())
    {
        ActiveMoveDesiredFacingDirection = ActiveMoveEvent.FromWorldTransform.GetRotation().GetForwardVector();
    }
    else
    {
        ActiveMoveDesiredFacingDirection.Normalize();
    }

    FTerraPieceAnimDriveState NewDriveState;
    NewDriveState.ActionState = MoveEvent.MoveType == ETerraPiecePresentationMoveType::Jump
        ? ETerraPieceAnimActionState::Jump
        : ETerraPieceAnimActionState::Move;
    NewDriveState.MoveSpeed = FVector::Distance(
        ActiveMoveEvent.FromWorldTransform.GetLocation(),
        ActiveMoveEvent.ToWorldTransform.GetLocation()) / ActiveMoveDurationSeconds;
    NewDriveState.NormalizedPhase = 0.0f;
    NewDriveState.FacingDirection = ActiveMoveDesiredFacingDirection;
    NewDriveState.bDead = false;
    SetDriveState_(NewDriveState);

    PlayConfiguredAnimation_(
        MoveEvent.MoveType == ETerraPiecePresentationMoveType::Jump ? VisualConfig.JumpAnimation : VisualConfig.MoveAnimation,
        MoveEvent.MoveType != ETerraPiecePresentationMoveType::Jump);

    SetActorTickEnabled(true);
}

void ATerraPieceActor::CancelPresentationMove(const FTerraPiecePresentationSnapshot& Snapshot, const FTerraPieceVisualConfig& VisualConfig)
{
    ApplyPresentationSnapshot(Snapshot, VisualConfig);
}

void ATerraPieceActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bHasActiveMove)
    {
        SetActorTickEnabled(false);
        return;
    }

    ActiveMoveElapsedSeconds += FMath::Max(DeltaSeconds, 0.0f);
    const float Alpha = FMath::Clamp(ActiveMoveElapsedSeconds / FMath::Max(ActiveMoveDurationSeconds, 0.001f), 0.0f, 1.0f);

    const FVector Position = EvaluateActiveMovePosition_(Alpha, CachedVisualConfig);
    FQuat DesiredRotation = BuildRotationFromFacing_(Position, ActiveMoveDesiredFacingDirection, ActiveMoveEvent.ToWorldTransform.GetRotation());
    const FQuat CurrentRotation = GetActorQuat();
    if ((CurrentRotation | DesiredRotation) < 0.0f)
    {
        DesiredRotation = DesiredRotation * -1.0f;
    }
    const float RotationAlpha = FMath::Clamp(DeltaSeconds * 12.0f, 0.0f, 1.0f);
    const FQuat Rotation = FQuat::Slerp(CurrentRotation, DesiredRotation, RotationAlpha).GetNormalized();
    SetActorTransform(FTransform(Rotation, Position, FVector::OneVector));

    FTerraPieceAnimDriveState NewDriveState = AnimDriveState;
    NewDriveState.NormalizedPhase = Alpha;
    NewDriveState.FacingDirection = GetActorForwardVector();
    SetDriveState_(NewDriveState);

    if (Alpha >= 1.0f - KINDA_SMALL_NUMBER)
    {
        FinishActiveMove_();
    }
}

void ATerraPieceActor::ApplyVisualConfig_(const FTerraPieceVisualConfig& VisualConfig)
{
    if (!HumanMesh)
    {
        return;
    }

    HumanMesh->SetSkeletalMesh(VisualConfig.ResolveMesh(PieceType));
    HumanMesh->SetRelativeLocation(VisualConfig.MeshRelativeLocation);
    HumanMesh->SetRelativeRotation(VisualConfig.MeshRelativeRotation);
    HumanMesh->SetRelativeScale3D(FVector(FMath::Max(VisualConfig.UniformScale, 0.001f)));
}

void ATerraPieceActor::SetDriveState_(const FTerraPieceAnimDriveState& NewDriveState)
{
    AnimDriveState = NewDriveState;

    if (!HumanMesh)
    {
        return;
    }

    if (UTerraPieceAnimInstance* AnimInstance = Cast<UTerraPieceAnimInstance>(HumanMesh->GetAnimInstance()))
    {
        AnimInstance->SetDriveState(AnimDriveState);
    }
}

void ATerraPieceActor::PlayConfiguredAnimation_(UAnimationAsset* AnimationAsset, bool bLooping)
{
    if (!HumanMesh || !AnimationAsset)
    {
        return;
    }

    if (Cast<UTerraPieceAnimInstance>(HumanMesh->GetAnimInstance()))
    {
        return;
    }

    HumanMesh->PlayAnimation(AnimationAsset, bLooping);
}

void ATerraPieceActor::FinishActiveMove_()
{
    const FVector TargetPosition = EvaluateActiveMovePosition_(1.0f, CachedVisualConfig);
    const FQuat TargetRotation = BuildRotationFromFacing_(TargetPosition, ActiveMoveDesiredFacingDirection, ActiveMoveEvent.ToWorldTransform.GetRotation());
    const FTransform TargetTransform(TargetRotation, TargetPosition, FVector::OneVector);

    bHasActiveMove = false;
    ActiveMoveElapsedSeconds = 0.0f;
    ActiveMoveDurationSeconds = 0.0f;
    SetActorTickEnabled(false);

    SetActorTransform(TargetTransform);

    FTerraPieceAnimDriveState NewDriveState;
    NewDriveState.ActionState = ETerraPieceAnimActionState::Idle;
    NewDriveState.MoveSpeed = 0.0f;
    NewDriveState.NormalizedPhase = 0.0f;
    NewDriveState.FacingDirection = GetActorForwardVector();
    NewDriveState.bDead = false;
    SetDriveState_(NewDriveState);
    PlayConfiguredAnimation_(CachedVisualConfig.IdleAnimation, true);
}

FVector ATerraPieceActor::EvaluateActiveMovePosition_(float Alpha, const FTerraPieceVisualConfig& VisualConfig) const
{
    const FVector From = ActiveMoveEvent.FromWorldTransform.GetLocation();
    const FVector To = ActiveMoveEvent.ToWorldTransform.GetLocation();
    const FVector Center = GetPiecePresentationCenter(this);

    const float ExtraHeightCM = ActiveMoveEvent.MoveType == ETerraPiecePresentationMoveType::Jump
        ? FMath::Max(VisualConfig.JumpHeightCM, 0.0f) * FMath::Sin(Alpha * PI)
        : 0.0f;

    return SphericalInterpPosition(Center, From, To, Alpha, ExtraHeightCM);
}

FQuat ATerraPieceActor::BuildRotationFromFacing_(const FVector& WorldPosition, const FVector& DesiredFacingDirection, const FQuat& FallbackRotation) const
{
    const FVector Center = GetPiecePresentationCenter(this);
    FVector Up = (WorldPosition - Center).GetSafeNormal();
    if (Up.IsNearlyZero())
    {
        Up = FallbackRotation.GetUpVector();
    }

    FVector Forward = FVector::VectorPlaneProject(DesiredFacingDirection, Up).GetSafeNormal();
    if (Forward.IsNearlyZero())
    {
        Forward = FVector::VectorPlaneProject(FallbackRotation.GetForwardVector(), Up).GetSafeNormal();
    }
    if (Forward.IsNearlyZero())
    {
        Forward = FVector::VectorPlaneProject(FVector::ForwardVector, Up).GetSafeNormal();
    }
    if (Forward.IsNearlyZero())
    {
        Forward = FVector::VectorPlaneProject(FVector::RightVector, Up).GetSafeNormal();
    }

    return FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat();
}
