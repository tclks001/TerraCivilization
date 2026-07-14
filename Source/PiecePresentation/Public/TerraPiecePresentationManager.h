#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "TerraPiecePresentationSequencer.h"
#include "TerraPiecePresentationTypes.h"
#include "TerraPiecePresentationManager.generated.h"

class ATerraPieceActor;
class ATerraPieceProjectileActor;
class ATerraEquipmentDropActor;

UCLASS(ClassGroup = (Terra), meta = (BlueprintSpawnableComponent))
class PIECEPRESENTATION_API UTerraPiecePresentationManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UTerraPiecePresentationManager();

    void SyncPieces(
        const TArray<FTerraPiecePresentationSnapshot>& Snapshots,
        const FTerraPieceVisualConfig& VisualConfig,
        const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents = TArray<FTerraPiecePresentationMoveEvent>(),
        const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents = TArray<FTerraPiecePresentationCaptureEvent>(),
        const TArray<FTerraPieceEquipmentDropSnapshot>& EquipmentDrops = TArray<FTerraPieceEquipmentDropSnapshot>());
    void ClearPieces();

    int32 GetPresentedPieceCount() const { return PieceActors.Num(); }

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(Transient)
    TMap<int32, TObjectPtr<ATerraPieceActor>> PieceActors;

    UPROPERTY(Transient)
    TMap<int32, TObjectPtr<ATerraEquipmentDropActor>> EquipmentDropActors;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ATerraPieceProjectileActor>> ActiveP6Projectiles;

    UPROPERTY(Transient)
    TArray<FTerraPiecePresentationCaptureEvent> PendingCaptureEvents;

    UPROPERTY(Transient)
    bool bP3CaptureSequenceActive = false;

    UPROPERTY(Transient)
    FTerraPieceVisualConfig CachedVisualConfig;

    FTerraPiecePresentationSequencer Sequencer;

    FTimerHandle P3CaptureTimerHandle;

    void EnqueueCaptureEvents_(const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents, const FTerraPieceVisualConfig& VisualConfig);
    void PlayNextCaptureEvent_();
    void RunP3MeleeAttackStep_(FTerraPiecePresentationCaptureEvent CaptureEvent);
    void RunP3ReturnAndFadeStep_(FTerraPiecePresentationCaptureEvent CaptureEvent);
    void FinishP3CaptureEvent_(FTerraPiecePresentationCaptureEvent CaptureEvent);
    ATerraPieceActor* FindPieceActor_(int32 PieceId) const;
    bool IsMeleePiece_(ETerraGameplayPieceType PieceType) const;
    bool IsP6ProjectilePiece_(ETerraGameplayPieceType PieceType) const;
    void SpawnP6Projectile_(const FTerraPiecePresentationCaptureParticipant& Source, const FTerraPiecePresentationCaptureParticipant& Target);
    FVector ResolveP6LaunchLocation_(ATerraPieceActor* SourceActor, FName AttachName, const FVector& RelativeLocation) const;
    void CleanupInactiveP6Projectiles_();
    float GetAnimationLength_(UAnimationAsset* AnimationAsset, float FallbackSeconds) const;
    bool IsP35RemoteAttackPiece_(ETerraGameplayPieceType PieceType) const;
    float GetP35RemoteAttackDurationSeconds_(ETerraGameplayPieceType PieceType) const;
    float GetP35RemoteAttackPlayRateScale_(ETerraGameplayPieceType PieceType) const;
    float ResolveAttackAnimationPlayRate_(ETerraGameplayPieceType PieceType, UAnimationAsset* AttackAnimation) const;
    float ResolveAttackAnimationDurationSeconds_(ETerraGameplayPieceType PieceType, UAnimationAsset* AttackAnimation, float FallbackSeconds) const;
    float GetP3AttackStepSeconds_(const FTerraPiecePresentationCaptureEvent& CaptureEvent) const;
    float GetP3SynchronizedHitDelaySeconds_(const FTerraPiecePresentationCaptureEvent& CaptureEvent) const;
};
