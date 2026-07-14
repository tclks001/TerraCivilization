#pragma once

#include "CoreMinimal.h"
#include "TerraGameplayTypes.h"

class GAMEPLAY_API FTerraGameplayContainer
{
public:
    struct FLegalActionQuery
    {
        int32 PieceId = INDEX_NONE;
        int32 FromCellId = INDEX_NONE;
        int32 ToCellId = INDEX_NONE;
        bool bIsJump = false;
        TArray<FTerraGameplayCaptureEntry> CaptureEntries;
    };

    struct FActionRiskQuery
    {
        bool bValidAction = false;
        bool bDestinationThreatened = false;
        int32 ThreatCount = 0;
        TArray<int32> ThreateningPieceIds;
        TArray<int32> ThreateningFactionIds;
    };

    struct FValidatedActionExecutionResult
    {
        bool bAccepted = false;
        bool bExecuted = false;
        FString RejectReason;
        int32 TurnIndexBefore = INDEX_NONE;
        int32 TurnIndexAfter = INDEX_NONE;
        int32 FactionIdBefore = INDEX_NONE;
        int32 FactionIdAfter = INDEX_NONE;
        int32 PieceId = INDEX_NONE;
        int32 FromCellId = INDEX_NONE;
        int32 ToCellId = INDEX_NONE;
        bool bWasJump = false;
        TArray<FTerraGameplayCaptureEntry> CaptureEntries;
        TArray<int32> DirtyCellIds;
    };

    struct FPieceTurnSurvey
    {
        int32 ReachableActionCount = 0;
        bool bCanCaptureNow = false;
        int32 MaxCaptureCount = 0;
        bool bThreatenedIfHold = false;
        int32 HoldThreatCount = 0;
        TArray<int32> ThreateningPieceIds;
    };

    struct FLocalTacticalSituationCard
    {
        int32 SchemaVersion = 1;
        int32 PieceId = INDEX_NONE;
        ETerraGameplayPieceType PieceType = ETerraGameplayPieceType::Infantry;
        int32 FromCellId = INDEX_NONE;
        int32 ToCellId = INDEX_NONE;
        bool bIsJump = false;
        int32 CaptureCount = 0;

        ETerraGameplayTerrainType FromTerrainType = ETerraGameplayTerrainType::Plain;
        ETerraGameplayTerrainType ToTerrainType = ETerraGameplayTerrainType::Plain;
        TArray<int32> FromNearbyFriendlyPieceIds;
        TArray<int32> FromNearbyEnemyPieceIds;
        TArray<int32> ToNearbyFriendlyPieceIds;
        TArray<int32> ToNearbyEnemyPieceIds;
        int32 NearestEnemyDistanceFrom = INDEX_NONE;
        int32 NearestEnemyDistanceTo = INDEX_NONE;
        bool bDestinationThreatened = false;
        int32 ThreatCount = 0;
        TArray<int32> ThreateningPieceIds;

        int32 ToCellNeighborCount = 0;
        TArray<int32> ApproachCellIds;
        TArray<int32> ForwardNeighborCellIds;
        int32 ForwardEmptyCellCount = 0;
        TArray<int32> ForwardFriendlyPieceIds;
        TArray<int32> ForwardEnemyPieceIds;
        int32 RingOneCellCount = 0;
        int32 RingOneForestCellCount = 0;
        int32 RingOneMountainCellCount = 0;
        int32 RingOneFriendlyPieceCount = 0;
        int32 RingOneEnemyPieceCount = 0;
        int32 RingTwoCellCount = 0;
        int32 RingTwoForestCellCount = 0;
        int32 RingTwoMountainCellCount = 0;
        int32 RingTwoFriendlyPieceCount = 0;
        int32 RingTwoEnemyPieceCount = 0;

