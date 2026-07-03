#include "TerraPiecePresentationSequencer.h"

void FTerraPiecePresentationSequencer::BuildSyncSteps(
    const TArray<FTerraPiecePresentationSnapshot>& Snapshots,
    const TSet<int32>& ExistingPieceIds,
    TArray<FTerraPiecePresentationStep>& OutSteps) const
{
    OutSteps.Reset();
    OutSteps.Reserve(Snapshots.Num());

    for (const FTerraPiecePresentationSnapshot& Snapshot : Snapshots)
    {
        FTerraPiecePresentationStep& Step = OutSteps.AddDefaulted_GetRef();
        Step.StepType = ExistingPieceIds.Contains(Snapshot.PieceId)
            ? ETerraPiecePresentationStepType::Update
            : ETerraPiecePresentationStepType::Spawn;
        Step.Snapshot = Snapshot;
    }
}
