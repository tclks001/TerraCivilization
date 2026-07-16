#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TerraGameplayTypes.h"
#include "TerraInGameHUDWidget.generated.h"

class UButton;
class UProgressBar;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UWidget;
class UPlanetGameplayComponent;

/** Native UI1 fallback for /Game/UI/Widgets/WBP_InGameHUD. */
UCLASS()
class TERRACIVILIZATION_API UTerraInGameHUDWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UFUNCTION()
    void HandleLogToggleClicked();

    UFUNCTION()
    void HandleActionLogCommitted(const FTerraGameplayActionLogEntry& Entry);

    void BindGameplay_();
    void AppendActionLogEntry_(const FTerraGameplayActionLogEntry& Entry);
    void ScrollLatestLogEntryToBottom_();
    void RefreshTechnologyPanels_();
    void RefreshFactionDetail_(int32 FactionId);
    FText FormatActionLogEntry_(const FTerraGameplayActionLogEntry& Entry) const;

    UFUNCTION()
    void HandleFactionPanelToggleClicked();

    UFUNCTION()
    void HandleFactionButtonClicked(int32 FactionId);

    UFUNCTION()
    void HandleTechnologyStateChanged();

    UPROPERTY(Transient)
    TObjectPtr<UButton> LogToggleButton;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> LogToggleText;

    UPROPERTY(Transient)
    TObjectPtr<UScrollBox> LogScrollBox;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> LogList;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> PlayerProgressText;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> PlayerProgressBar;

    UPROPERTY(Transient)
    TObjectPtr<UButton> FactionPanelToggleButton;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> FactionPanelToggleText;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> FactionPanel;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> FactionList;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> FactionDetailText;

    UPROPERTY(Transient)
    TObjectPtr<UWidget> LatestLogEntryWidget;

    UPROPERTY(Transient)
    TObjectPtr<UPlanetGameplayComponent> BoundGameplayComponent;

    bool bLogListExpanded = true;
    bool bFactionPanelExpanded = true;
    int32 SelectedFactionId = 0;
    int32 PendingLogBottomScrollTicks = 0;
};
