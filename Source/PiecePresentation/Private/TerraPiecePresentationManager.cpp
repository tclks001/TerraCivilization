#include "TerraPiecePresentationManager.h"

#include "Animation/AnimationAsset.h"
#include "Engine/World.h"
#include "Logging/LogMacros.h"
#include "TerraPieceActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerraPiecePresentation, Log, All);

UTerraPiecePresentationManager::UTerraPiecePresentationManager()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UTerraPiecePresentationManager::SyncPieces(
    const TArray<FTerraPiecePresentationSnapshot>& Snapshots,
    const FTerraPieceVisualConfig& VisualConfig,
    const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents,
    const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents)
{
    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!World || !Owner)
    {
        return;
    }

    CachedVisualConfig = VisualConfig;

    TSet<int32> ExistingPieceIds;
    ExistingPieceIds.Reserve(PieceActors.Num());
    for (const TPair<int32, TObjectPtr<ATerraPieceActor>>& Pair : PieceActors)
    {
        if (IsValid(Pair.Value))
        {
            ExistingPieceIds.Add(Pair.Key);
        }
    }

    TArray<FTerraPiecePresentationStep> Steps;
    Sequencer.BuildSyncSteps(Snapshots, ExistingPieceIds, Steps);

    TMap<int32, FTerraPiecePresentationMoveEvent> MoveEventsByPieceId;
    MoveEventsByPieceId.Reserve(MoveEvents.Num());
    for (const FTerraPiecePresentationMoveEvent& MoveEvent : MoveEvents)
    {
        if (MoveEvent.IsValidMove())
        {
            MoveEventsByPieceId.Add(MoveEvent.PieceId, MoveEvent);
        }
    }

    TSet<int32> LivePieceIds;
    LivePieceIds.Reserve(Snapshots.Num());
    TSet<int32> P3CapturedPieceIdsToKeep;
    for (const FTerraPiecePresentationCaptureEvent& CaptureEvent : CaptureEvents)
    {
        if (CaptureEvent.IsValidCapture())
        {
            P3CapturedPieceIdsToKeep.Add(CaptureEvent.Captured.PieceId);
        }
    }
    for (const FTerraPiecePresentationCaptureEvent& PendingCaptureEvent : PendingCaptureEvents)
    {
        if (PendingCaptureEvent.IsValidCapture())
        {
            P3CapturedPieceIdsToKeep.Add(PendingCaptureEvent.Captured.PieceId);
        }
    }

    int32 SpawnedCount = 0;
    int32 UpdatedCount = 0;
    int32 RemovedCount = 0;

    for (const FTerraPiecePresentationStep& Step : Steps)
    {
        const FTerraPiecePresentationSnapshot& Snapshot = Step.Snapshot;
        if (Snapshot.PieceId == INDEX_NONE)
        {
            continue;
        }

        LivePieceIds.Add(Snapshot.PieceId);

        TObjectPtr<ATerraPieceActor>* ExistingActorPtr = PieceActors.Find(Snapshot.PieceId);
        ATerraPieceActor* PieceActor = ExistingActorPtr ? ExistingActorPtr->Get() : nullptr;
        const FTerraPiecePresentationMoveEvent* MoveEvent = MoveEventsByPieceId.Find(Snapshot.PieceId);
        if (!IsValid(PieceActor))
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = Owner;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.Name = FName(*FString::Printf(TEXT("TerraPiece_%d"), Snapshot.PieceId));

            const FTransform SpawnTransform = MoveEvent ? MoveEvent->FromWorldTransform : Snapshot.WorldTransform;
            PieceActor = World->SpawnActor<ATerraPieceActor>(ATerraPieceActor::StaticClass(), SpawnTransform, SpawnParams);
            if (!PieceActor)
            {
                UE_LOG(LogTerraPiecePresentation, Warning,
                    TEXT("[PiecePresentation][P1] Failed to spawn piece actor. PieceId=%d CellId=%d"),
                    Snapshot.PieceId,
                    Snapshot.CellId);
                continue;
            }

            PieceActors.Add(Snapshot.PieceId, PieceActor);
            ++SpawnedCount;
        }
        else
        {
            ++UpdatedCount;
        }

        if (MoveEvent)
        {
            PieceActor->PlayPresentationMove(Snapshot, *MoveEvent, VisualConfig);
        }
        else
        {
            PieceActor->ApplyPresentationSnapshot(Snapshot, VisualConfig);
        }
    }

    TArray<int32> PieceIdsToRemove;
    for (const TPair<int32, TObjectPtr<ATerraPieceActor>>& Pair : PieceActors)
    {
        if (!LivePieceIds.Contains(Pair.Key))
        {
            PieceIdsToRemove.Add(Pair.Key);
        }
    }

    for (const int32 PieceIdToRemove : PieceIdsToRemove)
    {
        if (P3CapturedPieceIdsToKeep.Contains(PieceIdToRemove))
        {
            continue;
        }

        TObjectPtr<ATerraPieceActor> PieceActor;
        PieceActors.RemoveAndCopyValue(PieceIdToRemove, PieceActor);
        if (IsValid(PieceActor))
        {
            PieceActor->Destroy();
        }
        ++RemovedCount;
    }

    UE_LOG(LogTerraPiecePresentation, Verbose,
        TEXT("[PiecePresentation][P1/P2/P3] Sync pieces. Spawned=%d Updated=%d Removed=%d MoveEvents=%d CaptureEvents=%d AliveActors=%d"),
        SpawnedCount,
        UpdatedCount,
        RemovedCount,
        MoveEventsByPieceId.Num(),
        CaptureEvents.Num(),
        PieceActors.Num());

    EnqueueCaptureEvents_(CaptureEvents, VisualConfig);
}

