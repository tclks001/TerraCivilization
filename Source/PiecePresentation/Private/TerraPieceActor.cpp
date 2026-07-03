#include "TerraPieceActor.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "TerraPieceAnimInstance.h"

ATerraPieceActor::ATerraPieceActor()
{
    PrimaryActorTick.bCanEverTick = false;

    RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
    SetRootComponent(RootScene);

    HumanMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HumanMesh"));
    HumanMesh->SetupAttachment(RootScene);
    HumanMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HumanMesh->SetGenerateOverlapEvents(false);
    HumanMesh->SetCanEverAffectNavigation(false);
    HumanMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
}

void ATerraPieceActor::ApplyPresentationSnapshot(const FTerraPiecePresentationSnapshot& Snapshot, const FTerraPieceVisualConfig& VisualConfig)
{
    PieceId = Snapshot.PieceId;
    OwnerFactionId = Snapshot.OwnerFactionId;
    CellId = Snapshot.CellId;
    PieceType = Snapshot.PieceType;

    SetActorTransform(Snapshot.WorldTransform);

    if (HumanMesh)
    {
        HumanMesh->SetSkeletalMesh(VisualConfig.ResolveMesh(PieceType));
        HumanMesh->SetRelativeLocation(VisualConfig.MeshRelativeLocation);
        HumanMesh->SetRelativeRotation(VisualConfig.MeshRelativeRotation);
        HumanMesh->SetRelativeScale3D(FVector(FMath::Max(VisualConfig.UniformScale, 0.001f)));
    }

    AnimDriveState.ActionState = ETerraPieceAnimActionState::Idle;
    AnimDriveState.MoveSpeed = 0.0f;
    AnimDriveState.NormalizedPhase = 0.0f;
    AnimDriveState.FacingDirection = Snapshot.WorldTransform.GetRotation().GetForwardVector();
    AnimDriveState.bDead = false;

    if (HumanMesh)
    {
        if (UTerraPieceAnimInstance* AnimInstance = Cast<UTerraPieceAnimInstance>(HumanMesh->GetAnimInstance()))
        {
            AnimInstance->SetDriveState(AnimDriveState);
        }
    }
}
