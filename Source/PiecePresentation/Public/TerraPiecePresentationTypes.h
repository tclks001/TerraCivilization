#pragma once

#include "CoreMinimal.h"
#include "TerraGameplayTypes.h"
#include "TerraPiecePresentationTypes.generated.h"

class USkeletalMesh;
class UAnimationAsset;

UENUM(BlueprintType)
enum class ETerraPieceAnimActionState : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    Move UMETA(DisplayName = "Move"),
    Jump UMETA(DisplayName = "Jump"),
    Attack UMETA(DisplayName = "Attack"),
    Hit UMETA(DisplayName = "Hit"),
    Death UMETA(DisplayName = "Death"),
};

UENUM(BlueprintType)
enum class ETerraPiecePresentationStepType : uint8
{
    Spawn UMETA(DisplayName = "Spawn"),
    Update UMETA(DisplayName = "Update"),
    Remove UMETA(DisplayName = "Remove"),
};

UENUM(BlueprintType)
enum class ETerraPiecePresentationMoveType : uint8
{
    None UMETA(DisplayName = "None"),
    Move UMETA(DisplayName = "Move"),
    Jump UMETA(DisplayName = "Jump"),
};

USTRUCT(BlueprintType)
struct PIECEPRESENTATION_API FTerraPieceAnimDriveState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    ETerraPieceAnimActionState ActionState = ETerraPieceAnimActionState::Idle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float MoveSpeed = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float NormalizedPhase = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FVector FacingDirection = FVector::ForwardVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bDead = false;
};

USTRUCT(BlueprintType)
struct PIECEPRESENTATION_API FTerraPieceVisualConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation")
    TObjectPtr<USkeletalMesh> CommanderMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation")
    TObjectPtr<USkeletalMesh> InfantryMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation")
    TObjectPtr<USkeletalMesh> CavalryMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation")
    TObjectPtr<USkeletalMesh> ArcherMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation", meta = (ClampMin = "0.001"))
    float UniformScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation")
    FVector MeshRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation")
    FRotator MeshRelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P2")
    TObjectPtr<UAnimationAsset> IdleAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P2")
    TObjectPtr<UAnimationAsset> MoveAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P2")
    TObjectPtr<UAnimationAsset> JumpAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P2", meta = (ClampMin = "0.001"))
    float MoveDurationSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P2", meta = (ClampMin = "0.001"))
    float JumpDurationSeconds = 0.55f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P2", meta = (ClampMin = "0.0"))
    float JumpHeightCM = 650.0f;

    USkeletalMesh* ResolveMesh(ETerraGameplayPieceType PieceType) const;
};

USTRUCT(BlueprintType)
struct PIECEPRESENTATION_API FTerraPiecePresentationSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 PieceId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 OwnerFactionId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 CellId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    ETerraGameplayPieceType PieceType = ETerraGameplayPieceType::Infantry;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FTransform WorldTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bForceFacingFromSnapshot = false;
};

USTRUCT(BlueprintType)
struct PIECEPRESENTATION_API FTerraPiecePresentationStep
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    ETerraPiecePresentationStepType StepType = ETerraPiecePresentationStepType::Update;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FTerraPiecePresentationSnapshot Snapshot;
};

USTRUCT(BlueprintType)
struct PIECEPRESENTATION_API FTerraPiecePresentationMoveEvent
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 PieceId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 FromCellId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 ToCellId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    ETerraPiecePresentationMoveType MoveType = ETerraPiecePresentationMoveType::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FTransform FromWorldTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FTransform ToWorldTransform = FTransform::Identity;

    bool IsValidMove() const
    {
        return PieceId != INDEX_NONE
            && FromCellId != INDEX_NONE
            && ToCellId != INDEX_NONE
            && FromCellId != ToCellId
            && MoveType != ETerraPiecePresentationMoveType::None;
    }
};
