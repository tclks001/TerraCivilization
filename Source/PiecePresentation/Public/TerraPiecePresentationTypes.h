#pragma once

#include "CoreMinimal.h"
#include "TerraGameplayTypes.h"
#include "TerraPiecePresentationTypes.generated.h"

class USkeletalMesh;
class AActor;
class UAnimInstance;
class UAnimMontage;
class UAnimationAsset;
class UMaterialInterface;
class UStaticMesh;
class UTexture2D;

UENUM(BlueprintType)
enum class ETerraPieceProjectileVisualType : uint8
{
    Arrow UMETA(DisplayName = "Arrow"),
    Spell UMETA(DisplayName = "Spell"),
};

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

UENUM(BlueprintType)
enum class ETerraPiecePresentationForwardMode : uint8
{
    MoveDirection   UMETA(DisplayName = "Move Direction"),
    FaceTarget      UMETA(DisplayName = "Face Target"),
    LockSource      UMETA(DisplayName = "Lock Source Forward"),
};

USTRUCT(BlueprintType)
struct PIECEPRESENTATION_API FTerraPiecePaletteTile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7", meta = (ClampMin = "0", ClampMax = "3"))
    int32 Row = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7", meta = (ClampMin = "0", ClampMax = "7"))
    int32 Col = 0;
};

USTRUCT(BlueprintType)
struct PIECEPRESENTATION_API FTerraPiecePaletteMask
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
    ETerraGameplayPieceType PieceType = ETerraGameplayPieceType::Infantry;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
    TArray<FTerraPiecePaletteTile> PrimaryTiles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
    TArray<FTerraPiecePaletteTile> SecondaryTiles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinSaturationToReplace = 0.05f;
};

USTRUCT(BlueprintType)
struct PIECEPRESENTATION_API FTerraPieceFactionPalette
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
    FLinearColor PrimaryColor = FLinearColor(0.85f, 0.10f, 0.08f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
    FLinearColor SecondaryColor = FLinearColor(1.0f, 0.78f, 0.18f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float ColorStrength = 1.0f;
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P12")
    TObjectPtr<UStaticMesh> EquipmentDropBowMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P12")
    TObjectPtr<USkeletalMesh> EquipmentDropHorseMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P12")
    TObjectPtr<UAnimationAsset> EquipmentDropHorseIdleAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P12")
    FVector EquipmentDropBowRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P12")
    FRotator EquipmentDropBowRelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P12", meta = (ClampMin = "0.001"))
    float EquipmentDropBowUniformScale = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P12")
    FVector EquipmentDropHorseRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P12")
    FRotator EquipmentDropHorseRelativeRotation = FRotator(0.0f, -90.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P12", meta = (ClampMin = "0.001"))
    float EquipmentDropHorseUniformScale = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P12")
    TObjectPtr<UAnimMontage> ArcherCavalryUpperBodyAttackMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P12", meta = (ClampMin = "0.0"))
    float ArcherCavalryAttackToHitSeconds = 0.35f;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5")
    TObjectPtr<UStaticMesh> ArcherBowMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5")
    TObjectPtr<UStaticMesh> InfantrySwordMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5")
    TObjectPtr<UStaticMesh> InfantryShieldMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5")
    TObjectPtr<UStaticMesh> CavalryAxeMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5")
    FName ArcherBowAttachName = TEXT("handslot_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5")
    FName InfantrySwordAttachName = TEXT("handslot_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5")
    FName InfantryShieldAttachName = TEXT("handslot_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5")
    FName CavalryAxeAttachName = TEXT("handslot_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5")
    FVector ArcherBowRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5")
    FRotator ArcherBowRelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5", meta = (ClampMin = "0.001"))
    float ArcherBowUniformScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5")
    FVector InfantrySwordRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5")
    FRotator InfantrySwordRelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5", meta = (ClampMin = "0.001"))
    float InfantrySwordUniformScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5")
    FVector InfantryShieldRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5")
    FRotator InfantryShieldRelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5", meta = (ClampMin = "0.001"))
    float InfantryShieldUniformScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5")
    FVector CavalryAxeRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5")
    FRotator CavalryAxeRelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P5", meta = (ClampMin = "0.001"))
    float CavalryAxeUniformScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P6")
    TObjectPtr<UStaticMesh> P6ArrowProjectileMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P6")
    TSubclassOf<AActor> P6SpellProjectileActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P6")
    FName P6ArrowAttachName = TEXT("handslot_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P6")
    FName P6SpellAttachName = TEXT("handslot_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P6")
    FVector P6ArrowRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P6")
    FRotator P6ArrowRelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P6", meta = (ClampMin = "0.001"))
    float P6ArrowUniformScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P6")
    FVector P6ArrowTargetRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P6")
    FVector P6SpellRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P6")
    FRotator P6SpellRelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P6", meta = (ClampMin = "0.001"))
    float P6SpellUniformScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P6", meta = (ClampMin = "0.0"))
    float P6ArrowReleaseDelaySeconds = 0.18f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P6", meta = (ClampMin = "0.0"))
    float P6SpellReleaseDelaySeconds = 0.18f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P6", meta = (ClampMin = "0.001"))
    float P6ArrowFlightSeconds = 0.28f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P6", meta = (ClampMin = "0.001"))
    float P6SpellFlightSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P6", meta = (ClampMin = "0.0"))
    float P6ArrowArcHeightCM = 350.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
    TObjectPtr<UMaterialInterface> P7PaletteReplaceMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
    TArray<FTerraPieceFactionPalette> P7FactionPalettes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
    TObjectPtr<UTexture2D> P7CommanderBaseTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
    TObjectPtr<UTexture2D> P7InfantryBaseTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
    TObjectPtr<UTexture2D> P7CavalryRiderBaseTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
    TObjectPtr<UTexture2D> P7ArcherBaseTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
    TArray<FTerraPiecePaletteMask> P7PaletteMasksByPieceType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3.5", meta = (ClampMin = "0.001"))
    float P35CommanderAttackDurationSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3.5", meta = (ClampMin = "0.001"))
    float P35CommanderAttackPlayRateScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3.5", meta = (ClampMin = "0.001"))
    float P35ArcherAttackDurationSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P3.5", meta = (ClampMin = "0.001"))
    float P35ArcherAttackPlayRateScale = 1.0f;

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
    UTexture2D* ResolveP7BaseTexture(ETerraGameplayPieceType PieceType) const;
    const FTerraPiecePaletteMask* ResolveP7PaletteMask(ETerraGameplayPieceType PieceType) const;
    const FTerraPieceFactionPalette* ResolveP7FactionPalette(int32 FactionId) const;
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
struct PIECEPRESENTATION_API FTerraPieceEquipmentDropSnapshot
{
    GENERATED_BODY()

    int32 CellId = INDEX_NONE;
    bool bHasBow = false;
    bool bHasHorse = false;
    FTransform WorldTransform = FTransform::Identity;
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
