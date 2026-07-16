#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TerraGameplayContainer.h"
#include "TerraNpcMcpGameplayBridge.h"
#include "Tutorial/TerraTutorialScenarioData.h"
#include "Engine/TimerHandle.h"
#include "PlanetGameplayComponent.generated.h"

class APlanetTessellatedMesh;
struct FTerraPiecePresentationMoveEvent;
struct FTerraPiecePresentationCaptureEvent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTerraGameplayActionLogCommitted, const FTerraGameplayActionLogEntry&, Entry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTerraTechnologyChoiceRequested, int32, FactionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTerraTechnologyStateChanged);

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G10", meta = (ClampMin = "0"))
    int32 G10NeutralCommanderCount = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G10", meta = (ClampMin = "0"))
    int32 G10NeutralInfantryCount = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G10", meta = (ClampMin = "0"))
    int32 G10NeutralCavalryCount = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G10", meta = (ClampMin = "0"))
    int32 G10NeutralArcherCount = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G10")
    int32 G10NeutralSpawnSeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay T0")
    FTerraGameplayTechnologyProgressionConfig T0TechnologyProgressionConfig;

    UPROPERTY(BlueprintAssignable, Category = "Terra UI|Action Log")
    FTerraGameplayActionLogCommitted OnActionLogCommitted;

    UPROPERTY(BlueprintAssignable, Category = "Terra UI|Technology")
    FTerraTechnologyChoiceRequested OnTechnologyChoiceRequested;

    UPROPERTY(BlueprintAssignable, Category = "Terra UI|Technology")
    FTerraTechnologyStateChanged OnTechnologyStateChanged;

    void RebuildGameplay();
    /** WorldGen 输出的视觉山脊线；后续动态造山只需替换该数组并请求表面重建。 */
    void SetMountainRidgeSegments(const TArray<FIntPoint>& InSegments);
    const TArray<FIntPoint>& GetMountainRidgeSegments() const { return MountainRidgeSegments; }
    bool InitializeTutorialScenario(const UTerraTutorialScenarioData& Scenario, FString& OutError);
    void TickTutorialNpcScript();
    void RefreshGameplayHighlights(const TArray<int32>& DirtyCellIds);
    bool HandleGameplayCellClick(int32 CellId, const TCHAR* SourceLabel, int32 InstanceIndex, const FString& ComponentName);
    bool TryExecuteNpcValidatedAction(int32 ExpectedTurnIndex, int32 ExpectedFactionId, int32 PieceId, int32 ToCellId, FTerraGameplayContainer::FValidatedActionExecutionResult& OutResult);
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

    UFUNCTION(BlueprintPure, Category = "PlanetTopology|Tess|SimpleGameplay T0")
    bool GetT0FactionOwnedTechnologies(int32 FactionId, TArray<ETerraGameplayTechnologyId>& OutTechnologies) const;

    UFUNCTION(BlueprintPure, Category = "PlanetTopology|Tess|SimpleGameplay T0")
    bool GetT0FactionTechnologyScore(int32 FactionId, int32& OutScore) const;

    UFUNCTION(BlueprintPure, Category = "PlanetTopology|Tess|SimpleGameplay T0")
    void GetT0AllFactionTechnologyStates(TArray<FTerraGameplayFactionTechnologyState>& OutStates) const;

    const FTerraGameplayContainer* GetGameplayContainer() const { return GameplayContainer.Get(); }
    FTerraGameplayContainer* GetGameplayContainer() { return GameplayContainer.Get(); }
    int32 GetLastHighlightedFactionId() const { return G2_5LastHighlightedFactionId; }
    bool HasInitializedGameplay() const { return GameplayContainer.IsValid() && GameplayContainer->IsInitialized(); }
    bool IsTurnActivationReady() const { return bTurnActivationReady; }
    void CollectCommittedActionLogEntries(TArray<FTerraGameplayActionLogEntry>& OutEntries) const;
    bool GetFactionTechnologyState(int32 FactionId, FTerraGameplayFactionTechnologyState& OutState) const;
    void CollectFactionTechnologyStates(TArray<FTerraGameplayFactionTechnologyState>& OutStates) const;
    void CollectFactionStates(TArray<FTerraGameplayFactionState>& OutStates) const;
    FTerraGameplayTechnologyProgressionConfig GetTechnologyProgressionConfig() const { return T0TechnologyProgressionConfig; }
    bool ChoosePendingTechnology(int32 FactionId, ETerraGameplayTechnologyId TechnologyId, FString& OutError);

private:
    APlanetTessellatedMesh* GetHost() const;
    void BuildNpcMcpInteractionState_(FTerraNpcMcpGameplayBridge::FInteractionStateSnapshot& OutState) const;
    void FillNpcMcpReviewResult_(FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult, bool bOk, const FString& Error) const;
    void BeginTurnActivationGate_(int32 ExpectedTurnIndex, int32 ExpectedFactionId, float DelaySeconds);
    void OpenTurnActivationGate_(int32 ExpectedTurnIndex, int32 ExpectedFactionId);

    TUniquePtr<FTerraGameplayContainer> GameplayContainer;
    int32 G2_5LastHighlightedFactionId = INDEX_NONE;
    TArray<FTerraG1DebugPiece> G1DebugPieces;
    TArray<FIntPoint> MountainRidgeSegments;
    TArray<FTerraTutorialNpcAction> TutorialNpcActionSequence;
    bool bTutorialNpcScriptFailed = false;
    // Rules may already point at the next faction, but no input, camera turn focus, or NPC decision may start until this gate opens.
    bool bTurnActivationReady = true;
    int32 PendingTurnActivationTurnIndex = INDEX_NONE;
    int32 PendingTurnActivationFactionId = INDEX_NONE;
    FTimerHandle TurnActivationTimerHandle;
};
