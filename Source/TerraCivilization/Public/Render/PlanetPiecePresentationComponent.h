#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TerraPiecePresentationTypes.h"
#include "PlanetPiecePresentationComponent.generated.h"

class APlanetTessellatedMesh;
class AActor;
class UAnimationAsset;
class UAnimInstance;
class UAnimMontage;
class UMaterialInterface;
class UStaticMesh;
class USkeletalMesh;
class UTexture2D;
class UTerraPiecePresentationManager;
struct FTerraGameplayCaptureEntry;
struct FTerraGameplayPieceState;

UCLASS(ClassGroup = (TerraCivilization), meta = (BlueprintSpawnableComponent))
class TERRACIVILIZATION_API UPlanetPiecePresentationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPlanetPiecePresentationComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    bool bEnableP1PiecePresentation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    bool bHideG1DebugPiecesWhenP1IsActive = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
    float P1PieceRadiusOffsetCM = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P1PieceUniformScale = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FVector P1MeshRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FRotator P1MeshRelativeRotation = FRotator(0.0f, -90.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<USkeletalMesh> P1CommanderMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<USkeletalMesh> P1InfantryMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<USkeletalMesh> P1CavalryMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<USkeletalMesh> P1ArcherMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P2IdleAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P2MoveAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P2JumpAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P2MoveDurationSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P2JumpDurationSeconds = 0.55f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
    float P2JumpHeightCM = 650.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    bool bEnableP2_5HISMPieceHeightTrace = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "100000.0"))
    float P2_5PieceHeightTraceStartOffsetCM = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "100000.0"))
    float P2_5PieceHeightTracePastCenterOffsetCM = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    bool bDebugP2_5HISMPieceHeightTrace = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation",
        meta = (ClampMin = "0.0", ClampMax = "15.0", UIMin = "0.0", UIMax = "5.0"))
    float P2_6CavalryHeightTraceAngularOffsetDeg = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<USkeletalMesh> P4HorseMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P4HorseIdleAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P4HorseMoveAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P4HorseJumpAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P4HorseJumpPlayRateScale = 0.9f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P4HorseDeathAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P4RiderSittingAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TSubclassOf<UAnimInstance> P4RiderAnimInstanceClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimMontage> P4RiderUpperBodyAttackMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimMontage> P4RiderUpperBodyHitMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FName P4SaddleAttachName = TEXT("Torso");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FVector P4HorseRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FRotator P4HorseRelativeRotation = FRotator(0.0f, -90.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P4HorseUniformScale = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FVector P4RiderRelativeLocation = FVector(0.0f, -1.0f, 0.5f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FRotator P4RiderRelativeRotation = FRotator(0.0f, -90.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P4RiderUniformScale = 0.003f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FVector P4RiderDeathRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FRotator P4RiderDeathRelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UStaticMesh> P5ArcherBowMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UStaticMesh> P5InfantrySwordMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UStaticMesh> P5InfantryShieldMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UStaticMesh> P5CavalryAxeMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P12 Equipment Drop Presentation")
    TObjectPtr<UStaticMesh> P12DroppedBowMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P12 Equipment Drop Presentation")
    TObjectPtr<USkeletalMesh> P12DroppedHorseMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P12 Equipment Drop Presentation")
    TObjectPtr<UAnimationAsset> P12DroppedHorseIdleAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P12 Equipment Drop Presentation")
    TObjectPtr<UAnimMontage> P12ArcherCavalryUpperBodyAttackMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P12 Equipment Drop Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P12ArcherCavalryAttackToHitSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P12 Equipment Drop Presentation")
    FVector P12DroppedBowRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P12 Equipment Drop Presentation")
    FRotator P12DroppedBowRelativeRotation = FRotator(180.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P12 Equipment Drop Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P12DroppedBowUniformScale = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P12 Equipment Drop Presentation")
    FVector P12DroppedHorseRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P12 Equipment Drop Presentation")
    FRotator P12DroppedHorseRelativeRotation = FRotator(0.0f, -90.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P12 Equipment Drop Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P12DroppedHorseUniformScale = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FName P5ArcherBowAttachName = TEXT("handslot_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FName P5InfantrySwordAttachName = TEXT("handslot_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FName P5InfantryShieldAttachName = TEXT("handslot_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FName P5CavalryAxeAttachName = TEXT("handslot_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FVector P5ArcherBowRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FRotator P5ArcherBowRelativeRotation = FRotator(180.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P5ArcherBowUniformScale = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FVector P5InfantrySwordRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FRotator P5InfantrySwordRelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P5InfantrySwordUniformScale = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FVector P5InfantryShieldRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FRotator P5InfantryShieldRelativeRotation = FRotator(90.0f, 180.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P5InfantryShieldUniformScale = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FVector P5CavalryAxeRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FRotator P5CavalryAxeRelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P5CavalryAxeUniformScale = 0.015f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UStaticMesh> P6ArrowProjectileMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TSubclassOf<AActor> P6SpellProjectileActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FName P6ArrowAttachName = TEXT("handslot_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FName P6SpellAttachName = TEXT("handslot_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FVector P6ArrowRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FRotator P6ArrowRelativeRotation = FRotator(0.0f, -90.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P6ArrowUniformScale = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FVector P6ArrowTargetRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FVector P6SpellRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    FRotator P6SpellRelativeRotation = FRotator(90.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P6SpellUniformScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P6ArrowReleaseDelaySeconds = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P6SpellReleaseDelaySeconds = 0.18f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P6ArrowFlightSeconds = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P6SpellFlightSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
    float P6ArrowArcHeightCM = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P7 Faction Palette")
    TObjectPtr<UMaterialInterface> P7PaletteReplaceMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P7 Faction Palette")
    TArray<FTerraPieceFactionPalette> P7FactionPalettes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P7 Faction Palette")
    TObjectPtr<UTexture2D> P7CommanderBaseTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P7 Faction Palette")
    TObjectPtr<UTexture2D> P7InfantryBaseTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P7 Faction Palette")
    TObjectPtr<UTexture2D> P7CavalryRiderBaseTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P7 Faction Palette")
    TObjectPtr<UTexture2D> P7ArcherBaseTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P7 Faction Palette")
    TArray<FTerraPiecePaletteMask> P7PaletteMasksByPieceType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P35CommanderAttackDurationSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P35CommanderAttackPlayRateScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P35ArcherAttackDurationSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P35ArcherAttackPlayRateScale = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P3CommanderMagicAttackAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P3ArcherRangedAttackAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P3InfantryMeleeAttackAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P3CavalryMeleeAttackAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P3HitAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P3DeathAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float P3FacingBlendSeconds = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "5.0"))
    float P3MeleeRunInSeconds = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "5.0"))
    float P3MeleeReturnSeconds = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3AttackAnimationStartOffsetSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3CommanderAttackToHitSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3ArcherAttackToHitSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3InfantryAttackToHitSeconds = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3CavalryAttackToHitSeconds = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3HitReactDelaySeconds = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3HitAnimationStartOffsetSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3DeathAfterHitDelaySeconds = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3DeathAnimationStartOffsetSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3CapturedFadeSeconds = 0.35f;

    UFUNCTION(BlueprintCallable, Category = "PlanetTopology|Tess|Piece Presentation")
    UTerraPiecePresentationManager* GetPresentationManager() const { return PiecePresentationManager; }

    void SyncPresentation(
        const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents = TArray<FTerraPiecePresentationMoveEvent>(),
        const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents = TArray<FTerraPiecePresentationCaptureEvent>());

    void ClearPresentation();
    bool BuildPieceWorldTransform(int32 CellId, FTransform& OutWorldTransform) const;
    bool BuildPieceWorldTransform(int32 CellId, ETerraGameplayPieceType PieceType, FTransform& OutWorldTransform) const;
    bool BuildPieceWorldTransformForPiece(
        const FTerraGameplayPieceState& Piece,
        const TArray<FTerraGameplayPieceState>& Pieces,
        FTransform& OutWorldTransform) const;
    bool TryResolvePieceHeightFromHISM(
        int32 CellId,
        const FVector& TraceWorldUp,
        const FVector& PlacementWorldUp,
        float TraceAngularOffsetDeg,
        FVector& OutWorldPosition) const;
    void BuildCaptureEventsFromPendingEntries(
        const TArray<FTerraGameplayCaptureEntry>& CaptureEntries,
        const TArray<FTerraGameplayPieceState>& PiecesBeforeResolution,
        TArray<FTerraPiecePresentationCaptureEvent>& OutCaptureEvents) const;
    FTerraPieceVisualConfig BuildVisualConfig() const;

private:
    APlanetTessellatedMesh* GetHost() const;

    /**
     * SimpleGameplay P1：运行时懒创建的表现管理器。
     *
     * 注意：这里**不再**使用 CreateDefaultSubobject 且**不再**用 VisibleAnywhere 暴露给
     * 编辑器 Details 面板。原实现把一个 UActorComponent 子对象作为另一个 UActorComponent
     * 的 default subobject 并挂到 Details 里，会触发 UnrealEditor_PropertyEditor 在展开
     * 蓝图 CDO 时无限递归展开（EXCEPTION_STACK_OVERFLOW，一双击 BP 立刻崩溃）。
     * 现改为 SyncPresentation 首次调用时 NewObject 懒创建，Transient 只在运行时存在，
     * 不出现在 CDO / Details 面板，蓝图编辑器不再触发递归。
     */
    UPROPERTY(Transient)
    TObjectPtr<UTerraPiecePresentationManager> PiecePresentationManager;

    // Neutral pieces retain this presentation faction while the next turn is gated.
    int32 PresentedNeutralFactionId = INDEX_NONE;

    /** SimpleGameplay P1：确保 PiecePresentationManager 存在（懒创建）。返回是否可用。 */
    bool EnsurePiecePresentationManager_();
};
