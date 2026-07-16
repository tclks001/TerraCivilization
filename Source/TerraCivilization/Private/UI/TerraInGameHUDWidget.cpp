#include "UI/TerraInGameHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Render/PlanetGameplayComponent.h"
#include "Render/PlanetTessellatedMesh.h"
#include "UI/TerraFactionStatusButton.h"

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

    UVerticalBox* ProgressPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    UCanvasPanelSlot* ProgressSlot = Root->AddChildToCanvas(ProgressPanel);
    ProgressSlot->SetAnchors(FAnchors(0.5f, 0.0f));
    ProgressSlot->SetAlignment(FVector2D(0.5f, 0.0f));
    ProgressSlot->SetPosition(FVector2D(0.0f, 20.0f));
    ProgressSlot->SetSize(FVector2D(400.0f, 72.0f));
    PlayerProgressText = WidgetTree->ConstructWidget<UTextBlock>();
    PlayerProgressText->SetJustification(ETextJustify::Center);
    ProgressPanel->AddChildToVerticalBox(PlayerProgressText);
    PlayerProgressBar = WidgetTree->ConstructWidget<UProgressBar>();
    ProgressPanel->AddChildToVerticalBox(PlayerProgressBar)->SetPadding(FMargin(0.0f, 6.0f));

    UVerticalBox* LeftRoot = WidgetTree->ConstructWidget<UVerticalBox>();
    UCanvasPanelSlot* LeftSlot = Root->AddChildToCanvas(LeftRoot);
    LeftSlot->SetAnchors(FAnchors(0.0f, 0.15f));
    LeftSlot->SetPosition(FVector2D(24.0f, 0.0f));
    LeftSlot->SetSize(FVector2D(330.0f, 500.0f));
    FactionPanelToggleButton = WidgetTree->ConstructWidget<UButton>();
    FactionPanelToggleText = WidgetTree->ConstructWidget<UTextBlock>();
    FactionPanelToggleText->SetJustification(ETextJustify::Center);
    FactionPanelToggleButton->SetContent(FactionPanelToggleText);
    LeftRoot->AddChildToVerticalBox(FactionPanelToggleButton);
    FactionPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    LeftRoot->AddChildToVerticalBox(FactionPanel)->SetSize(ESlateSizeRule::Fill);
    FactionList = WidgetTree->ConstructWidget<UVerticalBox>();
    FactionPanel->AddChildToVerticalBox(FactionList);
    FactionDetailText = WidgetTree->ConstructWidget<UTextBlock>();
    FactionDetailText->SetAutoWrapText(true);
    FactionPanel->AddChildToVerticalBox(FactionDetailText)->SetPadding(FMargin(0.0f, 12.0f));

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
    if (FactionPanelToggleButton)
    {
        FactionPanelToggleButton->OnClicked.AddUniqueDynamic(this, &UTerraInGameHUDWidget::HandleFactionPanelToggleClicked);
    }
    bLogListExpanded = false;
    HandleLogToggleClicked();
    bFactionPanelExpanded = false;
    HandleFactionPanelToggleClicked();
    BindGameplay_();
    RefreshTechnologyPanels_();
}

