# TerraCivilization SimpleGameplay P2.5 棋子 HISM 高度射线修正设计稿
> 实现更新（2026-07-10）：
> - P2.5 的 HISM 高度采样逻辑已迁移到 `UPlanetPiecePresentationComponent::TryResolvePieceHeightFromHISM(...)`。
> - `bEnableP2_5HISMPieceHeightTrace`、`P2_5PieceHeightTraceStartOffsetCM`、`P2_5PieceHeightTracePastCenterOffsetCM` 等字段现由 `PlanetPiecePresentationComponent` 持有。
> - `APlanetTessellatedMesh` 仅保留兼容桥接函数，不再直接承载 P2.5 配置。

> 本稿对应 [PieceAnimationPresentationDesign.md](PieceAnimationPresentationDesign.md) 中的 **P2.5：棋子站位高度使用 HISM 碰撞修正**。
>
> P2.5 建立在 P1/P2 的棋子 Actor 表现框架之上。本阶段只修正棋子 Actor Transform 的球面高度，不做脚 IK，不使用碰撞法线，不改变移动、跳跃、朝向和动画状态机逻辑。

---

## 1. 目标

P2.5 完成后应具备：

1. 每个棋子 Actor 的站位高度不再只依赖 `GlobeRadiusCM`。
2. 通过从棋子外侧沿球心方向做射线检测，命中当前 HISM 瓦片碰撞。
3. 使用 HISM 命中点修正棋子 Actor 的球面高度。
4. 不使用命中法线，`Up` 仍然取 Cell 球面外法线。
5. `Piece Radius Offset CM` 作为碰撞命中点之后的额外外抬微调值。
6. 如果 HISM 未启用、碰撞未启用、射线未命中，则回退到 P1 的固定半径站位。

---

## 2. 模块边界

P2.5 的高度查询属于地形宿主职责：

| 层 | 职责 |
| --- | --- |
| `APlanetTessellatedMesh` | 根据 CellId、HISM 碰撞和 `P1PieceRadiusOffsetCM` 生成最终棋子 Transform |
| `PiecePresentation` | 只消费宿主提供的 Transform，不直接查询 HISM |
| `Gameplay` | 不感知地形碰撞和模型高度 |

理由：

- HISM 组件、实例反查、地形碰撞都在 `TerraCivilization` 地形宿主内。
- `PiecePresentation` 应继续独立于地形渲染细节。
- Gameplay 规则层仍只处理 CellId，不处理世界高度。

---

## 3. 射线规则

对某个 `CellId`：

1. 取 Cell 球面外法线：

```text
LocalUp = Cell.UnitCenter
WorldUp = ActorTransform.TransformVectorNoScale(LocalUp)
```

2. 从外侧向球心方向发射射线：

```text
TraceStart = PlanetCenter + WorldUp * (GlobeRadiusCM + P2_5TraceStartOffsetCM)
TraceEnd   = PlanetCenter - WorldUp * P2_5TracePastCenterOffsetCM
```

3. 使用 `LineTraceMultiByChannel` 查询 `ECC_WorldStatic`。
4. 只接受命中的 HISM 组件：

```text
PlainTileHISMComp
ForestTileHISMComp
MountainTileHISMComp
```

5. 若能通过实例反查确认命中的是当前 `CellId`，优先使用当前 Cell 的命中；否则使用第一条 HISM 命中作为兜底。
6. 最终棋子位置：

```text
WorldPosition = Hit.ImpactPoint + WorldUp * P1PieceRadiusOffsetCM
```

---

## 4. 不使用法线

P2.5 明确不使用 `Hit.ImpactNormal`。

棋子姿态仍然使用：

```text
Up = Cell.UnitCenter
Forward = P1/P2 既有方向规则
```

这样可以避免瓦片网格局部法线导致角色在格子边缘歪斜。

---

## 5. 可调参数

在 `APlanetTessellatedMesh` 上新增：

| 字段 | 默认值 | 作用 |
| --- | --- | --- |
| `bEnableP2_5HISMPieceHeightTrace` | `true` | 是否启用 HISM 高度射线修正 |
| `P2_5PieceHeightTraceStartOffsetCM` | `5000` | 射线从球面外侧多远开始 |
| `P2_5PieceHeightTracePastCenterOffsetCM` | `1000` | 射线穿过球心继续多远，防止中心附近数值误差 |

`P1PieceRadiusOffsetCM` 继续保留，但语义改为：

```text
命中 HISM 后沿 Cell 外法线额外外抬的模型微调高度
```

未命中 HISM 时仍按旧语义作为 `GlobeRadiusCM` 的外抬高度。

---

## 6. 对 P2 移动 / 跳跃的影响

P2 移动和跳跃的端点 Transform 都来自：

```text
BuildP1PieceWorldTransform_
```

因此 P2.5 生效后：

- 静止棋子会站到 HISM 碰撞面上方。
- 普通移动从修正后的起点移动到修正后的终点。
- 跳跃从修正后的起点跳到修正后的终点。
- 跳跃过程中的额外高度仍由 P2 的 `P2JumpHeightCM` 决定。

---

## 7. 失败回退

以下情况回退到 P1 固定半径：

- `bEnableP2_5HISMPieceHeightTrace = false`
- HISM tile rendering 未启用
- HISM collision 未启用
- HISM 组件不存在
- 当前 CellId 非法
- 射线没有命中任何 HISM

回退位置：

```text
Cell.UnitCenter * (GlobeRadiusCM + P1PieceRadiusOffsetCM)
```

---

## 8. 验收清单

- 编译通过。
- PIE 中棋子仍能生成和移动。
- 开启 HISM 碰撞后，棋子站位高度贴近 HISM 瓦片碰撞面。
- 调整 `P1PieceRadiusOffsetCM` 可以在碰撞高度基础上微调模型高低。
- 关闭 `bEnableP2_5HISMPieceHeightTrace` 后，棋子回退到 P1 固定半径站位。
- 碰撞法线不影响棋子倾斜，棋子 Up 仍沿 Cell 球面外法线。
