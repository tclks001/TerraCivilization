# UE 5.8 NPC MCP Runtime 阶段 2 设计稿

日期：2026-07-08

## 目标

阶段 2 在阶段 1 的 Runtime MCP 通路上增加确定性查询工具，让外部 LLM agent 可以按固定流程做一个“只读决策草案”：

1. 查询当前回合上下文。
2. 查询当前阵营有哪些合法行动。
3. 对候选行动做风险评估。
4. 提交一个行动 proposal，由 Gameplay 做合法性校验并回显。

本阶段仍然不执行 NPC 行动。`terra.submit_action_proposal` 只做 validate-only，不修改 Gameplay 状态。

## 核心结论

阶段 2 的实现原则是：

```text
LLM 不直接推导规则
  -> MCP tool 调 Gameplay public deterministic query API
  -> Gameplay 复用现有私有规则 helper
  -> MCP tool 输出结构化 JSON
  -> proposal 只校验，不执行
```

这样可以先验证 LLM/MCP 的思考流程和工具编排，而不把状态写入、动画表现、回合推进和失败回滚混在一起。

## 新增 MCP 工具

阶段 2 在 `NpcMcp` 模块中新增四个工具：

- `terra.get_turn_context`
- `terra.list_legal_actions`
- `terra.evaluate_action_risk`
- `terra.submit_action_proposal`

工具注册仍然走阶段 1 的白名单注册：

```cpp
RegisteredTools.Add(MakeTerraNpcMcpGetTurnContextTool());
RegisteredTools.Add(MakeTerraNpcMcpListLegalActionsTool());
RegisteredTools.Add(MakeTerraNpcMcpEvaluateActionRiskTool());
RegisteredTools.Add(MakeTerraNpcMcpSubmitActionProposalTool());
```

文件：

- `Source/NpcMcp/Private/TerraNpcMcpGameplayQueryTools.cpp`

## Gameplay Public Query API

阶段 2 在 `FTerraGameplayContainer` 增加只读查询结构和 API。

文件：

- `Source/Gameplay/Public/TerraGameplayContainer.h`
- `Source/Gameplay/Private/TerraGameplayContainer.cpp`

### `FLegalActionQuery`

```cpp
struct FLegalActionQuery
{
    int32 PieceId = INDEX_NONE;
    int32 FromCellId = INDEX_NONE;
    int32 ToCellId = INDEX_NONE;
    bool bIsJump = false;
    TArray<FTerraGameplayCaptureEntry> CaptureEntries;
};
```

表示当前阵营一个合法的单段行动候选：

- 普通移动：`bIsJump=false`
- 跳跃移动：`bIsJump=true`
- `CaptureEntries` 是该行动落点产生的确定性吃子预览

注意：阶段 2 的合法行动是“单次目标格”粒度。连续跳跃的完整多步路径还没有在本阶段展开。

### `FActionRiskQuery`

```cpp
struct FActionRiskQuery
{
    bool bValidAction = false;
    bool bDestinationThreatened = false;
    int32 ThreatCount = 0;
    TArray<int32> ThreateningPieceIds;
    TArray<int32> ThreateningFactionIds;
};
```

表示一个候选行动的基础风险：

- `bValidAction`：是否是当前阵营当前状态下的合法行动。
- `bDestinationThreatened`：目标格是否可被敌方棋子威胁。
- `ThreateningPieceIds`：威胁该目标格的敌方棋子。
- `ThreateningFactionIds`：威胁来源阵营。

阶段 2 的风险评估是保守的启发式查询，用于 LLM 排序候选行动，不是最终战斗模拟器。

### Public API

```cpp
bool CollectCurrentFactionLegalActions(TArray<FLegalActionQuery>& OutActions) const;
bool EvaluateCurrentFactionActionRisk(int32 PieceId, int32 ToCellId, FActionRiskQuery& OutRisk) const;
bool IsCurrentFactionLegalAction(int32 PieceId, int32 ToCellId, FLegalActionQuery& OutAction) const;
```

约束：

- 只读，不修改 `FTerraGameplayContainer` 状态。
- 只在 `InteractionPhase == Idle` 时列出行动，避免和玩家/UI 点击状态机交错。
- 只查询当前阵营行动，不允许 MCP tool 指定任意阵营。
- proposal 校验也只接受当前阵营合法行动。

## 私有规则 Helper 参数化

为了让风险评估能计算“敌方是否威胁某格”，阶段 2 对部分私有规则 helper 做了带阵营参数的包装：

```cpp
bool IsPieceSelectableForFaction_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId) const;
bool CanOrdinaryMoveForFaction_(const FTerraGameplayPieceState& Piece, int32 TargetCellId, int32 ActingFactionId) const;
void CollectOrdinaryMoveTargetsForFaction_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId, TSet<int32>& OutTargetCellIds) const;
void CollectJumpTargetsForFaction_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId, int32 BlockedReturnCellId, TSet<int32>& OutTargetCellIds) const;
void CollectCaptureEntriesAfterHypotheticalMoveForFaction_(const FTerraGameplayPieceState& Piece, int32 TargetCellId, int32 ActingFactionId, TMap<int32, FTerraGameplayCaptureEntry>& OutCaptureEntriesByCellId) const;
```

现有当前阵营 helper 保留，改为薄包装：

