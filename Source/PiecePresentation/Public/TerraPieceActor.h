#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerraPiecePresentationTypes.h"
#include "TerraPieceActor.generated.h"

class USceneComponent;
class USkeletalMeshComponent;

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
    void FinishActiveMove_();
    FVector EvaluateActiveMovePosition_(float Alpha, const FTerraPieceVisualConfig& VisualConfig) const;
    FQuat BuildRotationFromFacing_(const FVector& WorldPosition, const FVector& DesiredFacingDirection, const FQuat& FallbackRotation) const;

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
};
