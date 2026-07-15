#pragma once

#include "CoreMinimal.h"
#include "Blueprint\UserWidget.h"
#include "TerraMainMenuWidget.generated.h"

class UButton;

/** Native fallback for /Game/UI/Widgets/WBP_MainMenu. Blueprint children may replace its presentation. */
UCLASS()
class TERRACIVILIZATION_API UTerraMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

private:
    UFUNCTION()
    void HandleNewGameClicked();

    UFUNCTION()
    void HandleQuitClicked();

    UPROPERTY(Transient)
    TObjectPtr<UButton> NewGameButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> QuitButton;
};