        int32 OrdinaryTargetCountAfterMove = 0;
        int32 JumpTargetCountAfterMove = 0;
        int32 ReachableEndpointCountBeforeMove = 0;
        int32 ReachableEndpointCountAfterMove = 0;
        int32 ForwardEnterableCellCount = 0;
        TArray<int32> ForwardBlockedByMountainCellIds;
        TArray<int32> MovedPieceJumpAnchorFriendlyPieceIds;
        int32 SupportDelta = 0;
        bool bEnemyResponseThreatensMovedPiece = false;
        TArray<int32> EnemyResponseThreateningPieceIds;
        TArray<int32> EnemyArcherLineThreateningPieceIds;
        TArray<FString> SummaryTags;
    };

    struct FStrategicSnapshot
    {
        int32 TurnIndex = INDEX_NONE;
        int32 CurrentFactionId = INDEX_NONE;
        ETerraGameplayInteractionPhase InteractionPhase = ETerraGameplayInteractionPhase::Idle;
    };

    struct FFactionStrategicSummary
    {
        int32 TotalAlivePieceCount = 0;
        int32 CommanderCount = 0;
        int32 ArcherCount = 0;
        int32 CavalryCount = 0;
        int32 InfantryCount = 0;
        int32 MovablePieceCount = 0;
        int32 NearestEnemyDistance = INDEX_NONE;
        TArray<int32> FrontlinePieceIds;
        TArray<int32> IsolatedMovablePieceIds;
    };

    struct FFrontlineContact
    {
        int32 FriendlyPieceId = INDEX_NONE;
        int32 FriendlyCellId = INDEX_NONE;
        int32 EnemyPieceId = INDEX_NONE;
        int32 EnemyFactionId = INDEX_NONE;
        int32 EnemyCellId = INDEX_NONE;
        int32 Distance = INDEX_NONE;
        ETerraGameplayTerrainType FriendlyTerrainType = ETerraGameplayTerrainType::Plain;
        int32 FriendlyAdjacentSupportCount = 0;
    };

    struct FTerrainControlPoint
    {
        int32 CellId = INDEX_NONE;
        ETerraGameplayTerrainType TerrainType = ETerraGameplayTerrainType::Plain;
        int32 NearestFriendlyDistance = INDEX_NONE;
        int32 NearestEnemyDistance = INDEX_NONE;
        FString ControlStatus;
        TArray<int32> NearbyFriendlyPieceIds;
        TArray<int32> NearbyEnemyPieceIds;
    };

    struct FEnemyPressureSummary
    {
        int32 EnemyFactionId = INDEX_NONE;
        int32 NearestContactDistance = INDEX_NONE;
        int32 FrontlineContactCount = 0;
        int32 PressurePriority = 0;
        TArray<int32> RepresentativeEnemyPieceIds;
        TArray<int32> RepresentativeFriendlyPieceIds;
    };

    struct FStrategicOption
    {
        FString Intent;
        FString EvidenceScope;
        TArray<FString> EvidenceTags;
        TArray<int32> KeyPieceIds;
        TArray<int32> KeyCellIds;
        TArray<int32> RouteCellIds;
        TArray<FString> TerrainTags;
        int32 TargetEnemyFactionId = INDEX_NONE;
        TArray<FString> ValidationQuestions;
    };

    struct FCurrentFactionStrategicSnapshot
    {
        FStrategicSnapshot Snapshot;
        FFactionStrategicSummary FactionSummary;
        TArray<FFrontlineContact> FrontlineContacts;
        TArray<FTerrainControlPoint> TerrainControlPoints;
        TArray<FEnemyPressureSummary> EnemyPressures;
        TArray<FStrategicOption> StrategicOptions;
    };

    struct FLocalTopologyCellInfo
    {
        int32 CellId = INDEX_NONE;
        ETerraGameplayTerrainType TerrainType = ETerraGameplayTerrainType::Plain;
        int32 OccupyingPieceId = INDEX_NONE;
        int32 OccupyingFactionId = INDEX_NONE;
        ETerraGameplayPieceType OccupyingPieceType = ETerraGameplayPieceType::Infantry;
        bool bOccupyingPieceCanMove = false;
    };

