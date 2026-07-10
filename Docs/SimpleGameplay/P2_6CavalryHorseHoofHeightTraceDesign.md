# TerraCivilization SimpleGameplay P2.6 Cavalry Horse Hoof Height Trace Design
> 实现更新（2026-07-10）：
> - P2.6 的骑兵偏移采样逻辑已迁移到 `UPlanetPiecePresentationComponent::BuildPieceWorldTransform(...)`。
> - `P2_6CavalryHeightTraceAngularOffsetDeg` 现由 `PlanetPiecePresentationComponent` 持有。
> - 文中所有“在 `APlanetTessellatedMesh` 上调 P2.6 参数”的表述，现应理解为在 `PlanetPiecePresentationComponent` 上调参。

> 本稿对应 [PieceAnimationPresentationDesign.md](PieceAnimationPresentationDesign.md) 中的 **P2.6：骑兵马蹄高度偏移采样**。
>
> P2.6 建立在 P2 的移动 / 跳跃表现与 P2.5 的 HISM 高度射线之上。本阶段只改变骑兵高度射线的采样方向，不改变 Gameplay 规则、移动事件、跳跃弧线、朝向状态机或碰撞法线使用策略。

---

## 1. 目标

P2.6 解决骑兵马在弧面 HISM 地块上四蹄悬空的问题：

1. 非骑兵继续使用 P2.5 的 Cell 中心射线采样。
2. 骑兵不再用马腹中心点向下采样高度。
3. 骑兵高度射线相对 Cell 中心外法线偏移一个可调角度，用该角度落点近似四蹄所在的外圈高度。
4. 射线命中后只取 HISM 命中点的半径作为高度参考，骑兵 Actor 仍放在原 Cell 中心方向上。
5. `P1PieceRadiusOffsetCM` 仍作为命中高度之后的额外外抬微调。
6. `P2_6CavalryHeightTraceAngularOffsetDeg = 0` 时完全退回 P2.5 中心采样。

---

## 2. 根因

当前 P2.5 对任意棋子使用：

```text
TraceDirection = Cell.UnitCenter
TraceStart = PlanetCenter + TraceDirection * (GlobeRadiusCM + P2_5PieceHeightTraceStartOffsetCM)
TraceEnd   = PlanetCenter - TraceDirection * P2_5PieceHeightTracePastCenterOffsetCM
```

这对普通单体棋子通常足够，但骑兵的视觉支撑点不是马腹中心，而是四蹄。HISM 地块本身是带弧度的瓦片，中间高、外圈低；当骑兵用中心点高度作为整匹马高度时，四蹄会被放到地块中心高度上，从而在外圈位置悬空。

---

## 3. 数据流

P2.6 保持 P2.5 的宿主边界：

| 层 | 职责 |
| --- | --- |
| `APlanetTessellatedMesh` | 根据 `CellId` 与 `PieceType` 构建最终棋子 Transform，并对骑兵使用偏移采样 |
| `PiecePresentation` | 继续消费宿主传入的 Transform，不直接查询 HISM |
| `Gameplay` | 只输出棋子类型、Cell 和规则状态，不感知地形碰撞 |

关键路径：

```text
Gameplay PieceState
  -> APlanetTessellatedMesh::BuildP1PieceWorldTransform_(CellId, PieceType)
  -> TryResolveP2_5PieceHeightFromHISM_(TraceWorldUp, PlacementWorldUp, TraceAngularOffsetDeg)
  -> FTerraPiecePresentationSnapshot / FTerraPiecePresentationMoveEvent
  -> PiecePresentation Actor
```

---

## 4. 采样规则

对某个 `CellId`：

1. 先计算中心摆放方向：

```text
PlacementWorldUp = Cell.UnitCenter transformed to world
```

2. 非骑兵：

```text
TraceWorldUp = PlacementWorldUp
TraceAngularOffsetDeg = 0
```

3. 骑兵：

```text
TraceAngularOffsetDeg = Clamp(P2_6CavalryHeightTraceAngularOffsetDeg, 0, 15)
TraceWorldUp = Rotate(PlacementWorldUp, TraceAngularOffsetDeg, tangent rotation axis)
```

4. 用 `TraceWorldUp` 做 HISM 射线：

