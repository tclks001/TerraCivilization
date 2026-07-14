#include "TerraPieceActor.h"

#include "Animation/AnimationAsset.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TerraMountedRiderAnimInstance.h"
#include "TerraPieceAnimInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerraPieceActor, Log, All);

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

    uint32 PackP7TileMaskBits(const TArray<FTerraPiecePaletteTile>& Tiles)
    {
        uint32 Bits = 0;
        for (const FTerraPiecePaletteTile& Tile : Tiles)
        {
            if (Tile.Row >= 0 && Tile.Row < 4 && Tile.Col >= 0 && Tile.Col < 8)
            {
                const int32 MaterialRow = 3 - Tile.Row;
                Bits |= 1u << static_cast<uint32>(MaterialRow * 8 + Tile.Col);
            }
        }
        return Bits;
    }

    FColor PackP7TileMaskRGBA8(const TArray<FTerraPiecePaletteTile>& Tiles)
    {
        const uint32 Mask = PackP7TileMaskBits(Tiles);
        return FColor(
            static_cast<uint8>(Mask & 255u),
            static_cast<uint8>((Mask >> 8) & 255u),
            static_cast<uint8>((Mask >> 16) & 255u),
            static_cast<uint8>((Mask >> 24) & 255u));
    }

    FLinearColor NormalizeP7TileMaskColor(const FColor& Color)
    {
        return FLinearColor(
            static_cast<float>(Color.R) / 255.0f,
            static_cast<float>(Color.G) / 255.0f,
            static_cast<float>(Color.B) / 255.0f,
            static_cast<float>(Color.A) / 255.0f);
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

    HorseMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HorseMesh"));
    HorseMesh->SetupAttachment(RootScene);
    HorseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HorseMesh->SetGenerateOverlapEvents(false);
    HorseMesh->SetCanEverAffectNavigation(false);
    HorseMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    HorseMesh->SetHiddenInGame(true);
    HorseMesh->SetVisibility(false);

    RiderAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("RiderAnchor"));
    RiderAnchor->SetupAttachment(RootScene);

    RiderMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RiderMesh"));
    RiderMesh->SetupAttachment(RiderAnchor);
    RiderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RiderMesh->SetGenerateOverlapEvents(false);
    RiderMesh->SetCanEverAffectNavigation(false);
    RiderMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    RiderMesh->SetHiddenInGame(true);
    RiderMesh->SetVisibility(false);

    WeaponPrimaryMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponPrimaryMesh"));
    WeaponPrimaryMesh->SetupAttachment(RootScene);
    WeaponPrimaryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WeaponPrimaryMesh->SetGenerateOverlapEvents(false);
    WeaponPrimaryMesh->SetCanEverAffectNavigation(false);
    WeaponPrimaryMesh->SetHiddenInGame(true);
    WeaponPrimaryMesh->SetVisibility(false);

    WeaponSecondaryMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponSecondaryMesh"));
    WeaponSecondaryMesh->SetupAttachment(RootScene);
    WeaponSecondaryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WeaponSecondaryMesh->SetGenerateOverlapEvents(false);
    WeaponSecondaryMesh->SetCanEverAffectNavigation(false);
    WeaponSecondaryMesh->SetHiddenInGame(true);
    WeaponSecondaryMesh->SetVisibility(false);
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
    bPresentationOnlyMove = false;
    PresentationOnlyMoveForwardMode = ETerraPiecePresentationForwardMode::MoveDirection;
    PresentationOnlyMoveForwardTarget = FVector::ZeroVector;
    ActiveMoveElapsedSeconds = 0.0f;
    ActiveMoveDurationSeconds = 0.0f;
    ActiveMoveEvent = FTerraPiecePresentationMoveEvent();

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
    if (IsMountedCavalry_())
    {
        PlayMountedIdleAnimations_();
    }
    else
    {
        PlayConfiguredAnimation_(VisualConfig.IdleAnimation, true);
    }
    SetActorTickEnabled(bHasActiveFacingBlend || bHasActiveDeathFade);
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
    bPresentationOnlyMove = false;
    PresentationOnlyMoveForwardMode = ETerraPiecePresentationForwardMode::MoveDirection;
    PresentationOnlyMoveForwardTarget = FVector::ZeroVector;

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

    if (IsMountedCavalry_())
    {
        PlayMountedMoveAnimation_(MoveEvent.MoveType);
    }
    else
    {
        PlayConfiguredAnimation_(
            MoveEvent.MoveType == ETerraPiecePresentationMoveType::Jump ? VisualConfig.JumpAnimation : VisualConfig.MoveAnimation,
            MoveEvent.MoveType != ETerraPiecePresentationMoveType::Jump);
    }

    SetActorTickEnabled(true);
}

void ATerraPieceActor::CancelPresentationMove(const FTerraPiecePresentationSnapshot& Snapshot, const FTerraPieceVisualConfig& VisualConfig)
{
    ApplyPresentationSnapshot(Snapshot, VisualConfig);
}

void ATerraPieceActor::FaceTowards(const FVector& TargetWorldLocation, float BlendSeconds)
{
    const FVector Position = GetActorLocation();
    const FVector DesiredFacingDirection = TargetWorldLocation - Position;
    if (DesiredFacingDirection.IsNearlyZero())
    {
        return;
    }

    ActiveFacingBlendStartRotation = GetActorQuat();
    ActiveFacingBlendTargetRotation = BuildRotationFromFacing_(Position, DesiredFacingDirection, GetActorQuat());
    if ((ActiveFacingBlendStartRotation | ActiveFacingBlendTargetRotation) < 0.0f)
    {
        ActiveFacingBlendTargetRotation = ActiveFacingBlendTargetRotation * -1.0f;
    }
    ActiveFacingBlendElapsedSeconds = 0.0f;
    ActiveFacingBlendDurationSeconds = FMath::Max(BlendSeconds, 0.001f);
    bHasActiveFacingBlend = true;
    SetActorTickEnabled(true);
}