    struct FLocalTopologyEdge
    {
        int32 CellAId = INDEX_NONE;
        int32 CellBId = INDEX_NONE;
    };

    struct FLocalTopologyObservation
    {
        FStrategicSnapshot Snapshot;
        int32 CenterCellId = INDEX_NONE;
        int32 Radius = 3;
        TArray<FLocalTopologyCellInfo> Cells;
        TArray<FLocalTopologyEdge> Edges;
    };

    struct FInteractionUndoSnapshot
    {
        TArray<FTerraGameplayPieceState> Pieces;
        TArray<int32> CellToPieceId;
        TMap<int32, FTerraGameplayEquipmentDropState> EquipmentDropsByCellId;
        TMap<int32, FTerraGameplayCellHighlight> GameplayHighlights;
        TSet<int32> OrdinaryMoveTargetCellIds;
        TSet<int32> JumpTargetCellIds;
        TMap<int32, TSet<int32>> ActionTargetCellIdToCaptureCellIds;
        TMap<int32, FTerraGameplayCaptureEntry> PendingCaptureEntriesByPieceId;
        TSet<int32> PendingCapturePieceIds;
        TSet<int32> PendingCaptureCellIds;
        TArray<int32> CurrentActionPathCellIds;
        int32 SelectedPieceId = INDEX_NONE;
        int32 SelectedPieceStartCellId = INDEX_NONE;
        int32 LastJumpStartCellId = INDEX_NONE;
        ETerraGameplayInteractionPhase InteractionPhase = ETerraGameplayInteractionPhase::Idle;
    };

    void Initialize(const TArray<FTerraGameplayCellState>& InCells, const FTerraGameplayNeutralSpawnConfig& InNeutralSpawnConfig = FTerraGameplayNeutralSpawnConfig());

    bool IsInitialized() const { return bInitialized; }
    int32 GetCurrentFactionId() const { return CurrentFactionId; }
    int32 GetTurnIndex() const { return TurnIndex; }
    int32 GetSelectedPieceId() const { return SelectedPieceId; }
    ETerraGameplayInteractionPhase GetInteractionPhase() const { return InteractionPhase; }
    bool IsMatchEnded() const { return bMatchEnded; }
    int32 GetWinningFactionId() const { return WinningFactionId; }

