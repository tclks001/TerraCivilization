#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TerraGameplayContainer.h"
#include "TerraNpcMcpGameplayBridge.h"
#include "PlanetGameplayComponent.generated.h"

class APlanetTessellatedMesh;
struct FTerraPiecePresentationMoveEvent;
struct FTerraPiecePresentationCaptureEvent;

enum class ETerraG1DebugPieceType : uint8
{
    Base,
    Infantry,
    Cavalry,
    Archer,
};

struct FTerraG1DebugPiece
{
    int32 FactionId = INDEX_NONE;
    int32 CellId = INDEX_NONE;
    ETerraG1DebugPieceType PieceType = ETerraG1DebugPieceType::Infantry;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TERRACIVILIZATION_API UPlanetGameplayComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPlanetGameplayComponent();

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|SimpleGameplay G1")
    bool bEnableG1DebugPieces = true;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|SimpleGameplay G1", meta = (ClampMin = "10.0", ClampMax = "1000.0"))
    float G1DebugPieceRadiusCM = 140.0f;

    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|SimpleGameplay G1", meta = (ClampMin = "0.0", ClampMax = "5000.0"))
    float G1DebugPieceHeightOffsetCM = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G2.5")
    FLinearColor G2_5CurrentFactionPieceColor = FLinearColor(1.0f, 0.45f, 0.68f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G2.5")
    FLinearColor G2_5CurrentFactionPieceHoverColor = FLinearColor(1.0f, 0.22f, 0.32f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G3|Debug")
    bool bG3DebugKeepSameFactionOnEndTurn = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G3")
    FLinearColor G3ActionTargetHoverColor = FLinearColor(0.08f, 0.45f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G4")
    FLinearColor G4CaptureTargetHoverColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);

    void RebuildGameplay();
    void RefreshGameplayHighlights(const TArray<int32>& DirtyCellIds);
    bool HandleGameplayCellClick(int32 CellId, const TCHAR* SourceLabel, int32 InstanceIndex, const FString& ComponentName);
    bool TryExecuteNpcMcpValidatedAction(int32 ExpectedTurnIndex, int32 ExpectedFactionId, int32 PieceId, int32 ToCellId, FTerraGameplayContainer::FValidatedActionExecutionResult& OutResult);
    bool TryNpcMcpUiBeginTurnReview(FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult);
    bool TryNpcMcpUiSelectPiece(int32 PieceId, FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult);
    bool TryNpcMcpUiPreviewMove(int32 PieceId, int32 ToCellId, FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult);
    bool TryNpcMcpUiCancelSelection(FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult);
    bool TryNpcMcpUiConfirmAction(FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult, FTerraGameplayContainer::FValidatedActionExecutionResult& OutExecutionResult);
    void RefreshFactionPieceHighlights(int32 FactionId);
    void RefreshCurrentFactionPieceHighlights();
    void ExecuteC6_5DelayedTurnStartFocus(int32 ExpectedTurnIndex, int32 ExpectedFactionId);
    bool HandleC5NavigateCurrentFactionPiece(bool bReverse);
    bool HandleHISMUndo();
    void RebuildG1DebugPieces();
    void DrawG1DebugPieces() const;

    const FTerraGameplayContainer* GetGameplayContainer() const { return GameplayContainer.Get(); }
    FTerraGameplayContainer* GetGameplayContainer() { return GameplayContainer.Get(); }
    int32 GetLastHighlightedFactionId() const { return G2_5LastHighlightedFactionId; }
    bool HasInitializedGameplay() const { return GameplayContainer.IsValid() && GameplayContainer->IsInitialized(); }

private:
    APlanetTessellatedMesh* GetHost() const;
    void BuildNpcMcpInteractionState_(FTerraNpcMcpGameplayBridge::FInteractionStateSnapshot& OutState) const;
    void FillNpcMcpReviewResult_(FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult, bool bOk, const FString& Error) const;

    TUniquePtr<FTerraGameplayContainer> GameplayContainer;
    int32 G2_5LastHighlightedFactionId = INDEX_NONE;
    TArray<FTerraG1DebugPiece> G1DebugPieces;
};