void ATerraPieceActor::PlayPresentationOnlyMoveTo(const FTransform& TargetTransform, float DurationSeconds, UAnimationAsset* MoveAnimation)
{
    PlayPresentationOnlyMoveTo(TargetTransform, DurationSeconds, MoveAnimation, ETerraPiecePresentationForwardMode::MoveDirection);
}

void ATerraPieceActor::PlayPresentationOnlyMoveTo(
    const FTransform& TargetTransform,
    float DurationSeconds,
    UAnimationAsset* MoveAnimation,
    ETerraPiecePresentationForwardMode ForwardMode,
    const FVector& ForwardTarget)
{
    CachedVisualConfig.MoveDurationSeconds = FMath::Max(DurationSeconds, 0.001f);
    ActiveMoveEvent = FTerraPiecePresentationMoveEvent();
    ActiveMoveEvent.PieceId = PieceId;
    ActiveMoveEvent.FromCellId = CellId;
    ActiveMoveEvent.ToCellId = CellId;
    ActiveMoveEvent.MoveType = ETerraPiecePresentationMoveType::Move;
    ActiveMoveEvent.FromWorldTransform = GetActorTransform();
    ActiveMoveEvent.ToWorldTransform = TargetTransform;
    ActiveMoveElapsedSeconds = 0.0f;
    ActiveMoveDurationSeconds = FMath::Max(DurationSeconds, 0.001f);
    bHasActiveMove = true;
    bPresentationOnlyMove = true;
    PresentationOnlyMoveForwardMode = ForwardMode;
    PresentationOnlyMoveForwardTarget = ForwardTarget;

    SetActorTransform(ActiveMoveEvent.FromWorldTransform);

    ActiveMoveDesiredFacingDirection = ActiveMoveEvent.ToWorldTransform.GetLocation() - ActiveMoveEvent.FromWorldTransform.GetLocation();
    if (ActiveMoveDesiredFacingDirection.IsNearlyZero())
    {
        ActiveMoveDesiredFacingDirection = GetActorForwardVector();
    }
    else
    {
        ActiveMoveDesiredFacingDirection.Normalize();
    }

    FTerraPieceAnimDriveState NewDriveState;
    NewDriveState.ActionState = ETerraPieceAnimActionState::Move;
    NewDriveState.MoveSpeed = FVector::Distance(
        ActiveMoveEvent.FromWorldTransform.GetLocation(),
        ActiveMoveEvent.ToWorldTransform.GetLocation()) / ActiveMoveDurationSeconds;
    NewDriveState.NormalizedPhase = 0.0f;
    NewDriveState.FacingDirection = ActiveMoveDesiredFacingDirection;
    NewDriveState.bDead = false;
    SetDriveState_(NewDriveState);
    if (IsMountedCavalry_())
    {
        PlayAnimationOnMesh_(HorseMesh, CachedVisualConfig.HorseMoveAnimation, true, 0.0f);
        if (UTerraMountedRiderAnimInstance* RiderAnimInstance = GetMountedRiderAnimInstance_())
        {
            RiderAnimInstance->SetSittingAnimation(CachedVisualConfig.RiderSittingAnimation);
            RiderAnimInstance->EnterSitting();
        }
        else
        {
            PlayAnimationOnMesh_(RiderMesh, CachedVisualConfig.RiderSittingAnimation, true, 0.0f);
        }
    }
    else
    {
        PlayConfiguredAnimation_(MoveAnimation, true);
    }
    SetActorTickEnabled(true);
}

void ATerraPieceActor::PlayAttackAnimation(UAnimationAsset* AttackAnimation, float StartOffsetSeconds)
{
    PlayAttackAnimation(AttackAnimation, StartOffsetSeconds, 1.0f);
}

void ATerraPieceActor::PlayAttackAnimation(UAnimationAsset* AttackAnimation, float StartOffsetSeconds, float PlayRate)
{
    bHasActiveMove = false;
    bPresentationOnlyMove = false;
    PresentationOnlyMoveForwardMode = ETerraPiecePresentationForwardMode::MoveDirection;
    PresentationOnlyMoveForwardTarget = FVector::ZeroVector;
    FTerraPieceAnimDriveState NewDriveState = AnimDriveState;
    NewDriveState.ActionState = ETerraPieceAnimActionState::Attack;
    NewDriveState.MoveSpeed = 0.0f;
    NewDriveState.NormalizedPhase = 0.0f;
    NewDriveState.FacingDirection = GetActorForwardVector();
    NewDriveState.bDead = false;
    SetDriveState_(NewDriveState);
    if (IsMountedCavalry_())
    {
        ApplyMountedRiderSaddleTransform_();
        UAnimMontage* AttackMontage = PieceType == ETerraGameplayPieceType::ArcherCavalry
            ? CachedVisualConfig.ArcherCavalryUpperBodyAttackMontage.Get()
            : CachedVisualConfig.RiderUpperBodyAttackMontage.Get();
        const bool bMontagePlayed = PlayMountedRiderUpperBodyMontage_(AttackMontage, StartOffsetSeconds);
        if (!bMontagePlayed)
        {
            PlayAnimationOnMesh_(RiderMesh, AttackAnimation, false, StartOffsetSeconds, PlayRate);
        }
        PlayAnimationOnMesh_(HorseMesh, CachedVisualConfig.HorseIdleAnimation, true, 0.0f);
    }
    else
    {
        PlayAnimationOnMesh_(HumanMesh, AttackAnimation, false, StartOffsetSeconds, PlayRate);
    }
    SetActorTickEnabled(bHasActiveFacingBlend || bHasActiveDeathFade);
}

