#include "STerraSphericalTileGeneratorPanel.h"

#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "TerraSphericalTileGeneratorLibrary.h"
#include "TerraSphericalTileGeneratorSettings.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "STerraSphericalTileGeneratorPanel"

void STerraSphericalTileGeneratorPanel::Construct(const FArguments& InArgs)
{
	SettingsObject = GetMutableDefault<UTerraSphericalTileGeneratorSettings>();
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
				.Text(LOCTEXT("ReadyStatus", "Ready. Configure a shape and output asset path."))
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
				.Text(LOCTEXT("GenerateSelected", "Generate Selected"))
				.ToolTipText(LOCTEXT("GenerateSelectedTooltip", "Generate the AssetShape currently selected in the settings."))
				.HAlign(HAlign_Center)
				.OnClicked(this, &STerraSphericalTileGeneratorPanel::GenerateSelected)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(3.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("GenerateRidge", "Generate Ridge"))
				.ToolTipText(LOCTEXT("GenerateRidgeTooltip", "Generate the configured SV9 ridge asset."))
				.HAlign(HAlign_Center)
				.OnClicked(this, &STerraSphericalTileGeneratorPanel::GenerateRidge)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(3.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("GeneratePeak", "Generate Peak"))
				.ToolTipText(LOCTEXT("GeneratePeakTooltip", "Generate the configured SV9 peak asset."))
				.HAlign(HAlign_Center)
				.OnClicked(this, &STerraSphericalTileGeneratorPanel::GeneratePeak)
			]
		]
	];
}

FReply STerraSphericalTileGeneratorPanel::GenerateSelected()
{
	return SettingsObject ? GenerateShape(SettingsObject->BuildSettings.AssetShape) : GenerateShape(ETSTGAssetShape::Tile);
}

FReply STerraSphericalTileGeneratorPanel::GenerateRidge()
{
	return GenerateShape(ETSTGAssetShape::Ridge);
}

FReply STerraSphericalTileGeneratorPanel::GeneratePeak()
{
	return GenerateShape(ETSTGAssetShape::Peak);
}

FReply STerraSphericalTileGeneratorPanel::GenerateShape(ETSTGAssetShape Shape)
{
	if (SettingsObject == nullptr)
	{
		SetStatus(LOCTEXT("MissingSettings", "Generator settings are unavailable."), true);
		return FReply::Handled();
	}

	SettingsObject->BuildSettings.AssetShape = Shape;
	SettingsObject->SaveConfig();
	if (DetailsView.IsValid())
	{
		DetailsView->ForceRefresh();
	}

	FString Message;
	UStaticMesh* Mesh = UTerraSphericalTileGeneratorLibrary::GenerateSphericalTerrainStaticMeshAsset(
		SettingsObject->BuildSettings,
		Message);
	SetStatus(FText::FromString(Message), Mesh == nullptr);
	return FReply::Handled();
}

void STerraSphericalTileGeneratorPanel::SetStatus(const FText& Message, bool bIsError)
{
	if (StatusText.IsValid())
	{
		StatusText->SetText(Message);
		StatusText->SetColorAndOpacity(bIsError ? FLinearColor(1.0f, 0.25f, 0.2f) : FLinearColor(0.45f, 1.0f, 0.55f));
	}
}

#undef LOCTEXT_NAMESPACE
