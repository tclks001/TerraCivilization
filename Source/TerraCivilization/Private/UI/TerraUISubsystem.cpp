#include "UI/TerraUISubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Render/PlanetTessellatedMesh.h"
#include "Render/PlanetGameplayComponent.h"
#include "Render/PlanetCameraComponent.h"
#include "Interaction/PlanetInteractionController.h"
#include "UI/TerraMainMenuWidget.h"
#include "UI/TerraNewGameSetupWidget.h"
#include "UI/TerraInGameHUDWidget.h"
#include "UI/TerraPauseMenuWidget.h"
#include "UI/TerraTechnologyChoiceWidget.h"
#include "UI/TerraUISettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerraUI, Log, All);

void UTerraUISubsystem::Deinitialize()
{
    if (ActiveScreen)
    {
        ActiveScreen->RemoveFromParent();
        ActiveScreen = nullptr;
    }
    if (InGameHUD)
    {
        InGameHUD->RemoveFromParent();
        InGameHUD = nullptr;
    }
    if (BoundGameplayComponent)
    {
        BoundGameplayComponent->OnTechnologyChoiceRequested.RemoveDynamic(this, &UTerraUISubsystem::HandleTechnologyChoiceRequested);
        BoundGameplayComponent->OnTechnologyStateChanged.RemoveDynamic(this, &UTerraUISubsystem::HandleTechnologyStateChanged);
        BoundGameplayComponent = nullptr;
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
    EnsureInGameHUD_();
    if (APlanetTessellatedMesh* Planet = Cast<APlanetTessellatedMesh>(
            UGameplayStatics::GetActorOfClass(GetWorld(), APlanetTessellatedMesh::StaticClass())))
    {
        if (UPlanetCameraComponent* Camera = Planet->GetPlanetCameraComponent())
        {
            Camera->FocusCameraOnCurrentFactionBase();
        }
    }
    ActiveRoute = ETerraUIRoute::InGame;
    OnRouteChanged.Broadcast(ActiveRoute);
}

void UTerraUISubsystem::TogglePauseMenu()
{
    if (ActiveRoute == ETerraUIRoute::Paused)
    {
        ResumeGame();
        return;
    }
    if (ActiveRoute != ETerraUIRoute::InGame)
    {
        return;
    }
    if (APlanetInteractionController* InteractionController = Cast<APlanetInteractionController>(GetLocalPlayer()->GetPlayerController(GetWorld())))
    {
        InteractionController->CaptureCurrentViewForPause();
    }
    UGameplayStatics::SetGamePaused(GetWorld(), true);
    ShowRoute_(ETerraUIRoute::Paused, UTerraPauseMenuWidget::StaticClass());
}

void UTerraUISubsystem::ResumeGame()
{
    if (ActiveRoute != ETerraUIRoute::Paused)
    {
        return;
    }
    if (ActiveScreen)
    {
        ActiveScreen->RemoveFromParent();
        ActiveScreen = nullptr;
    }
    ApplyFrontEndInput_(false);
    UGameplayStatics::SetGamePaused(GetWorld(), false);
    ActiveRoute = ETerraUIRoute::InGame;
    OnRouteChanged.Broadcast(ActiveRoute);
}

void UTerraUISubsystem::CloseTechnologyChoice()
{
    if (ActiveRoute != ETerraUIRoute::TechnologyChoice)
    {
        return;
    }
    if (ActiveScreen)
    {
        ActiveScreen->RemoveFromParent();
        ActiveScreen = nullptr;
    }
    ApplyFrontEndInput_(false);
    ActiveRoute = ETerraUIRoute::InGame;
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
    else if (Route == ETerraUIRoute::Paused)
    {
        ConfiguredClass = Settings->PauseMenuWidgetClass;
    }
    else if (Route == ETerraUIRoute::TechnologyChoice)
    {
        ConfiguredClass = Settings->TechnologyChoiceWidgetClass;
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

void UTerraUISubsystem::EnsureInGameHUD_()
{
    if (InGameHUD)
    {
        return;
    }

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    APlayerController* PlayerController = LocalPlayer ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
    if (!PlayerController)
    {
        return;
    }

    const UTerraUISettings* Settings = GetDefault<UTerraUISettings>();
    TSubclassOf<UUserWidget> WidgetClass = UTerraInGameHUDWidget::StaticClass();
    if (!Settings->InGameHUDWidgetClass.IsNull())
    {
        WidgetClass = Settings->InGameHUDWidgetClass.LoadSynchronous();
    }
    if (!WidgetClass)
    {
        UE_LOG(LogTerraUI, Error, TEXT("[UI1] Could not resolve in-game HUD widget class."));
        return;
    }

    InGameHUD = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
    if (InGameHUD)
    {
        InGameHUD->AddToViewport(10);
    }
    BindGameplayTechnologyEvents_();
}

void UTerraUISubsystem::BindGameplayTechnologyEvents_()
{
    APlanetTessellatedMesh* Planet = Cast<APlanetTessellatedMesh>(
        UGameplayStatics::GetActorOfClass(GetWorld(), APlanetTessellatedMesh::StaticClass()));
    BoundGameplayComponent = Planet ? Planet->GetPlanetGameplayComponent() : nullptr;
    if (BoundGameplayComponent)
    {
        BoundGameplayComponent->OnTechnologyChoiceRequested.AddUniqueDynamic(this, &UTerraUISubsystem::HandleTechnologyChoiceRequested);
        BoundGameplayComponent->OnTechnologyStateChanged.AddUniqueDynamic(this, &UTerraUISubsystem::HandleTechnologyStateChanged);
    }
}

void UTerraUISubsystem::HandleTechnologyChoiceRequested(int32 FactionId)
{
    // Technology selection is only interactive for the local player faction. NPC factions
    // resolve their choices through their behavior tree and must never take over the HUD.
    if (FactionId != 0 || ActiveRoute == ETerraUIRoute::TechnologyChoice)
    {
        return;
    }
    ShowRoute_(ETerraUIRoute::TechnologyChoice, UTerraTechnologyChoiceWidget::StaticClass());
}

void UTerraUISubsystem::HandleTechnologyStateChanged()
{
    if (ActiveRoute != ETerraUIRoute::TechnologyChoice || !BoundGameplayComponent)
    {
        return;
    }

    const FTerraGameplayContainer* GameplayContainer = BoundGameplayComponent->GetGameplayContainer();
    const bool bPlayerStillChoosing = GameplayContainer
        && GameplayContainer->GetCurrentFactionId() == 0
        && GameplayContainer->IsFactionWaitingForTechnologyChoice(0);
    if (!bPlayerStillChoosing)
    {
        CloseTechnologyChoice();
    }
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