void ATerraPieceActor::PlayHitAnimation(UAnimationAsset* HitAnimation, float StartOffsetSeconds)
{
    bHasActiveMove = false;
    bPresentationOnlyMove = false;
    PresentationOnlyMoveForwardMode = ETerraPiecePresentationForwardMode::MoveDirection;
    PresentationOnlyMoveForwardTarget = FVector::ZeroVector;
    FTerraPieceAnimDriveState NewDriveState = AnimDriveState;
    NewDriveState.ActionState = ETerraPieceAnimActionState::Hit;
    NewDriveState.MoveSpeed = 0.0f;
    NewDriveState.NormalizedPhase = 0.0f;
    NewDriveState.FacingDirection = GetActorForwardVector();
    NewDriveState.bDead = false;
    SetDriveState_(NewDriveState);
    if (IsMountedCavalry_())
    {
        ApplyMountedRiderSaddleTransform_();
        if (!PlayMountedRiderUpperBodyMontage_(CachedVisualConfig.RiderUpperBodyHitMontage, StartOffsetSeconds))
        {
            PlayAnimationOnMesh_(RiderMesh, HitAnimation, false, StartOffsetSeconds);
        }
        PlayAnimationOnMesh_(HorseMesh, CachedVisualConfig.HorseIdleAnimation, true, 0.0f);
    }
    else
    {
        PlayConfiguredAnimation_(HitAnimation, false, StartOffsetSeconds);
    }
    SetActorTickEnabled(bHasActiveFacingBlend || bHasActiveDeathFade);
}

void ATerraPieceActor::PlayDeathAnimation(UAnimationAsset* DeathAnimation, float StartOffsetSeconds)
{
    bHasActiveMove = false;
    bPresentationOnlyMove = false;
    PresentationOnlyMoveForwardMode = ETerraPiecePresentationForwardMode::MoveDirection;
    PresentationOnlyMoveForwardTarget = FVector::ZeroVector;
    FTerraPieceAnimDriveState NewDriveState = AnimDriveState;
    NewDriveState.ActionState = ETerraPieceAnimActionState::Death;
    NewDriveState.MoveSpeed = 0.0f;
    NewDriveState.NormalizedPhase = 1.0f;
    NewDriveState.FacingDirection = GetActorForwardVector();
    NewDriveState.bDead = true;
    SetDriveState_(NewDriveState);
    if (IsMountedCavalry_())
    {
        ApplyMountedRiderDeathTransform_();
        if (UTerraMountedRiderAnimInstance* RiderAnimInstance = GetMountedRiderAnimInstance_())
        {
            RiderAnimInstance->EnterDeath();
        }
        // Mounted death uses SingleNode so the rider can play a full-body death animation on the ground.
        // Bypass PlayAnimationOnMesh_, which intentionally ignores MountedRider AnimBP instances.
        RiderMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        RiderMesh->PlayAnimation(DeathAnimation, false);
        if (StartOffsetSeconds > 0.0f)
        {
            RiderMesh->SetPosition(FMath::Max(StartOffsetSeconds, 0.0f), false);
        }
        HorseMesh->SetHiddenInGame(true);
        HorseMesh->SetVisibility(false);
    }
    else
    {
        PlayConfiguredAnimation_(DeathAnimation, false, StartOffsetSeconds);
    }
    SetActorTickEnabled(bHasActiveFacingBlend || bHasActiveDeathFade);
}

void ATerraPieceActor::StartDeathFade(float FadeSeconds)
{
    ActiveDeathFadeElapsedSeconds = 0.0f;
    ActiveDeathFadeDurationSeconds = FMath::Max(FadeSeconds, 0.001f);
    DeathFadeStartScale = GetActorScale3D();
    bHasActiveDeathFade = true;
    SetActorTickEnabled(true);
}

void ATerraPieceActor::ReturnToIdlePresentation()
{
    bHasActiveMove = false;
    bPresentationOnlyMove = false;
    PresentationOnlyMoveForwardMode = ETerraPiecePresentationForwardMode::MoveDirection;
    PresentationOnlyMoveForwardTarget = FVector::ZeroVector;

    FTerraPieceAnimDriveState NewDriveState = AnimDriveState;
    NewDriveState.ActionState = ETerraPieceAnimActionState::Idle;
    NewDriveState.MoveSpeed = 0.0f;
    NewDriveState.NormalizedPhase = 0.0f;
    NewDriveState.FacingDirection = GetActorForwardVector();
    NewDriveState.bDead = false;
    SetDriveState_(NewDriveState);

    if (IsMountedCavalry_())
    {
        PlayMountedIdleAnimations_();
    }
    else
    {
        PlayConfiguredAnimation_(CachedVisualConfig.IdleAnimation, true);
    }

    SetActorTickEnabled(bHasActiveFacingBlend || bHasActiveDeathFade);
}

FTransform ATerraPieceActor::ResolveAttachmentWorldTransform(FName AttachName) const
{
    USkeletalMeshComponent* SourceMesh = IsMountedCavalry_() ? RiderMesh.Get() : HumanMesh.Get();
    if (!SourceMesh)
    {
        return GetActorTransform();
    }

    FTransform AttachTransform = SourceMesh->GetComponentTransform();
    if (!AttachName.IsNone())
    {
        const bool bHasSocket = SourceMesh->DoesSocketExist(AttachName);
        const bool bHasBone = SourceMesh->GetBoneIndex(AttachName) != INDEX_NONE;
        if (bHasSocket || bHasBone)
        {
            AttachTransform = SourceMesh->GetSocketTransform(AttachName, RTS_World);
        }
    }

    return AttachTransform;
}

