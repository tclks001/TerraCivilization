#pragma once

#include "CoreMinimal.h"
#include "TerraGameplayTypes.h"
#include "TerraPiecePresentationTypes.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UAnimMontage;
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.1")
    TObjectPtr<USkeletalMesh> HorseMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.1")
    TObjectPtr<UAnimationAsset> HorseIdleAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.2")
    TObjectPtr<UAnimationAsset> HorseMoveAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.2")
    TObjectPtr<UAnimationAsset> HorseJumpAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.2", meta = (ClampMin = "0.001"))
    float HorseJumpPlayRateScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.3")
    TObjectPtr<UAnimationAsset> HorseDeathAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.1")
    TObjectPtr<UAnimationAsset> RiderSittingAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.4")
    TSubclassOf<UAnimInstance> RiderAnimInstanceClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.4")
    TObjectPtr<UAnimMontage> RiderUpperBodyAttackMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.4")
    TObjectPtr<UAnimMontage> RiderUpperBodyHitMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.5")
    FName RiderSaddleAttachName = TEXT("Torso");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.1")
    FVector HorseRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.1")
    FRotator HorseRelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.1", meta = (ClampMin = "0.001"))
    float HorseUniformScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.1")
    FVector RiderRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.1")
    FRotator RiderRelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.1", meta = (ClampMin = "0.001"))
    float RiderUniformScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.3")
    FVector RiderDeathRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P4.3")
    FRotator RiderDeathRelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P2")
    TObjectPtr<UAnimationAsset> IdleAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P2")
    TObjectPtr<UAnimationAsset> MoveAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P2")
    TObjectPtr<UAnimationAsset> JumpAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3")
    TObjectPtr<UAnimationAsset> CommanderMagicAttackAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3")
    TObjectPtr<UAnimationAsset> ArcherRangedAttackAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3")
    TObjectPtr<UAnimationAsset> InfantryMeleeAttackAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3")
    TObjectPtr<UAnimationAsset> CavalryMeleeAttackAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3")
    TObjectPtr<UAnimationAsset> HitAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3")
    TObjectPtr<UAnimationAsset> DeathAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P2", meta = (ClampMin = "0.001"))
    float MoveDurationSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P2", meta = (ClampMin = "0.001"))
    float JumpDurationSeconds = 0.55f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P2", meta = (ClampMin = "0.0"))
    float JumpHeightCM = 650.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3", meta = (ClampMin = "0.0"))
    float P3FacingBlendSeconds = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3", meta = (ClampMin = "0.001"))
    float P3MeleeRunInSeconds = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3", meta = (ClampMin = "0.001"))
    float P3MeleeReturnSeconds = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3", meta = (ClampMin = "0.0"))
    float P3AttackAnimationStartOffsetSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3", meta = (ClampMin = "0.0"))
    float P3CommanderAttackToHitSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3", meta = (ClampMin = "0.0"))
    float P3ArcherAttackToHitSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3", meta = (ClampMin = "0.0"))
    float P3InfantryAttackToHitSeconds = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3", meta = (ClampMin = "0.0"))
    float P3CavalryAttackToHitSeconds = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3", meta = (ClampMin = "0.0"))
    float P3HitReactDelaySeconds = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3", meta = (ClampMin = "0.0"))
    float P3HitAnimationStartOffsetSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3", meta = (ClampMin = "0.0"))
    float P3DeathAfterHitDelaySeconds = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3", meta = (ClampMin = "0.0"))
    float P3DeathAnimationStartOffsetSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3", meta = (ClampMin = "0.0"))
    float P3CapturedFadeSeconds = 0.35f;

    USkeletalMesh* ResolveMesh(ETerraGameplayPieceType PieceType) const;
    UAnimationAsset* ResolveAttackAnimation(ETerraGameplayPieceType PieceType) const;
    float ResolveAttackToHitSeconds(ETerraGameplayPieceType PieceType) const;
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

USTRUCT(BlueprintType)
struct PIECEPRESENTATION_API FTerraPiecePresentationCaptureParticipant
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 PieceId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 CellId = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    ETerraGameplayPieceType PieceType = ETerraGameplayPieceType::Infantry;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FTransform WorldTransform = FTransform::Identity;

    bool IsValid() const
    {
        return PieceId != INDEX_NONE && CellId != INDEX_NONE;
    }
};

USTRUCT(BlueprintType)
struct PIECEPRESENTATION_API FTerraPiecePresentationCaptureEvent
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FTerraPiecePresentationCaptureParticipant Captured;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FTerraPiecePresentationCaptureParticipant Attacker;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FTerraPiecePresentationCaptureParticipant Vanguard;

    bool IsValidCapture() const
    {
        return Captured.IsValid() && (Attacker.IsValid() || Vanguard.IsValid());
    }
};
