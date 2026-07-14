#include "TerraEquipmentDropActor.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

ATerraEquipmentDropActor::ATerraEquipmentDropActor()
{
    PrimaryActorTick.bCanEverTick = false;
    RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
    SetRootComponent(RootScene);

    BowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BowMesh"));
    BowMesh->SetupAttachment(RootScene);
    BowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BowMesh->SetGenerateOverlapEvents(false);
    BowMesh->SetCanEverAffectNavigation(false);

    HorseMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HorseMesh"));
    HorseMesh->SetupAttachment(RootScene);
    HorseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HorseMesh->SetGenerateOverlapEvents(false);
    HorseMesh->SetCanEverAffectNavigation(false);
}

void ATerraEquipmentDropActor::ApplySnapshot(const FTerraPieceEquipmentDropSnapshot& Snapshot, const FTerraPieceVisualConfig& VisualConfig)
{
    SetActorTransform(Snapshot.WorldTransform);

    BowMesh->SetStaticMesh(Snapshot.bHasBow ? VisualConfig.EquipmentDropBowMesh.Get() : nullptr);
    BowMesh->SetRelativeLocation(VisualConfig.EquipmentDropBowRelativeLocation);
    BowMesh->SetRelativeRotation(VisualConfig.EquipmentDropBowRelativeRotation);
    BowMesh->SetRelativeScale3D(FVector(VisualConfig.EquipmentDropBowUniformScale));
    BowMesh->SetHiddenInGame(!Snapshot.bHasBow);
    BowMesh->SetVisibility(Snapshot.bHasBow);

    HorseMesh->SetSkeletalMesh(Snapshot.bHasHorse ? VisualConfig.EquipmentDropHorseMesh.Get() : nullptr);
    HorseMesh->SetRelativeLocation(VisualConfig.EquipmentDropHorseRelativeLocation);
    HorseMesh->SetRelativeRotation(VisualConfig.EquipmentDropHorseRelativeRotation);
    HorseMesh->SetRelativeScale3D(FVector(VisualConfig.EquipmentDropHorseUniformScale));
    HorseMesh->SetHiddenInGame(!Snapshot.bHasHorse);
    HorseMesh->SetVisibility(Snapshot.bHasHorse);
    if (Snapshot.bHasHorse)
    {
        HorseMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        HorseMesh->PlayAnimation(VisualConfig.EquipmentDropHorseIdleAnimation.Get(), true);
    }
}