FVector ATerraPieceActor::ResolveAttachmentWorldLocation(FName AttachName, const FVector& RelativeLocation) const
{
    return ResolveAttachmentWorldTransform(AttachName).TransformPosition(RelativeLocation);
}

void ATerraPieceActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, 0.0f);

    if (bHasActiveFacingBlend)
    {
        ActiveFacingBlendElapsedSeconds += SafeDeltaSeconds;
        const float Alpha = FMath::Clamp(ActiveFacingBlendElapsedSeconds / FMath::Max(ActiveFacingBlendDurationSeconds, 0.001f), 0.0f, 1.0f);
        const FQuat Rotation = FQuat::Slerp(ActiveFacingBlendStartRotation, ActiveFacingBlendTargetRotation, Alpha).GetNormalized();
        SetActorRotation(Rotation);
        if (Alpha >= 1.0f - KINDA_SMALL_NUMBER)
        {
            bHasActiveFacingBlend = false;
        }
    }

    if (bHasActiveMove)
    {
        ActiveMoveElapsedSeconds += SafeDeltaSeconds;
        const float Alpha = FMath::Clamp(ActiveMoveElapsedSeconds / FMath::Max(ActiveMoveDurationSeconds, 0.001f), 0.0f, 1.0f);

        const FVector Position = EvaluateActiveMovePosition_(Alpha, CachedVisualConfig);
        FQuat DesiredRotation;
        if (bPresentationOnlyMove)
        {
            DesiredRotation = BuildPresentationOnlyMoveRotation_(Position);
        }
        else
        {
            DesiredRotation = BuildRotationFromFacing_(Position, ActiveMoveDesiredFacingDirection, ActiveMoveEvent.ToWorldTransform.GetRotation());
        }
        const FQuat CurrentRotation = GetActorQuat();
        if ((CurrentRotation | DesiredRotation) < 0.0f)
        {
            DesiredRotation = DesiredRotation * -1.0f;
        }
        const float RotationAlpha = FMath::Clamp(SafeDeltaSeconds * 12.0f, 0.0f, 1.0f);
        const FQuat Rotation = FQuat::Slerp(CurrentRotation, DesiredRotation, RotationAlpha).GetNormalized();
        SetActorTransform(FTransform(Rotation, Position, GetActorScale3D()));

        FTerraPieceAnimDriveState NewDriveState = AnimDriveState;
        NewDriveState.NormalizedPhase = Alpha;
        NewDriveState.FacingDirection = GetActorForwardVector();
        SetDriveState_(NewDriveState);

        if (Alpha >= 1.0f - KINDA_SMALL_NUMBER)
        {
            FinishActiveMove_();
        }
    }

    if (bHasActiveDeathFade)
    {
        ActiveDeathFadeElapsedSeconds += SafeDeltaSeconds;
        const float Alpha = FMath::Clamp(ActiveDeathFadeElapsedSeconds / FMath::Max(ActiveDeathFadeDurationSeconds, 0.001f), 0.0f, 1.0f);
        const float ScaleAlpha = 1.0f - Alpha;
        SetActorScale3D(DeathFadeStartScale * ScaleAlpha);
        if (Alpha >= 1.0f - KINDA_SMALL_NUMBER)
        {
            bHasActiveDeathFade = false;
            SetActorHiddenInGame(true);
        }
    }

    if (!bHasActiveMove && !bHasActiveFacingBlend && !bHasActiveDeathFade)
    {
        SetActorTickEnabled(false);
    }
}

void ATerraPieceActor::ApplyVisualConfig_(const FTerraPieceVisualConfig& VisualConfig)
{
    if (!HumanMesh)
    {
        return;
    }

    CachedVisualConfig = VisualConfig;
    HumanMesh->SetSkeletalMesh(VisualConfig.ResolveMesh(PieceType));

    if (IsMountedCavalry_())
    {
        HumanMesh->SetHiddenInGame(true);
        HumanMesh->SetVisibility(false);

        if (HorseMesh)
        {
            HorseMesh->SetSkeletalMesh(VisualConfig.HorseMesh);
            HorseMesh->SetRelativeLocation(VisualConfig.HorseRelativeLocation);
            HorseMesh->SetRelativeRotation(VisualConfig.HorseRelativeRotation);
            HorseMesh->SetRelativeScale3D(FVector(FMath::Max(VisualConfig.HorseUniformScale, 0.001f)));
            HorseMesh->SetHiddenInGame(false);
            HorseMesh->SetVisibility(true);
        }

        if (RiderAnchor)
        {
            ApplyMountedRiderSaddleTransform_();
        }

        if (RiderMesh)
        {
            RiderMesh->SetSkeletalMesh(VisualConfig.ResolveMesh(PieceType));
            if (VisualConfig.RiderAnimInstanceClass)
            {
                RiderMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
                RiderMesh->SetAnimInstanceClass(VisualConfig.RiderAnimInstanceClass);
                if (UTerraMountedRiderAnimInstance* RiderAnimInstance = GetMountedRiderAnimInstance_())
                {
                    RiderAnimInstance->SetSittingAnimation(VisualConfig.RiderSittingAnimation);
                }
            }
            else
            {
                RiderMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
                RiderMesh->SetAnimInstanceClass(nullptr);
            }
            RiderMesh->SetRelativeLocation(FVector::ZeroVector);
            RiderMesh->SetRelativeRotation(VisualConfig.MeshRelativeRotation);
            RiderMesh->SetRelativeScale3D(FVector(FMath::Max(VisualConfig.UniformScale, 0.001f)));
            RiderMesh->SetHiddenInGame(false);
            RiderMesh->SetVisibility(true);
        }
    }
    else
    {
        HumanMesh->SetRelativeLocation(VisualConfig.MeshRelativeLocation);
        HumanMesh->SetRelativeRotation(VisualConfig.MeshRelativeRotation);
        HumanMesh->SetRelativeScale3D(FVector(FMath::Max(VisualConfig.UniformScale, 0.001f)));
        HumanMesh->SetHiddenInGame(false);
        HumanMesh->SetVisibility(true);

        if (HorseMesh)
        {
            HorseMesh->SetHiddenInGame(true);
            HorseMesh->SetVisibility(false);
        }
        if (RiderMesh)
        {
            RiderMesh->SetHiddenInGame(true);
            RiderMesh->SetVisibility(false);
        }
    }

    ApplyP7FactionMaterials_(VisualConfig);
    ApplyWeaponAttachments_();
    SetActorHiddenInGame(false);
    SetActorScale3D(FVector::OneVector);
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
    PlayConfiguredAnimation_(AnimationAsset, bLooping, 0.0f);
}

