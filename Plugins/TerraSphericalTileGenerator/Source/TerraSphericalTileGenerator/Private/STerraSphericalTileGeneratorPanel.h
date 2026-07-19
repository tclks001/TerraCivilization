#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class STextBlock;
class UTerraSphericalTileGeneratorSettings;
enum class ETSTGAssetShape : uint8;

class STerraSphericalTileGeneratorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STerraSphericalTileGeneratorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply GenerateSelected();
	FReply GenerateRidge();
	FReply GeneratePeak();
	FReply GenerateShape(ETSTGAssetShape Shape);
	void SetStatus(const FText& Message, bool bIsError);

	TObjectPtr<UTerraSphericalTileGeneratorSettings> SettingsObject = nullptr;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<STextBlock> StatusText;
};
