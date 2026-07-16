#include "UI/TerraTechnologyChoiceWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Render/PlanetGameplayComponent.h"
#include "Render/PlanetTessellatedMesh.h"
#include "UI/TerraTechnologyChoiceButton.h"
#include "UI/TerraUISubsystem.h"

namespace TerraUI2
{
    const TCHAR* TechnologyName(ETerraGameplayTechnologyId TechnologyId)
    {
        switch (TechnologyId)
        {
        case ETerraGameplayTechnologyId::PlaceholderTraining: return TEXT("基础训练");
        case ETerraGameplayTechnologyId::PlaceholderLogistics: return TEXT("战地后勤");
        case ETerraGameplayTechnologyId::PlaceholderDoctrine: return TEXT("作战学说");
        default: return TEXT("未知科技");
        }
    }
}

void UTerraTechnologyChoiceWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree->RootWidget)
    {
        return;
    }
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>();
    UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
    PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    PanelSlot->SetSize(FVector2D(520.0f, 350.0f));
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(FText::FromString(TEXT("选择一项科技")));
    Title->SetJustification(ETextJustify::Center);
    Panel->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));

    if (UPlanetGameplayComponent* Gameplay = FindGameplay_())
    {
        FactionId = Gameplay->GetGameplayContainer() ? Gameplay->GetGameplayContainer()->GetCurrentFactionId() : INDEX_NONE;
        TArray<ETerraGameplayTechnologyId> Choices;
        if (FactionId != INDEX_NONE && Gameplay->GetGameplayContainer()->GetPendingTechnologyChoices(FactionId, Choices))
        {
            for (const ETerraGameplayTechnologyId TechnologyId : Choices)
            {
                UTerraTechnologyChoiceButton* Button = WidgetTree->ConstructWidget<UTerraTechnologyChoiceButton>();
                UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
                Label->SetText(FText::FromString(TerraUI2::TechnologyName(TechnologyId)));
                Label->SetJustification(ETextJustify::Center);
                Button->SetContent(Label);
                Button->InitializeTechnology(TechnologyId);
                Button->OnTechnologyClicked.AddUniqueDynamic(this, &UTerraTechnologyChoiceWidget::HandleTechnologyClicked);
                Panel->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 8.0f));
            }
        }
    }
    WidgetTree->RootWidget = Root;
}

void UTerraTechnologyChoiceWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
    SetKeyboardFocus();
}

void UTerraTechnologyChoiceWidget::HandleTechnologyClicked(ETerraGameplayTechnologyId TechnologyId)
{
    FString Error;
    if (UPlanetGameplayComponent* Gameplay = FindGameplay_(); Gameplay && Gameplay->ChoosePendingTechnology(FactionId, TechnologyId, Error))
    {
        if (UTerraUISubsystem* UI = GetOwningLocalPlayer()->GetSubsystem<UTerraUISubsystem>())
        {
            UI->CloseTechnologyChoice();
        }
    }
}

UPlanetGameplayComponent* UTerraTechnologyChoiceWidget::FindGameplay_() const
{
    APlanetTessellatedMesh* Planet = Cast<APlanetTessellatedMesh>(UGameplayStatics::GetActorOfClass(this, APlanetTessellatedMesh::StaticClass()));
    return Planet ? Planet->GetPlanetGameplayComponent() : nullptr;
}