void UTerraPiecePresentationManager::ClearPieces()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(P3CaptureTimerHandle);
    }
    PendingCaptureEvents.Reset();
    bP3CaptureSequenceActive = false;

    for (const TPair<int32, TObjectPtr<ATerraPieceActor>>& Pair : PieceActors)
    {
        if (IsValid(Pair.Value))
        {
            Pair.Value->Destroy();
        }
    }
    PieceActors.Reset();
}

void UTerraPiecePresentationManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearPieces();
    Super::EndPlay(EndPlayReason);
}

void UTerraPiecePresentationManager::EnqueueCaptureEvents_(const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents, const FTerraPieceVisualConfig& VisualConfig)
{
    int32 AddedCount = 0;
    for (const FTerraPiecePresentationCaptureEvent& CaptureEvent : CaptureEvents)
    {
        if (!CaptureEvent.IsValidCapture())
        {
            continue;
        }

        PendingCaptureEvents.Add(CaptureEvent);
        ++AddedCount;
    }

    if (AddedCount <= 0)
    {
        return;
    }

    CachedVisualConfig = VisualConfig;
    UE_LOG(LogTerraPiecePresentation, Log,
        TEXT("[PiecePresentation][P3] Enqueued capture events. Added=%d Pending=%d Active=%d"),
        AddedCount,
        PendingCaptureEvents.Num(),
        bP3CaptureSequenceActive ? 1 : 0);

    if (!bP3CaptureSequenceActive)
    {
        PlayNextCaptureEvent_();
    }
}

