# L3.1 局部拓扑观察工具设计稿

日期：2026-07-13  
状态：已实现并完成独立 Runtime/LLM Agent 首轮验收。

## 目标

L3.1 在 [L3 战略语义工具](L3StrategicSemanticToolsDesign.md) 与 [L2 局部战术情报卡](L2LocalTacticalSituationCardDesign.md) 之间增加一个可主动调查的局部战场观察工具：

```text
terra.inspect_local_topology(cell_id)
```

它服务于“战略工具指出值得关注的位置后，LLM 像玩家一样查看该位置周围棋盘”的过程。工具返回真实 Gameplay cell graph 的局部诱导子图，而不是让 LLM 从 CellId 数字、世界坐标或浮点 Offset 推测球面拓扑。

## 边界与原则

```text
输入：
  cell_id：必填，任意有效 Gameplay CellId。

范围：
  固定 graph radius = 3；不提供 radius 参数，避免 Agent 通过大范围图查询消耗上下文。

输出：
  cells：半径内每个单 Cell 的纯状态。
  edges：两端都位于该半径内的全部无向真实邻接边。

不输出：
  neighbor_cell_ids、distance_from_center、offset、世界坐标、路径推断、行动合法性、风险评分。

状态：
  只读、幂等、AlwaysOn；在 Idle、选子、预览、连跳、确认前均可调用。
  不修改 interaction phase、selected piece、highlight、camera、turn 或 action log。
```

`cells` 故意不混入拓扑字段，`edges` 是唯一的拓扑权威来源。这样不会为每个 Cell 重复发送邻接表，也不会让模型把展示坐标误认为规则。

## 数据模型

Gameplay 新增 typed API：

```cpp
struct FLocalTopologyCellInfo
{
    int32 CellId;
    ETerraGameplayTerrainType TerrainType;
    int32 OccupyingPieceId;       // INDEX_NONE means empty
    int32 OccupyingFactionId;     // INDEX_NONE means empty
    ETerraGameplayPieceType OccupyingPieceType;
    bool bOccupyingPieceCanMove;
};

struct FLocalTopologyEdge
{
    int32 CellAId; // CellAId < CellBId
    int32 CellBId;
};

struct FLocalTopologyObservation
{
    FStrategicSnapshot Snapshot;
    int32 CenterCellId;
    int32 Radius = 3;
    TArray<FLocalTopologyCellInfo> Cells; // sorted by CellId
    TArray<FLocalTopologyEdge> Edges;     // lexicographically sorted
};

bool BuildLocalTopologyObservation(int32 CenterCellId, FLocalTopologyObservation& OutObservation) const;
```

构建算法：

```text
1. 从 center_cell_id 进行 BFS，最多扩展三条真实 NeighborCellIds 边。
2. 收集所有访问到的 CellId；五边形的有效邻居数为 5，普通格为 6。
3. 对局部 Cell 集合中每一格扫描真实邻居；仅当 cell_a < cell_b 时输出一条 edge。
4. 以 CellId 稳定排序 cells，以 (cell_a_id, cell_b_id) 稳定排序 edges。
5. 从当前 CellToPieceId 与 Pieces 填充单格占据状态；死亡棋子不作为占据者返回。
```

不应以世界空间距离或 CellId 数值范围代替 BFS；局部子图必须直接反映 Gameplay 的球面邻接图。

## MCP 契约

输入 schema：

```json
{
  "type": "object",
  "required": ["cell_id"],
  "properties": {
    "cell_id": { "type": "integer" }
  },
  "additionalProperties": false
}
```

成功示例：

```json
{
  "ok": true,
  "snapshot": {
    "turn_index": 4,
    "current_faction_id": 4,
    "interaction_phase": "Idle"
  },
  "center_cell_id": 42,
  "radius": 3,
  "cells": [
    {
      "cell_id": 42,
      "terrain_tag": "plain",
      "piece": {
        "piece_id": 6,
        "faction_id": 0,
        "piece_type": "Cavalry",
        "can_move": true
      }
    },
    {
      "cell_id": 12,
      "terrain_tag": "mountain",
      "piece": null
    }
  ],
  "edges": [
    { "cell_a_id": 12, "cell_b_id": 42 }
  ]
}
```

失败返回：

```json
{ "ok": false, "error": "invalid_cell_id" }
```

与战略工具一致，`gameplay_unavailable` 与 `match_ended` 也是明确错误。`snapshot` 用于让 Agent 识别查询是否跨回合或跨 phase。

## Agent 使用原则

L3.1 不应被每回合无差别调用。LLM 应说明它为什么要观察该 Cell，并由 Agent 日志记录该目的。推荐触发条件：

```text
1. L3 的 strategic option 给出 key_cell_id，需判断山地/森林及局部通路。
2. L3 的 frontline contact 给出关键己方或敌方 Cell，需理解周围友军、敌军和绕行空间。
3. L2 的 local_tactical_card 出现 mountain block、jump anchor、archer forest outpost 或可疑前进路线。
4. 在确认动作前，需要验证预览落点周围是否形成死角、通道、屏障或协同机会。
```

LLM 提示词必须强调：

