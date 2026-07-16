#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TerraGameplayTypes.h"
#include "TerraTechnologyChoiceWidget.generated.h"

class UPlanetGameplayComponent;

UCLASS()
class TERRACIVILIZATION_API UTerraTechnologyChoiceWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

private:
    UFUNCTION()
    void HandleTechnologyClicked(ETerraGameplayTechnologyId TechnologyId);

    UPlanetGameplayComponent* FindGameplay_() const;

    int32 FactionId = INDEX_NONE;
};
