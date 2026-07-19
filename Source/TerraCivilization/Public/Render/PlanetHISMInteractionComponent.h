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

    /** Plain tile UV radius at which the HISM highlight ring begins. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight|Plain",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PlainHISMHighlightInnerRadius = 0.20f;

    /** Plain tile UV radius at which the HISM highlight ring reaches full strength. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight|Plain",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PlainHISMHighlightOuterRadius = 0.50f;

    /** Forest tile UV radius at which the HISM highlight ring begins. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight|Forest",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ForestHISMHighlightInnerRadius = 0.20f;

    /** Forest tile UV radius at which the HISM highlight ring reaches full strength. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight|Forest",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ForestHISMHighlightOuterRadius = 0.50f;

    /** Mountain tile UV radius at which the HISM highlight ring begins. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight|Mountain",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MountainHISMHighlightInnerRadius = 0.20f;

    /** Mountain tile UV radius at which the HISM highlight ring reaches full strength. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight|Mountain",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MountainHISMHighlightOuterRadius = 0.50f;

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
    FPlanetHISMHighlightConfig BuildRuntimeHighlightConfig_() const;
    bool TryResolveInteractionHitToCellId_(const FHitResult& Hit, int32& OutCellId) const;
};
