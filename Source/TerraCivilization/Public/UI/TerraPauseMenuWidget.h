#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TerraPauseMenuWidget.generated.h"

class UButton;

/** Native UI1 fallback for /Game/UI/Widgets/WBP_PauseMenu. */
UCLASS()
class TERRACIVILIZATION_API UTerraPauseMenuWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

private:
    UFUNCTION()
    void HandleResumeClicked();

    UPROPERTY(Transient)
    TObjectPtr<UButton> ResumeButton;
};
