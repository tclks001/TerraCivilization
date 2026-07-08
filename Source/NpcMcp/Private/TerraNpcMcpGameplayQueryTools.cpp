#include "IModelContextProtocolTool.h"
#include "ModelContextProtocolToolResults.h"
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
