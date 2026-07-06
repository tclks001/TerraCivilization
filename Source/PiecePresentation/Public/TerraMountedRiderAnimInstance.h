#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "TerraMountedRiderAnimInstance.generated.h"

class UAnimMontage;
class UAnimationAsset;

UENUM(BlueprintType)
enum class ETerraMountedRiderAnimState : uint8
{
    Sitting UMETA(DisplayName = "Sitting"),
    UpperBodyAction UMETA(DisplayName = "UpperBodyAction"),
    Death UMETA(DisplayName = "Death"),
};

UCLASS(Blueprintable, BlueprintType)
class PIECEPRESENTATION_API UTerraMountedRiderAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Terra|Mounted Rider")
    void SetSittingAnimation(UAnimationAsset* InSittingAnimation);

    UFUNCTION(BlueprintCallable, Category = "Terra|Mounted Rider")
    void EnterSitting();

    UFUNCTION(BlueprintCallable, Category = "Terra|Mounted Rider")
    float PlayUpperBodyMontage(UAnimMontage* Montage, float StartOffsetSeconds);

    UFUNCTION(BlueprintCallable, Category = "Terra|Mounted Rider")
    void EnterDeath();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Mounted Rider")
    TObjectPtr<UAnimationAsset> SittingAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Mounted Rider")
    ETerraMountedRiderAnimState RiderState = ETerraMountedRiderAnimState::Sitting;
};