void UTerraPiecePresentationManager::PlayNextCaptureEvent_()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        bP3CaptureSequenceActive = false;
        return;
    }

    if (PendingCaptureEvents.Num() <= 0)
    {
        bP3CaptureSequenceActive = false;
        return;
    }

    bP3CaptureSequenceActive = true;
    const FTerraPiecePresentationCaptureEvent CaptureEvent = PendingCaptureEvents[0];

    ATerraPieceActor* CapturedActor = FindPieceActor_(CaptureEvent.Captured.PieceId);
    ATerraPieceActor* AttackerActor = FindPieceActor_(CaptureEvent.Attacker.PieceId);
    ATerraPieceActor* VanguardActor = FindPieceActor_(CaptureEvent.Vanguard.PieceId);

    const bool bWaitForActiveMove =
        (IsValid(CapturedActor) && CapturedActor->IsPresentationMoveActive())
        || (IsValid(AttackerActor) && AttackerActor->IsPresentationMoveActive())
        || (IsValid(VanguardActor) && VanguardActor->IsPresentationMoveActive());
    if (bWaitForActiveMove)
    {
        const FTimerDelegate RetryDelegate = FTimerDelegate::CreateUObject(this, &UTerraPiecePresentationManager::PlayNextCaptureEvent_);
        World->GetTimerManager().SetTimer(P3CaptureTimerHandle, RetryDelegate, 0.03f, false);
        return;
    }

    PendingCaptureEvents.RemoveAt(0);

    if (!IsValid(CapturedActor))
    {
        UE_LOG(LogTerraPiecePresentation, Warning,
            TEXT("[PiecePresentation][P3] Skip capture: captured actor missing. Captured=%d Attacker=%d Vanguard=%d"),
            CaptureEvent.Captured.PieceId,
            CaptureEvent.Attacker.PieceId,
            CaptureEvent.Vanguard.PieceId);
        PlayNextCaptureEvent_();
        return;
    }

    const FVector CapturedLocation = CapturedActor->GetActorLocation();
    if (IsValid(AttackerActor))
    {
        AttackerActor->FaceTowards(CapturedLocation, CachedVisualConfig.P3FacingBlendSeconds);
    }
    if (IsValid(VanguardActor) && VanguardActor != AttackerActor)
    {
        VanguardActor->FaceTowards(CapturedLocation, CachedVisualConfig.P3FacingBlendSeconds);
    }

    const ATerraPieceActor* CapturedFacingSource = IsValid(VanguardActor) ? VanguardActor : AttackerActor;
    if (CapturedFacingSource)
    {
        CapturedActor->FaceTowards(CapturedFacingSource->GetActorLocation(), CachedVisualConfig.P3FacingBlendSeconds);
    }

    UE_LOG(LogTerraPiecePresentation, Log,
        TEXT("[PiecePresentation][P3] Start capture. Captured=%d Attacker=%d Vanguard=%d PendingAfter=%d"),
        CaptureEvent.Captured.PieceId,
        CaptureEvent.Attacker.PieceId,
        CaptureEvent.Vanguard.PieceId,
        PendingCaptureEvents.Num());

    const FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UTerraPiecePresentationManager::RunP3MeleeAttackStep_, CaptureEvent);
    World->GetTimerManager().SetTimer(
        P3CaptureTimerHandle,
        Delegate,
        FMath::Max(CachedVisualConfig.P3FacingBlendSeconds, 0.0f),
        false);
}

void UTerraPiecePresentationManager::RunP3MeleeAttackStep_(FTerraPiecePresentationCaptureEvent CaptureEvent)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        bP3CaptureSequenceActive = false;
        return;
    }

    ATerraPieceActor* AttackerActor = FindPieceActor_(CaptureEvent.Attacker.PieceId);
    ATerraPieceActor* VanguardActor = FindPieceActor_(CaptureEvent.Vanguard.PieceId);

    auto RunInIfMelee = [this, &CaptureEvent](ATerraPieceActor* PieceActor, const FTerraPiecePresentationCaptureParticipant& Participant)
    {
        if (!IsValid(PieceActor) || !IsMeleePiece_(Participant.PieceType))
        {
            return;
        }

        FTransform AttackTransform = CaptureEvent.Captured.WorldTransform;
        AttackTransform.SetRotation(PieceActor->GetActorQuat());
        PieceActor->PlayPresentationOnlyMoveTo(
            AttackTransform,
            FMath::Max(CachedVisualConfig.P3MeleeRunInSeconds, 0.001f),
            CachedVisualConfig.MoveAnimation);
    };

    RunInIfMelee(AttackerActor, CaptureEvent.Attacker);
    if (CaptureEvent.Vanguard.PieceId != CaptureEvent.Attacker.PieceId)
    {
        RunInIfMelee(VanguardActor, CaptureEvent.Vanguard);
    }

    const FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UTerraPiecePresentationManager::RunP3ReturnAndFadeStep_, CaptureEvent);
    World->GetTimerManager().SetTimer(
        P3CaptureTimerHandle,
        Delegate,
        FMath::Max(CachedVisualConfig.P3MeleeRunInSeconds, 0.001f),
        false);
}

