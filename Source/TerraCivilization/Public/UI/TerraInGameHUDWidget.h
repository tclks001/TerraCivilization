#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TerraGameplayTypes.h"
#include "TerraInGameHUDWidget.generated.h"

class UButton;
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
    FText FormatActionLogEntry_(const FTerraGameplayActionLogEntry& Entry) const;

    UPROPERTY(Transient)
    TObjectPtr<UButton> LogToggleButton;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> LogToggleText;

    UPROPERTY(Transient)
    TObjectPtr<UScrollBox> LogScrollBox;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> LogList;

    UPROPERTY(Transient)
    TObjectPtr<UWidget> LatestLogEntryWidget;

    UPROPERTY(Transient)
    TObjectPtr<UPlanetGameplayComponent> BoundGameplayComponent;

    bool bLogListExpanded = true;
    int32 PendingLogBottomScrollTicks = 0;
};
