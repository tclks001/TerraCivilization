#pragma once

#include "CoreMinimal.h"
#include "Subsystems\LocalPlayerSubsystem.h"
#include "WorldGenSettings.h"
#include "TerraUISubsystem.generated.h"

class UUserWidget;
class UPlanetGameplayComponent;

UENUM(BlueprintType)
enum class ETerraUIRoute : uint8
{
    None,
    MainMenu,
    NewGameSetup,
    InGame,
    Paused,
    TechnologyChoice,
};

/** The UI0 subset of the deterministic match-start configuration. */
USTRUCT(BlueprintType)
struct TERRACIVILIZATION_API FTerraNewGameConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="New Game")
    FWorldGenSettings WorldGenSettings;

    /** Reserved for the piece-presentation palette bridge introduced after UI0. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="New Game")
    FLinearColor PlayerFactionColor = FLinearColor(0.12f, 0.55f, 0.95f, 1.0f);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTerraUIRouteChanged, ETerraUIRoute, NewRoute);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTerraNewGameStarted, const FTerraNewGameConfig&, Config);

/**
 * Owns the local UI0 screen route. It creates one full-screen front-end widget at a time and
 * applies only real, existing WorldGen parameters when a new match is requested.
 */
UCLASS()
class TERRACIVILIZATION_API UTerraUISubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="Terra UI")
    void ShowMainMenu();

    UFUNCTION(BlueprintCallable, Category="Terra UI")
    void ShowNewGameSetup();

    UFUNCTION(BlueprintCallable, Category="Terra UI")
    void CloseFrontEnd();

    UFUNCTION(BlueprintCallable, Category="Terra UI")
    void TogglePauseMenu();

    UFUNCTION(BlueprintCallable, Category="Terra UI")
    void ResumeGame();

    UFUNCTION(BlueprintCallable, Category="Terra UI")
    void CloseTechnologyChoice();

    UFUNCTION(BlueprintCallable, Category="Terra UI")
    bool StartNewGame(const FTerraNewGameConfig& Config);

    UFUNCTION(BlueprintPure, Category="Terra UI")
    bool IsBlockingGameInput() const
    {
        return ActiveRoute == ETerraUIRoute::MainMenu
            || ActiveRoute == ETerraUIRoute::NewGameSetup
            || ActiveRoute == ETerraUIRoute::Paused
            || ActiveRoute == ETerraUIRoute::TechnologyChoice;
    }

    UFUNCTION(BlueprintPure, Category="Terra UI")
    ETerraUIRoute GetActiveRoute() const { return ActiveRoute; }

    UFUNCTION(BlueprintPure, Category="Terra UI")
    FTerraNewGameConfig GetLastNewGameConfig() const { return LastNewGameConfig; }

    UPROPERTY(BlueprintAssignable, Category="Terra UI")
    FTerraUIRouteChanged OnRouteChanged;

    UPROPERTY(BlueprintAssignable, Category="Terra UI")
    FTerraNewGameStarted OnNewGameStarted;

private:
    void ShowRoute_(ETerraUIRoute Route, TSubclassOf<UUserWidget> FallbackClass);
    void EnsureInGameHUD_();
    void BindGameplayTechnologyEvents_();
    UFUNCTION()
    void HandleTechnologyChoiceRequested(int32 FactionId);
    UFUNCTION()
    void HandleTechnologyStateChanged();
    void ApplyFrontEndInput_(bool bEnable);

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> ActiveScreen;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> InGameHUD;

    UPROPERTY(Transient)
    TObjectPtr<UPlanetGameplayComponent> BoundGameplayComponent;

    ETerraUIRoute ActiveRoute = ETerraUIRoute::None;
    FTerraNewGameConfig LastNewGameConfig;
};