```text
edges 表示唯一真实邻接关系；不可把 CellId 数值接近视为相邻。
局部图可用于形成或否定战术假设，不可作为行动合法性证明。
具体走法仍须通过 ui_select_piece / ui_preview_move 由 Gameplay 验证。
调用目的要回答“我希望从这个局部图确认什么结构”。
```

新实验 Agent 将同时暴露：

```text
当前 phase 可见 terra.ui_* 工具
五个 terra.strategy.* 工具
terra.inspect_local_topology
```

它沿用每次工具结果后的 LLM takeaway，以观察模型是否把 `cells + edges` 用于后续选子、预览、否定路线或改变战略意图。

## 实现位置

```text
Gameplay：
  Source/Gameplay/Public/TerraGameplayContainer.h
  Source/Gameplay/Private/TerraGameplayContainer.cpp

NpcMcp：
  Source/NpcMcp/Private/TerraNpcMcpGameplayTopologyTools.cpp
  Source/NpcMcp/Private/NpcMcp.cpp

Agent：
  Tools/NpcAgent/src/run-llm-strategy-topology-interactive-think.js
  Tools/NpcAgent/package.json
```

## 验收标准

```text
1. tools/list 的所有 phase 都可见 terra.inspect_local_topology。
2. 给定有效 CellId，返回 radius=3 内所有且仅所有 BFS 可达 Cell 与内部无向边。
3. cells 不返回 neighbor、distance、offset 等拓扑字段；edges 无重复、端点均在 cells 内、稳定排序。
4. Cell 的 terrain、piece_id、faction_id、piece_type 与 Gameplay 当前状态一致。
5. 调用前后 interaction_state、highlight、camera、turn、action log 均不变。
6. 无效 CellId 返回 invalid_cell_id。
7. 独立 Game 中 LLM 主动调用该工具；日志明确说明查询目的，并记录输出如何影响后续决策。
8. 验收报告区分“模型调用了拓扑工具”与“模型正确依据拓扑调整行动”；不能把前者误认为后者。
```

## 风险与后续

半径 3 子图在普通六边格附近约有 37 个节点，包含 terrain、piece 与 edge 信息，可能占用显著 token。L3.1 的目标是验证 LLM 是否能主动提出有意义的观察问题，而不是宣称它已可靠完成任意图论推理。

若 Agent 仍频繁遗漏关键边或在 mountain block 后坚持错误路线，下一步不应继续增加原始图字段，而应提供 L3.2/L4 的确定性细节工具，例如 control point 可达性、指定兵种 terrain block、局部连通分区、两回合协同候选和目标对齐评分。

## 实施与首轮验收

已落地：

```text
Gameplay：
  FLocalTopologyCellInfo、FLocalTopologyEdge、FLocalTopologyObservation。
  BuildLocalTopologyObservation(CenterCellId, ...) 以真实 NeighborCellIds BFS 固定扩展三层。

NpcMcp：
  TerraNpcMcpGameplayTopologyTools.cpp。
  terra.inspect_local_topology 加入 AlwaysOnToolNames。

Agent：
  run-llm-strategy-topology-interactive-think.js。
  同时暴露当前 ui_*、五个 terra.strategy.*、terra.inspect_local_topology。
  每次工具调用后由 LLM 生成一句 takeaway，并加入下一步 history。
```

独立 Staged Game 已加载 `/Game/Map/NowMap` 并注册该工具。首轮 LLM 验收的调用序列为：

```text
ui_begin_turn_review
-> strategy.describe_strategic_options
-> inspect_local_topology(cell_id=12)
-> ui_select_piece(6)
-> ui_preview_move(6, 167)
-> ui_confirm_action
```

模型调用该工具的明确目的为：确认战略 control point `12` 周边的友军接近度、地形与潜在移动路径。后续 reasoning 正确引用了局部图中的 `42 -> 167 -> 12` 邻接链，说明它至少能使用 `cells + edges` 识别直接局部关系，而没有把 CellId 数值当坐标。

但此验收不能证明模型已可靠理解完整图论或形成有效战术：

```text
1. 拓扑 takeaway 退化为“半径内有多种敌我兵种”的泛化总结，未主动提炼 mountain、关键边或可通行性。
2. 模型随后选择 Cavalry 6 前往 167；L2 preview 已确定给出 cavalry_forward_mountain_blocked，
   但模型仍把该行动描述为争夺 mountain 12，最终确认了行动。
3. 因而 L3.1 已验证“主动观察与局部连接理解”，尚未验证“根据拓扑和兵种规则否定错误战略路线”。
```

这次调用原因直接指向下一批确定性细节工具：

```text
1. control point approachability：指定 piece 或 piece type 对目标 control point 的可达性、最短图距离变化与阻断原因。
2. terrain-constrained local route：返回局部图中对 cavalry / archer / infantry 可通行的路径或连通分区，而不是由 LLM 将 mountain rule 套到 raw edges。
3. goal alignment：对已 preview 的行动说明它是否实质改善指定 strategic key cell 的可达性/控制，还是仅接近最近敌人。
4. topology focus summary：围绕调用目的而不是全子图生成确定性摘要，例如“目标 mountain 的邻接入口、可进入该入口的友军兵种、被阻断的兵种”。
```

这些应作为 L3.2/L4 的候选方向；不应以继续扩展 radius 或重复增加原始 edge 字段替代它们。
