#include "TerrainVisualSurfaceComponent.h"

#include "FSphereTopology.h"

UTerrainVisualSurfaceComponent::UTerrainVisualSurfaceComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetMobility(EComponentMobility::Movable);
    SetCanEverAffectNavigation(false);
    bUseAsyncCooking = false;
    bUseComplexAsSimpleCollision = true;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetCollisionObjectType(ECC_WorldStatic);
    SetCollisionResponseToAllChannels(ECR_Ignore);
    SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

bool UTerrainVisualSurfaceComponent::RebuildBaseSphere(const FSphereTopology& SurfaceTopology, float RadiusCM)
{
    ClearSurface();

    if (RadiusCM <= KINDA_SMALL_NUMBER
        || SurfaceTopology.PrimalVertsUnit.Num() == 0
        || SurfaceTopology.PrimalTris.Num() == 0)
    {
        return false;
    }

    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FLinearColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;
    TArray<int32> Triangles;

    const int32 VertexCount = SurfaceTopology.PrimalVertsUnit.Num();
    Vertices.Reserve(VertexCount);
    Normals.Reserve(VertexCount);
    UV0.Reserve(VertexCount);
    VertexColors.Reserve(VertexCount);

    for (const FVector& UnitVertex : SurfaceTopology.PrimalVertsUnit)
    {
        const FVector UnitDirection = UnitVertex.GetSafeNormal();
        if (UnitDirection.IsNearlyZero())
        {
            ClearSurface();
            return false;
        }

        Vertices.Add(UnitDirection * RadiusCM);
        Normals.Add(UnitDirection);
        UV0.Add(FVector2D::ZeroVector);
        VertexColors.Add(FLinearColor::White);
    }

    Triangles.Reserve(SurfaceTopology.PrimalTris.Num() * 3);
    for (const FIntVector& Triangle : SurfaceTopology.PrimalTris)
    {
        if (!Vertices.IsValidIndex(Triangle.X)
            || !Vertices.IsValidIndex(Triangle.Y)
            || !Vertices.IsValidIndex(Triangle.Z))
        {
            ClearSurface();
            return false;
        }

        Triangles.Add(Triangle.X);
        Triangles.Add(Triangle.Y);
        Triangles.Add(Triangle.Z);
    }

    CreateMeshSection_LinearColor(
        0,
        Vertices,
        Triangles,
        Normals,
        UV0,
        VertexColors,
        Tangents,
        true);

    bHasBuiltSurface = true;
    BuiltSubdivisionLevel = SurfaceTopology.SubdivisionLevel;
    return true;
}

void UTerrainVisualSurfaceComponent::ClearSurface()
{
    ClearAllMeshSections();
    bHasBuiltSurface = false;
    BuiltSubdivisionLevel = INDEX_NONE;
}
