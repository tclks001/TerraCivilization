#include "UI/TerraNewGameSetupWidget.h"

#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/TerraUISubsystem.h"
#include "Blueprint/WidgetTree.h"

namespace TerraUI0
{
    UEditableTextBox* AddIntegerField(UWidgetTree* Tree, UVerticalBox* Box, const TCHAR* Label, int32 InitialValue)
    {
        UTextBlock* LabelWidget = Tree->ConstructWidget<UTextBlock>();
        LabelWidget->SetText(FText::FromString(Label));
        Box->AddChildToVerticalBox(LabelWidget);
        UEditableTextBox* Field = Tree->ConstructWidget<UEditableTextBox>();
        Field->SetText(FText::AsNumber(InitialValue));
        Box->AddChildToVerticalBox(Field)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
        return Field;
    }
    UButton* AddActionButton(UWidgetTree* Tree, UVerticalBox* Box, const TCHAR* Label)
    {
        UButton* Button = Tree->ConstructWidget<UButton>();
        UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Label));
        Text->SetJustification(ETextJustify::Center);
        Button->SetContent(Text);
        Box->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 6.0f));
        return Button;
    }
}

void UTerraNewGameSetupWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!WidgetTree->RootWidget)
    {
        UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
        UVerticalBox* Form = WidgetTree->ConstructWidget<UVerticalBox>();
        UCanvasPanelSlot* FormSlot = Root->AddChildToCanvas(Form);
        FormSlot->SetAnchors(FAnchors(0.5f, 0.5f));
        FormSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        FormSlot->SetSize(FVector2D(440.0f, 380.0f));
        UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
        Title->SetText(FText::FromString(TEXT("新游戏")));
        Title->SetJustification(ETextJustify::Center);
        Form->AddChildToVerticalBox(Title);
        SeedTextBox = TerraUI0::AddIntegerField(WidgetTree, Form, TEXT("随机种子"), 0);
        MountainStripCountTextBox = TerraUI0::AddIntegerField(WidgetTree, Form, TEXT("山脉条带数量"), 8);
        ForestPatchCountTextBox = TerraUI0::AddIntegerField(WidgetTree, Form, TEXT("森林块数量"), 20);
        UButton* StartButton = TerraUI0::AddActionButton(WidgetTree, Form, TEXT("开始游戏"));
        UButton* BackButton = TerraUI0::AddActionButton(WidgetTree, Form, TEXT("返回"));
        StartButton->OnClicked.AddUniqueDynamic(this, &UTerraNewGameSetupWidget::HandleStartClicked);
        BackButton->OnClicked.AddUniqueDynamic(this, &UTerraNewGameSetupWidget::HandleBackClicked);
        WidgetTree->RootWidget = Root;
    }
}

void UTerraNewGameSetupWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (SeedTextBox)
    {
        SeedTextBox->SetKeyboardFocus();
    }
}

void UTerraNewGameSetupWidget::HandleStartClicked()
{
    int32 Seed = 0;
    int32 MountainStripCount = 0;
    int32 ForestPatchCount = 0;
    if (!ReadInt_(SeedTextBox, Seed) || !ReadInt_(MountainStripCountTextBox, MountainStripCount) || !ReadInt_(ForestPatchCountTextBox, ForestPatchCount))
    {
        SetValidationMessage_(FText::FromString(TEXT("请输入有效的整数。")), true);
        return;
    }
    if (MountainStripCount < 0 || ForestPatchCount < 0)
    {
        SetValidationMessage_(FText::FromString(TEXT("地形数量不能为负数。")), true);
        return;
    }
    FTerraNewGameConfig Config;
    Config.WorldGenSettings.RandomSeed = Seed;
    Config.WorldGenSettings.MountainStripCount = MountainStripCount;
    Config.WorldGenSettings.ForestPatchCount = ForestPatchCount;
    if (UTerraUISubsystem* UI = GetOwningLocalPlayer()->GetSubsystem<UTerraUISubsystem>())
    {
        if (!UI->StartNewGame(Config))
        {
            SetValidationMessage_(FText::FromString(TEXT("当前地图没有可重建的星球棋盘。")), true);
        }
    }
}

void UTerraNewGameSetupWidget::HandleBackClicked()
{
    if (UTerraUISubsystem* UI = GetOwningLocalPlayer()->GetSubsystem<UTerraUISubsystem>())
    {
        UI->ShowMainMenu();
    }
}

bool UTerraNewGameSetupWidget::ReadInt_(const UEditableTextBox* TextBox, int32& OutValue) const
{
    return TextBox && LexTryParseString(OutValue, *TextBox->GetText().ToString());
}

void UTerraNewGameSetupWidget::SetValidationMessage_(const FText& Message, bool bIsError)
{
    if (bIsError)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UI0] %s"), *Message.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[UI0] %s"), *Message.ToString());
    }
}
