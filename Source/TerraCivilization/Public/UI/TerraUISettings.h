#pragma once

#include "CoreMinimal.h"
#include "Engine\DeveloperSettings.h"
#include "TerraUISettings.generated.h"

class UUserWidget;

/**
 * Project-level attachment points for the UI0 front-end widgets.
 * Leave a class empty to use the native fallback widget while a Blueprint asset is being authored.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Terra UI"))
class TERRACIVILIZATION_API UTerraUISettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category="Front End")
    bool bShowFrontEndOnStartup = true;

    UPROPERTY(Config, EditAnywhere, Category="Front End", meta=(AllowedClasses="/Script/UMG.UserWidget"))
    TSoftClassPtr<UUserWidget> MainMenuWidgetClass;

    UPROPERTY(Config, EditAnywhere, Category="Front End", meta=(AllowedClasses="/Script/UMG.UserWidget"))
    TSoftClassPtr<UUserWidget> NewGameSetupWidgetClass;

    UPROPERTY(Config, EditAnywhere, Category="In Game", meta=(AllowedClasses="/Script/UMG.UserWidget"))
    TSoftClassPtr<UUserWidget> InGameHUDWidgetClass;

    UPROPERTY(Config, EditAnywhere, Category="In Game", meta=(AllowedClasses="/Script/UMG.UserWidget"))
    TSoftClassPtr<UUserWidget> PauseMenuWidgetClass;

    UPROPERTY(Config, EditAnywhere, Category="In Game", meta=(AllowedClasses="/Script/UMG.UserWidget"))
    TSoftClassPtr<UUserWidget> TechnologyChoiceWidgetClass;
};
