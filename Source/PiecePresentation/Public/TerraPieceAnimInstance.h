#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "TerraPiecePresentationTypes.h"
#include "TerraPieceAnimInstance.generated.h"

UCLASS(Blueprintable, BlueprintType)
class PIECEPRESENTATION_API UTerraPieceAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Terra|Piece Presentation")
    void SetDriveState(const FTerraPieceAnimDriveState& InDriveState);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation")
    FTerraPieceAnimDriveState DriveState;
};
