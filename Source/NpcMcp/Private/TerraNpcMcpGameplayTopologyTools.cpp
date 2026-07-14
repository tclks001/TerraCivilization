#include "IModelContextProtocolTool.h"
#include "ModelContextProtocolToolResults.h"
#include "TerraGameplayContainer.h"
#include "TerraNpcMcpGameplayBridge.h"

namespace
{
    FModelContextProtocolToolResult MakeResult_(const TSharedRef<FJsonObject>& Object)
    {
        TSharedPtr<FJsonValue> StructuredValue = MakeShared<FJsonValueObject>(Object);
        return UE::ModelContextProtocol::MakeStructuredContentResult(StructuredValue);
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

    FString PieceTypeTag_(ETerraGameplayPieceType PieceType)
    {
        switch (PieceType)
        {
        case ETerraGameplayPieceType::Commander: return TEXT("Commander");
        case ETerraGameplayPieceType::Archer: return TEXT("Archer");
        case ETerraGameplayPieceType::Cavalry: return TEXT("Cavalry");
        case ETerraGameplayPieceType::Infantry:
        default: return TEXT("Infantry");
        }
    }

    TSharedPtr<FJsonObject> MakeInputSchema_()
    {
        TSharedRef<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));
        TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
        TSharedRef<FJsonObject> CellIdProperty = MakeShared<FJsonObject>();
        CellIdProperty->SetStringField(TEXT("type"), TEXT("integer"));
        Properties->SetObjectField(TEXT("cell_id"), CellIdProperty);
        Schema->SetObjectField(TEXT("properties"), Properties);
        Schema->SetArrayField(TEXT("required"), { MakeShared<FJsonValueString>(TEXT("cell_id")) });
        Schema->SetBoolField(TEXT("additionalProperties"), false);
        return Schema;
    }

    void AddSnapshot_(const FTerraGameplayContainer::FStrategicSnapshot& Snapshot, const TSharedRef<FJsonObject>& Result)
    {
        TSharedRef<FJsonObject> SnapshotObject = MakeShared<FJsonObject>();
        SnapshotObject->SetNumberField(TEXT("turn_index"), Snapshot.TurnIndex);
        SnapshotObject->SetNumberField(TEXT("current_faction_id"), Snapshot.CurrentFactionId);
        SnapshotObject->SetStringField(TEXT("interaction_phase"), StaticEnum<ETerraGameplayInteractionPhase>()->GetNameStringByValue(static_cast<int64>(Snapshot.InteractionPhase)));
        Result->SetObjectField(TEXT("snapshot"), SnapshotObject);
    }

    TArray<TSharedPtr<FJsonValue>> MakeCells_(const TArray<FTerraGameplayContainer::FLocalTopologyCellInfo>& Cells)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        Result.Reserve(Cells.Num());
        for (const FTerraGameplayContainer::FLocalTopologyCellInfo& Cell : Cells)
        {
            TSharedRef<FJsonObject> CellObject = MakeShared<FJsonObject>();
            CellObject->SetNumberField(TEXT("cell_id"), Cell.CellId);
            CellObject->SetStringField(TEXT("terrain_tag"), TerrainTag_(Cell.TerrainType));
            if (Cell.OccupyingPieceId == INDEX_NONE)
            {
                CellObject->SetField(TEXT("piece"), MakeShared<FJsonValueNull>());
            }
            else
            {
                TSharedRef<FJsonObject> PieceObject = MakeShared<FJsonObject>();
                PieceObject->SetNumberField(TEXT("piece_id"), Cell.OccupyingPieceId);
                PieceObject->SetNumberField(TEXT("faction_id"), Cell.OccupyingFactionId);
                PieceObject->SetStringField(TEXT("piece_type"), PieceTypeTag_(Cell.OccupyingPieceType));
                PieceObject->SetBoolField(TEXT("can_move"), Cell.bOccupyingPieceCanMove);
                CellObject->SetObjectField(TEXT("piece"), PieceObject);
            }
            Result.Add(MakeShared<FJsonValueObject>(CellObject));
        }
        return Result;
    }

    TArray<TSharedPtr<FJsonValue>> MakeEdges_(const TArray<FTerraGameplayContainer::FLocalTopologyEdge>& Edges)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        Result.Reserve(Edges.Num());
        for (const FTerraGameplayContainer::FLocalTopologyEdge& Edge : Edges)
        {
            TSharedRef<FJsonObject> EdgeObject = MakeShared<FJsonObject>();
            EdgeObject->SetNumberField(TEXT("cell_a_id"), Edge.CellAId);
            EdgeObject->SetNumberField(TEXT("cell_b_id"), Edge.CellBId);
            Result.Add(MakeShared<FJsonValueObject>(EdgeObject));
        }
        return Result;
    }

    class FInspectLocalTopologyTool final : public IModelContextProtocolTool
    {
    public:
        virtual FString GetName() const override { return TEXT("terra.inspect_local_topology"); }
        virtual FString GetDescription() const override { return TEXT("Returns the read-only real Gameplay graph induced by all cells within fixed radius 3 of cell_id. Cells carry terrain and occupant state; undirected edges are the only topology relation."); }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override { return MakeInputSchema_(); }

        virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
        {
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            int32 CellId = INDEX_NONE;
            if (!Params || !Params->TryGetNumberField(TEXT("cell_id"), CellId))
            {
                Result->SetBoolField(TEXT("ok"), false);
                Result->SetStringField(TEXT("error"), TEXT("invalid_params"));
                return MakeResult_(Result);
            }

            const FTerraGameplayContainer* GameplayContainer = FTerraNpcMcpGameplayBridge::GetGameplayContainer();
            if (!GameplayContainer || !GameplayContainer->IsInitialized())
            {
                Result->SetBoolField(TEXT("ok"), false);
                Result->SetStringField(TEXT("error"), TEXT("gameplay_unavailable"));
                return MakeResult_(Result);
            }
            if (GameplayContainer->IsMatchEnded())
            {
                Result->SetBoolField(TEXT("ok"), false);
                Result->SetStringField(TEXT("error"), TEXT("match_ended"));
                return MakeResult_(Result);
            }

            FTerraGameplayContainer::FLocalTopologyObservation Observation;
            if (!GameplayContainer->BuildLocalTopologyObservation(CellId, Observation))
            {
                Result->SetBoolField(TEXT("ok"), false);
                Result->SetStringField(TEXT("error"), TEXT("invalid_cell_id"));
                return MakeResult_(Result);
            }

            Result->SetBoolField(TEXT("ok"), true);
            AddSnapshot_(Observation.Snapshot, Result);
            Result->SetNumberField(TEXT("center_cell_id"), Observation.CenterCellId);
            Result->SetNumberField(TEXT("radius"), Observation.Radius);
            Result->SetArrayField(TEXT("cells"), MakeCells_(Observation.Cells));
            Result->SetArrayField(TEXT("edges"), MakeEdges_(Observation.Edges));
            return MakeResult_(Result);
        }
    };
}

TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpInspectLocalTopologyTool()
{
    return MakeShared<FInspectLocalTopologyTool>();
}
