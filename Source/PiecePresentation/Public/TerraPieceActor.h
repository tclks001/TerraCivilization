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

    int32 GetPieceId() const { return PieceId; }
    int32 GetCellId() const { return CellId; }
    ETerraGameplayPieceType GetPieceType() const { return PieceType; }
    const FTerraPieceAnimDriveState& GetAnimDriveState() const { return AnimDriveState; }

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
};