void ATerraPieceActor::PlayConfiguredAnimation_(UAnimationAsset* AnimationAsset, bool bLooping, float StartOffsetSeconds)
{
    PlayAnimationOnMesh_(HumanMesh, AnimationAsset, bLooping, StartOffsetSeconds);
}

void ATerraPieceActor::PlayAnimationOnMesh_(USkeletalMeshComponent* MeshComponent, UAnimationAsset* AnimationAsset, bool bLooping, float StartOffsetSeconds, float PlayRate)
{
    if (!MeshComponent || !AnimationAsset)
    {
        return;
    }

    if (Cast<UTerraPieceAnimInstance>(MeshComponent->GetAnimInstance()))
    {
        return;
    }

    // 防御性守卫：若 Mesh 上正在使用 UTerraMountedRiderAnimInstance（骑手上半身分层 AnimBP），
    // 直接调用 PlayAnimation 会强制切成 AnimationSingleNode 模式并把 AnimBP 踢掉。
    // 这里拒绝该调用，坐姿等 Base Pose 应通过 UTerraMountedRiderAnimInstance::EnterSitting 内部驱动。
    if (Cast<UTerraMountedRiderAnimInstance>(MeshComponent->GetAnimInstance()))
    {
        return;
    }

    MeshComponent->PlayAnimation(AnimationAsset, bLooping);
    MeshComponent->SetPlayRate(FMath::Max(PlayRate, 0.001f));
    if (StartOffsetSeconds > 0.0f)
    {
        MeshComponent->SetPosition(FMath::Max(StartOffsetSeconds, 0.0f), false);
    }
}

void ATerraPieceActor::PlayMountedMoveAnimation_(ETerraPiecePresentationMoveType MoveType)
{
    ApplyMountedRiderSaddleTransform_();

    if (MoveType == ETerraPiecePresentationMoveType::Jump)
    {
        float HorseJumpPlayRate = 1.0f;
        if (CachedVisualConfig.HorseJumpAnimation && ActiveMoveDurationSeconds > KINDA_SMALL_NUMBER)
        {
            HorseJumpPlayRate = CachedVisualConfig.HorseJumpAnimation->GetPlayLength()
                / ActiveMoveDurationSeconds
                * FMath::Max(CachedVisualConfig.HorseJumpPlayRateScale, 0.001f);
        }

        PlayAnimationOnMesh_(HorseMesh, CachedVisualConfig.HorseJumpAnimation, false, 0.0f, HorseJumpPlayRate);
    }
    else
    {
        PlayAnimationOnMesh_(HorseMesh, CachedVisualConfig.HorseMoveAnimation, true, 0.0f);
    }

    if (UTerraMountedRiderAnimInstance* RiderAnimInstance = GetMountedRiderAnimInstance_())
    {
        RiderAnimInstance->SetSittingAnimation(CachedVisualConfig.RiderSittingAnimation);
        RiderAnimInstance->EnterSitting();
    }
    else
    {
        PlayAnimationOnMesh_(RiderMesh, CachedVisualConfig.RiderSittingAnimation, true, 0.0f);
    }
}

void ATerraPieceActor::PlayMountedIdleAnimations_()
{
    ApplyMountedRiderSaddleTransform_();
    PlayAnimationOnMesh_(HorseMesh, CachedVisualConfig.HorseIdleAnimation, true, 0.0f);
    if (UTerraMountedRiderAnimInstance* RiderAnimInstance = GetMountedRiderAnimInstance_())
    {
        RiderAnimInstance->SetSittingAnimation(CachedVisualConfig.RiderSittingAnimation);
        RiderAnimInstance->EnterSitting();
    }
    else
    {
        PlayAnimationOnMesh_(RiderMesh, CachedVisualConfig.RiderSittingAnimation, true, 0.0f);
    }
}

bool ATerraPieceActor::PlayMountedRiderUpperBodyMontage_(UAnimMontage* Montage, float StartOffsetSeconds)
{
    UTerraMountedRiderAnimInstance* RiderAnimInstance = GetMountedRiderAnimInstance_();
    if (!RiderAnimInstance || !Montage)
    {
        return false;
    }

    RiderAnimInstance->SetSittingAnimation(CachedVisualConfig.RiderSittingAnimation);
    return RiderAnimInstance->PlayUpperBodyMontage(Montage, StartOffsetSeconds) > 0.0f;
}

UTerraMountedRiderAnimInstance* ATerraPieceActor::GetMountedRiderAnimInstance_() const
{
    if (!RiderMesh)
    {
        return nullptr;
    }
    UAnimInstance* RawAnimInstance = RiderMesh->GetAnimInstance();
    UTerraMountedRiderAnimInstance* CastResult = Cast<UTerraMountedRiderAnimInstance>(RawAnimInstance);
    return CastResult;
}

