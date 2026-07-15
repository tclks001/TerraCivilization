#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TerraGameplayTypes.h"
#include "TerraTutorialScenarioData.generated.h"

USTRUCT(BlueprintType)
struct FTerraTutorialPiecePlacement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    int32 PieceId = INDEX_NONE;

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    int32 CellId = INDEX_NONE;

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    int32 OwnerFactionId = INDEX_NONE;

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    ETerraGameplayPieceType PieceType = ETerraGameplayPieceType::Infantry;

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    bool bIsNeutral = false;

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    bool bCanMove = true;
};

USTRUCT(BlueprintType)
struct FTerraTutorialTerrainPlacement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    int32 CellId = INDEX_NONE;

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    ETerraGameplayTerrainType Terrain = ETerraGameplayTerrainType::Plain;
};

USTRUCT(BlueprintType)
struct FTerraTutorialEquipmentDropPlacement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    int32 CellId = INDEX_NONE;

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    bool bHasBow = false;

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    bool bHasHorse = false;
};

USTRUCT(BlueprintType)
struct FTerraTutorialCameraConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    int32 FocusCellId = INDEX_NONE;

    UPROPERTY(EditAnywhere, Category = "Tutorial", meta = (ClampMin = "0.0"))
    float DistanceToFocusCM = 8000.0f;
};

USTRUCT(BlueprintType)
struct FTerraTutorialNpcAction
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    int32 TurnIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    int32 PieceId = INDEX_NONE;

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    int32 TargetCellId = INDEX_NONE;
};

UCLASS(BlueprintType)
class TERRACIVILIZATION_API UTerraTutorialScenarioData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Tutorial")
    FName ScenarioId;

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    FText DisplayName;

    UPROPERTY(EditAnywhere, Category = "Tutorial", meta = (ClampMin = "1", ClampMax = "5"))
    int32 CellSubdivisionLevel = 3;

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    int32 InitialTurnFactionId = 0;

    UPROPERTY(EditAnywhere, Category = "Tutorial", meta = (ClampMin = "0"))
    int32 InitialTurnIndex = 0;

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    TArray<FTerraTutorialPiecePlacement> InitialPieces;

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    TArray<FTerraTutorialTerrainPlacement> TerrainOverrides;

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    TArray<FTerraTutorialEquipmentDropPlacement> InitialEquipmentDrops;

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    FTerraTutorialCameraConfig InitialCamera;

    UPROPERTY(EditAnywhere, Category = "Tutorial")
    TArray<FTerraTutorialNpcAction> NpcActionSequence;
};
