#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "TerraFactionStatusButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTerraFactionStatusButtonClicked, int32, FactionId);

UCLASS()
class TERRACIVILIZATION_API UTerraFactionStatusButton : public UButton
{
    GENERATED_BODY()

public:
    void InitializeFaction(int32 InFactionId);

    UPROPERTY(BlueprintAssignable, Category="Terra UI")
    FTerraFactionStatusButtonClicked OnFactionClicked;

private:
    UFUNCTION()
    void HandleClicked();

    int32 FactionId = INDEX_NONE;
    bool bClickBound = false;
};