void ATerraPieceActor::AttachMountedRiderToSaddle_()
{
    if (!RiderAnchor || !HorseMesh)
    {
        return;
    }

    FName AttachName = CachedVisualConfig.RiderSaddleAttachName;
    if (AttachName.IsNone())
    {
        AttachName = TEXT("Torso");
    }

    const bool bHasSocket = HorseMesh->DoesSocketExist(AttachName);
    const bool bHasBone = HorseMesh->GetBoneIndex(AttachName) != INDEX_NONE;
    const FName SocketOrBoneName = (bHasSocket || bHasBone) ? AttachName : NAME_None;
    if (SocketOrBoneName.IsNone() && !AttachName.IsNone() && LastMissingRiderSaddleAttachName != AttachName)
    {
        LastMissingRiderSaddleAttachName = AttachName;
        UE_LOG(LogTerraPieceActor, Warning,
            TEXT("[PiecePresentation][P4.5] Rider saddle attach target not found. PieceId=%d AttachName=%s HorseMesh=%s"),
            PieceId,
            *AttachName.ToString(),
            *GetNameSafe(HorseMesh->GetSkeletalMeshAsset()));
    }
    else if (!SocketOrBoneName.IsNone())
    {
        LastMissingRiderSaddleAttachName = NAME_None;
    }

    if (RiderAnchor->GetAttachParent() != HorseMesh || RiderAnchor->GetAttachSocketName() != SocketOrBoneName)
    {
        RiderAnchor->AttachToComponent(
            HorseMesh,
            FAttachmentTransformRules::KeepRelativeTransform,
            SocketOrBoneName);
    }
}

void ATerraPieceActor::AttachMountedRiderToRootForDeath_()
{
    if (!RiderAnchor || !RootScene)
    {
        return;
    }

    if (RiderAnchor->GetAttachParent() != RootScene)
    {
        RiderAnchor->AttachToComponent(
            RootScene,
            FAttachmentTransformRules::KeepRelativeTransform);
    }
}

void ATerraPieceActor::ApplyMountedRiderSaddleTransform_()
{
    if (!RiderAnchor)
    {
        return;
    }

    AttachMountedRiderToSaddle_();
    RiderAnchor->SetRelativeLocation(CachedVisualConfig.RiderRelativeLocation);
    RiderAnchor->SetRelativeRotation(CachedVisualConfig.RiderRelativeRotation);
    RiderAnchor->SetRelativeScale3D(FVector(FMath::Max(CachedVisualConfig.RiderUniformScale, 0.001f)));
}

void ATerraPieceActor::ApplyMountedRiderDeathTransform_()
{
    if (!RiderAnchor)
    {
        return;
    }

    AttachMountedRiderToRootForDeath_();
    RiderAnchor->SetRelativeLocation(CachedVisualConfig.RiderDeathRelativeLocation);
    RiderAnchor->SetRelativeRotation(CachedVisualConfig.RiderDeathRelativeRotation);
    RiderAnchor->SetRelativeScale3D(FVector(FMath::Max(CachedVisualConfig.RiderUniformScale, 0.001f)));
}

void ATerraPieceActor::ApplyWeaponAttachments_()
{
    if (!WeaponPrimaryMesh || !WeaponSecondaryMesh)
    {
        return;
    }

    HideWeaponComponent_(WeaponPrimaryMesh);
    HideWeaponComponent_(WeaponSecondaryMesh);

    switch (PieceType)
    {
    case ETerraGameplayPieceType::Commander:
        break;
    case ETerraGameplayPieceType::Archer:
        ConfigureWeaponComponent_(
            WeaponPrimaryMesh,
            HumanMesh,
            CachedVisualConfig.ArcherBowMesh,
            CachedVisualConfig.ArcherBowAttachName,
            CachedVisualConfig.ArcherBowRelativeLocation,
            CachedVisualConfig.ArcherBowRelativeRotation,
            CachedVisualConfig.ArcherBowUniformScale);
        break;
    case ETerraGameplayPieceType::ArcherCavalry:
        ConfigureWeaponComponent_(
            WeaponPrimaryMesh,
            RiderMesh,
            CachedVisualConfig.ArcherBowMesh,
            CachedVisualConfig.ArcherBowAttachName,
            CachedVisualConfig.ArcherBowRelativeLocation,
            CachedVisualConfig.ArcherBowRelativeRotation,
            CachedVisualConfig.ArcherBowUniformScale);
        break;
    case ETerraGameplayPieceType::Infantry:
        ConfigureWeaponComponent_(
            WeaponPrimaryMesh,
            HumanMesh,
            CachedVisualConfig.InfantrySwordMesh,
            CachedVisualConfig.InfantrySwordAttachName,
            CachedVisualConfig.InfantrySwordRelativeLocation,
            CachedVisualConfig.InfantrySwordRelativeRotation,
            CachedVisualConfig.InfantrySwordUniformScale);
        ConfigureWeaponComponent_(
            WeaponSecondaryMesh,
            HumanMesh,
            CachedVisualConfig.InfantryShieldMesh,
            CachedVisualConfig.InfantryShieldAttachName,
            CachedVisualConfig.InfantryShieldRelativeLocation,
            CachedVisualConfig.InfantryShieldRelativeRotation,
            CachedVisualConfig.InfantryShieldUniformScale);
        break;
    case ETerraGameplayPieceType::Cavalry:
        ConfigureWeaponComponent_(
            WeaponPrimaryMesh,
            RiderMesh,
            CachedVisualConfig.CavalryAxeMesh,
            CachedVisualConfig.CavalryAxeAttachName,
            CachedVisualConfig.CavalryAxeRelativeLocation,
            CachedVisualConfig.CavalryAxeRelativeRotation,
            CachedVisualConfig.CavalryAxeUniformScale);
        break;
    default:
        break;
    }
}

