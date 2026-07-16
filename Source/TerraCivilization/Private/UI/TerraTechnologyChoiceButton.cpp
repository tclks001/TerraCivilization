#include "UI/TerraTechnologyChoiceButton.h"

void UTerraTechnologyChoiceButton::InitializeTechnology(ETerraGameplayTechnologyId InTechnologyId)
{
    TechnologyId = InTechnologyId;
    if (!bClickBound)
    {
        OnClicked.AddUniqueDynamic(this, &UTerraTechnologyChoiceButton::HandleClicked);
        bClickBound = true;
    }
}

void UTerraTechnologyChoiceButton::HandleClicked()
{
    OnTechnologyClicked.Broadcast(TechnologyId);
}