void UTerraPiecePresentationManager::RunP3ReturnAndFadeStep_(FTerraPiecePresentationCaptureEvent CaptureEvent)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        bP3CaptureSequenceActive = false;
        return;
    }

    ATerraPieceActor* CapturedActor = FindPieceActor_(CaptureEvent.Captured.PieceId);
    ATerraPieceActor* AttackerActor = FindPieceActor_(CaptureEvent.Attacker.PieceId);
    ATerraPieceActor* VanguardActor = FindPieceActor_(CaptureEvent.Vanguard.PieceId);
    const float SynchronizedHitDelaySeconds = GetP3SynchronizedHitDelaySeconds_(CaptureEvent);

    auto ScheduleAttackIfValid = [this, World, SynchronizedHitDelaySeconds](ATerraPieceActor* PieceActor, const FTerraPiecePresentationCaptureParticipant& Participant)
    {
        if (!IsValid(PieceActor) || !Participant.IsValid())
        {
            return;
        }

        const float AttackToHitSeconds = FMath::Max(CachedVisualConfig.ResolveAttackToHitSeconds(Participant.PieceType), 0.0f);
        const float AttackStartDelaySeconds = FMath::Max(SynchronizedHitDelaySeconds - AttackToHitSeconds, 0.0f);
        const int32 PieceId = Participant.PieceId;
        const ETerraGameplayPieceType PieceType = Participant.PieceType;
        const FTimerDelegate AttackDelegate = FTimerDelegate::CreateWeakLambda(this, [this, PieceId, PieceType]()
        {
            if (ATerraPieceActor* Actor = FindPieceActor_(PieceId))
            {
                Actor->PlayAttackAnimation(
                    CachedVisualConfig.ResolveAttackAnimation(PieceType),
                    CachedVisualConfig.P3AttackAnimationStartOffsetSeconds);
            }
        });

        if (AttackStartDelaySeconds <= KINDA_SMALL_NUMBER)
        {
            AttackDelegate.ExecuteIfBound();
            return;
        }

        FTimerHandle AttackTimerHandle;
        World->GetTimerManager().SetTimer(AttackTimerHandle, AttackDelegate, AttackStartDelaySeconds, false);
    };

    ScheduleAttackIfValid(AttackerActor, CaptureEvent.Attacker);
    if (CaptureEvent.Vanguard.PieceId != CaptureEvent.Attacker.PieceId)
    {
        ScheduleAttackIfValid(VanguardActor, CaptureEvent.Vanguard);
    }

    if (IsValid(CapturedActor))
    {
        const FTimerDelegate HitDelegate = FTimerDelegate::CreateWeakLambda(this, [this, CapturedPieceId = CaptureEvent.Captured.PieceId]()
        {
            if (ATerraPieceActor* Actor = FindPieceActor_(CapturedPieceId))
            {
                Actor->PlayHitAnimation(CachedVisualConfig.HitAnimation, CachedVisualConfig.P3HitAnimationStartOffsetSeconds);
            }
        });
        FTimerHandle HitTimerHandle;
        World->GetTimerManager().SetTimer(
            HitTimerHandle,
            HitDelegate,
            FMath::Max(SynchronizedHitDelaySeconds, 0.0f),
            false);

        const FTimerDelegate DeathDelegate = FTimerDelegate::CreateWeakLambda(this, [this, CapturedPieceId = CaptureEvent.Captured.PieceId]()
        {
            if (ATerraPieceActor* Actor = FindPieceActor_(CapturedPieceId))
            {
                Actor->PlayDeathAnimation(CachedVisualConfig.DeathAnimation, CachedVisualConfig.P3DeathAnimationStartOffsetSeconds);
            }
        });
        FTimerHandle DeathTimerHandle;
        World->GetTimerManager().SetTimer(
            DeathTimerHandle,
            DeathDelegate,
            FMath::Max(SynchronizedHitDelaySeconds + CachedVisualConfig.P3DeathAfterHitDelaySeconds, 0.0f),
            false);
    }

    const float AttackStepSeconds = GetP3AttackStepSeconds_(CaptureEvent);
    const FTimerDelegate FinishDelegate = FTimerDelegate::CreateUObject(this, &UTerraPiecePresentationManager::FinishP3CaptureEvent_, CaptureEvent);
    World->GetTimerManager().SetTimer(
        P3CaptureTimerHandle,
        FinishDelegate,
        FMath::Max(AttackStepSeconds, 0.001f),
        false);
}

