#pragma once

#include "CoreMinimal.h"
#include "TerraGameplayTypes.generated.h"

UENUM(BlueprintType)
enum class ETerraGameplayTerrainType : uint8
{
    Plain UMETA(DisplayName = "Plain"),
    Forest UMETA(DisplayName = "Forest"),
    Mountain UMETA(DisplayName = "Mountain"),
};

UENUM(BlueprintType)
enum class ETerraGameplayPieceType : uint8
{
    Commander UMETA(DisplayName = "Commander"),
    Infantry UMETA(DisplayName = "Infantry"),
    Cavalry UMETA(DisplayName = "Cavalry"),
    Archer UMETA(DisplayName = "Archer"),
    ArcherCavalry UMETA(DisplayName = "Archer Cavalry"),
};

UENUM(BlueprintType)
enum class ETerraGameplayEquipmentDropType : uint8
{
    None UMETA(DisplayName = "None"),
    Bow UMETA(DisplayName = "Bow"),
    Horse UMETA(DisplayName = "Horse"),
};

UENUM(BlueprintType)
enum class ETerraGameplayInteractionPhase : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    PieceSelected UMETA(DisplayName = "Piece Selected"),
    PieceMovedCanEndTurn UMETA(DisplayName = "Piece Moved Can End Turn"),
    PieceJumpingCanContinue UMETA(DisplayName = "Piece Jumping Can Continue"),
};

USTRUCT(BlueprintType)
struct GAMEPLAY_API FTerraGameplayNeutralSpawnConfig
{
    GENERATED_BODY()

    int32 CommanderCount = 20;
    int32 InfantryCount = 20;
    int32 CavalryCount = 20;
    int32 ArcherCount = 20;
    int32 SpawnSeed = 0;
};

USTRUCT(BlueprintType)
struct GAMEPLAY_API FTerraGameplayCellState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 CellId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bIsPentagon = false;

    TStaticArray<int32, 6> NeighborCellIds{INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    ETerraGameplayTerrainType TerrainType = ETerraGameplayTerrainType::Plain;
};

USTRUCT(BlueprintType)
struct GAMEPLAY_API FTerraGameplayPieceState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 PieceId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 OwnerFactionId = INDEX_NONE;

    /** Neutral pieces have no permanent faction and temporarily support the active faction. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bIsNeutral = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 CellId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    ETerraGameplayPieceType PieceType = ETerraGameplayPieceType::Infantry;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bAlive = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bCanMove = true;
};

USTRUCT(BlueprintType)
struct GAMEPLAY_API FTerraGameplayFactionState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 FactionId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 BaseCellId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 CommanderPieceId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bAlive = true;
};

USTRUCT(BlueprintType)
struct GAMEPLAY_API FTerraGameplayEquipmentDropState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 CellId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bHasBow = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bHasHorse = false;
};

USTRUCT(BlueprintType)
struct GAMEPLAY_API FTerraGameplayCellHighlight
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FLinearColor Color = FLinearColor::Black;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float Intensity = 0.0f;

    bool IsActive() const
    {
        return Intensity > KINDA_SMALL_NUMBER;
    }
};

USTRUCT(BlueprintType)
struct GAMEPLAY_API FTerraGameplayCaptureEntry
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 CapturedPieceId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 CapturedCellId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 AttackerPieceId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 VanguardPieceId = INDEX_NONE;
};

