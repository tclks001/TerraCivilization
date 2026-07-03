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

    int32 GetPieceId() const { return PieceId; }
    int32 GetCellId() const { return CellId; }
    ETerraGameplayPieceType GetPieceType() const { return PieceType; }
    const FTerraPieceAnimDriveState& GetAnimDriveState() const { return AnimDriveState; }

protected:
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
};
