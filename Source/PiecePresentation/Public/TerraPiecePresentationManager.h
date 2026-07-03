#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TerraPiecePresentationSequencer.h"
#include "TerraPiecePresentationTypes.h"
#include "TerraPiecePresentationManager.generated.h"

class ATerraPieceActor;

UCLASS(ClassGroup = (Terra), meta = (BlueprintSpawnableComponent))
class PIECEPRESENTATION_API UTerraPiecePresentationManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UTerraPiecePresentationManager();

    void SyncPieces(const TArray<FTerraPiecePresentationSnapshot>& Snapshots, const FTerraPieceVisualConfig& VisualConfig);
    void ClearPieces();

    int32 GetPresentedPieceCount() const { return PieceActors.Num(); }

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(Transient)
    TMap<int32, TObjectPtr<ATerraPieceActor>> PieceActors;

    FTerraPiecePresentationSequencer Sequencer;
};
