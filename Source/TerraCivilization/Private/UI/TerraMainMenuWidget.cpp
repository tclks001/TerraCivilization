#include "UI/TerraMainMenuWidget.h"

#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/TerraUISubsystem.h"
#include "Blueprint/WidgetTree.h"

namespace TerraUI0
{
    UButton* AddMenuButton(UWidgetTree* Tree, UVerticalBox* Box, const FText& Label)
    {
        UButton* Button = Tree->ConstructWidget<UButton>();
        UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
        Text->SetText(Label);
        Text->SetJustification(ETextJustify::Center);
        Button->SetContent(Text);
        UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Button);
        Slot->SetPadding(FMargin(0.0f, 6.0f));
        Slot->SetHorizontalAlignment(HAlign_Fill);
        return Button;
    }
}

void UTerraMainMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!WidgetTree->RootWidget)
    {
        UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
        UVerticalBox* Menu = WidgetTree->ConstructWidget<UVerticalBox>();
        UCanvasPanelSlot* MenuSlot = Root->AddChildToCanvas(Menu);
        MenuSlot->SetAnchors(FAnchors(0.5f, 0.5f));
        MenuSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        MenuSlot->SetSize(FVector2D(340.0f, 420.0f));
        UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
        Title->SetText(FText::FromString(TEXT("Terra Civilization")));
        Title->SetJustification(ETextJustify::Center);
        Menu->AddChildToVerticalBox(Title);
        NewGameButton = TerraUI0::AddMenuButton(WidgetTree, Menu, FText::FromString(TEXT("新游戏")));
        TerraUI0::AddMenuButton(WidgetTree, Menu, FText::FromString(TEXT("继续游戏")))->SetIsEnabled(false);
        TerraUI0::AddMenuButton(WidgetTree, Menu, FText::FromString(TEXT("教学关")))->SetIsEnabled(false);
        TerraUI0::AddMenuButton(WidgetTree, Menu, FText::FromString(TEXT("历史记录")))->SetIsEnabled(false);
        TerraUI0::AddMenuButton(WidgetTree, Menu, FText::FromString(TEXT("设置")))->SetIsEnabled(false);
        QuitButton = TerraUI0::AddMenuButton(WidgetTree, Menu, FText::FromString(TEXT("退出")));
        WidgetTree->RootWidget = Root;
    }
}

void UTerraMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (NewGameButton)
    {
        NewGameButton->OnClicked.AddUniqueDynamic(this, &UTerraMainMenuWidget::HandleNewGameClicked);
        NewGameButton->SetKeyboardFocus();
    }
    if (QuitButton)
    {
        QuitButton->OnClicked.AddUniqueDynamic(this, &UTerraMainMenuWidget::HandleQuitClicked);
    }
}

void UTerraMainMenuWidget::HandleNewGameClicked()
{
    if (UTerraUISubsystem* UI = GetOwningLocalPlayer()->GetSubsystem<UTerraUISubsystem>())
    {
        UI->ShowNewGameSetup();
    }
}

void UTerraMainMenuWidget::HandleQuitClicked()
{
    if (APlayerController* PlayerController = GetOwningPlayer())
    {
        UKismetSystemLibrary::QuitGame(this, PlayerController, EQuitPreference::Quit, false);
    }
}
