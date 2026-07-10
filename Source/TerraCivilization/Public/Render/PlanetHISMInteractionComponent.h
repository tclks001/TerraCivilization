#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Render/PlanetHISMTileRenderer.h"
#include "PlanetHISMInteractionComponent.generated.h"

class APlanetTessellatedMesh;
class UTexture2D;
struct FHitResult;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TERRACIVILIZATION_API UPlanetHISMInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPlanetHISMInteractionComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|R11 Highlight")
    FLinearColor HighlightHoverColor = FLinearColor(1.0f, 0.85f, 0.10f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|R11 Highlight",
              meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float HighlightStrength = 1.50f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight")
    bool bEnableHISMInstanceHighlight = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight",
              meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float HISMHoverFadeDuration = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HISMHighlightInnerRadius = 0.20f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HISMHighlightOuterRadius = 0.50f;

    FPlanetHISMHighlightConfig BuildHighlightConfig() const;
    void TickHoverFade(float DeltaSeconds);
    bool TryResolveHISMHitToCellId(const FHitResult& Hit, int32& OutCellId) const;
    bool HandleHISMHoverHit(const FHitResult& Hit);
    bool HandleHISMClickHit(const FHitResult& Hit);
    void ClearHISMHover();
    void ClearAllHISMHighlights();
    void WriteHISMHighlightForCell(int32 CellId, bool bMarkRenderStateDirty = true);
    void RefreshCapturePreviewCellsForActionTarget(int32 ActionTargetCellId, bool bMarkLastRenderStateDirty = true);
    void UpdateHISMHoverCell(int32 NewCellId);
    void PrepareHighlightComponents();
    void SetHighlightLUT(UTexture2D* InLUT);
    int32 GetLastHISMPickedCellId() const;
    int32 GetLastHISMClickedCellId() const;

private:
    APlanetTessellatedMesh* GetHost() const;
};
