#include "TerraPiecePresentationManager.h"

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
    const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents)
{
    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!World || !Owner)
    {
        return;
    }

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
        TObjectPtr<ATerraPieceActor> PieceActor;
        PieceActors.RemoveAndCopyValue(PieceIdToRemove, PieceActor);
        if (IsValid(PieceActor))
        {
            PieceActor->Destroy();
        }
        ++RemovedCount;
    }

    UE_LOG(LogTerraPiecePresentation, Verbose,
        TEXT("[PiecePresentation][P1/P2] Sync pieces. Spawned=%d Updated=%d Removed=%d MoveEvents=%d AliveActors=%d"),
        SpawnedCount,
        UpdatedCount,
        RemovedCount,
        MoveEventsByPieceId.Num(),
        PieceActors.Num());
}

void UTerraPiecePresentationManager::ClearPieces()
{
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
