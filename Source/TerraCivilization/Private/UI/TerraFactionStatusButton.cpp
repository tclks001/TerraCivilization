#include "UI/TerraFactionStatusButton.h"

void UTerraFactionStatusButton::InitializeFaction(int32 InFactionId)
{
    FactionId = InFactionId;
    if (!bClickBound)
    {
        OnClicked.AddUniqueDynamic(this, &UTerraFactionStatusButton::HandleClicked);
        bClickBound = true;
    }
}

void UTerraFactionStatusButton::HandleClicked()
{
    OnFactionClicked.Broadcast(FactionId);
}
