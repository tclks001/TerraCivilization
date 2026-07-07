#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerraPiecePresentationTypes.h"
#include "TerraPieceProjectileActor.generated.h"

class UChildActorComponent;
class USceneComponent;
class UStaticMeshComponent;
class ATerraPieceActor;

USTRUCT()
struct PIECEPRESENTATION_API FTerraPieceProjectileLaunchParams
{
    GENERATED_BODY()

    ETerraPieceProjectileVisualType VisualType = ETerraPieceProjectileVisualType::Arrow;
    FVector StartWorldLocation = FVector::ZeroVector;
    FVector TargetWorldLocation = FVector::ZeroVector;
    FVector PlanetCenterWorldLocation = FVector::ZeroVector;
    TObjectPtr<ATerraPieceActor> SourceActor;
    FName AttachName = NAME_None;
    FVector AttachRelativeLocation = FVector::ZeroVector;
    TObjectPtr<UStaticMesh> ArrowMesh;
    TSubclassOf<AActor> SpellActorClass;
    FRotator RelativeRotation = FRotator::ZeroRotator;
    float UniformScale = 1.0f;
    float ReleaseDelaySeconds = 0.0f;
    float FlightSeconds = 0.3f;
    float ArcHeightCM = 0.0f;
};

UCLASS()
class PIECEPRESENTATION_API ATerraPieceProjectileActor : public AActor
{
    GENERATED_BODY()

public:
    ATerraPieceProjectileActor();

    void Launch(const FTerraPieceProjectileLaunchParams& Params);

protected:
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terra|Piece Presentation|P6")
    TObjectPtr<USceneComponent> RootScene;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terra|Piece Presentation|P6")
    TObjectPtr<UStaticMeshComponent> ArrowMeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terra|Piece Presentation|P6")
    TObjectPtr<UChildActorComponent> SpellActorComponent;

private:
    FVector EvaluatePosition_(float Alpha) const;
    FQuat BuildRotationForPosition_(const FVector& Position, const FVector& NextPosition) const;
    bool UpdateHeldTransform_();
    void Release_();

    UPROPERTY(Transient)
    ETerraPieceProjectileVisualType VisualType = ETerraPieceProjectileVisualType::Arrow;

    UPROPERTY(Transient)
    FVector StartWorldLocation = FVector::ZeroVector;

    UPROPERTY(Transient)
    FVector TargetWorldLocation = FVector::ZeroVector;

    UPROPERTY(Transient)
    FVector PlanetCenterWorldLocation = FVector::ZeroVector;

    UPROPERTY(Transient)
    TObjectPtr<ATerraPieceActor> SourceActor;

    UPROPERTY(Transient)
    FName AttachName = NAME_None;

    UPROPERTY(Transient)
    FVector AttachRelativeLocation = FVector::ZeroVector;

    UPROPERTY(Transient)
    FQuat RelativeRotation = FQuat::Identity;

    UPROPERTY(Transient)
    float ReleaseDelaySeconds = 0.0f;

    UPROPERTY(Transient)
    float FlightSeconds = 0.3f;

    UPROPERTY(Transient)
    float ArcHeightCM = 0.0f;

    UPROPERTY(Transient)
    float ElapsedSeconds = 0.0f;

    UPROPERTY(Transient)
    float FlightElapsedSeconds = 0.0f;

    UPROPERTY(Transient)
    bool bActive = false;

    UPROPERTY(Transient)
    bool bReleased = false;
};
