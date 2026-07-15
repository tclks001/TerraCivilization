#pragma once

#include "CoreMinimal.h"
#include "Blueprint\UserWidget.h"
#include "TerraNewGameSetupWidget.generated.h"

class UButton;
class UEditableTextBox;

/** Native fallback for /Game/UI/Widgets/WBP_NewGameSetup. */
UCLASS()
class TERRACIVILIZATION_API UTerraNewGameSetupWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

private:
    UFUNCTION()
    void HandleStartClicked();

    UFUNCTION()
    void HandleBackClicked();

    bool ReadInt_(const UEditableTextBox* TextBox, int32& OutValue) const;
    void SetValidationMessage_(const FText& Message, bool bIsError);

    UPROPERTY(Transient)
    TObjectPtr<UEditableTextBox> SeedTextBox;

    UPROPERTY(Transient)
    TObjectPtr<UEditableTextBox> MountainStripCountTextBox;

    UPROPERTY(Transient)
    TObjectPtr<UEditableTextBox> ForestPatchCountTextBox;
};
