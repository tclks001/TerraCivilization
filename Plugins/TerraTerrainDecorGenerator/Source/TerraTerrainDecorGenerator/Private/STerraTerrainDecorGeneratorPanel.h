#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class STextBlock;
class UTerraTerrainDecorGeneratorSettings;

class STerraTerrainDecorGeneratorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STerraTerrainDecorGeneratorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply GenerateSingle();
	FReply GenerateBatch();
	void SetStatus(const FText& Message, bool bIsError);

	TObjectPtr<UTerraTerrainDecorGeneratorSettings> SettingsObject = nullptr;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<STextBlock> StatusText;
};