    bool HandleCellClick(int32 CellId, TArray<int32>& OutDirtyCellIds);
    bool UndoCurrentInteraction(TArray<int32>& OutDirtyCellIds);
    void SetDebugKeepSameFactionOnEndTurn(bool bInKeepSameFaction) { bDebugKeepSameFactionOnEndTurn = bInKeepSameFaction; }
    bool GetHighlightForCell(int32 CellId, FTerraGameplayCellHighlight& OutHighlight) const;
    int32 GetCurrentFactionBaseCellId() const;
    bool IsCurrentFactionPieceCell(int32 CellId) const;
    bool IsCurrentActionTargetCell(int32 CellId) const;
    bool IsCapturePreviewCellForActionTarget(int32 CaptureCellId, int32 ActionTargetCellId) const;
    bool CollectCapturePreviewCellIdsForActionTarget(int32 ActionTargetCellId, TArray<int32>& OutCellIds) const;
    bool CollectFactionPieceCellIds(int32 FactionId, TArray<int32>& OutCellIds) const;
    bool CollectCurrentFactionPieceCellIds(TArray<int32>& OutCellIds) const;
    bool CollectCurrentFactionSelectablePieceIds(TArray<int32>& OutPieceIds) const;
    bool CollectPendingCaptureEntries(TArray<FTerraGameplayCaptureEntry>& OutCaptureEntries) const;
    bool CollectCurrentFactionLegalActions(TArray<FLegalActionQuery>& OutActions) const;
    bool CollectSelectedPieceLegalActions(TArray<FLegalActionQuery>& OutActions) const;
    bool GetSelectedPieceLegalAction(int32 ToCellId, FLegalActionQuery& OutAction) const;
    bool EvaluateCurrentFactionActionRisk(int32 PieceId, int32 ToCellId, FActionRiskQuery& OutRisk) const;
    bool IsCurrentFactionLegalAction(int32 PieceId, int32 ToCellId, FLegalActionQuery& OutAction) const;
    bool TryExecuteValidatedAction(int32 ExpectedTurnIndex, int32 ExpectedFactionId, int32 PieceId, int32 ToCellId, FValidatedActionExecutionResult& OutResult);
    bool TryGetPieceCellId(int32 PieceId, int32& OutCellId) const;
    bool GetCellTerrainType(int32 CellId, ETerraGameplayTerrainType& OutTerrainType) const;
    bool CollectAdjacentPieceIds(int32 CellId, int32 PerspectiveFactionId, TArray<int32>& OutFriendlyPieceIds, TArray<int32>& OutEnemyPieceIds) const;
    bool FindNearestEnemyDistance(int32 CellId, int32 PerspectiveFactionId, int32& OutDistance) const;
    bool QueryCurrentFactionPieceTurnSurvey(int32 PieceId, FPieceTurnSurvey& OutSurvey) const;
    bool BuildLocalTacticalSituationCard(const FLegalActionQuery& Action, FLocalTacticalSituationCard& OutCard) const;
    bool BuildCurrentFactionStrategicSnapshot(FCurrentFactionStrategicSnapshot& OutSnapshot) const;
    bool BuildLocalTopologyObservation(int32 CenterCellId, FLocalTopologyObservation& OutObservation) const;
    const TArray<FTerraGameplayPieceState>& GetPieces() const { return Pieces; }
    const TArray<FTerraGameplayFactionState>& GetFactions() const { return Factions; }
    const TArray<int32>& GetCellToPieceId() const { return CellToPieceId; }
    void CollectEquipmentDrops(TArray<FTerraGameplayEquipmentDropState>& OutEquipmentDrops) const;

private:
    void ResetRuntimeState_();
    void BuildInitialPieces_();
    void BuildNeutralPieces_(const FTerraGameplayNeutralSpawnConfig& NeutralSpawnConfig);
    bool AddPiece_(int32 FactionId, int32 CellId, ETerraGameplayPieceType PieceType, int32& OutPieceId);
    bool AddPieceIfFree_(int32 FactionId, int32 CellId, ETerraGameplayPieceType PieceType, int32& OutPieceId);
    bool TryAddNeutralPiece_(ETerraGameplayPieceType PieceType, int32& OutPieceId);