void ATerraPieceActor::ApplyP7FactionMaterials_(const FTerraPieceVisualConfig& VisualConfig)
{
    UMaterialInterface* ParentMaterial = VisualConfig.P7PaletteReplaceMaterial;
    const FTerraPieceFactionPalette* Palette = VisualConfig.ResolveP7FactionPalette(OwnerFactionId);
    const FTerraPiecePaletteMask* Mask = VisualConfig.ResolveP7PaletteMask(PieceType);

    if (!ParentMaterial || !Palette || !Mask)
    {
        P7HumanMID = nullptr;
        P7RiderMID = nullptr;
        if (HumanMesh)
        {
            HumanMesh->SetMaterial(0, nullptr);
        }
        if (RiderMesh)
        {
            RiderMesh->SetMaterial(0, nullptr);
        }
        return;
    }

    if (OwnerFactionId != INDEX_NONE && !VisualConfig.P7FactionPalettes.IsValidIndex(OwnerFactionId))
    {
        UE_LOG(LogTerraPieceActor, Warning,
            TEXT("[PiecePresentation][P7] OwnerFactionId out of range; using fallback palette. PieceId=%d OwnerFactionId=%d PaletteCount=%d"),
            PieceId,
            OwnerFactionId,
            VisualConfig.P7FactionPalettes.Num());
    }

    if (IsMountedCavalry_())
    {
        P7HumanMID = nullptr;
        ApplyP7MaterialToMesh_(
            RiderMesh,
            VisualConfig.ResolveP7BaseTexture(PieceType),
            *Mask,
            *Palette,
            ParentMaterial,
            P7RiderMID);
    }
    else
    {
        P7RiderMID = nullptr;
        ApplyP7MaterialToMesh_(
            HumanMesh,
            VisualConfig.ResolveP7BaseTexture(PieceType),
            *Mask,
            *Palette,
            ParentMaterial,
            P7HumanMID);
    }
}

void ATerraPieceActor::ApplyP7MaterialToMesh_(
    USkeletalMeshComponent* MeshComponent,
    UTexture2D* BaseTexture,
    const FTerraPiecePaletteMask& Mask,
    const FTerraPieceFactionPalette& Palette,
    UMaterialInterface* ParentMaterial,
    TObjectPtr<UMaterialInstanceDynamic>& InOutMID)
{
    if (!MeshComponent || !ParentMaterial || !BaseTexture)
    {
        InOutMID = nullptr;
        if (MeshComponent)
        {
            MeshComponent->SetMaterial(0, nullptr);
        }
        return;
    }

    InOutMID = UMaterialInstanceDynamic::Create(ParentMaterial, this);
    if (!InOutMID)
    {
        return;
    }

    InOutMID->SetTextureParameterValue(TEXT("BaseColorTexture"), BaseTexture);
    InOutMID->SetScalarParameterValue(TEXT("PaletteGridColumns"), 8.0f);
    InOutMID->SetScalarParameterValue(TEXT("PaletteGridRows"), 4.0f);
    InOutMID->SetVectorParameterValue(TEXT("FactionPrimaryColor"), Palette.PrimaryColor);
    InOutMID->SetVectorParameterValue(TEXT("FactionSecondaryColor"), Palette.SecondaryColor);
    InOutMID->SetScalarParameterValue(TEXT("FactionColorStrength"), FMath::Max(Palette.ColorStrength, 0.0f));
    InOutMID->SetScalarParameterValue(TEXT("PreserveValueStrength"), 1.0f);
    InOutMID->SetScalarParameterValue(TEXT("MinSaturationToReplace"), FMath::Clamp(Mask.MinSaturationToReplace, 0.0f, 1.0f));

    const FColor PrimaryMaskColor = PackP7TileMaskRGBA8(Mask.PrimaryTiles);
    const FColor SecondaryMaskColor = PackP7TileMaskRGBA8(Mask.SecondaryTiles);
    InOutMID->SetVectorParameterValue(TEXT("PrimaryTileMaskRGBA8"), NormalizeP7TileMaskColor(PrimaryMaskColor));
    InOutMID->SetVectorParameterValue(TEXT("SecondaryTileMaskRGBA8"), NormalizeP7TileMaskColor(SecondaryMaskColor));

    MeshComponent->SetMaterial(0, InOutMID);
}

void ATerraPieceActor::HideWeaponComponent_(UStaticMeshComponent* WeaponComponent)
{
    if (!WeaponComponent)
    {
        return;
    }

    WeaponComponent->SetStaticMesh(nullptr);
    if (WeaponComponent->GetAttachParent() != RootScene)
    {
        WeaponComponent->AttachToComponent(RootScene, FAttachmentTransformRules::KeepRelativeTransform);
    }
    WeaponComponent->SetRelativeTransform(FTransform::Identity);
    WeaponComponent->SetHiddenInGame(true);
    WeaponComponent->SetVisibility(false);
}

void ATerraPieceActor::ConfigureWeaponComponent_(
    UStaticMeshComponent* WeaponComponent,
    USkeletalMeshComponent* ParentMesh,
    UStaticMesh* StaticMesh,
    FName AttachName,
    const FVector& RelativeLocation,
    const FRotator& RelativeRotation,
    float UniformScale)
{
    if (!WeaponComponent || !ParentMesh || !StaticMesh)
    {
        return;
    }

    const FName SocketOrBoneName = ResolveWeaponAttachName_(ParentMesh, AttachName);
    if (WeaponComponent->GetAttachParent() != ParentMesh || WeaponComponent->GetAttachSocketName() != SocketOrBoneName)
    {
        WeaponComponent->AttachToComponent(
            ParentMesh,
            FAttachmentTransformRules::KeepRelativeTransform,
            SocketOrBoneName);
    }

    WeaponComponent->SetStaticMesh(StaticMesh);
    WeaponComponent->SetRelativeLocation(RelativeLocation);
    WeaponComponent->SetRelativeRotation(RelativeRotation);
    WeaponComponent->SetRelativeScale3D(FVector(FMath::Max(UniformScale, 0.001f)));
    WeaponComponent->SetHiddenInGame(false);
    WeaponComponent->SetVisibility(true);
}

