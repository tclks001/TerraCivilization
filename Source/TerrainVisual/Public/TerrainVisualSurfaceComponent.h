#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "TerrainVisualSurfaceComponent.generated.h"

class FSphereTopology;

UCLASS(ClassGroup=(TerrainVisual), meta=(BlueprintSpawnableComponent))
class TERRAINVISUAL_API UTerrainVisualSurfaceComponent : public UProceduralMeshComponent
{
    GENERATED_BODY()

public:
    UTerrainVisualSurfaceComponent(const FObjectInitializer& ObjectInitializer);

    bool RebuildBaseSphere(const FSphereTopology& SurfaceTopology, float RadiusCM);
    void ClearSurface();

    bool HasBuiltSurface() const { return bHasBuiltSurface; }
    int32 GetBuiltSubdivisionLevel() const { return BuiltSubdivisionLevel; }

private:
    bool bHasBuiltSurface = false;
    int32 BuiltSubdivisionLevel = INDEX_NONE;
};
