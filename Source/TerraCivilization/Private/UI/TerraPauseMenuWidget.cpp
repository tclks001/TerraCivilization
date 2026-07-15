#include "UI/TerraPauseMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/TerraUISubsystem.h"

void UTerraPauseMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree->RootWidget)
    {
        return;
    }
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    UVerticalBox* Menu = WidgetTree->ConstructWidget<UVerticalBox>();
    UCanvasPanelSlot* MenuSlot = Root->AddChildToCanvas(Menu);
    MenuSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    MenuSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    MenuSlot->SetSize(FVector2D(300.0f, 180.0f));
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(FText::FromString(TEXT("暂停")));
    Title->SetJustification(ETextJustify::Center);
    Menu->AddChildToVerticalBox(Title);
    ResumeButton = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* ResumeText = WidgetTree->ConstructWidget<UTextBlock>();
    ResumeText->SetText(FText::FromString(TEXT("继续")));
    ResumeText->SetJustification(ETextJustify::Center);
    ResumeButton->SetContent(ResumeText);
    Menu->AddChildToVerticalBox(ResumeButton)->SetPadding(FMargin(0.0f, 12.0f));
    WidgetTree->RootWidget = Root;
}

void UTerraPauseMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (ResumeButton)
    {
        ResumeButton->OnClicked.AddUniqueDynamic(this, &UTerraPauseMenuWidget::HandleResumeClicked);
        ResumeButton->SetKeyboardFocus();
    }
}

void UTerraPauseMenuWidget::HandleResumeClicked()
{
    if (UTerraUISubsystem* UI = GetOwningLocalPlayer()->GetSubsystem<UTerraUISubsystem>())
    {
        UI->ResumeGame();
    }
}
