#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "TerraGameplayTypes.h"
#include "TerraTechnologyChoiceButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTerraTechnologyChoiceButtonClicked, ETerraGameplayTechnologyId, TechnologyId);

UCLASS()
class TERRACIVILIZATION_API UTerraTechnologyChoiceButton : public UButton
{
    GENERATED_BODY()

public:
    void InitializeTechnology(ETerraGameplayTechnologyId InTechnologyId);

    UPROPERTY(BlueprintAssignable, Category="Terra UI")
    FTerraTechnologyChoiceButtonClicked OnTechnologyClicked;

private:
    UFUNCTION()
    void HandleClicked();

    ETerraGameplayTechnologyId TechnologyId = ETerraGameplayTechnologyId::None;
    bool bClickBound = false;
};