```text
TraceStart = PlanetCenter + TraceWorldUp * (GlobeRadiusCM + P2_5PieceHeightTraceStartOffsetCM)
TraceEnd   = PlanetCenter - TraceWorldUp * P2_5PieceHeightTracePastCenterOffsetCM
```

5. 只接受 `PlainTileHISMComp` / `ForestTileHISMComp` / `MountainTileHISMComp` 的命中，仍优先选择能反查到当前 `CellId` 的命中，否则使用第一条 HISM 命中兜底。

6. 最终 Actor 位置不使用偏移命中点本身，而是回到中心摆放方向：

```text
ImpactRadius = Distance(SelectedHit.ImpactPoint, PlanetCenter)
FinalRadius  = ImpactRadius + P1PieceRadiusOffsetCM
WorldPosition = PlanetCenter + PlacementWorldUp * FinalRadius
```

这样偏移只负责取高度，不会让骑兵 Actor 横向滑离 Cell 中心。

---

## 5. C++ 接口

`APlanetTessellatedMesh` Details 面板新增：

| 字段 | 默认值 | 作用 |
| --- | --- | --- |
| `P2_6CavalryHeightTraceAngularOffsetDeg` | `1.0` | 骑兵高度射线相对 Cell 中心外法线的偏移角度，单位为度 |

约定：

- `0`：关闭 P2.6 行为，骑兵沿用 P2.5 中心采样。
- `0.5 ~ 2.0`：推荐调试范围，通常足以从中心高度采到马蹄外圈高度。
- `15`：硬上限，避免采样跨出当前 Cell 过远。

---

## 6. 对 P2 移动 / 跳跃的影响

P2 移动事件的 `FromWorldTransform` 与 `ToWorldTransform` 现在按实际 `PieceType` 构建：

- 骑兵移动起点、终点都使用 P2.6 偏移高度。
- 非骑兵保持 P2.5 行为。
- 移动和跳跃插值仍由 `PiecePresentation` 按现有球面插值执行。
- 跳跃额外高度仍由 `P2JumpHeightCM` 决定。

Undo 路径同样按实际棋子类型构建起止 Transform，避免骑兵回退时短暂回到中心采样高度。

---

## 7. 失败回退

以下情况继续回退到 P1 固定半径：

- `bEnableP2_5HISMPieceHeightTrace = false`
- HISM tile rendering 关闭
- HISM tile collision 关闭
- HISM 组件不存在
- CellId 非法
- 偏移后的射线没有命中任何 HISM

回退位置：

```text
Cell.UnitCenter * (GlobeRadiusCM + P1PieceRadiusOffsetCM)
```

---

## 8. 验收清单

- 编译通过。
- PIE 中非骑兵站位高度与 P2.5 一致。
- 骑兵静止站在弧面 HISM 地块上时，马蹄不再明显悬空。
- 调整 `P2_6CavalryHeightTraceAngularOffsetDeg` 时，骑兵高度随外圈采样高度变化。
- 将 `P2_6CavalryHeightTraceAngularOffsetDeg` 设为 `0` 后，骑兵回到 P2.5 中心采样表现。
- 骑兵普通移动、跳跃、Undo 的起点和终点都使用同一套偏移高度。
- 棋子 Up 仍沿 Cell 中心外法线，不使用 HISM 命中法线。

---

## 9. 排错表

| 症状 | 可能原因 | 修复 |
| --- | --- | --- |
| 骑兵仍悬空 | 偏移角度过小，采样仍接近中心高度 | 增大 `P2_6CavalryHeightTraceAngularOffsetDeg` |
| 骑兵下沉 | 偏移角度过大，采样到过低外圈或邻近 Cell | 减小偏移角度，优先回到 `0.5 ~ 2.0` |
| 非骑兵高度变化 | 调试时误改了 P2.5 的通用高度参数 | 检查 `P1PieceRadiusOffsetCM`、HISM collision 与 P2.5 开关 |
| 日志出现 `NoHISMHit` | 偏移后射线没有命中 HISM 瓦片 | 确认 HISM rendering/collision 开启，或减小偏移角度 |
| 骑兵移动时高度正确、静止后不一致 | 快照路径未按 `PieceType` 构建 Transform | 检查 `BuildP1PieceWorldTransformForPiece_` 是否调用带 `PieceType` 的重载 |