FName ATerraPieceActor::ResolveWeaponAttachName_(USkeletalMeshComponent* ParentMesh, FName AttachName)
{
    if (!ParentMesh || AttachName.IsNone())
    {
        return NAME_None;
    }

    const bool bHasSocket = ParentMesh->DoesSocketExist(AttachName);
    const bool bHasBone = ParentMesh->GetBoneIndex(AttachName) != INDEX_NONE;
    if (bHasSocket || bHasBone)
    {
        LastMissingWeaponAttachName = NAME_None;
        return AttachName;
    }

    if (LastMissingWeaponAttachName != AttachName)
    {
        LastMissingWeaponAttachName = AttachName;
        UE_LOG(LogTerraPieceActor, Warning,
            TEXT("[PiecePresentation][P5] Weapon attach target not found. PieceId=%d PieceType=%d AttachName=%s ParentMesh=%s"),
            PieceId,
            static_cast<int32>(PieceType),
            *AttachName.ToString(),
            *GetNameSafe(ParentMesh->GetSkeletalMeshAsset()));
    }

    return NAME_None;
}

void ATerraPieceActor::FinishActiveMove_()
{
    const FVector TargetPosition = EvaluateActiveMovePosition_(1.0f, CachedVisualConfig);
    const FQuat TargetRotation = bPresentationOnlyMove
        ? BuildPresentationOnlyMoveRotation_(TargetPosition)
        : BuildRotationFromFacing_(TargetPosition, ActiveMoveDesiredFacingDirection, ActiveMoveEvent.ToWorldTransform.GetRotation());
    const FTransform TargetTransform(TargetRotation, TargetPosition, FVector::OneVector);

    bHasActiveMove = false;
    bPresentationOnlyMove = false;
    PresentationOnlyMoveForwardMode = ETerraPiecePresentationForwardMode::MoveDirection;
    PresentationOnlyMoveForwardTarget = FVector::ZeroVector;
    ActiveMoveElapsedSeconds = 0.0f;
    ActiveMoveDurationSeconds = 0.0f;

    SetActorTransform(TargetTransform);

    FTerraPieceAnimDriveState NewDriveState;
    NewDriveState.ActionState = ETerraPieceAnimActionState::Idle;
    NewDriveState.MoveSpeed = 0.0f;
    NewDriveState.NormalizedPhase = 0.0f;
    NewDriveState.FacingDirection = GetActorForwardVector();
    NewDriveState.bDead = false;
    SetDriveState_(NewDriveState);
    if (IsMountedCavalry_())
    {
        PlayMountedIdleAnimations_();
    }
    else
    {
        PlayConfiguredAnimation_(CachedVisualConfig.IdleAnimation, true);
    }
    SetActorTickEnabled(bHasActiveFacingBlend || bHasActiveDeathFade);
}

FVector ATerraPieceActor::EvaluateActiveMovePosition_(float Alpha, const FTerraPieceVisualConfig& VisualConfig) const
{
    const FVector From = ActiveMoveEvent.FromWorldTransform.GetLocation();
    const FVector To = ActiveMoveEvent.ToWorldTransform.GetLocation();
    const FVector Center = GetPiecePresentationCenter(this);

    const float ExtraHeightCM = ActiveMoveEvent.MoveType == ETerraPiecePresentationMoveType::Jump
        && !bPresentationOnlyMove
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

FQuat ATerraPieceActor::BuildPresentationOnlyMoveRotation_(const FVector& WorldPosition) const
{
    const FVector Center = GetPiecePresentationCenter(this);
    FVector Up = (WorldPosition - Center).GetSafeNormal();
    if (Up.IsNearlyZero())
    {
        Up = ActiveMoveEvent.ToWorldTransform.GetRotation().GetUpVector();
    }

    FVector Forward = FVector::VectorPlaneProject(ActiveMoveDesiredFacingDirection, Up).GetSafeNormal();
    switch (PresentationOnlyMoveForwardMode)
    {
    case ETerraPiecePresentationForwardMode::MoveDirection:
        Forward = FVector::VectorPlaneProject(ActiveMoveDesiredFacingDirection, Up).GetSafeNormal();
        break;
    case ETerraPiecePresentationForwardMode::FaceTarget:
    {
        const FVector FaceDirection = (PresentationOnlyMoveForwardTarget - WorldPosition);
        if (!FaceDirection.IsNearlyZero())
        {
            Forward = FVector::VectorPlaneProject(FaceDirection, Up).GetSafeNormal();
        }
        break;
    }
    case ETerraPiecePresentationForwardMode::LockSource:
        Forward = FVector::VectorPlaneProject(ActiveMoveEvent.FromWorldTransform.GetRotation().GetForwardVector(), Up).GetSafeNormal();
        break;
    }

    if (Forward.IsNearlyZero())
    {
        Forward = FVector::VectorPlaneProject(ActiveMoveEvent.FromWorldTransform.GetRotation().GetForwardVector(), Up).GetSafeNormal();
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

bool ATerraPieceActor::IsMountedCavalry_() const
{
    return PieceType == ETerraGameplayPieceType::Cavalry
        || PieceType == ETerraGameplayPieceType::ArcherCavalry;
}
