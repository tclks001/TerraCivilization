#include "STerraTerrainDecorGeneratorPanel.h"

#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "TerraTerrainDecorGeneratorLibrary.h"
#include "TerraTerrainDecorGeneratorSettings.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "STerraTerrainDecorGeneratorPanel"

void STerraTerrainDecorGeneratorPanel::Construct(const FArguments& InArgs)
{
	SettingsObject = GetMutableDefault<UTerraTerrainDecorGeneratorSettings>();
	SettingsObject->LoadConfig();

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bLockable = false;
	DetailsArgs.bUpdatesFromSelection = false;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsArgs.NotifyHook = nullptr;

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	DetailsView = PropertyEditor.CreateDetailView(DetailsArgs);
	DetailsView->SetObject(SettingsObject);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(6.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				DetailsView.ToSharedRef()
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.0f, 0.0f, 6.0f, 6.0f)
		[
			SNew(SBorder)
			.Padding(6.0f)
			[
				SAssignNew(StatusText, STextBlock)
				.Text(LOCTEXT("ReadyStatus", "Ready. Existing assets are never overwritten."))
				.AutoWrapText(true)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.0f, 0.0f, 6.0f, 6.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f, 0.0f, 3.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("GenerateSingle", "Generate First Variant"))
				.ToolTipText(LOCTEXT("GenerateSingleTooltip", "Generate only FirstVariantIndex with the configured seed."))
				.HAlign(HAlign_Center)
				.OnClicked(this, &STerraTerrainDecorGeneratorPanel::GenerateSingle)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(3.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("GenerateBatch", "Generate Ridge Batch"))
				.ToolTipText(LOCTEXT("GenerateBatchTooltip", "Generate VariantCount assets starting at FirstVariantIndex."))
				.HAlign(HAlign_Center)
				.OnClicked(this, &STerraTerrainDecorGeneratorPanel::GenerateBatch)
			]
		]
	];
}

FReply STerraTerrainDecorGeneratorPanel::GenerateSingle()
{
	if (SettingsObject == nullptr)
	{
		SetStatus(LOCTEXT("MissingSettings", "Generator settings are unavailable."), true);
		return FReply::Handled();
	}

	SettingsObject->SaveConfig();
	FString Message;
	UStaticMesh* Mesh = UTerraTerrainDecorGeneratorLibrary::GenerateRidgeStaticMeshAsset(
		SettingsObject->RidgeBuildSettings,
		SettingsObject->RidgeBuildSettings.Output.FirstVariantIndex,
		Message);
	SetStatus(FText::FromString(Message), Mesh == nullptr);
	return FReply::Handled();
}

FReply STerraTerrainDecorGeneratorPanel::GenerateBatch()
{
	if (SettingsObject == nullptr)
	{
		SetStatus(LOCTEXT("MissingSettings", "Generator settings are unavailable."), true);
		return FReply::Handled();
	}

	SettingsObject->SaveConfig();
	TArray<UStaticMesh*> Assets;
	FString Message;
	const bool bSuccess = UTerraTerrainDecorGeneratorLibrary::GenerateRidgeStaticMeshAssets(
		SettingsObject->RidgeBuildSettings,
		Assets,
		Message);
	SetStatus(FText::FromString(Message), !bSuccess);
	return FReply::Handled();
}

void STerraTerrainDecorGeneratorPanel::SetStatus(const FText& Message, bool bIsError)
{
	if (StatusText.IsValid())
	{
		StatusText->SetText(Message);
		StatusText->SetColorAndOpacity(bIsError ? FLinearColor(1.0f, 0.25f, 0.2f) : FLinearColor(0.45f, 1.0f, 0.55f));
	}
}

#undef LOCTEXT_NAMESPACE