    bool IsValidCellId_(int32 CellId) const;
    bool IsValidPieceId_(int32 PieceId) const;
    bool IsCellEmpty_(int32 CellId) const;
    bool IsNeighbor_(int32 FromCellId, int32 ToCellId) const;
    int32 GetPieceIdAtCell_(int32 CellId) const;
    FTerraGameplayPieceState* GetMutablePiece_(int32 PieceId);
    const FTerraGameplayPieceState* GetPiece_(int32 PieceId) const;
    bool IsNeutralPiece_(const FTerraGameplayPieceState& Piece) const;
    bool IsPieceFriendlyToFaction_(const FTerraGameplayPieceState& Piece, int32 FactionId) const;
    bool IsPieceEnemyToFaction_(const FTerraGameplayPieceState& Piece, int32 FactionId) const;
    bool IsCavalryCapable_(const FTerraGameplayPieceState& Piece) const;
    bool IsArcherCapable_(const FTerraGameplayPieceState& Piece) const;
    void AddEquipmentDropsForCapturedPiece_(const FTerraGameplayPieceState& CapturedPiece, int32 CaptureCellId, TArray<int32>& OutDirtyCellIds);
    void TryCollectEquipmentAtCell_(FTerraGameplayPieceState& Piece, TArray<int32>& OutDirtyCellIds);
    bool IsCurrentFactionPiece_(const FTerraGameplayPieceState& Piece) const;
    bool IsPieceSelectable_(const FTerraGameplayPieceState& Piece) const;
    bool IsPieceSelectableForFaction_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId) const;
    bool CanEnterTerrain_(const FTerraGameplayPieceState& Piece, int32 TargetCellId) const;
    bool CanOrdinaryMove_(const FTerraGameplayPieceState& Piece, int32 TargetCellId) const;
    bool CanOrdinaryMoveForFaction_(const FTerraGameplayPieceState& Piece, int32 TargetCellId, int32 ActingFactionId) const;
    bool StepForwardBranches_(int32 PrevCellId, int32 CurCellId, TArray<int32>& OutNextCellIds) const;
    void CollectOrdinaryMoveTargets_(const FTerraGameplayPieceState& Piece, TSet<int32>& OutTargetCellIds) const;
    void CollectOrdinaryMoveTargetsForFaction_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId, TSet<int32>& OutTargetCellIds) const;
    void CollectJumpTargets_(const FTerraGameplayPieceState& Piece, TSet<int32>& OutTargetCellIds) const;
    void CollectJumpTargetsForFaction_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId, int32 BlockedReturnCellId, TSet<int32>& OutTargetCellIds) const;
    void CollectJumpTargetsForHypotheticalBoard_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId, int32 CurrentCellId, int32 BlockedReturnCellId, const TArray<int32>& HypotheticalCellToPieceId, TSet<int32>& OutTargetCellIds) const;
    void CollectOrdinaryMoveTargetsForHypotheticalBoard_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId, const TArray<int32>& HypotheticalCellToPieceId, TSet<int32>& OutTargetCellIds) const;
    void CollectReachableEndpointsForHypotheticalBoard_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId, const TArray<int32>& HypotheticalCellToPieceId, TSet<int32>& OutEndpointCellIds, int32& OutOrdinaryTargetCount, int32& OutJumpTargetCount) const;
    void CollectAdjacentPieceIdsForBoard_(int32 CellId, int32 PerspectiveFactionId, const TArray<int32>& HypotheticalCellToPieceId, TArray<int32>& OutFriendlyPieceIds, TArray<int32>& OutEnemyPieceIds) const;
    bool FindNearestEnemyDistanceForBoard_(int32 CellId, int32 PerspectiveFactionId, const TArray<int32>& HypotheticalCellToPieceId, int32& OutDistance) const;
    void BuildHypotheticalBoardForAction_(const FLegalActionQuery& Action, TArray<int32>& OutCellToPieceId) const;
    void FindNearestPieceIdsForFactions_(int32 StartCellId, const TSet<int32>& TargetFactionIds, int32& OutDistance, TArray<int32>& OutPieceIds) const;
    void CollectCaptureEntriesAfterHypotheticalMove_(const FTerraGameplayPieceState& Piece, int32 TargetCellId, TMap<int32, FTerraGameplayCaptureEntry>& OutCaptureEntriesByCellId) const;
    void CollectCaptureEntriesAfterHypotheticalMoveForFaction_(const FTerraGameplayPieceState& Piece, int32 TargetCellId, int32 ActingFactionId, TMap<int32, FTerraGameplayCaptureEntry>& OutCaptureEntriesByCellId) const;
    void CollectCaptureCellsAfterHypotheticalMove_(const FTerraGameplayPieceState& Piece, int32 TargetCellId, TSet<int32>& OutCaptureCellIds) const;
    void RebuildActionTargetCapturePreviews_(TArray<int32>& OutDirtyCellIds);
    bool HasCapturePreviewForActionTarget_(int32 ActionTargetCellId) const;
    void LockPendingCapturesForActionTarget_(int32 ActionTargetCellId, TArray<int32>& OutDirtyCellIds);
    void RebuildPendingCapturesForSelectedPieceCell_(TArray<int32>& OutDirtyCellIds);
    void ResolvePendingCaptures_(TArray<int32>& OutDirtyCellIds);
    void EliminateFaction_(int32 FactionId, int32 ConqueringFactionId, TArray<int32>& OutDirtyCellIds);
    void EvaluateWinStateAfterCaptures_();
    void FinalizeTurnAfterResolution_();
    void InitializeActionLogFilePath_();
    void PushUndoSnapshot_();
    void ClearUndoSnapshots_();
    TArray<FTerraGameplayCaptureEntry> GetSortedPendingCaptureEntries_() const;
    FString BuildActionLogJson_(int32 ActionTurnIndex, int32 PlayerId, int32 PieceId, const TArray<int32>& PathCellIds, const TArray<FTerraGameplayCaptureEntry>& CaptureEntries) const;
    void AppendActionLogToFile_(const FString& ActionLogJson) const;
    void EmitActionLog_(int32 ActionTurnIndex, int32 PlayerId, int32 PieceId, const TArray<int32>& PathCellIds, const TArray<FTerraGameplayCaptureEntry>& CaptureEntries) const;

    bool TrySelectPieceAtCell_(int32 CellId, TArray<int32>& OutDirtyCellIds);
    bool TryMoveSelectedPieceToOrdinaryTarget_(int32 TargetCellId, TArray<int32>& OutDirtyCellIds);
    bool TryJumpSelectedPieceTo_(int32 TargetCellId, TArray<int32>& OutDirtyCellIds);
    bool TryEndTurnOnSelectedCell_(int32 CellId, TArray<int32>& OutDirtyCellIds);

    void RefreshSelectedPieceHighlights_(bool bIncludeOrdinaryMoves, TArray<int32>& OutDirtyCellIds);
    void SetSingleHighlight_(int32 CellId, const FLinearColor& Color, float Intensity, TArray<int32>& OutDirtyCellIds);
    void ClearGameplayHighlights_(TArray<int32>& OutDirtyCellIds);
    void AddDirtyCell_(int32 CellId, TArray<int32>& OutDirtyCellIds) const;
    void AdvanceTurn_();

