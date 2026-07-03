#pragma once

#include "CoreMinimal.h"
#include "TerraPiecePresentationTypes.h"

class PIECEPRESENTATION_API FTerraPiecePresentationSequencer
{
public:
    void BuildSyncSteps(
        const TArray<FTerraPiecePresentationSnapshot>& Snapshots,
        const TSet<int32>& ExistingPieceIds,
        TArray<FTerraPiecePresentationStep>& OutSteps) const;
};
