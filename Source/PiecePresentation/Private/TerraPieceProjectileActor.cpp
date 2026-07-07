#include "TerraPieceProjectileActor.h"

#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "TerraPieceActor.h"

namespace
{
    FVector SphericalInterpPosition(const FVector& Center, const FVector& From, const FVector& To, float Alpha)
    {
        const FVector FromOffset = From - Center;
        const FVector ToOffset = To - Center;
        const float FromRadius = FromOffset.Length();
        const float ToRadius = ToOffset.Length();

        const FVector FromDir = FromOffset.GetSafeNormal();
        const FVector ToDir = ToOffset.GetSafeNormal();
        if (FromDir.IsNearlyZero() || ToDir.IsNearlyZero())
        {
            return FMath::Lerp(From, To, Alpha);
        }

        const float Dot = FMath::Clamp(FVector::DotProduct(FromDir, ToDir), -1.0f, 1.0f);
        const float Angle = FMath::Acos(Dot);

        FVector Direction = FVector::ZeroVector;
        if (Angle <= KINDA_SMALL_NUMBER)
        {
            Direction = FMath::Lerp(FromDir, ToDir, Alpha).GetSafeNormal();
        }
        else
        {
            const float SinAngle = FMath::Sin(Angle);
            Direction = ((FMath::Sin((1.0f - Alpha) * Angle) / SinAngle) * FromDir
                + (FMath::Sin(Alpha * Angle) / SinAngle) * ToDir).GetSafeNormal();
        }

        if (Direction.IsNearlyZero())
        {
            Direction = FMath::Lerp(FromDir, ToDir, Alpha).GetSafeNormal();
        }

        const float Radius = FMath::Lerp(FromRadius, ToRadius, Alpha);
        return Center + Direction * Radius;
    }

    FVector MatchRadiusToReference(const FVector& Center, const FVector& Target, const FVector& Reference)
    {
        const FVector TargetDirection = (Target - Center).GetSafeNormal();
        if (TargetDirection.IsNearlyZero())
        {
            return Target;
        }

        const float ReferenceRadius = (Reference - Center).Length();
        return Center + TargetDirection * ReferenceRadius;
    }

    float EvaluateGravityArcHeight(float Alpha, float MaxHeightCM)
    {
        const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
        return 4.0f * FMath::Max(MaxHeightCM, 0.0f) * ClampedAlpha * (1.0f - ClampedAlpha);
    }
}

ATerraPieceProjectileActor::ATerraPieceProjectileActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
    SetRootComponent(RootScene);

    ArrowMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowMesh"));
    ArrowMeshComponent->SetupAttachment(RootScene);
    ArrowMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ArrowMeshComponent->SetGenerateOverlapEvents(false);
    ArrowMeshComponent->SetCanEverAffectNavigation(false);
    ArrowMeshComponent->SetHiddenInGame(true);
    ArrowMeshComponent->SetVisibility(false);

    SpellActorComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("SpellActor"));
    SpellActorComponent->SetupAttachment(RootScene);
    SpellActorComponent->SetHiddenInGame(true);
    SpellActorComponent->SetVisibility(false);
}

void ATerraPieceProjectileActor::Launch(const FTerraPieceProjectileLaunchParams& Params)
{
    VisualType = Params.VisualType;
    StartWorldLocation = Params.StartWorldLocation;
    TargetWorldLocation = Params.TargetWorldLocation;
    PlanetCenterWorldLocation = Params.PlanetCenterWorldLocation;
    SourceActor = Params.SourceActor;
    AttachName = Params.AttachName;
    AttachRelativeLocation = Params.AttachRelativeLocation;
    RelativeRotation = Params.RelativeRotation.Quaternion();
    ReleaseDelaySeconds = FMath::Max(Params.ReleaseDelaySeconds, 0.0f);
    FlightSeconds = FMath::Max(Params.FlightSeconds, 0.001f);
    ArcHeightCM = FMath::Max(Params.ArcHeightCM, 0.0f);
    ElapsedSeconds = 0.0f;
    FlightElapsedSeconds = 0.0f;
    bActive = true;
    bReleased = false;

    if (ArrowMeshComponent)
    {
        ArrowMeshComponent->SetStaticMesh(VisualType == ETerraPieceProjectileVisualType::Arrow ? Params.ArrowMesh.Get() : nullptr);
        ArrowMeshComponent->SetRelativeLocation(FVector::ZeroVector);
        ArrowMeshComponent->SetRelativeRotation(FRotator::ZeroRotator);
        ArrowMeshComponent->SetRelativeScale3D(FVector(FMath::Max(Params.UniformScale, 0.001f)));
        const bool bShowArrow = VisualType == ETerraPieceProjectileVisualType::Arrow && Params.ArrowMesh;
        ArrowMeshComponent->SetHiddenInGame(!bShowArrow);
        ArrowMeshComponent->SetVisibility(bShowArrow);
    }

    if (SpellActorComponent)
    {
        SpellActorComponent->SetChildActorClass(VisualType == ETerraPieceProjectileVisualType::Spell ? Params.SpellActorClass : nullptr);
        SpellActorComponent->SetRelativeLocation(FVector::ZeroVector);
        SpellActorComponent->SetRelativeRotation(FRotator::ZeroRotator);
        SpellActorComponent->SetRelativeScale3D(FVector(FMath::Max(Params.UniformScale, 0.001f)));
        const bool bShowSpell = VisualType == ETerraPieceProjectileVisualType::Spell && Params.SpellActorClass;
        SpellActorComponent->SetHiddenInGame(!bShowSpell);
        SpellActorComponent->SetVisibility(bShowSpell);
    }

    const bool bHasHeldTransform = UpdateHeldTransform_();
    if (!bHasHeldTransform)
    {
        const FVector Start = EvaluatePosition_(0.0f);
        const FVector Next = EvaluatePosition_(0.02f);
        SetActorLocationAndRotation(Start, BuildRotationForPosition_(Start, Next));
    }
    if (ReleaseDelaySeconds <= KINDA_SMALL_NUMBER)
    {
        Release_();
    }
    SetActorTickEnabled(true);
}

void ATerraPieceProjectileActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bActive)
    {
        return;
    }

    ElapsedSeconds += FMath::Max(DeltaSeconds, 0.0f);
    if (!bReleased)
    {
        if (ElapsedSeconds < ReleaseDelaySeconds)
        {
            UpdateHeldTransform_();
            return;
        }

        Release_();
    }

    FlightElapsedSeconds += FMath::Max(DeltaSeconds, 0.0f);
    const float Alpha = FMath::Clamp(FlightElapsedSeconds / FMath::Max(FlightSeconds, 0.001f), 0.0f, 1.0f);
    const FVector Position = EvaluatePosition_(Alpha);
    const FVector NextPosition = EvaluatePosition_(FMath::Min(Alpha + 0.02f, 1.0f));
    SetActorLocationAndRotation(Position, BuildRotationForPosition_(Position, NextPosition));

    if (Alpha >= 1.0f - KINDA_SMALL_NUMBER)
    {
        bActive = false;
        SetActorTickEnabled(false);
        Destroy();
    }
}

FVector ATerraPieceProjectileActor::EvaluatePosition_(float Alpha) const
{
    const FVector BasePosition = SphericalInterpPosition(PlanetCenterWorldLocation, StartWorldLocation, TargetWorldLocation, Alpha);
    if (VisualType != ETerraPieceProjectileVisualType::Arrow)
    {
        return BasePosition;
    }

    FVector Up = (BasePosition - PlanetCenterWorldLocation).GetSafeNormal();
    if (Up.IsNearlyZero())
    {
        Up = FVector::UpVector;
    }

    return BasePosition + Up * EvaluateGravityArcHeight(Alpha, ArcHeightCM);
}

bool ATerraPieceProjectileActor::UpdateHeldTransform_()
{
    if (!IsValid(SourceActor))
    {
        return false;
    }

    const FTransform AttachTransform = SourceActor->ResolveAttachmentWorldTransform(AttachName);
    SetActorLocationAndRotation(
        AttachTransform.TransformPosition(AttachRelativeLocation),
        AttachTransform.GetRotation() * RelativeRotation);
    return true;
}

void ATerraPieceProjectileActor::Release_()
{
    UpdateHeldTransform_();
    StartWorldLocation = GetActorLocation();
    if (VisualType == ETerraPieceProjectileVisualType::Arrow)
    {
        TargetWorldLocation = MatchRadiusToReference(PlanetCenterWorldLocation, TargetWorldLocation, StartWorldLocation);
    }
    FlightElapsedSeconds = 0.0f;
    bReleased = true;
}

FQuat ATerraPieceProjectileActor::BuildRotationForPosition_(const FVector& Position, const FVector& NextPosition) const
{
    FVector Up = (Position - PlanetCenterWorldLocation).GetSafeNormal();
    if (Up.IsNearlyZero())
    {
        Up = FVector::UpVector;
    }

    const FVector VelocityDirection = (NextPosition - Position).GetSafeNormal();
    FVector Forward = VisualType == ETerraPieceProjectileVisualType::Arrow
        ? VelocityDirection
        : FVector::VectorPlaneProject(NextPosition - Position, Up).GetSafeNormal();
    if (Forward.IsNearlyZero())
    {
        const FVector FallbackDirection = (TargetWorldLocation - StartWorldLocation).GetSafeNormal();
        Forward = VisualType == ETerraPieceProjectileVisualType::Arrow
            ? FallbackDirection
            : FVector::VectorPlaneProject(TargetWorldLocation - StartWorldLocation, Up).GetSafeNormal();
    }
    if (Forward.IsNearlyZero())
    {
        Forward = FVector::ForwardVector;
    }

    if (VisualType == ETerraPieceProjectileVisualType::Arrow)
    {
        const FVector Side = FVector::CrossProduct(Up, Forward).GetSafeNormal();
        if (!Side.IsNearlyZero())
        {
            Up = FVector::CrossProduct(Forward, Side).GetSafeNormal();
        }
    }

    return FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat() * RelativeRotation;
}