private:
    bool bInitialized = false;

    TArray<FTerraGameplayCellState> Cells;
    TArray<FTerraGameplayPieceState> Pieces;
    TArray<FTerraGameplayFactionState> Factions;
    TArray<int32> CellToPieceId;
    TMap<int32, FTerraGameplayEquipmentDropState> EquipmentDropsByCellId;
    TMap<int32, FTerraGameplayCellHighlight> GameplayHighlights;
    TSet<int32> OrdinaryMoveTargetCellIds;
    TSet<int32> JumpTargetCellIds;
    TMap<int32, TSet<int32>> ActionTargetCellIdToCaptureCellIds;
    TMap<int32, FTerraGameplayCaptureEntry> PendingCaptureEntriesByPieceId;
    TSet<int32> PendingCapturePieceIds;
    TSet<int32> PendingCaptureCellIds;

    bool bDebugKeepSameFactionOnEndTurn = false;
    bool bMatchEnded = false;
    int32 CurrentFactionId = INDEX_NONE;
    int32 TurnIndex = 0;
    int32 WinningFactionId = INDEX_NONE;
    int32 SelectedPieceId = INDEX_NONE;
    int32 SelectedPieceStartCellId = INDEX_NONE;
    int32 LastJumpStartCellId = INDEX_NONE;
    FString ActionLogFilePath;
    TArray<int32> CurrentActionPathCellIds;
    TArray<FInteractionUndoSnapshot> InteractionUndoSnapshots;
    ETerraGameplayInteractionPhase InteractionPhase = ETerraGameplayInteractionPhase::Idle;
};




