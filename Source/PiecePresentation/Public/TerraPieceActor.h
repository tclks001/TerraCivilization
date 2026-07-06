#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerraPiecePresentationTypes.h"
#include "TerraPieceActor.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UAnimMontage;
class UTerraMountedRiderAnimInstance;

UCLASS()
class PIECEPRESENTATION_API ATerraPieceActor : public AActor
{
    GENERATED_BODY()

public:
    ATerraPieceActor();

    void ApplyPresentationSnapshot(const FTerraPiecePresentationSnapshot& Snapshot, const FTerraPieceVisualConfig& VisualConfig);
    void PlayPresentationMove(const FTerraPiecePresentationSnapshot& TargetSnapshot, const FTerraPiecePresentationMoveEvent& MoveEvent, const FTerraPieceVisualConfig& VisualConfig);
    void CancelPresentationMove(const FTerraPiecePresentationSnapshot& Snapshot, const FTerraPieceVisualConfig& VisualConfig);
    void FaceTowards(const FVector& TargetWorldLocation, float BlendSeconds);
    void PlayPresentationOnlyMoveTo(const FTransform& TargetTransform, float DurationSeconds, UAnimationAsset* MoveAnimation);
    void PlayPresentationOnlyMoveTo(const FTransform& TargetTransform, float DurationSeconds, UAnimationAsset* MoveAnimation, ETerraPiecePresentationForwardMode ForwardMode, const FVector& ForwardTarget = FVector::ZeroVector);
    void PlayAttackAnimation(UAnimationAsset* AttackAnimation, float StartOffsetSeconds);
    void PlayHitAnimation(UAnimationAsset* HitAnimation, float StartOffsetSeconds);
    void PlayDeathAnimation(UAnimationAsset* DeathAnimation, float StartOffsetSeconds);
    void StartDeathFade(float FadeSeconds);

    int32 GetPieceId() const { return PieceId; }
    int32 GetCellId() const { return CellId; }
    ETerraGameplayPieceType GetPieceType() const { return PieceType; }
    const FTerraPieceAnimDriveState& GetAnimDriveState() const { return AnimDriveState; }
    bool IsPresentationMoveActive() const { return bHasActiveMove; }

protected:
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terra|Piece Presentation")
    TObjectPtr<USceneComponent> RootScene;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terra|Piece Presentation")
    TObjectPtr<USkeletalMeshComponent> HumanMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terra|Piece Presentation|P4.1")
    TObjectPtr<USkeletalMeshComponent> HorseMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terra|Piece Presentation|P4.1")
    TObjectPtr<USceneComponent> RiderAnchor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terra|Piece Presentation|P4.1")
    TObjectPtr<USkeletalMeshComponent> RiderMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terra|Piece Presentation|P5")
    TObjectPtr<UStaticMeshComponent> WeaponPrimaryMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terra|Piece Presentation|P5")
    TObjectPtr<UStaticMeshComponent> WeaponSecondaryMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terra|Piece Presentation")
    int32 PieceId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terra|Piece Presentation")
    int32 OwnerFactionId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terra|Piece Presentation")
    int32 CellId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terra|Piece Presentation")
    ETerraGameplayPieceType PieceType = ETerraGameplayPieceType::Infantry;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terra|Piece Presentation")
    FTerraPieceAnimDriveState AnimDriveState;

private:
    void ApplyVisualConfig_(const FTerraPieceVisualConfig& VisualConfig);
    void SetDriveState_(const FTerraPieceAnimDriveState& NewDriveState);
    void PlayConfiguredAnimation_(UAnimationAsset* AnimationAsset, bool bLooping);
    void PlayConfiguredAnimation_(UAnimationAsset* AnimationAsset, bool bLooping, float StartOffsetSeconds);
    void PlayAnimationOnMesh_(USkeletalMeshComponent* MeshComponent, UAnimationAsset* AnimationAsset, bool bLooping, float StartOffsetSeconds, float PlayRate = 1.0f);
    void PlayMountedMoveAnimation_(ETerraPiecePresentationMoveType MoveType);
    void PlayMountedIdleAnimations_();
    bool PlayMountedRiderUpperBodyMontage_(UAnimMontage* Montage, float StartOffsetSeconds);
    UTerraMountedRiderAnimInstance* GetMountedRiderAnimInstance_() const;
    void AttachMountedRiderToSaddle_();
    void AttachMountedRiderToRootForDeath_();
    void ApplyMountedRiderSaddleTransform_();
    void ApplyMountedRiderDeathTransform_();
    void ApplyWeaponAttachments_();
    void HideWeaponComponent_(UStaticMeshComponent* WeaponComponent);
    void ConfigureWeaponComponent_(UStaticMeshComponent* WeaponComponent, USkeletalMeshComponent* ParentMesh, UStaticMesh* StaticMesh, FName AttachName, const FVector& RelativeLocation, const FRotator& RelativeRotation, float UniformScale);
    FName ResolveWeaponAttachName_(USkeletalMeshComponent* ParentMesh, FName AttachName);
    void FinishActiveMove_();
    FVector EvaluateActiveMovePosition_(float Alpha, const FTerraPieceVisualConfig& VisualConfig) const;
    FQuat BuildRotationFromFacing_(const FVector& WorldPosition, const FVector& DesiredFacingDirection, const FQuat& FallbackRotation) const;
    FQuat BuildPresentationOnlyMoveRotation_(const FVector& WorldPosition) const;
    bool IsMountedCavalry_() const;

private:
    UPROPERTY(Transient)
    FTerraPiecePresentationMoveEvent ActiveMoveEvent;

    UPROPERTY(Transient)
    bool bHasActiveMove = false;

    UPROPERTY(Transient)
    float ActiveMoveElapsedSeconds = 0.0f;

    UPROPERTY(Transient)
    float ActiveMoveDurationSeconds = 0.0f;

    UPROPERTY(Transient)
    FTerraPieceVisualConfig CachedVisualConfig;

    UPROPERTY(Transient)
    FVector ActiveMoveDesiredFacingDirection = FVector::ForwardVector;

    UPROPERTY(Transient)
    bool bPresentationOnlyMove = false;

    UPROPERTY(Transient)
    ETerraPiecePresentationForwardMode PresentationOnlyMoveForwardMode = ETerraPiecePresentationForwardMode::MoveDirection;

    UPROPERTY(Transient)
    FVector PresentationOnlyMoveForwardTarget = FVector::ZeroVector;

    UPROPERTY(Transient)
    bool bHasActiveFacingBlend = false;

    UPROPERTY(Transient)
    float ActiveFacingBlendElapsedSeconds = 0.0f;

    UPROPERTY(Transient)
    float ActiveFacingBlendDurationSeconds = 0.0f;

    UPROPERTY(Transient)
    FQuat ActiveFacingBlendStartRotation = FQuat::Identity;

    UPROPERTY(Transient)
    FQuat ActiveFacingBlendTargetRotation = FQuat::Identity;

    UPROPERTY(Transient)
    bool bHasActiveDeathFade = false;

    UPROPERTY(Transient)
    float ActiveDeathFadeElapsedSeconds = 0.0f;

    UPROPERTY(Transient)
    float ActiveDeathFadeDurationSeconds = 0.0f;

    UPROPERTY(Transient)
    FVector DeathFadeStartScale = FVector::OneVector;

    UPROPERTY(Transient)
    FName LastMissingRiderSaddleAttachName;

    UPROPERTY(Transient)
    FName LastMissingWeaponAttachName;

};
