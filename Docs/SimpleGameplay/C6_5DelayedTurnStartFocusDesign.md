# TerraCivilization SimpleGameplay C6.5 延迟回合回正设计稿

> 本稿从 [CameraTrackingAndInteractionDesign.md](CameraTrackingAndInteractionDesign.md) 的 C6 行动镜头追踪之后补充。
> C6.5 最初只处理“行动表现尚未结束时，不要立刻执行下一回合战区回正镜头”。2026-07-15 起，它的同一套表现时长估算也被 Gameplay 回合激活门闩复用：规则结算仍立即推进，但下一回合在动画窗口结束前不可操作。

---

## 1. 目标

C6.5 要达成：

1. 玩家或 AI 确认行动后，Gameplay 仍立即结算并推进到下一回合。
2. 如果本次行动产生了棋子移动、跳跃、攻击、受击、死亡或淡出表现，镜头不立刻按 C2.5 回正到当前阵营战区中心。
3. 待本次行动表现预计结束后，再调用既有 C2/C2.5 回合镜头入口，把视角平滑切到当前阵营战区中心。
4. Gameplay 规则状态仍立即推进；但 `UPlanetGameplayComponent` 的 `bTurnActivationReady` 在表现窗口结束前为 false。
5. 门闩关闭期间，玩家输入、MCP 最终执行和 NPC 行为树都不能开始新回合行动；门闩打开后才触发 C2.5 回合镜头焦点。

---

## 2. 边界

C6.5 的时间估算仍属于镜头/表现配置层；回合激活门闩属于 Gameplay 编排层。

允许：

- `UPlanetGameplayComponent` 用 C6.5 的表现时长估算统一延迟回合激活和 C2.5 回合焦点 Blend。
- 使用 P2 移动事件和 P3 吃子事件估算本次表现时长。
- 使用 timer 在表现结束后触发既有回合镜头入口。

不允许：

- 修改 `FTerraGameplayContainer` 的回合推进时机。
- 让 Gameplay 依赖 PiecePresentation 或 CameraTracking。
- 把“动画是否结束”写入 `FTerraGameplayContainer` 的规则真值。
- 回滚或延后 G7 行动日志输出。

---

## 3. 触发条件

C6.5 只在“本次点击导致当前阵营变化”时生效。

流程：

```text
HandleGameplayCellClick
    -> GameplayContainer->HandleCellClick(...)
    -> 若 NewFactionId != PrevFactionId，说明行动已提交并换回合
    -> 构造 P2MoveEvents / P3CaptureEvents
    -> SyncP1PiecePresentation_(P2MoveEvents, P3CaptureEvents)
    -> 如果存在待播放表现：
           关闭 bTurnActivationReady
           用 C6.5 时长估算启动 Gameplay timer
       否则：立即打开 bTurnActivationReady
    -> timer 到点后，以 (TurnIndex, FactionId) 校验未过期
       -> 打开 bTurnActivationReady
       -> 调用既有 C2.5 回合回正入口
```

---

## 4. 延迟时长

首版不要求 PiecePresentation 提供精确完成回调，C6.5 使用与当前表现配置一致的保守估算。

移动 / 跳跃：

| 事件 | 延迟 |
| --- | --- |
| Move | `P2MoveDurationSeconds` |
| Jump | `P2JumpDurationSeconds` |

攻击 / 吃子：

延迟由当前 P3 配置估算：

```text
P3FacingBlendSeconds
+ P3MeleeRunInSeconds
+ 攻击 / 受击 / 死亡中最长的表现时长
+ max(P3MeleeReturnSeconds, P3CapturedFadeSeconds)
+ C6_5TurnStartFocusDelayPaddingSeconds
```

若一次行动同时吃多个子，首版按当前 P3 队列的串行播放模型累加每个 capture event 的预计时长。

---

## 5. 过期保护

Gameplay 激活 timer 到点时必须校验：

```text
GameplayContainer->GetTurnIndex() == ExpectedTurnIndex
GameplayContainer->GetCurrentFactionId() == ExpectedFactionId
```

如果校验失败，说明期间已经发生了新行动、重建或其他状态变化，本次延迟回正过期，直接丢弃。

---

## 6. 与 C2 / C2.5 / C6 的关系

- C2 仍只负责本局第一次游戏开始镜头硬设置。
- C2.5 仍负责回合开始战区中心平滑回正。
- C6 仍负责移动 / 跳跃期间是否跟随棋子落点。
- C6.5 负责提供“表现何时结束”的保守时长估算；Gameplay 回合激活门闩决定 C2.5、输入和 NPC 何时同时放行。

也就是说，C6.5 不新增一套相机 Blend，不改变相机目标计算，只延后调用既有入口。

---

## 7. 验收

1. 棋子普通移动后确认结束回合：镜头先保留在行动区域；玩家点击、MCP 执行和 NPC BT 均不能行动；待移动表现窗口结束后再同时开放并回正到下一阵营战区中心。
2. 棋子跳跃后确认结束回合：镜头先保留在跳跃区域，待跳跃表现结束后再回正。
3. 行动造成吃子：镜头不在攻击 / 受击 / 死亡表现开始前立刻切到下一阵营中心。
4. 无移动、无攻击表现的换回合路径仍立即执行原有 C2.5 回合回正。
5. C6.5 不改变 `FTerraGameplayContainer`：行动日志、回合数、当前阵营在规则层仍立即更新；`bTurnActivationReady` 仅是 Gameplay 编排层的行动许可。

---

## 8. 暂不处理

C6.5 暂不实现：

- PiecePresentation 完成回调驱动的精确回正。
- 攻击者与被攻击者双目标镜头框选。
- 多段攻击期间的动态镜头剪辑。
- 玩家手动输入后是否取消“延迟回正”的长期偏好策略。
> Implementation note (2026-07-15): C6.5 delay estimation remains in `UPlanetCameraComponent`，但 timer ownership 和 stale-turn validation 已迁入 `UPlanetGameplayComponent`。这是为了让镜头、玩家输入、MCP 与 NPC BT 共享同一个回合激活边界。
