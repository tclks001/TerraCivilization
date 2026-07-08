#include "IModelContextProtocolTool.h"
#include "ModelContextProtocolToolResults.h"
#include "TerraGameplayContainer.h"
#include "TerraNpcMcpGameplayBridge.h"

class FTerraNpcMcpPingGameplayTool final : public IModelContextProtocolTool
{
public:
    virtual FString GetName() const override
    {
        return TEXT("terra.ping_gameplay");
    }

    virtual FString GetDescription() const override
    {
        return TEXT("Returns a minimal read-only snapshot proving the Terra NPC MCP runtime tool can see Gameplay state.");
    }

    virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override
    {
        TSharedRef<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));
        Schema->SetObjectField(TEXT("properties"), MakeShared<FJsonObject>());
        Schema->SetBoolField(TEXT("additionalProperties"), false);
        return Schema;
    }

    virtual TSharedPtr<FJsonObject> GetOutputJsonSchema() const override
    {
        TSharedRef<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));

        TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
        AddTypedProperty_(Properties, TEXT("ok"), TEXT("boolean"));
        AddTypedProperty_(Properties, TEXT("gameplay_registered"), TEXT("boolean"));
        AddTypedProperty_(Properties, TEXT("gameplay_initialized"), TEXT("boolean"));
        AddTypedProperty_(Properties, TEXT("turn_index"), TEXT("integer"));
        AddTypedProperty_(Properties, TEXT("current_faction_id"), TEXT("integer"));
        AddTypedProperty_(Properties, TEXT("piece_count"), TEXT("integer"));
        AddTypedProperty_(Properties, TEXT("alive_piece_count"), TEXT("integer"));
        AddTypedProperty_(Properties, TEXT("faction_count"), TEXT("integer"));
        AddTypedProperty_(Properties, TEXT("alive_faction_count"), TEXT("integer"));
        AddTypedProperty_(Properties, TEXT("match_ended"), TEXT("boolean"));
        AddTypedProperty_(Properties, TEXT("winning_faction_id"), TEXT("integer"));
        Schema->SetObjectField(TEXT("properties"), Properties);
        Schema->SetBoolField(TEXT("additionalProperties"), false);
        return Schema;
    }

    virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
    {
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("ok"), true);

        const FTerraGameplayContainer* GameplayContainer = FTerraNpcMcpGameplayBridge::GetGameplayContainer();
        Result->SetBoolField(TEXT("gameplay_registered"), GameplayContainer != nullptr);

        if (!GameplayContainer)
        {
            Result->SetBoolField(TEXT("gameplay_initialized"), false);
            Result->SetNumberField(TEXT("turn_index"), INDEX_NONE);
            Result->SetNumberField(TEXT("current_faction_id"), INDEX_NONE);
            Result->SetNumberField(TEXT("piece_count"), 0);
            Result->SetNumberField(TEXT("alive_piece_count"), 0);
            Result->SetNumberField(TEXT("faction_count"), 0);
            Result->SetNumberField(TEXT("alive_faction_count"), 0);
            Result->SetBoolField(TEXT("match_ended"), false);
            Result->SetNumberField(TEXT("winning_faction_id"), INDEX_NONE);
            TSharedPtr<FJsonValue> StructuredContent = MakeShared<FJsonValueObject>(Result);
            return UE::ModelContextProtocol::MakeStructuredContentResult(StructuredContent);
        }

        int32 AlivePieceCount = 0;
        for (const FTerraGameplayPieceState& Piece : GameplayContainer->GetPieces())
        {
            if (Piece.bAlive)
            {
                ++AlivePieceCount;
            }
        }

        int32 AliveFactionCount = 0;
        for (const FTerraGameplayFactionState& Faction : GameplayContainer->GetFactions())
        {
            if (Faction.bAlive)
            {
                ++AliveFactionCount;
            }
        }

        Result->SetBoolField(TEXT("gameplay_initialized"), GameplayContainer->IsInitialized());
        Result->SetNumberField(TEXT("turn_index"), GameplayContainer->GetTurnIndex());
        Result->SetNumberField(TEXT("current_faction_id"), GameplayContainer->GetCurrentFactionId());
        Result->SetNumberField(TEXT("piece_count"), GameplayContainer->GetPieces().Num());
        Result->SetNumberField(TEXT("alive_piece_count"), AlivePieceCount);
        Result->SetNumberField(TEXT("faction_count"), GameplayContainer->GetFactions().Num());
        Result->SetNumberField(TEXT("alive_faction_count"), AliveFactionCount);
        Result->SetBoolField(TEXT("match_ended"), GameplayContainer->IsMatchEnded());
        Result->SetNumberField(TEXT("winning_faction_id"), GameplayContainer->GetWinningFactionId());

        TSharedPtr<FJsonValue> StructuredContent = MakeShared<FJsonValueObject>(Result);
        return UE::ModelContextProtocol::MakeStructuredContentResult(StructuredContent);
    }

private:
    static void AddTypedProperty_(const TSharedRef<FJsonObject>& Properties, const TCHAR* Name, const TCHAR* Type)
    {
        TSharedRef<FJsonObject> Property = MakeShared<FJsonObject>();
        Property->SetStringField(TEXT("type"), Type);
        Properties->SetObjectField(Name, Property);
    }
};

TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpPingGameplayTool()
{
    return MakeShared<FTerraNpcMcpPingGameplayTool>();
}