void UTerraPiecePresentationManager::FinishP3CaptureEvent_(FTerraPiecePresentationCaptureEvent CaptureEvent)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        bP3CaptureSequenceActive = false;
        return;
    }

    ATerraPieceActor* AttackerActor = FindPieceActor_(CaptureEvent.Attacker.PieceId);
    ATerraPieceActor* VanguardActor = FindPieceActor_(CaptureEvent.Vanguard.PieceId);
    ATerraPieceActor* CapturedActor = FindPieceActor_(CaptureEvent.Captured.PieceId);

    auto ReturnIfMelee = [this, &CaptureEvent](ATerraPieceActor* PieceActor, const FTerraPiecePresentationCaptureParticipant& Participant)
    {
        if (!IsValid(PieceActor) || !IsMeleePiece_(Participant.PieceType))
        {
            return;
        }

        FTransform ReturnTransform = Participant.WorldTransform;
        ReturnTransform.SetRotation(FRotationMatrix::MakeFromXZ(
            CaptureEvent.Captured.WorldTransform.GetLocation() - Participant.WorldTransform.GetLocation(),
            Participant.WorldTransform.GetRotation().GetUpVector()).ToQuat());
        PieceActor->PlayPresentationOnlyMoveTo(
            ReturnTransform,
            FMath::Max(CachedVisualConfig.P3MeleeReturnSeconds, 0.001f),
            CachedVisualConfig.MoveAnimation);
    };

    ReturnIfMelee(AttackerActor, CaptureEvent.Attacker);
    if (CaptureEvent.Vanguard.PieceId != CaptureEvent.Attacker.PieceId)
    {
        ReturnIfMelee(VanguardActor, CaptureEvent.Vanguard);
    }

    if (IsValid(CapturedActor))
    {
        CapturedActor->StartDeathFade(CachedVisualConfig.P3CapturedFadeSeconds);
    }

    const float FinishDelay = FMath::Max(CachedVisualConfig.P3MeleeReturnSeconds, CachedVisualConfig.P3CapturedFadeSeconds);
    const FTimerDelegate NextDelegate = FTimerDelegate::CreateWeakLambda(this, [this, CapturedPieceId = CaptureEvent.Captured.PieceId]()
    {
        TObjectPtr<ATerraPieceActor> CapturedActorPtr;
        PieceActors.RemoveAndCopyValue(CapturedPieceId, CapturedActorPtr);
        if (IsValid(CapturedActorPtr))
        {
            CapturedActorPtr->Destroy();
        }
        PlayNextCaptureEvent_();
    });
    World->GetTimerManager().SetTimer(
        P3CaptureTimerHandle,
        NextDelegate,
        FMath::Max(FinishDelay, 0.001f),
        false);
}

