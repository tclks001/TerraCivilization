#include "UI/TerraUISubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Render/PlanetTessellatedMesh.h"
#include "UI/TerraMainMenuWidget.h"
#include "UI/TerraNewGameSetupWidget.h"
#include "UI/TerraUISettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerraUI, Log, All);

void UTerraUISubsystem::Deinitialize()
{
    if (ActiveScreen)
    {
        ActiveScreen->RemoveFromParent();
        ActiveScreen = nullptr;
    }
    ActiveRoute = ETerraUIRoute::None;
    Super::Deinitialize();
}

void UTerraUISubsystem::ShowMainMenu()
{
    ShowRoute_(ETerraUIRoute::MainMenu, UTerraMainMenuWidget::StaticClass());
}

void UTerraUISubsystem::ShowNewGameSetup()
{
    ShowRoute_(ETerraUIRoute::NewGameSetup, UTerraNewGameSetupWidget::StaticClass());
}

void UTerraUISubsystem::CloseFrontEnd()
{
    if (ActiveScreen)
    {
        ActiveScreen->RemoveFromParent();
        ActiveScreen = nullptr;
    }
    ActiveRoute = ETerraUIRoute::None;
    ApplyFrontEndInput_(false);
    OnRouteChanged.Broadcast(ActiveRoute);
}

bool UTerraUISubsystem::StartNewGame(const FTerraNewGameConfig& Config)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTerraUI, Error, TEXT("[UI0] StartNewGame failed: LocalPlayer has no world."));
        return false;
    }

    APlanetTessellatedMesh* Planet = Cast<APlanetTessellatedMesh>(
        UGameplayStatics::GetActorOfClass(World, APlanetTessellatedMesh::StaticClass()));
    if (!Planet)
    {
        UE_LOG(LogTerraUI, Error, TEXT("[UI0] StartNewGame failed: no APlanetTessellatedMesh exists in the current map."));
        return false;
    }

    LastNewGameConfig = Config;
    Planet->WorldGenSettings = Config.WorldGenSettings;
    Planet->Rebuild();
    UE_LOG(LogTerraUI, Log, TEXT("[UI0] New game started. Seed=%d MountainStrips=%d ForestPatches=%d PlayerColor=%s"),
        Config.WorldGenSettings.RandomSeed,
        Config.WorldGenSettings.MountainStripCount,
        Config.WorldGenSettings.ForestPatchCount,
        *Config.PlayerFactionColor.ToString());
    OnNewGameStarted.Broadcast(LastNewGameConfig);
    CloseFrontEnd();
    return true;
}

void UTerraUISubsystem::ShowRoute_(ETerraUIRoute Route, TSubclassOf<UUserWidget> FallbackClass)
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (!LocalPlayer)
    {
        return;
    }
    if (ActiveScreen)
    {
        ActiveScreen->RemoveFromParent();
        ActiveScreen = nullptr;
    }

    const UTerraUISettings* Settings = GetDefault<UTerraUISettings>();
    TSoftClassPtr<UUserWidget> ConfiguredClass;
    if (Route == ETerraUIRoute::MainMenu)
    {
        ConfiguredClass = Settings->MainMenuWidgetClass;
    }
    else if (Route == ETerraUIRoute::NewGameSetup)
    {
        ConfiguredClass = Settings->NewGameSetupWidgetClass;
    }
    TSubclassOf<UUserWidget> WidgetClass = FallbackClass;
    if (!ConfiguredClass.IsNull())
    {
        WidgetClass = ConfiguredClass.LoadSynchronous();
    }
    if (!WidgetClass)
    {
        UE_LOG(LogTerraUI, Error, TEXT("[UI0] Could not resolve widget class for route %d."), static_cast<int32>(Route));
        return;
    }

    APlayerController* PlayerController = LocalPlayer->GetPlayerController(GetWorld());
    if (!PlayerController)
    {
        UE_LOG(LogTerraUI, Error, TEXT("[UI0] Could not create widget for route %d: no PlayerController."), static_cast<int32>(Route));
        return;
    }

    ActiveScreen = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
    if (!ActiveScreen)
    {
        UE_LOG(LogTerraUI, Error, TEXT("[UI0] Could not create widget for route %d."), static_cast<int32>(Route));
        return;
    }
    ActiveScreen->AddToViewport(100);
    ActiveRoute = Route;
    ApplyFrontEndInput_(true);
    OnRouteChanged.Broadcast(ActiveRoute);
}

void UTerraUISubsystem::ApplyFrontEndInput_(bool bEnable)
{
    APlayerController* PlayerController = GetLocalPlayer() ? GetLocalPlayer()->GetPlayerController(GetWorld()) : nullptr;
    if (!PlayerController)
    {
        return;
    }
    // The planet's HISM hover/click loop is mouse-driven during gameplay too.
    // Closing a front-end screen must restore game input without hiding that cursor.
    PlayerController->bShowMouseCursor = true;
    if (bEnable)
    {
        FInputModeUIOnly InputMode;
        InputMode.SetWidgetToFocus(ActiveScreen ? ActiveScreen->TakeWidget() : TSharedPtr<SWidget>());
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PlayerController->SetInputMode(InputMode);
    }
    else
    {
        FInputModeGameOnly InputMode;
        // The HISM interaction controller polls WasInputKeyJustPressed. Do not let UE consume the
        // first board click merely to recapture the viewport after leaving the UI-only front end.
        InputMode.SetConsumeCaptureMouseDown(false);
        PlayerController->SetInputMode(InputMode);
    }
}