```cpp
IsPieceSelectable_(Piece)
CanOrdinaryMove_(Piece, TargetCellId)
CollectOrdinaryMoveTargets_(Piece, OutTargetCellIds)
CollectJumpTargets_(Piece, OutTargetCellIds)
CollectCaptureEntriesAfterHypotheticalMove_(Piece, TargetCellId, OutCaptureEntriesByCellId)
```

这样现有 UI 点击行为不变，同时 MCP 查询可以复用相同规则。

## Tool 设计

### `terra.get_turn_context`

输入：

```json
{}
```

输出：

```json
{
  "ok": true,
  "turn_index": 0,
  "current_faction_id": 0,
  "selected_piece_id": -1,
  "interaction_phase": 0,
  "match_ended": false,
  "winning_faction_id": -1,
  "cell_count": 642,
  "piece_count": 48,
  "faction_count": 12,
  "current_faction_piece_ids": [1, 2, 3, 4]
}
```

用途：

- LLM 决策流程第一步。
- 确认当前回合、当前阵营和可控棋子集合。

### `terra.list_legal_actions`

输入：

```json
{}
```

输出：

```json
{
  "ok": true,
  "turn_index": 0,
  "current_faction_id": 0,
  "action_count": 3,
  "actions": [
    {
      "piece_id": 1,
      "from_cell_id": 10,
      "to_cell_id": 11,
      "is_jump": false,
      "capture_count": 0,
      "captures": []
    }
  ]
}
```

用途：

- LLM 不自己从棋盘状态推导合法走法。
- 工具输出是 Gameplay 规则计算结果。

### `terra.evaluate_action_risk`

输入：

```json
{
  "piece_id": 1,
  "to_cell_id": 11
}
```

输出：

```json
{
  "ok": true,
  "piece_id": 1,
  "to_cell_id": 11,
  "valid_action": true,
  "destination_threatened": false,
  "threat_count": 0,
  "threatening_piece_ids": [],
  "threatening_faction_ids": []
}
```

用途：

- LLM 对候选行动做基础风险排序。
- 如果 `valid_action=false`，LLM 应该丢弃该候选。

### `terra.submit_action_proposal`

输入：

```json
{
  "piece_id": 1,
  "to_cell_id": 11
}
```

输出：

```json
{
  "ok": true,
  "accepted": true,
  "executed": false,
  "phase": "phase2_validate_only",
  "turn_index": 0,
  "current_faction_id": 0,
  "piece_id": 1,
  "to_cell_id": 11,
  "legal_action": {
    "piece_id": 1,
    "from_cell_id": 10,
    "to_cell_id": 11,
    "is_jump": false,
    "capture_count": 0,
    "captures": []
  }
}
```

如果 proposal 非法：

```json
{
  "ok": true,
  "accepted": false,
  "executed": false,
  "phase": "phase2_validate_only",
  "reject_reason": "not_a_current_faction_legal_action"
}
```

用途：

- 验证 LLM 最终输出能被 Gameplay 识别为合法行动。
- 阶段 2 不执行，避免状态写入和回合推进复杂度。

## LLM 推荐调用流程

```text
1. terra.get_turn_context
2. terra.list_legal_actions
3. 对若干候选调用 terra.evaluate_action_risk
4. 选择一个候选
5. terra.submit_action_proposal
6. 如果 accepted=false，回到第 2 步重新选择
```

阶段 2 不需要长期对话上下文。每次回合决策可以使用一次短会话，长期阵营记忆后续通过独立工具查询。

## 验收

启动参数沿用阶段 1：

```text
-TerraNpcMcpStartServer -TerraNpcMcpPort=8765 -TerraNpcMcpPath=/terra-npc-mcp
```

MCP Inspector 连接：

```text
Transport Type: Streamable HTTP
URL: http://127.0.0.1:8765/terra-npc-mcp
```

验收项：

1. `tools/list` 中包含五个工具：
   - `terra.ping_gameplay`
   - `terra.get_turn_context`
   - `terra.list_legal_actions`
   - `terra.evaluate_action_risk`
   - `terra.submit_action_proposal`
2. `terra.get_turn_context` 返回当前回合和当前阵营。
3. `terra.list_legal_actions` 在 `InteractionPhase == Idle` 时返回合法行动数组。
4. 从 `actions[0]` 取 `piece_id` 和 `to_cell_id` 调用 `terra.evaluate_action_risk`，返回 `valid_action=true`。
5. 用同一组参数调用 `terra.submit_action_proposal`，返回 `accepted=true`、`executed=false`。
6. 用非法参数调用 `terra.submit_action_proposal`，返回 `accepted=false`。

## 已验证

编译命令：

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
  TerraCivilizationEditor Win64 Development `
  -Project="C:\workspace\TerraCivilization\TerraCivilization.uproject" `
  -WaitMutex -NoHotReload
```

结果：

```text
Result: Succeeded
```

如果 UE Editor 正在运行，后续编译可能因为 DLL 被占用出现链接错误。这属于开发环境状态，不代表 C++ 编译错误。

## 阶段 3 建议

阶段 3 再引入执行闭环：

1. 增加 Gameplay public API：`TryExecuteValidatedAction(PieceId, ToCellId, OutDirtyCellIds)`。
2. `terra.submit_action_proposal` 或新增 `terra.execute_validated_action` 才允许写状态。
3. 执行前再次校验 `TurnIndex`、`CurrentFactionId`、`PieceId`、`ToCellId`，避免 LLM 基于旧 snapshot 行动。
4. 执行后写 AI decision log：tool 调用摘要、proposal、validator 结果、fallback 标记。
5. 引入超时和 fallback deterministic AI。
