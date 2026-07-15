#include "UI/TerraInGameHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Render/PlanetGameplayComponent.h"
#include "Render/PlanetTessellatedMesh.h"

namespace TerraUI1
{
    const TCHAR* PieceTypeName(ETerraGameplayPieceType PieceType)
    {
        switch (PieceType)
        {
        case ETerraGameplayPieceType::Commander: return TEXT("主将");
        case ETerraGameplayPieceType::Infantry: return TEXT("步兵");
        case ETerraGameplayPieceType::Cavalry: return TEXT("骑兵");
        case ETerraGameplayPieceType::Archer: return TEXT("弓兵");
        case ETerraGameplayPieceType::ArcherCavalry: return TEXT("弓骑兵");
        default: return TEXT("未知兵种");
        }
    }
}

void UTerraInGameHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree->RootWidget)
    {
        return;
    }

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    // The HUD covers the viewport visually, but empty space must not block the planet's cursor trace.
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
    PanelSlot->SetAnchors(FAnchors(1.0f, 0.15f, 1.0f, 0.15f));
    PanelSlot->SetAlignment(FVector2D(1.0f, 0.0f));
    PanelSlot->SetPosition(FVector2D(-24.0f, 0.0f));
    PanelSlot->SetSize(FVector2D(390.0f, 540.0f));

    LogToggleButton = WidgetTree->ConstructWidget<UButton>();
    LogToggleText = WidgetTree->ConstructWidget<UTextBlock>();
    LogToggleText->SetJustification(ETextJustify::Center);
    LogToggleButton->SetContent(LogToggleText);
    Panel->AddChildToVerticalBox(LogToggleButton)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

    LogScrollBox = WidgetTree->ConstructWidget<UScrollBox>();
    LogList = WidgetTree->ConstructWidget<UVerticalBox>();
    LogScrollBox->AddChild(LogList);
    Panel->AddChildToVerticalBox(LogScrollBox)->SetSize(ESlateSizeRule::Fill);

    WidgetTree->RootWidget = Root;
}

void UTerraInGameHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (LogToggleButton)
    {
        LogToggleButton->OnClicked.AddUniqueDynamic(this, &UTerraInGameHUDWidget::HandleLogToggleClicked);
    }
    bLogListExpanded = false;
    HandleLogToggleClicked();
    BindGameplay_();
}

void UTerraInGameHUDWidget::NativeDestruct()
{
    if (BoundGameplayComponent)
    {
        BoundGameplayComponent->OnActionLogCommitted.RemoveDynamic(this, &UTerraInGameHUDWidget::HandleActionLogCommitted);
        BoundGameplayComponent = nullptr;
    }
    Super::NativeDestruct();
}

void UTerraInGameHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (PendingLogBottomScrollTicks > 0)
    {
        ScrollLatestLogEntryToBottom_();
        --PendingLogBottomScrollTicks;
    }
}

void UTerraInGameHUDWidget::HandleLogToggleClicked()
{
    bLogListExpanded = !bLogListExpanded;
    if (LogList)
    {
        LogList->SetVisibility(bLogListExpanded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (LogToggleText)
    {
        LogToggleText->SetText(FText::FromString(bLogListExpanded ? TEXT("行动日志  收起") : TEXT("行动日志  展开")));
    }
}

void UTerraInGameHUDWidget::HandleActionLogCommitted(const FTerraGameplayActionLogEntry& Entry)
{
    AppendActionLogEntry_(Entry);
}

void UTerraInGameHUDWidget::BindGameplay_()
{
    APlanetTessellatedMesh* Planet = Cast<APlanetTessellatedMesh>(
        UGameplayStatics::GetActorOfClass(this, APlanetTessellatedMesh::StaticClass()));
    BoundGameplayComponent = Planet ? Planet->GetPlanetGameplayComponent() : nullptr;
    if (!BoundGameplayComponent)
    {
        return;
    }

    BoundGameplayComponent->OnActionLogCommitted.AddUniqueDynamic(this, &UTerraInGameHUDWidget::HandleActionLogCommitted);
    TArray<FTerraGameplayActionLogEntry> ExistingEntries;
    BoundGameplayComponent->CollectCommittedActionLogEntries(ExistingEntries);
    for (const FTerraGameplayActionLogEntry& Entry : ExistingEntries)
    {
        AppendActionLogEntry_(Entry);
    }
}

void UTerraInGameHUDWidget::AppendActionLogEntry_(const FTerraGameplayActionLogEntry& Entry)
{
    if (!LogList)
    {
        return;
    }
    UTextBlock* EntryText = WidgetTree->ConstructWidget<UTextBlock>();
    EntryText->SetAutoWrapText(true);
    EntryText->SetText(FormatActionLogEntry_(Entry));
    LogList->AddChildToVerticalBox(EntryText)->SetPadding(FMargin(6.0f, 4.0f));
    LatestLogEntryWidget = EntryText;
    // Auto-wrap needs multiple Slate layout passes before the final child height is stable.
    PendingLogBottomScrollTicks = 3;
}

void UTerraInGameHUDWidget::ScrollLatestLogEntryToBottom_()
{
    if (LogScrollBox && LatestLogEntryWidget)
    {
        LogList->ForceLayoutPrepass();
        LogScrollBox->ForceLayoutPrepass();
        LogScrollBox->SetScrollOffset(LogScrollBox->GetScrollOffsetOfEnd());
    }
}

FText UTerraInGameHUDWidget::FormatActionLogEntry_(const FTerraGameplayActionLogEntry& Entry) const
{
    const int32 FromCellId = Entry.PathCellIds.Num() > 0 ? Entry.PathCellIds[0] : INDEX_NONE;
    const int32 ToCellId = Entry.PathCellIds.Num() > 0 ? Entry.PathCellIds.Last() : INDEX_NONE;
    FString Text = FString::Printf(TEXT("第 %d 回合：阵营 %d 移动%s %d，从地块 %d 到地块 %d"),
        Entry.TurnIndex + 1,
        Entry.PlayerId + 1,
        TerraUI1::PieceTypeName(Entry.ActionPieceType),
        Entry.PieceId,
        FromCellId,
        ToCellId);
    if (Entry.PathCellIds.Num() > 2)
    {
        Text += FString::Printf(TEXT("，进行了 %d 段跳跃"), Entry.PathCellIds.Num() - 1);
    }
    if (Entry.bPromoted)
    {
        Text += FString::Printf(TEXT("，升变成%s"), TerraUI1::PieceTypeName(Entry.PromotedPieceType));
    }
    for (const FTerraGameplayCapturedPieceLog& CapturedPiece : Entry.CapturedPieces)
    {
        Text += FString::Printf(TEXT("，击杀了阵营 %d 的%s"),
            CapturedPiece.FactionId + 1,
            TerraUI1::PieceTypeName(CapturedPiece.PieceType));
    }
    for (const int32 DefeatedFactionId : Entry.DefeatedFactionIds)
    {
        Text += FString::Printf(TEXT("，击败了阵营 %d"), DefeatedFactionId + 1);
    }
    Text += TEXT("。");
    return FText::FromString(Text);
}
