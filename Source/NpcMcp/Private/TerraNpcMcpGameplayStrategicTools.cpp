#include "IModelContextProtocolTool.h"
#include "ModelContextProtocolToolResults.h"
#include "TerraGameplayContainer.h"
#include "TerraNpcMcpGameplayBridge.h"

namespace
{
    TSharedPtr<FJsonObject> MakeObjectSchema_()
    {
        TSharedRef<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));
        Schema->SetObjectField(TEXT("properties"), MakeShared<FJsonObject>());
        Schema->SetBoolField(TEXT("additionalProperties"), false);
        return Schema;
    }

    FModelContextProtocolToolResult MakeResult_(const TSharedRef<FJsonObject>& Object)
    {
        TSharedPtr<FJsonValue> StructuredValue = MakeShared<FJsonValueObject>(Object);
        return UE::ModelContextProtocol::MakeStructuredContentResult(StructuredValue);
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

    FString TerrainTag_(ETerraGameplayTerrainType TerrainType)
    {
        switch (TerrainType)
        {
        case ETerraGameplayTerrainType::Forest: return TEXT("forest");
        case ETerraGameplayTerrainType::Mountain: return TEXT("mountain");
        case ETerraGameplayTerrainType::Plain:
        default: return TEXT("plain");
        }
    }

    void AddSnapshot_(const FTerraGameplayContainer::FStrategicSnapshot& Snapshot, const TSharedRef<FJsonObject>& Result)
    {
        TSharedRef<FJsonObject> SnapshotObject = MakeShared<FJsonObject>();
        SnapshotObject->SetNumberField(TEXT("turn_index"), Snapshot.TurnIndex);
        SnapshotObject->SetNumberField(TEXT("current_faction_id"), Snapshot.CurrentFactionId);
        SnapshotObject->SetStringField(TEXT("interaction_phase"), StaticEnum<ETerraGameplayInteractionPhase>()->GetNameStringByValue(static_cast<int64>(Snapshot.InteractionPhase)));
        Result->SetObjectField(TEXT("snapshot"), SnapshotObject);
    }

    bool TryBuildSnapshot_(TSharedRef<FJsonObject>& OutResult, FTerraGameplayContainer::FCurrentFactionStrategicSnapshot& OutSnapshot)
    {
        const FTerraGameplayContainer* GameplayContainer = FTerraNpcMcpGameplayBridge::GetGameplayContainer();
        if (!GameplayContainer || !GameplayContainer->IsInitialized())
        {
            OutResult->SetBoolField(TEXT("ok"), false);
            OutResult->SetStringField(TEXT("error"), TEXT("gameplay_unavailable"));
            return false;
        }
        if (GameplayContainer->IsMatchEnded())
        {
            OutResult->SetBoolField(TEXT("ok"), false);
            OutResult->SetStringField(TEXT("error"), TEXT("match_ended"));
            return false;
        }
        if (!GameplayContainer->BuildCurrentFactionStrategicSnapshot(OutSnapshot))
        {
            OutResult->SetBoolField(TEXT("ok"), false);
            OutResult->SetStringField(TEXT("error"), TEXT("strategic_snapshot_unavailable"));
            return false;
        }
        OutResult->SetBoolField(TEXT("ok"), true);
        AddSnapshot_(OutSnapshot.Snapshot, OutResult);
        return true;
    }

    void AddFactionSummary_(const FTerraGameplayContainer::FFactionStrategicSummary& Summary, const TSharedRef<FJsonObject>& Result)
    {
        TSharedRef<FJsonObject> Material = MakeShared<FJsonObject>();
        Material->SetNumberField(TEXT("total_alive_piece_count"), Summary.TotalAlivePieceCount);
        Material->SetNumberField(TEXT("commander_count"), Summary.CommanderCount);
        Material->SetNumberField(TEXT("archer_count"), Summary.ArcherCount);
        Material->SetNumberField(TEXT("cavalry_count"), Summary.CavalryCount);
        Material->SetNumberField(TEXT("infantry_count"), Summary.InfantryCount);
        Material->SetNumberField(TEXT("movable_piece_count"), Summary.MovablePieceCount);
        Result->SetObjectField(TEXT("material"), Material);

        TSharedRef<FJsonObject> Proximity = MakeShared<FJsonObject>();
        if (Summary.NearestEnemyDistance != INDEX_NONE)
        {
            Proximity->SetNumberField(TEXT("nearest_enemy_distance"), Summary.NearestEnemyDistance);
        }
        else
        {
            Proximity->SetField(TEXT("nearest_enemy_distance"), MakeShared<FJsonValueNull>());
        }
        Proximity->SetNumberField(TEXT("frontline_piece_count"), Summary.FrontlinePieceIds.Num());
        Proximity->SetNumberField(TEXT("isolated_movable_piece_count"), Summary.IsolatedMovablePieceIds.Num());
        Result->SetObjectField(TEXT("proximity"), Proximity);
        Result->SetArrayField(TEXT("frontline_piece_ids"), MakeNumberArray_(Summary.FrontlinePieceIds));
        Result->SetArrayField(TEXT("isolated_movable_piece_ids"), MakeNumberArray_(Summary.IsolatedMovablePieceIds));
    }

    TArray<TSharedPtr<FJsonValue>> MakeFrontlineContacts_(const TArray<FTerraGameplayContainer::FFrontlineContact>& Contacts)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        for (const FTerraGameplayContainer::FFrontlineContact& Contact : Contacts)
        {
            TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
            Object->SetNumberField(TEXT("friendly_piece_id"), Contact.FriendlyPieceId);
            Object->SetNumberField(TEXT("friendly_cell_id"), Contact.FriendlyCellId);
            Object->SetNumberField(TEXT("enemy_piece_id"), Contact.EnemyPieceId);
            Object->SetNumberField(TEXT("enemy_faction_id"), Contact.EnemyFactionId);
            Object->SetNumberField(TEXT("enemy_cell_id"), Contact.EnemyCellId);
            Object->SetNumberField(TEXT("distance"), Contact.Distance);
            Object->SetStringField(TEXT("friendly_terrain_tag"), TerrainTag_(Contact.FriendlyTerrainType));
            Object->SetNumberField(TEXT("friendly_adjacent_support_count"), Contact.FriendlyAdjacentSupportCount);
            Result.Add(MakeShared<FJsonValueObject>(Object));
        }
        return Result;
    }

    TArray<TSharedPtr<FJsonValue>> MakeTerrainPoints_(const TArray<FTerraGameplayContainer::FTerrainControlPoint>& Points)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        for (const FTerraGameplayContainer::FTerrainControlPoint& Point : Points)
        {
            TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
            Object->SetNumberField(TEXT("cell_id"), Point.CellId);
            Object->SetStringField(TEXT("terrain_tag"), TerrainTag_(Point.TerrainType));
            if (Point.NearestFriendlyDistance != INDEX_NONE) Object->SetNumberField(TEXT("nearest_friendly_distance"), Point.NearestFriendlyDistance); else Object->SetField(TEXT("nearest_friendly_distance"), MakeShared<FJsonValueNull>());
            if (Point.NearestEnemyDistance != INDEX_NONE) Object->SetNumberField(TEXT("nearest_enemy_distance"), Point.NearestEnemyDistance); else Object->SetField(TEXT("nearest_enemy_distance"), MakeShared<FJsonValueNull>());
            Object->SetStringField(TEXT("control_status"), Point.ControlStatus);
            Object->SetArrayField(TEXT("nearby_friendly_piece_ids"), MakeNumberArray_(Point.NearbyFriendlyPieceIds));
            Object->SetArrayField(TEXT("nearby_enemy_piece_ids"), MakeNumberArray_(Point.NearbyEnemyPieceIds));
            Result.Add(MakeShared<FJsonValueObject>(Object));
        }
        return Result;
    }

    TArray<TSharedPtr<FJsonValue>> MakeEnemyPressures_(const TArray<FTerraGameplayContainer::FEnemyPressureSummary>& Pressures)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        for (const FTerraGameplayContainer::FEnemyPressureSummary& Pressure : Pressures)
        {
            TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
            Object->SetNumberField(TEXT("enemy_faction_id"), Pressure.EnemyFactionId);
            if (Pressure.NearestContactDistance != INDEX_NONE) Object->SetNumberField(TEXT("nearest_contact_distance"), Pressure.NearestContactDistance); else Object->SetField(TEXT("nearest_contact_distance"), MakeShared<FJsonValueNull>());
            Object->SetNumberField(TEXT("frontline_contact_count"), Pressure.FrontlineContactCount);
            Object->SetNumberField(TEXT("pressure_priority"), Pressure.PressurePriority);
            Object->SetArrayField(TEXT("representative_enemy_piece_ids"), MakeNumberArray_(Pressure.RepresentativeEnemyPieceIds));
            Object->SetArrayField(TEXT("representative_friendly_piece_ids"), MakeNumberArray_(Pressure.RepresentativeFriendlyPieceIds));
            Result.Add(MakeShared<FJsonValueObject>(Object));
        }
        return Result;
    }

    TArray<TSharedPtr<FJsonValue>> MakeStrategicOptions_(const TArray<FTerraGameplayContainer::FStrategicOption>& Options)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        for (const FTerraGameplayContainer::FStrategicOption& Option : Options)
        {
            TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
            Object->SetStringField(TEXT("intent"), Option.Intent);
            Object->SetStringField(TEXT("evidence_scope"), Option.EvidenceScope);
            Object->SetArrayField(TEXT("evidence_tags"), MakeStringArray_(Option.EvidenceTags));
            Object->SetArrayField(TEXT("key_piece_ids"), MakeNumberArray_(Option.KeyPieceIds));
            Object->SetArrayField(TEXT("key_cell_ids"), MakeNumberArray_(Option.KeyCellIds));
            Object->SetArrayField(TEXT("route_cell_ids"), MakeNumberArray_(Option.RouteCellIds));
            Object->SetArrayField(TEXT("terrain_tags"), MakeStringArray_(Option.TerrainTags));
            if (Option.TargetEnemyFactionId != INDEX_NONE) Object->SetNumberField(TEXT("target_enemy_faction_id"), Option.TargetEnemyFactionId); else Object->SetField(TEXT("target_enemy_faction_id"), MakeShared<FJsonValueNull>());
            Object->SetArrayField(TEXT("validation_questions"), MakeStringArray_(Option.ValidationQuestions));
            Result.Add(MakeShared<FJsonValueObject>(Object));
        }
        return Result;
    }

    class FStrategicFactionStateTool final : public IModelContextProtocolTool
    {
    public:
        virtual FString GetName() const override { return TEXT("terra.strategy.summarize_faction_state"); }
        virtual FString GetDescription() const override { return TEXT("Returns an idempotent current-faction material and proximity summary for strategic planning."); }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override { return MakeObjectSchema_(); }
        virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
        {
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            FTerraGameplayContainer::FCurrentFactionStrategicSnapshot Snapshot;
            if (TryBuildSnapshot_(Result, Snapshot)) AddFactionSummary_(Snapshot.FactionSummary, Result);
            return MakeResult_(Result);
        }
    };

    class FStrategicFrontlineTool final : public IModelContextProtocolTool
    {
    public:
        virtual FString GetName() const override { return TEXT("terra.strategy.describe_frontline"); }
        virtual FString GetDescription() const override { return TEXT("Returns nearest deterministic friendly-enemy frontline contacts for the current faction."); }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override { return MakeObjectSchema_(); }
        virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
        {
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            FTerraGameplayContainer::FCurrentFactionStrategicSnapshot Snapshot;
            if (TryBuildSnapshot_(Result, Snapshot)) Result->SetArrayField(TEXT("frontline_contacts"), MakeFrontlineContacts_(Snapshot.FrontlineContacts));
            return MakeResult_(Result);
        }
    };

    class FStrategicTerrainTool final : public IModelContextProtocolTool
    {
    public:
        virtual FString GetName() const override { return TEXT("terra.strategy.find_terrain_control_points"); }
        virtual FString GetDescription() const override { return TEXT("Returns nearby-relevant forest and mountain control points with deterministic distance-based control status."); }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override { return MakeObjectSchema_(); }
        virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
        {
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            FTerraGameplayContainer::FCurrentFactionStrategicSnapshot Snapshot;
            if (TryBuildSnapshot_(Result, Snapshot)) Result->SetArrayField(TEXT("terrain_control_points"), MakeTerrainPoints_(Snapshot.TerrainControlPoints));
            return MakeResult_(Result);
        }
    };

    class FStrategicEnemyPressureTool final : public IModelContextProtocolTool
    {
    public:
        virtual FString GetName() const override { return TEXT("terra.strategy.find_enemy_pressure"); }
        virtual FString GetDescription() const override { return TEXT("Returns idempotent enemy-faction pressure summaries ordered by transparent proximity priority."); }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override { return MakeObjectSchema_(); }
        virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
        {
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            FTerraGameplayContainer::FCurrentFactionStrategicSnapshot Snapshot;
            if (TryBuildSnapshot_(Result, Snapshot)) Result->SetArrayField(TEXT("enemy_pressures"), MakeEnemyPressures_(Snapshot.EnemyPressures));
            return MakeResult_(Result);
        }
    };

    class FStrategicOptionsTool final : public IModelContextProtocolTool
    {
    public:
        virtual FString GetName() const override { return TEXT("terra.strategy.describe_strategic_options"); }
        virtual FString GetDescription() const override { return TEXT("Returns transparent current-turn strategic intents backed by faction, frontline, terrain, and pressure evidence."); }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override { return MakeObjectSchema_(); }
        virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
        {
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            FTerraGameplayContainer::FCurrentFactionStrategicSnapshot Snapshot;
            if (TryBuildSnapshot_(Result, Snapshot)) Result->SetArrayField(TEXT("strategic_options"), MakeStrategicOptions_(Snapshot.StrategicOptions));
            return MakeResult_(Result);
        }
    };
}

TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpStrategicFactionStateTool() { return MakeShared<FStrategicFactionStateTool>(); }
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpStrategicFrontlineTool() { return MakeShared<FStrategicFrontlineTool>(); }
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpStrategicTerrainTool() { return MakeShared<FStrategicTerrainTool>(); }
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpStrategicEnemyPressureTool() { return MakeShared<FStrategicEnemyPressureTool>(); }
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpStrategicOptionsTool() { return MakeShared<FStrategicOptionsTool>(); }