void UTerraInGameHUDWidget::NativeDestruct()
{
    if (BoundGameplayComponent)
    {
        BoundGameplayComponent->OnActionLogCommitted.RemoveDynamic(this, &UTerraInGameHUDWidget::HandleActionLogCommitted);
        BoundGameplayComponent->OnTechnologyStateChanged.RemoveDynamic(this, &UTerraInGameHUDWidget::HandleTechnologyStateChanged);
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
    BoundGameplayComponent->OnTechnologyStateChanged.AddUniqueDynamic(this, &UTerraInGameHUDWidget::HandleTechnologyStateChanged);
    TArray<FTerraGameplayActionLogEntry> ExistingEntries;
    BoundGameplayComponent->CollectCommittedActionLogEntries(ExistingEntries);
    for (const FTerraGameplayActionLogEntry& Entry : ExistingEntries)
    {
        AppendActionLogEntry_(Entry);
    }
    RefreshTechnologyPanels_();
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

void UTerraInGameHUDWidget::HandleFactionPanelToggleClicked()
{
    bFactionPanelExpanded = !bFactionPanelExpanded;
    if (FactionPanel)
    {
        FactionPanel->SetVisibility(bFactionPanelExpanded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (FactionPanelToggleText)
    {
        FactionPanelToggleText->SetText(FText::FromString(bFactionPanelExpanded ? TEXT("阵营总览  收起") : TEXT("阵营总览  展开")));
    }
}

void UTerraInGameHUDWidget::HandleFactionButtonClicked(int32 FactionId)
{
    SelectedFactionId = FactionId;
    RefreshFactionDetail_(FactionId);
}

void UTerraInGameHUDWidget::HandleTechnologyStateChanged()
{
    RefreshTechnologyPanels_();
}

void UTerraInGameHUDWidget::RefreshTechnologyPanels_()
{
    if (!BoundGameplayComponent)
    {
        return;
    }
    const FTerraGameplayTechnologyProgressionConfig Config = BoundGameplayComponent->GetTechnologyProgressionConfig();
    FTerraGameplayFactionTechnologyState PlayerState;
    if (BoundGameplayComponent->GetFactionTechnologyState(/*FactionId=*/0, PlayerState))
    {
        const int32 CurrentScore = PlayerState.AccumulatedScore + PlayerState.ScoreEarnedThisTurn;
        const int32 LevelStartScore = PlayerState.UnlockCount <= 0
            ? 0
            : FMath::Max(0, PlayerState.NextUnlockScore - Config.TechnologyUnlockScoreIncrement);
        const int32 LevelSpan = FMath::Max(1, PlayerState.NextUnlockScore - LevelStartScore);
        const float Progress = FMath::Clamp(static_cast<float>(CurrentScore - LevelStartScore) / LevelSpan, 0.0f, 1.0f);
        if (PlayerProgressText)
        {
            PlayerProgressText->SetText(FText::FromString(FString::Printf(TEXT("阵营 1  等级 %d  |  %d / %d"), PlayerState.UnlockCount + 1, CurrentScore, PlayerState.NextUnlockScore)));
        }
        if (PlayerProgressBar)
        {
            PlayerProgressBar->SetPercent(Progress);
        }
    }

    TArray<FTerraGameplayFactionState> Factions;
    BoundGameplayComponent->CollectFactionStates(Factions);
    if (FactionList)
    {
        FactionList->ClearChildren();
        for (const FTerraGameplayFactionState& Faction : Factions)
        {
            UTerraFactionStatusButton* Button = WidgetTree->ConstructWidget<UTerraFactionStatusButton>();
            UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
            Label->SetText(FText::FromString(FString::Printf(TEXT("阵营 %d  %s"), Faction.FactionId + 1, Faction.bAlive ? TEXT("存活") : TEXT("已败"))));
            Label->SetJustification(ETextJustify::Center);
            Button->SetContent(Label);
            Button->InitializeFaction(Faction.FactionId);
            Button->OnFactionClicked.AddUniqueDynamic(this, &UTerraInGameHUDWidget::HandleFactionButtonClicked);
            FactionList->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 2.0f));
        }
    }
    RefreshFactionDetail_(SelectedFactionId);
}

void UTerraInGameHUDWidget::RefreshFactionDetail_(int32 FactionId)
{
    if (!FactionDetailText || !BoundGameplayComponent)
    {
        return;
    }
    FTerraGameplayFactionTechnologyState State;
    TArray<FTerraGameplayFactionState> Factions;
    BoundGameplayComponent->CollectFactionStates(Factions);
    const FTerraGameplayFactionState* Faction = Factions.FindByPredicate([FactionId](const FTerraGameplayFactionState& Candidate)
    {
        return Candidate.FactionId == FactionId;
    });
    if (!Faction || !BoundGameplayComponent->GetFactionTechnologyState(FactionId, State))
    {
        FactionDetailText->SetText(FText::FromString(TEXT("未找到阵营信息。")));
        return;
    }
    FString Details = FString::Printf(TEXT("阵营 %d\n状态：%s\n累计分数：%d\n已持有科技："),
        FactionId + 1,
        Faction->bAlive ? TEXT("存活") : TEXT("已败"),
        State.AccumulatedScore);
    if (State.OwnedTechnologies.IsEmpty())
    {
        Details += TEXT("无");
    }
    else
    {
        for (const ETerraGameplayTechnologyId TechnologyId : State.OwnedTechnologies)
        {
            Details += FString::Printf(TEXT("\n- %s"), TerraUI1::TechnologyName(TechnologyId));
        }
    }
    FactionDetailText->SetText(FText::FromString(Details));
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
