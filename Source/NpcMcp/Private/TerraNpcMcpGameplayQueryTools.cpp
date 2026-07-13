#include "IModelContextProtocolTool.h"
#include "ModelContextProtocolToolResults.h"
#include "NpcMcp.h"
#include "TerraGameplayContainer.h"
#include "TerraNpcMcpGameplayBridge.h"

namespace
{
    TSharedPtr<FJsonValue> MakeStructuredValue_(const TSharedRef<FJsonObject>& Object)
    {
        return MakeShared<FJsonValueObject>(Object);
    }

    FModelContextProtocolToolResult MakeStructuredResult_(const TSharedRef<FJsonObject>& Object)
    {
        return UE::ModelContextProtocol::MakeStructuredContentResult(MakeStructuredValue_(Object));
    }

    void AddTypedProperty_(const TSharedRef<FJsonObject>& Properties, const TCHAR* Name, const TCHAR* Type)
    {
        TSharedRef<FJsonObject> Property = MakeShared<FJsonObject>();
        Property->SetStringField(TEXT("type"), Type);
        Properties->SetObjectField(Name, Property);
    }

    TSharedPtr<FJsonObject> MakeObjectSchema_(bool bAllowAdditionalProperties = false)
    {
        TSharedRef<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));
        Schema->SetObjectField(TEXT("properties"), MakeShared<FJsonObject>());
        Schema->SetBoolField(TEXT("additionalProperties"), bAllowAdditionalProperties);
        return Schema;
    }

    TSharedPtr<FJsonObject> MakeActionInputSchema_()
    {
        TSharedRef<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));

        TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
        AddTypedProperty_(Properties, TEXT("piece_id"), TEXT("integer"));
        AddTypedProperty_(Properties, TEXT("to_cell_id"), TEXT("integer"));
        Schema->SetObjectField(TEXT("properties"), Properties);

        TArray<TSharedPtr<FJsonValue>> Required;
        Required.Add(MakeShared<FJsonValueString>(TEXT("piece_id")));
        Required.Add(MakeShared<FJsonValueString>(TEXT("to_cell_id")));
        Schema->SetArrayField(TEXT("required"), Required);
        Schema->SetBoolField(TEXT("additionalProperties"), false);
        return Schema;
    }

    TSharedPtr<FJsonObject> MakeExecuteActionInputSchema_()
    {
        TSharedRef<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));

        TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
        AddTypedProperty_(Properties, TEXT("expected_turn_index"), TEXT("integer"));
        AddTypedProperty_(Properties, TEXT("expected_faction_id"), TEXT("integer"));
        AddTypedProperty_(Properties, TEXT("piece_id"), TEXT("integer"));
        AddTypedProperty_(Properties, TEXT("to_cell_id"), TEXT("integer"));
        Schema->SetObjectField(TEXT("properties"), Properties);

        TArray<TSharedPtr<FJsonValue>> Required;
        Required.Add(MakeShared<FJsonValueString>(TEXT("expected_turn_index")));
        Required.Add(MakeShared<FJsonValueString>(TEXT("expected_faction_id")));
        Required.Add(MakeShared<FJsonValueString>(TEXT("piece_id")));
        Required.Add(MakeShared<FJsonValueString>(TEXT("to_cell_id")));
        Schema->SetArrayField(TEXT("required"), Required);
        Schema->SetBoolField(TEXT("additionalProperties"), false);
        return Schema;
    }

    TSharedPtr<FJsonObject> MakePieceInputSchema_()
    {
        TSharedRef<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));

        TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
        AddTypedProperty_(Properties, TEXT("piece_id"), TEXT("integer"));
        Schema->SetObjectField(TEXT("properties"), Properties);

        TArray<TSharedPtr<FJsonValue>> Required;
        Required.Add(MakeShared<FJsonValueString>(TEXT("piece_id")));
        Schema->SetArrayField(TEXT("required"), Required);
        Schema->SetBoolField(TEXT("additionalProperties"), false);
        return Schema;
    }

    TSharedRef<FJsonObject> MakeCaptureObject_(const FTerraGameplayCaptureEntry& CaptureEntry)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetNumberField(TEXT("captured_piece_id"), CaptureEntry.CapturedPieceId);
        Object->SetNumberField(TEXT("captured_cell_id"), CaptureEntry.CapturedCellId);
        Object->SetNumberField(TEXT("attacker_piece_id"), CaptureEntry.AttackerPieceId);
        Object->SetNumberField(TEXT("vanguard_piece_id"), CaptureEntry.VanguardPieceId);
        return Object;
    }

    TSharedRef<FJsonObject> MakeActionObject_(const FTerraGameplayContainer::FLegalActionQuery& Action)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetNumberField(TEXT("piece_id"), Action.PieceId);
        Object->SetNumberField(TEXT("from_cell_id"), Action.FromCellId);
        Object->SetNumberField(TEXT("to_cell_id"), Action.ToCellId);
        Object->SetBoolField(TEXT("is_jump"), Action.bIsJump);
        Object->SetNumberField(TEXT("capture_count"), Action.CaptureEntries.Num());

        TArray<TSharedPtr<FJsonValue>> Captures;
        for (const FTerraGameplayCaptureEntry& CaptureEntry : Action.CaptureEntries)
        {
            Captures.Add(MakeShared<FJsonValueObject>(MakeCaptureObject_(CaptureEntry)));
        }
        Object->SetArrayField(TEXT("captures"), Captures);
        return Object;
    }

    FString TerrainTypeToTag_(ETerraGameplayTerrainType TerrainType)
    {
        switch (TerrainType)
        {
        case ETerraGameplayTerrainType::Forest:
            return TEXT("forest");
        case ETerraGameplayTerrainType::Mountain:
            return TEXT("mountain");
        case ETerraGameplayTerrainType::Plain:
        default:
            return TEXT("plain");
        }
    }

    TArray<TSharedPtr<FJsonValue>> MakeNumberArray_(const TArray<int32>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        for (const int32 Value : Values)
        {
            Result.Add(MakeShared<FJsonValueNumber>(Value));
        }
        return Result;
    }

    TArray<TSharedPtr<FJsonValue>> MakeStringArray_(const TArray<FString>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        for (const FString& Value : Values)
        {
            Result.Add(MakeShared<FJsonValueString>(Value));
        }
        return Result;
    }

    void BuildLocalInfoFields_(const FTerraGameplayContainer& GameplayContainer, int32 PerspectiveFactionId, int32 CellId, const TSharedRef<FJsonObject>& Object)
    {
        ETerraGameplayTerrainType TerrainType = ETerraGameplayTerrainType::Plain;
        TArray<FString> TerrainTags;
        if (GameplayContainer.GetCellTerrainType(CellId, TerrainType))
        {
            TerrainTags.Add(TerrainTypeToTag_(TerrainType));
        }

        TArray<int32> NearbyFriendlyPieceIds;
        TArray<int32> NearbyEnemyPieceIds;
        GameplayContainer.CollectAdjacentPieceIds(CellId, PerspectiveFactionId, NearbyFriendlyPieceIds, NearbyEnemyPieceIds);

        TArray<FString> SummaryTags;
        if (TerrainTags.Contains(TEXT("forest")))
        {
            SummaryTags.Add(TEXT("forest_cover"));
        }
        if (TerrainTags.Contains(TEXT("mountain")))
        {
            SummaryTags.Add(TEXT("mountain_control"));
        }
        if (NearbyEnemyPieceIds.Num() > 0)
        {
            SummaryTags.Add(TEXT("contested_frontier"));
        }
        if (NearbyFriendlyPieceIds.Num() >= 2)
        {
            SummaryTags.Add(TEXT("friendly_support"));
        }
        if (NearbyEnemyPieceIds.Num() == 0 && NearbyFriendlyPieceIds.Num() == 0)
        {
            SummaryTags.Add(TEXT("isolated_cell"));
        }

        Object->SetArrayField(TEXT("terrain_tags"), MakeStringArray_(TerrainTags));
        Object->SetArrayField(TEXT("nearby_enemy_piece_ids"), MakeNumberArray_(NearbyEnemyPieceIds));
        Object->SetArrayField(TEXT("nearby_friendly_piece_ids"), MakeNumberArray_(NearbyFriendlyPieceIds));
        Object->SetArrayField(TEXT("summary_tags"), MakeStringArray_(SummaryTags));
    }

    void AddNearestEnemyDistanceFields_(const FTerraGameplayContainer& GameplayContainer, int32 PerspectiveFactionId, int32 FromCellId, int32 ToCellId, const TSharedRef<FJsonObject>& Object)
    {
        int32 FromDistance = INDEX_NONE;
        if (GameplayContainer.FindNearestEnemyDistance(FromCellId, PerspectiveFactionId, FromDistance))
        {
            Object->SetNumberField(TEXT("nearest_enemy_distance_from"), FromDistance);
        }
        else
        {
            Object->SetField(TEXT("nearest_enemy_distance_from"), MakeShared<FJsonValueNull>());
        }

        int32 ToDistance = INDEX_NONE;
        if (GameplayContainer.FindNearestEnemyDistance(ToCellId, PerspectiveFactionId, ToDistance))
        {
            Object->SetNumberField(TEXT("nearest_enemy_distance_to"), ToDistance);
        }
        else
        {
            Object->SetField(TEXT("nearest_enemy_distance_to"), MakeShared<FJsonValueNull>());
        }

        if (FromDistance != INDEX_NONE && ToDistance != INDEX_NONE)
        {
            Object->SetNumberField(TEXT("approach_to_enemy"), FromDistance - ToDistance);
        }
        else
        {
            Object->SetField(TEXT("approach_to_enemy"), MakeShared<FJsonValueNull>());
        }
    }

    TSharedRef<FJsonObject> MakeExecutionResultObject_(const FTerraGameplayContainer::FValidatedActionExecutionResult& ExecutionResult)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetBoolField(TEXT("accepted"), ExecutionResult.bAccepted);
        Object->SetBoolField(TEXT("executed"), ExecutionResult.bExecuted);
        Object->SetStringField(TEXT("reject_reason"), ExecutionResult.RejectReason);
        Object->SetNumberField(TEXT("turn_index_before"), ExecutionResult.TurnIndexBefore);
        Object->SetNumberField(TEXT("turn_index_after"), ExecutionResult.TurnIndexAfter);
        Object->SetNumberField(TEXT("faction_id_before"), ExecutionResult.FactionIdBefore);
        Object->SetNumberField(TEXT("faction_id_after"), ExecutionResult.FactionIdAfter);
        Object->SetNumberField(TEXT("piece_id"), ExecutionResult.PieceId);
        Object->SetNumberField(TEXT("from_cell_id"), ExecutionResult.FromCellId);
        Object->SetNumberField(TEXT("to_cell_id"), ExecutionResult.ToCellId);
        Object->SetBoolField(TEXT("was_jump"), ExecutionResult.bWasJump);
        Object->SetNumberField(TEXT("capture_count"), ExecutionResult.CaptureEntries.Num());

        TArray<TSharedPtr<FJsonValue>> Captures;
        for (const FTerraGameplayCaptureEntry& CaptureEntry : ExecutionResult.CaptureEntries)
        {
            Captures.Add(MakeShared<FJsonValueObject>(MakeCaptureObject_(CaptureEntry)));
        }
        Object->SetArrayField(TEXT("captures"), Captures);

        TArray<TSharedPtr<FJsonValue>> DirtyCellIds;
        for (const int32 DirtyCellId : ExecutionResult.DirtyCellIds)
        {
            DirtyCellIds.Add(MakeShared<FJsonValueNumber>(DirtyCellId));
        }
        Object->SetArrayField(TEXT("dirty_cell_ids"), DirtyCellIds);
        return Object;
    }

    TSharedRef<FJsonObject> MakeInteractionStateObject_(const FTerraNpcMcpGameplayBridge::FInteractionStateSnapshot& InteractionState)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetNumberField(TEXT("turn_index"), InteractionState.TurnIndex);
        Object->SetNumberField(TEXT("current_faction_id"), InteractionState.CurrentFactionId);
        Object->SetStringField(TEXT("phase"), StaticEnum<ETerraGameplayInteractionPhase>()->GetNameStringByValue(static_cast<int64>(InteractionState.InteractionPhase)));
        Object->SetNumberField(TEXT("selected_piece_id"), InteractionState.SelectedPieceId);
        Object->SetNumberField(TEXT("selected_piece_cell_id"), InteractionState.SelectedPieceCellId);
        Object->SetBoolField(TEXT("can_select_piece_now"), InteractionState.bCanSelectPieceNow);
        Object->SetBoolField(TEXT("can_preview_target_now"), InteractionState.bCanPreviewTargetNow);
        Object->SetBoolField(TEXT("can_confirm_now"), InteractionState.bCanConfirmNow);
        Object->SetBoolField(TEXT("can_cancel_now"), InteractionState.bCanCancelNow);

        TArray<TSharedPtr<FJsonValue>> ContinueJumpTargetCellIds;
        for (const int32 CellId : InteractionState.ContinueJumpTargetCellIds)
        {
            ContinueJumpTargetCellIds.Add(MakeShared<FJsonValueNumber>(CellId));
        }
        Object->SetArrayField(TEXT("continue_jump_target_cell_ids"), ContinueJumpTargetCellIds);
        return Object;
    }

    void AddGameplayUnavailableFields_(const TSharedRef<FJsonObject>& Result)
    {
        Result->SetBoolField(TEXT("ok"), false);
        Result->SetStringField(TEXT("error"), TEXT("gameplay_unavailable"));
    }

    const FTerraGameplayContainer* GetReadyGameplay_(const TSharedRef<FJsonObject>& Result)
    {
        const FTerraGameplayContainer* GameplayContainer = FTerraNpcMcpGameplayBridge::GetGameplayContainer();
        if (!GameplayContainer || !GameplayContainer->IsInitialized())
        {
            AddGameplayUnavailableFields_(Result);
            return nullptr;
        }
        return GameplayContainer;
    }

    class FGetTurnContextTool final : public IModelContextProtocolTool
    {
    public:
        virtual FString GetName() const override { return TEXT("terra.get_turn_context"); }
        virtual FString GetDescription() const override { return TEXT("Returns the current turn snapshot used as the first deterministic query for NPC decision making."); }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override { return MakeObjectSchema_(); }

        virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
        {
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            const FTerraGameplayContainer* GameplayContainer = GetReadyGameplay_(Result);
            if (!GameplayContainer)
            {
                return MakeStructuredResult_(Result);
            }

            Result->SetBoolField(TEXT("ok"), true);
            Result->SetNumberField(TEXT("turn_index"), GameplayContainer->GetTurnIndex());
            Result->SetNumberField(TEXT("current_faction_id"), GameplayContainer->GetCurrentFactionId());
            Result->SetNumberField(TEXT("selected_piece_id"), GameplayContainer->GetSelectedPieceId());
            Result->SetNumberField(TEXT("interaction_phase"), static_cast<int32>(GameplayContainer->GetInteractionPhase()));
            Result->SetBoolField(TEXT("match_ended"), GameplayContainer->IsMatchEnded());
            Result->SetNumberField(TEXT("winning_faction_id"), GameplayContainer->GetWinningFactionId());
            Result->SetNumberField(TEXT("cell_count"), GameplayContainer->GetCellToPieceId().Num());
            Result->SetNumberField(TEXT("piece_count"), GameplayContainer->GetPieces().Num());
            Result->SetNumberField(TEXT("faction_count"), GameplayContainer->GetFactions().Num());

            TArray<TSharedPtr<FJsonValue>> CurrentFactionPieceIds;
            for (const FTerraGameplayPieceState& Piece : GameplayContainer->GetPieces())
            {
                if (Piece.bAlive && Piece.OwnerFactionId == GameplayContainer->GetCurrentFactionId())
                {
                    CurrentFactionPieceIds.Add(MakeShared<FJsonValueNumber>(Piece.PieceId));
                }
            }
            Result->SetArrayField(TEXT("current_faction_piece_ids"), CurrentFactionPieceIds);
            FNpcMcpModule::RefreshInteractiveToolAvailability();
            return MakeStructuredResult_(Result);
        }
    };

    class FListLegalActionsTool final : public IModelContextProtocolTool
    {
    public:
        virtual FString GetName() const override { return TEXT("terra.list_legal_actions"); }
        virtual FString GetDescription() const override { return TEXT("Lists deterministic legal one-step actions for the current faction without mutating Gameplay state."); }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override { return MakeObjectSchema_(); }

        virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
        {
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            const FTerraGameplayContainer* GameplayContainer = GetReadyGameplay_(Result);
            if (!GameplayContainer)
            {
                return MakeStructuredResult_(Result);
            }

            TArray<FTerraGameplayContainer::FLegalActionQuery> Actions;
            GameplayContainer->CollectCurrentFactionLegalActions(Actions);

            TArray<TSharedPtr<FJsonValue>> ActionValues;
            for (const FTerraGameplayContainer::FLegalActionQuery& Action : Actions)
            {
                ActionValues.Add(MakeShared<FJsonValueObject>(MakeActionObject_(Action)));
            }

            Result->SetBoolField(TEXT("ok"), true);
            Result->SetNumberField(TEXT("turn_index"), GameplayContainer->GetTurnIndex());
            Result->SetNumberField(TEXT("current_faction_id"), GameplayContainer->GetCurrentFactionId());
            Result->SetNumberField(TEXT("action_count"), Actions.Num());
            Result->SetArrayField(TEXT("actions"), ActionValues);
            return MakeStructuredResult_(Result);
        }
    };

    class FEvaluateActionRiskTool final : public IModelContextProtocolTool
    {
    public:
        virtual FString GetName() const override { return TEXT("terra.evaluate_action_risk"); }
        virtual FString GetDescription() const override { return TEXT("Evaluates whether a proposed current-faction action is legal and whether the destination is threatened."); }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override { return MakeActionInputSchema_(); }

        virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
        {
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            const FTerraGameplayContainer* GameplayContainer = GetReadyGameplay_(Result);
            if (!GameplayContainer)
            {
                return MakeStructuredResult_(Result);
            }

            int32 PieceId = INDEX_NONE;
            int32 ToCellId = INDEX_NONE;
            if (!Params || !Params->TryGetNumberField(TEXT("piece_id"), PieceId) || !Params->TryGetNumberField(TEXT("to_cell_id"), ToCellId))
            {
                Result->SetBoolField(TEXT("ok"), false);
                Result->SetStringField(TEXT("error"), TEXT("invalid_params"));
                return MakeStructuredResult_(Result);
            }

            FTerraGameplayContainer::FActionRiskQuery Risk;
            GameplayContainer->EvaluateCurrentFactionActionRisk(PieceId, ToCellId, Risk);

            TArray<TSharedPtr<FJsonValue>> ThreateningPieceIds;
            for (const int32 ThreateningPieceId : Risk.ThreateningPieceIds)
            {
                ThreateningPieceIds.Add(MakeShared<FJsonValueNumber>(ThreateningPieceId));
            }

            TArray<TSharedPtr<FJsonValue>> ThreateningFactionIds;
            for (const int32 ThreateningFactionId : Risk.ThreateningFactionIds)
            {
                ThreateningFactionIds.Add(MakeShared<FJsonValueNumber>(ThreateningFactionId));
            }

            Result->SetBoolField(TEXT("ok"), true);
            Result->SetNumberField(TEXT("piece_id"), PieceId);
            Result->SetNumberField(TEXT("to_cell_id"), ToCellId);
            Result->SetBoolField(TEXT("valid_action"), Risk.bValidAction);
            Result->SetBoolField(TEXT("destination_threatened"), Risk.bDestinationThreatened);
            Result->SetNumberField(TEXT("threat_count"), Risk.ThreatCount);
            Result->SetArrayField(TEXT("threatening_piece_ids"), ThreateningPieceIds);
            Result->SetArrayField(TEXT("threatening_faction_ids"), ThreateningFactionIds);
            return MakeStructuredResult_(Result);
        }
    };

    class FSubmitActionProposalTool final : public IModelContextProtocolTool
    {
    public:
        virtual FString GetName() const override { return TEXT("terra.submit_action_proposal"); }
        virtual FString GetDescription() const override { return TEXT("Validates and echoes an NPC action proposal. Phase 2 does not execute the action."); }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override { return MakeActionInputSchema_(); }

        virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
        {
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            const FTerraGameplayContainer* GameplayContainer = GetReadyGameplay_(Result);
            if (!GameplayContainer)
            {
                return MakeStructuredResult_(Result);
            }

            int32 PieceId = INDEX_NONE;
            int32 ToCellId = INDEX_NONE;
            if (!Params || !Params->TryGetNumberField(TEXT("piece_id"), PieceId) || !Params->TryGetNumberField(TEXT("to_cell_id"), ToCellId))
            {
                Result->SetBoolField(TEXT("ok"), false);
                Result->SetStringField(TEXT("error"), TEXT("invalid_params"));
                return MakeStructuredResult_(Result);
            }

            FTerraGameplayContainer::FLegalActionQuery LegalAction;
            const bool bLegal = GameplayContainer->IsCurrentFactionLegalAction(PieceId, ToCellId, LegalAction);

            Result->SetBoolField(TEXT("ok"), true);
            Result->SetBoolField(TEXT("accepted"), bLegal);
            Result->SetBoolField(TEXT("executed"), false);
            Result->SetStringField(TEXT("phase"), TEXT("phase2_validate_only"));
            Result->SetNumberField(TEXT("turn_index"), GameplayContainer->GetTurnIndex());
            Result->SetNumberField(TEXT("current_faction_id"), GameplayContainer->GetCurrentFactionId());
            Result->SetNumberField(TEXT("piece_id"), PieceId);
            Result->SetNumberField(TEXT("to_cell_id"), ToCellId);
            if (bLegal)
            {
                Result->SetObjectField(TEXT("legal_action"), MakeActionObject_(LegalAction));
            }
            else
            {
                Result->SetStringField(TEXT("reject_reason"), TEXT("not_a_current_faction_legal_action"));
            }
            return MakeStructuredResult_(Result);
        }
    };

    class FExecuteValidatedActionTool final : public IModelContextProtocolTool
    {
    public:
        virtual FString GetName() const override { return TEXT("terra.execute_validated_action"); }
        virtual FString GetDescription() const override { return TEXT("Executes a previously validated current-faction action after checking snapshot turn and faction ids."); }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override { return MakeExecuteActionInputSchema_(); }

        virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
        {
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            FTerraGameplayContainer* GameplayContainer = FTerraNpcMcpGameplayBridge::GetMutableGameplayContainer();
            if (!GameplayContainer || !GameplayContainer->IsInitialized())
            {
                AddGameplayUnavailableFields_(Result);
                return MakeStructuredResult_(Result);
            }

            int32 ExpectedTurnIndex = INDEX_NONE;
            int32 ExpectedFactionId = INDEX_NONE;
            int32 PieceId = INDEX_NONE;
            int32 ToCellId = INDEX_NONE;
            if (!Params
                || !Params->TryGetNumberField(TEXT("expected_turn_index"), ExpectedTurnIndex)
                || !Params->TryGetNumberField(TEXT("expected_faction_id"), ExpectedFactionId)
                || !Params->TryGetNumberField(TEXT("piece_id"), PieceId)
                || !Params->TryGetNumberField(TEXT("to_cell_id"), ToCellId))
            {
                Result->SetBoolField(TEXT("ok"), false);
                Result->SetStringField(TEXT("error"), TEXT("invalid_params"));
                return MakeStructuredResult_(Result);
            }

            FTerraGameplayContainer::FValidatedActionExecutionResult ExecutionResult;
            FTerraNpcMcpGameplayBridge::TryExecuteValidatedAction(ExpectedTurnIndex, ExpectedFactionId, PieceId, ToCellId, ExecutionResult);

            Result->SetBoolField(TEXT("ok"), true);
            Result->SetObjectField(TEXT("execution"), MakeExecutionResultObject_(ExecutionResult));
            Result->SetBoolField(TEXT("accepted"), ExecutionResult.bAccepted);
            Result->SetBoolField(TEXT("executed"), ExecutionResult.bExecuted);
            Result->SetStringField(TEXT("reject_reason"), ExecutionResult.RejectReason);
            Result->SetNumberField(TEXT("turn_index_before"), ExecutionResult.TurnIndexBefore);
            Result->SetNumberField(TEXT("turn_index_after"), ExecutionResult.TurnIndexAfter);
            Result->SetNumberField(TEXT("faction_id_before"), ExecutionResult.FactionIdBefore);
            Result->SetNumberField(TEXT("faction_id_after"), ExecutionResult.FactionIdAfter);
            Result->SetNumberField(TEXT("piece_id"), ExecutionResult.PieceId);
            Result->SetNumberField(TEXT("from_cell_id"), ExecutionResult.FromCellId);
            Result->SetNumberField(TEXT("to_cell_id"), ExecutionResult.ToCellId);
            Result->SetBoolField(TEXT("was_jump"), ExecutionResult.bWasJump);
            Result->SetNumberField(TEXT("capture_count"), ExecutionResult.CaptureEntries.Num());
            FNpcMcpModule::RefreshInteractiveToolAvailability();
            return MakeStructuredResult_(Result);
        }
    };

    class FUiBeginTurnReviewTool final : public IModelContextProtocolTool
    {
    public:
        virtual FString GetName() const override { return TEXT("terra.ui_begin_turn_review"); }
        virtual FString GetDescription() const override { return TEXT("Returns the current turn summary for interactive tactical review. Only available in Idle phase."); }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override { return MakeObjectSchema_(); }

        virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
        {
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            FTerraNpcMcpGameplayBridge::FUiReviewResult ReviewResult;
            const bool bOk = FTerraNpcMcpGameplayBridge::TryUiBeginTurnReview(ReviewResult);
            Result->SetBoolField(TEXT("ok"), bOk && ReviewResult.bOk);
            if (!ReviewResult.Error.IsEmpty())
            {
                Result->SetStringField(TEXT("error"), ReviewResult.Error);
            }
            Result->SetObjectField(TEXT("interaction_state"), MakeInteractionStateObject_(ReviewResult.InteractionState));

            if (const FTerraGameplayContainer* GameplayContainer = FTerraNpcMcpGameplayBridge::GetGameplayContainer();
                GameplayContainer && GameplayContainer->IsInitialized())
            {
                TArray<int32> SelectablePieceIds;
                GameplayContainer->CollectCurrentFactionSelectablePieceIds(SelectablePieceIds);
                TArray<TSharedPtr<FJsonValue>> Pieces;
                for (const int32 PieceId : SelectablePieceIds)
                {
                    int32 CellId = INDEX_NONE;
                    if (!GameplayContainer->TryGetPieceCellId(PieceId, CellId))
                    {
                        continue;
                    }
                    const TArray<FTerraGameplayPieceState>& PieceStates = GameplayContainer->GetPieces();
                    if (!PieceStates.IsValidIndex(PieceId))
                    {
                        continue;
                    }

                    TSharedRef<FJsonObject> PieceObject = MakeShared<FJsonObject>();
                    PieceObject->SetNumberField(TEXT("piece_id"), PieceId);
                    PieceObject->SetStringField(TEXT("piece_type"), StaticEnum<ETerraGameplayPieceType>()->GetNameStringByValue(static_cast<int64>(PieceStates[PieceId].PieceType)));
                    PieceObject->SetNumberField(TEXT("cell_id"), CellId);
                    BuildLocalInfoFields_(*GameplayContainer, GameplayContainer->GetCurrentFactionId(), CellId, PieceObject);

                    FTerraGameplayContainer::FPieceTurnSurvey Survey;
                    if (GameplayContainer->QueryCurrentFactionPieceTurnSurvey(PieceId, Survey))
                    {
                        PieceObject->SetNumberField(TEXT("legal_action_count"), Survey.ReachableActionCount);
                        PieceObject->SetBoolField(TEXT("can_capture_now"), Survey.bCanCaptureNow);
                        PieceObject->SetNumberField(TEXT("max_capture_count"), Survey.MaxCaptureCount);
                        PieceObject->SetBoolField(TEXT("threatened_if_hold"), Survey.bThreatenedIfHold);
                        PieceObject->SetNumberField(TEXT("hold_threat_count"), Survey.HoldThreatCount);
                        PieceObject->SetArrayField(TEXT("threatening_piece_ids"), MakeNumberArray_(Survey.ThreateningPieceIds));
                    }
                    else
                    {
                        PieceObject->SetNumberField(TEXT("legal_action_count"), 0);
                        PieceObject->SetBoolField(TEXT("can_capture_now"), false);
                        PieceObject->SetNumberField(TEXT("max_capture_count"), 0);
                        PieceObject->SetBoolField(TEXT("threatened_if_hold"), false);
                        PieceObject->SetNumberField(TEXT("hold_threat_count"), 0);
                        PieceObject->SetArrayField(TEXT("threatening_piece_ids"), TArray<TSharedPtr<FJsonValue>>());
                    }
                    Pieces.Add(MakeShared<FJsonValueObject>(PieceObject));
                }

                Result->SetNumberField(TEXT("piece_count"), Pieces.Num());
                Result->SetArrayField(TEXT("pieces"), Pieces);
            }

            FNpcMcpModule::RefreshInteractiveToolAvailability();
            return MakeStructuredResult_(Result);
        }
    };

    class FUiSelectPieceTool final : public IModelContextProtocolTool
    {
    public:
        virtual FString GetName() const override { return TEXT("terra.ui_select_piece"); }
        virtual FString GetDescription() const override { return TEXT("Clicks a selectable current-faction piece and returns current move options. Only available in Idle or PieceSelected phase."); }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override { return MakePieceInputSchema_(); }

        virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
        {
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            int32 PieceId = INDEX_NONE;
            if (!Params || !Params->TryGetNumberField(TEXT("piece_id"), PieceId))
            {
                Result->SetBoolField(TEXT("ok"), false);
                Result->SetStringField(TEXT("error"), TEXT("invalid_params"));
                return MakeStructuredResult_(Result);
            }

            FTerraNpcMcpGameplayBridge::FUiReviewResult ReviewResult;
            const bool bOk = FTerraNpcMcpGameplayBridge::TryUiSelectPiece(PieceId, ReviewResult);
            Result->SetBoolField(TEXT("ok"), bOk && ReviewResult.bOk);
            if (!ReviewResult.Error.IsEmpty())
            {
                Result->SetStringField(TEXT("error"), ReviewResult.Error);
            }
            Result->SetObjectField(TEXT("interaction_state"), MakeInteractionStateObject_(ReviewResult.InteractionState));
            Result->SetNumberField(TEXT("selected_piece_id"), ReviewResult.InteractionState.SelectedPieceId);
            Result->SetNumberField(TEXT("selected_piece_cell_id"), ReviewResult.InteractionState.SelectedPieceCellId);

            if (const FTerraGameplayContainer* GameplayContainer = FTerraNpcMcpGameplayBridge::GetGameplayContainer();
                GameplayContainer && GameplayContainer->IsInitialized() && ReviewResult.InteractionState.SelectedPieceId != INDEX_NONE)
            {
                TArray<FTerraGameplayContainer::FLegalActionQuery> Actions;
                GameplayContainer->CollectSelectedPieceLegalActions(Actions);

                TArray<TSharedPtr<FJsonValue>> MoveOptions;
                for (const FTerraGameplayContainer::FLegalActionQuery& Action : Actions)
                {
                    if (Action.PieceId != ReviewResult.InteractionState.SelectedPieceId)
                    {
                        continue;
                    }

                    TSharedRef<FJsonObject> MoveObject = MakeActionObject_(Action);
                    FTerraGameplayContainer::FActionRiskQuery Risk;
                    GameplayContainer->EvaluateCurrentFactionActionRisk(Action.PieceId, Action.ToCellId, Risk);
                    MoveObject->SetBoolField(TEXT("destination_threatened"), Risk.bDestinationThreatened);
                    MoveObject->SetNumberField(TEXT("threat_count"), Risk.ThreatCount);
                    BuildLocalInfoFields_(*GameplayContainer, GameplayContainer->GetCurrentFactionId(), Action.ToCellId, MoveObject);
                    AddNearestEnemyDistanceFields_(*GameplayContainer, GameplayContainer->GetCurrentFactionId(), Action.FromCellId, Action.ToCellId, MoveObject);
                    MoveOptions.Add(MakeShared<FJsonValueObject>(MoveObject));
                }
                Result->SetArrayField(TEXT("move_options"), MoveOptions);
            }

            FNpcMcpModule::RefreshInteractiveToolAvailability();
            return MakeStructuredResult_(Result);
        }
    };

    class FUiPreviewMoveTool final : public IModelContextProtocolTool
    {
    public:
        virtual FString GetName() const override { return TEXT("terra.ui_preview_move"); }
        virtual FString GetDescription() const override { return TEXT("Clicks a target cell for the currently selected piece and returns the resulting interaction state. Available in PieceSelected or PieceJumpingCanContinue phase."); }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override { return MakeActionInputSchema_(); }

        virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
        {
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            int32 PieceId = INDEX_NONE;
            int32 ToCellId = INDEX_NONE;
            if (!Params || !Params->TryGetNumberField(TEXT("piece_id"), PieceId) || !Params->TryGetNumberField(TEXT("to_cell_id"), ToCellId))
            {
                Result->SetBoolField(TEXT("ok"), false);
                Result->SetStringField(TEXT("error"), TEXT("invalid_params"));
                return MakeStructuredResult_(Result);
            }

            FTerraGameplayContainer::FLegalActionQuery CachedLegalAction;
            bool bHasCachedLegalAction = false;
            FTerraGameplayContainer::FActionRiskQuery CachedRisk;
            bool bHasCachedRisk = false;
            if (const FTerraGameplayContainer* GameplayContainerBeforePreview = FTerraNpcMcpGameplayBridge::GetGameplayContainer();
                GameplayContainerBeforePreview && GameplayContainerBeforePreview->IsInitialized())
            {
                bHasCachedLegalAction = GameplayContainerBeforePreview->GetSelectedPieceLegalAction(ToCellId, CachedLegalAction);
                bHasCachedRisk = GameplayContainerBeforePreview->EvaluateCurrentFactionActionRisk(PieceId, ToCellId, CachedRisk);
            }

            FTerraNpcMcpGameplayBridge::FUiReviewResult ReviewResult;
            const bool bOk = FTerraNpcMcpGameplayBridge::TryUiPreviewMove(PieceId, ToCellId, ReviewResult);
            Result->SetBoolField(TEXT("ok"), bOk && ReviewResult.bOk);
            if (!ReviewResult.Error.IsEmpty())
            {
                Result->SetStringField(TEXT("error"), ReviewResult.Error);
            }
            Result->SetObjectField(TEXT("interaction_state"), MakeInteractionStateObject_(ReviewResult.InteractionState));

            if (const FTerraGameplayContainer* GameplayContainer = FTerraNpcMcpGameplayBridge::GetGameplayContainer();
                GameplayContainer && GameplayContainer->IsInitialized())
            {
                if (bHasCachedLegalAction)
                {
                    TSharedRef<FJsonObject> PreviewObject = MakeActionObject_(CachedLegalAction);
                    PreviewObject->SetBoolField(TEXT("destination_threatened"), bHasCachedRisk ? CachedRisk.bDestinationThreatened : false);
                    PreviewObject->SetNumberField(TEXT("threat_count"), bHasCachedRisk ? CachedRisk.ThreatCount : 0);
                    BuildLocalInfoFields_(*GameplayContainer, GameplayContainer->GetCurrentFactionId(), ToCellId, PreviewObject);
                    AddNearestEnemyDistanceFields_(*GameplayContainer, GameplayContainer->GetCurrentFactionId(), CachedLegalAction.FromCellId, CachedLegalAction.ToCellId, PreviewObject);
                    Result->SetObjectField(TEXT("preview"), PreviewObject);
                    Result->SetBoolField(TEXT("can_confirm_now"), ReviewResult.InteractionState.bCanConfirmNow);
                    Result->SetBoolField(TEXT("can_continue_jump"), ReviewResult.InteractionState.ContinueJumpTargetCellIds.Num() > 0);
                    Result->SetBoolField(TEXT("destination_threatened"), bHasCachedRisk ? CachedRisk.bDestinationThreatened : false);
                    Result->SetNumberField(TEXT("threat_count"), bHasCachedRisk ? CachedRisk.ThreatCount : 0);
                }
            }

            FNpcMcpModule::RefreshInteractiveToolAvailability();
            return MakeStructuredResult_(Result);
        }
    };

    class FUiCancelSelectionTool final : public IModelContextProtocolTool
    {
    public:
        virtual FString GetName() const override { return TEXT("terra.ui_cancel_selection"); }
        virtual FString GetDescription() const override { return TEXT("Cancels the current interaction using the existing undo/cancel logic."); }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override { return MakeObjectSchema_(); }

        virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
        {
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            FTerraNpcMcpGameplayBridge::FUiReviewResult ReviewResult;
            const bool bOk = FTerraNpcMcpGameplayBridge::TryUiCancelSelection(ReviewResult);
            Result->SetBoolField(TEXT("ok"), bOk && ReviewResult.bOk);
            if (!ReviewResult.Error.IsEmpty())
            {
                Result->SetStringField(TEXT("error"), ReviewResult.Error);
            }
            Result->SetObjectField(TEXT("interaction_state"), MakeInteractionStateObject_(ReviewResult.InteractionState));
            FNpcMcpModule::RefreshInteractiveToolAvailability();
            return MakeStructuredResult_(Result);
        }
    };

    class FUiConfirmActionTool final : public IModelContextProtocolTool
    {
    public:
        virtual FString GetName() const override { return TEXT("terra.ui_confirm_action"); }
        virtual FString GetDescription() const override { return TEXT("Confirms the current in-progress action using the existing gameplay interaction state."); }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override { return MakeObjectSchema_(); }

        virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
        {
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            FTerraNpcMcpGameplayBridge::FUiReviewResult ReviewResult;
            FTerraGameplayContainer::FValidatedActionExecutionResult ExecutionResult;
            const bool bOk = FTerraNpcMcpGameplayBridge::TryUiConfirmAction(ReviewResult, ExecutionResult);
            Result->SetBoolField(TEXT("ok"), bOk && ReviewResult.bOk);
            if (!ReviewResult.Error.IsEmpty())
            {
                Result->SetStringField(TEXT("error"), ReviewResult.Error);
            }
            Result->SetObjectField(TEXT("interaction_state"), MakeInteractionStateObject_(ReviewResult.InteractionState));
            Result->SetObjectField(TEXT("execution"), MakeExecutionResultObject_(ExecutionResult));
            Result->SetBoolField(TEXT("accepted"), ExecutionResult.bAccepted);
            Result->SetBoolField(TEXT("executed"), ExecutionResult.bExecuted);
            Result->SetStringField(TEXT("reject_reason"), ExecutionResult.RejectReason);
            FNpcMcpModule::RefreshInteractiveToolAvailability();
            return MakeStructuredResult_(Result);
        }
    };
}

TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpGetTurnContextTool()
{
    return MakeShared<FGetTurnContextTool>();
}

TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpListLegalActionsTool()
{
    return MakeShared<FListLegalActionsTool>();
}

TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpEvaluateActionRiskTool()
{
    return MakeShared<FEvaluateActionRiskTool>();
}

TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpSubmitActionProposalTool()
{
    return MakeShared<FSubmitActionProposalTool>();
}

TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpExecuteValidatedActionTool()
{
    return MakeShared<FExecuteValidatedActionTool>();
}

TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpUiBeginTurnReviewTool()
{
    return MakeShared<FUiBeginTurnReviewTool>();
}

TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpUiSelectPieceTool()
{
    return MakeShared<FUiSelectPieceTool>();
}

TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpUiPreviewMoveTool()
{
    return MakeShared<FUiPreviewMoveTool>();
}

TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpUiCancelSelectionTool()
{
    return MakeShared<FUiCancelSelectionTool>();
}

TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpUiConfirmActionTool()
{
    return MakeShared<FUiConfirmActionTool>();
}
