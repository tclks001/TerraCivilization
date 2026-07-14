#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerraPiecePresentationTypes.h"
#include "TerraEquipmentDropActor.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

UCLASS()
class PIECEPRESENTATION_API ATerraEquipmentDropActor : public AActor
{
    GENERATED_BODY()

public:
    ATerraEquipmentDropActor();
    void ApplySnapshot(const FTerraPieceEquipmentDropSnapshot& Snapshot, const FTerraPieceVisualConfig& VisualConfig);

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> RootScene;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> BowMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkeletalMeshComponent> HorseMesh;
};