ATerraPieceActor* UTerraPiecePresentationManager::FindPieceActor_(int32 PieceId) const
{
    const TObjectPtr<ATerraPieceActor>* ActorPtr = PieceActors.Find(PieceId);
    return ActorPtr ? ActorPtr->Get() : nullptr;
}

bool UTerraPiecePresentationManager::IsMeleePiece_(ETerraGameplayPieceType PieceType) const
{
    return PieceType == ETerraGameplayPieceType::Infantry
        || PieceType == ETerraGameplayPieceType::Cavalry;
}

float UTerraPiecePresentationManager::GetAnimationLength_(UAnimationAsset* AnimationAsset, float FallbackSeconds) const
{
    return AnimationAsset ? FMath::Max(AnimationAsset->GetPlayLength(), 0.001f) : FMath::Max(FallbackSeconds, 0.001f);
}

float UTerraPiecePresentationManager::GetP3AttackStepSeconds_(const FTerraPiecePresentationCaptureEvent& CaptureEvent) const
{
    float MaxSeconds = 0.0f;
    const float SynchronizedHitDelaySeconds = GetP3SynchronizedHitDelaySeconds_(CaptureEvent);

    auto IncludeAttackLength = [this, &MaxSeconds, SynchronizedHitDelaySeconds](const FTerraPiecePresentationCaptureParticipant& Participant)
    {
        if (!Participant.IsValid())
        {
            return;
        }

        const float AttackToHitSeconds = FMath::Max(CachedVisualConfig.ResolveAttackToHitSeconds(Participant.PieceType), 0.0f);
        const float AttackStartDelaySeconds = FMath::Max(SynchronizedHitDelaySeconds - AttackToHitSeconds, 0.0f);
        MaxSeconds = FMath::Max(
            MaxSeconds,
            AttackStartDelaySeconds
                + CachedVisualConfig.P3AttackAnimationStartOffsetSeconds
                + GetAnimationLength_(CachedVisualConfig.ResolveAttackAnimation(Participant.PieceType), 0.5f));
    };

    IncludeAttackLength(CaptureEvent.Attacker);
    if (CaptureEvent.Vanguard.PieceId != CaptureEvent.Attacker.PieceId)
    {
        IncludeAttackLength(CaptureEvent.Vanguard);
    }

    const float HitSeconds = SynchronizedHitDelaySeconds
        + CachedVisualConfig.P3HitAnimationStartOffsetSeconds
        + GetAnimationLength_(CachedVisualConfig.HitAnimation, 0.35f);
    const float DeathSeconds = SynchronizedHitDelaySeconds
        + CachedVisualConfig.P3DeathAfterHitDelaySeconds
        + CachedVisualConfig.P3DeathAnimationStartOffsetSeconds
        + GetAnimationLength_(CachedVisualConfig.DeathAnimation, 0.75f);

    MaxSeconds = FMath::Max(MaxSeconds, HitSeconds);
    MaxSeconds = FMath::Max(MaxSeconds, DeathSeconds);
    return MaxSeconds;
}

float UTerraPiecePresentationManager::GetP3SynchronizedHitDelaySeconds_(const FTerraPiecePresentationCaptureEvent& CaptureEvent) const
{
    float MaxAttackToHitSeconds = FMath::Max(CachedVisualConfig.P3HitReactDelaySeconds, 0.0f);

    auto IncludeParticipant = [this, &MaxAttackToHitSeconds](const FTerraPiecePresentationCaptureParticipant& Participant)
    {
        if (!Participant.IsValid())
        {
            return;
        }

        MaxAttackToHitSeconds = FMath::Max(
            MaxAttackToHitSeconds,
            FMath::Max(CachedVisualConfig.ResolveAttackToHitSeconds(Participant.PieceType), 0.0f));
    };

    IncludeParticipant(CaptureEvent.Attacker);
    if (CaptureEvent.Vanguard.PieceId != CaptureEvent.Attacker.PieceId)
    {
        IncludeParticipant(CaptureEvent.Vanguard);
    }

    return MaxAttackToHitSeconds;
}
