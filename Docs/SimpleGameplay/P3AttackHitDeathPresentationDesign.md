# TerraCivilization SimpleGameplay P3 攻击、受击与死亡表现设计稿

> 本稿对应 [PieceAnimationPresentationDesign.md](PieceAnimationPresentationDesign.md) 中的 **P3：攻击、受击与死亡表现**。
>
> P3 建立在 P1/P2/P2.5 已有 `PiecePresentation` 独立模块之上：棋子已经能以 Actor 形式站在 Cell 上，并能播放移动 / 跳跃表现。本阶段只处理"回合确认后产生吃子"的表现，不改变 Gameplay 规则结算。

---

## 1. 目标

P3 完成后应具备：

1. 一次玩家行动确认结束后，表现层消费 Gameplay 已经产出的吃子归因。
2. 若本次行动包含多个吃子，按 `capturedPieceIds / attackerPieceIds / vanguardPieceIds` 的索引顺序串行播放。
3. 每次吃子的表现流程为：
   - 攻击棋子和先锋棋子面向被吃棋子。
   - 被吃棋子面向先锋棋子；若先锋无效，则面向攻击棋子。
   - 弓兵原地播放射箭动画。
   - 主将原地播放施法动画。
   - 步兵和骑兵先跑到被吃棋子的 Cell 内，再播放各自近战攻击动画。
   - 被吃棋子播放受击动画，再播放死亡动画。
   - 步兵和骑兵跑回原位，并恢复攻击时面向被吃棋子的朝向。
   - 被吃棋子停在死亡动画末尾并淡出。
4. 攻击表现不驱动规则，不影响行动日志，不参与 Undo。
5. 本阶段暂不处理武器挂载、箭矢弹幕、法术特效、攻击镜头。

---

## 2. 四层职责

| 层 | P3 职责 |
| --- | --- |
| Gameplay 规则层 | 仍负责吃子合法性、结算、行动日志；对表现层只暴露已产出的吃子归因 |
| 表现编排层 / Presentation Sequencer | 把一次行动的吃子条目拆成串行攻击段 |
| 动画驱动层 / Animation State Driver | 驱动 `Attack / Hit / Death / Move / Idle` 状态与播放时序 |
| AnimBP / SkeletalMesh | 播放具体 Animation Sequence；当前阶段仍允许直接 `PlayAnimation` |

P3 不让 AnimBP 读取 Gameplay 内部状态，也不让 Gameplay 依赖骨骼动画资产。

---

## 3. 事件来源

Gameplay 层在行动结算时已经有以下归因：

```text
capturedPieceIds[i]
attackerPieceIds[i]
vanguardPieceIds[i]
```

语义：

```text
capturedPieceIds[i] 被 attackerPieceIds[i] 与 vanguardPieceIds[i] 共同吃掉
```

P3 在宿主层 `APlanetTessellatedMesh` 中，在确认结束回合之前读取待结算吃子条目，在 Gameplay 完成结算之后把它们转换成：

```text
FTerraPiecePresentationCaptureEvent
```

该事件包含：

- 被吃棋子 ID / Cell / 类型 / Transform
- 攻击棋子 ID / Cell / 类型 / Transform
- 先锋棋子 ID / Cell / 类型 / Transform

这样死亡棋子即使在 Gameplay 快照中已经 `bAlive=false`，表现层仍能找到它原来的 Actor 并播放死亡表现。

---

## 4. 表现数据结构

P3 在 `PiecePresentation` 中新增：

```text
FTerraPiecePresentationCaptureParticipant
FTerraPiecePresentationCaptureEvent
```

单个参与者包含：

```text
PieceId
CellId
PieceType
WorldTransform
```

单个吃子事件包含：

```text
Captured
Attacker
Vanguard
```

`UTerraPiecePresentationManager::SyncPieces` 新增可选 `CaptureEvents` 参数。同步快照时：

1. 活棋子继续按 P1/P2 增量同步。
2. 本次被吃棋子即使不在活棋子快照中，也不会立刻销毁。
3. 吃子表现完成并淡出后，Manager 再销毁对应 Actor。

---

## 5. 单次吃子流程

### 5.1 转向

开始时：

- 攻击棋子面向被吃棋子。
- 先锋棋子面向被吃棋子。
- 被吃棋子面向先锋棋子。
- 若先锋棋子无效，则被吃棋子面向攻击棋子。

转向使用 slerp，在 `P3FacingBlendSeconds` 时间内完成，避免瞬间旋转。

### 5.2 近战棋子跑入

若攻击棋子或先锋棋子是：

- `Infantry`
- `Cavalry`

则该棋子先从原位跑到被吃棋子的 Cell 内。

使用：

```text
P2MoveAnimation
P3MeleeRunInSeconds
```

跑入只影响表现 Actor，不改变 Gameplay 规则位置。

### 5.3 攻击与受击死亡

所有攻击参与者同时播放各自攻击动画：

| 兵种 | 动画 |
| --- | --- |
| 主将 `Commander` | `P3CommanderMagicAttackAnimation` |
| 弓兵 `Archer` | `P3ArcherRangedAttackAnimation` |
| 步兵 `Infantry` | `P3InfantryMeleeAttackAnimation` |
| 骑兵 `Cavalry` | `P3CavalryMeleeAttackAnimation` |

被吃棋子播放：

1. `P3HitAnimation`
2. `P3DeathAnimation`

受击和死亡动画全兵种共用。

攻击动画与受击动画的同步不再使用单一固定延迟。P3 按本次吃子中所有攻击参与者计算：

```text
SynchronizedHitDelay = max(
    P3HitReactDelaySeconds,
    ResolveAttackToHitSeconds(AttackerPieceType),
    ResolveAttackToHitSeconds(VanguardPieceType)
)
```

然后反推每个攻击者的起播时间：

```text
AttackStartDelay(piece) = SynchronizedHitDelay - ResolveAttackToHitSeconds(piece.PieceType)
```

结果是：

- 命中帧较晚的攻击动画先播。
- 命中帧较早的攻击动画后播。
- 所有攻击动画的"有效命中点"对齐到同一个受击动画开始时刻。
- `P3HitReactDelaySeconds` 保留为最小受击延迟 / 兼容旧参数。

### 5.4 近战棋子返回

攻击结束后，步兵 / 骑兵攻击者跑回各自原来的 Cell。

使用：

```text
P2MoveAnimation
P3MeleeReturnSeconds
```

返回结束后回到 Idle，并恢复攻击时面向被吃棋子的朝向。也就是说，近战棋子虽然位置回到原 Cell，但最终朝向仍保持"原位指向被吃棋子"的方向，而不是面向返回移动方向。

返回阶段的旋转使用 **Up 随位置变化** 模型：

- **Up** 每帧直接从当前插值位置的球面法线 `(Position - Center).Normalized()` 推导，严格贴合脚下地表，绝不继承任何来源 Cell 的法线。
- **Forward** 由统一的 `ETerraPiecePresentationForwardMode` 控制。返回过程使用 `MoveDirection` 模式，即 Forward 始终指向移动方向（从当前吃子位置到原 Cell），棋子沿球面"面向前进方向"自然跑回。
- 返回移动结束时，由 Manager 在回调中调用 `FaceTowards(被吃棋子位置)` 将朝向切为"原位指向被吃棋子"，满足设计稿要求。

`ETerraPiecePresentationForwardMode` 提供三种模式：

| 模式 | Forward 行为 | 典型用途 |
| --- | --- | --- |
| `MoveDirection` | 始终指向移动方向（From → To） | 跑入、返回、一般移动表现 |
| `FaceTarget` | 持续指向一个固定世界位置 | 面向特定目标移动（未来用途） |
| `LockSource` | 锁定起始时的 Forward 方向 | 需要保持起始朝向的移动（未来用途） |

这一设计将 Up 和 Forward 解耦：Up 总是正确的脚下法线，Forward 由模式独立决定，从根源上消除了跨 Cell 移动时的旋转歪斜问题。

### 5.5 被吃棋子淡出

被吃棋子死亡动画结束后，保持死亡末尾姿态，随后用 `P3CapturedFadeSeconds` 淡出。

当前 C++ 首版以模型缩放到 0 后销毁 Actor 表示淡出；若后续材质改为透明混合，可替换为材质透明度淡出。

---

## 6. 可调参数

P3 在 `APlanetTessellatedMesh` 上暴露：

| 字段 | 默认值 | 用途 |
| --- | --- | --- |
| `P3CommanderMagicAttackAnimation` | `Rig_Medium_GeneralMagic_Spell_Casting` | 主将施法 |
| `P3ArcherRangedAttackAnimation` | `Rig_Medium_GeneralShooting_Arrow` | 弓兵射击 |
| `P3InfantryMeleeAttackAnimation` | `Rig_Medium_GeneralSword_And_Shield_Slash` | 步兵近战 |
| `P3CavalryMeleeAttackAnimation` | `Rig_Medium_GeneralUpward_Thrust` | 骑兵占位突刺 |
| `P3HitAnimation` | `Rig_Medium_GeneralHit_A` | 通用受击 |
| `P3DeathAnimation` | `Rig_Medium_GeneralDeath_A` | 通用死亡 |
| `P3FacingBlendSeconds` | `0.15` | 攻击前转向时长 |
| `P3MeleeRunInSeconds` | `0.25` | 近战跑入时长 |
| `P3MeleeReturnSeconds` | `0.25` | 近战返回时长 |
| `P3AttackAnimationStartOffsetSeconds` | `0.0` | 攻击动画起播偏移，用于跳过动画开头空帧 |
| `P3CommanderAttackToHitSeconds` | `0.35` | 主将攻击动画从起播到受击开始的校准时间 |
| `P3ArcherAttackToHitSeconds` | `0.35` | 弓兵攻击动画从起播到受击开始的校准时间 |
| `P3InfantryAttackToHitSeconds` | `0.25` | 步兵攻击动画从起播到受击开始的校准时间 |
| `P3CavalryAttackToHitSeconds` | `0.25` | 骑兵攻击动画从起播到受击开始的校准时间 |
| `P3HitReactDelaySeconds` | `0.2` | 最小受击延迟 / 兼容旧参数；实际受击时刻会与各攻击者 AttackToHit 取最大值 |
| `P3HitAnimationStartOffsetSeconds` | `0.0` | 受击动画起播偏移 |
| `P3DeathAfterHitDelaySeconds` | `0.2` | 受击触发后多久触发死亡 |
| `P3DeathAnimationStartOffsetSeconds` | `0.0` | 死亡动画起播偏移 |
| `P3CapturedFadeSeconds` | `0.35` | 死亡后淡出时长 |

这些参数用于校准攻击帧、受击帧和死亡帧，避免攻击已经结束但受击刚开始等不同步问题。

---

## 7. 与 Undo 的关系

P3 不处理 Undo 攻击动画。

原因：

- Undo 只在回合内有效。
- 攻击 / 受击 / 死亡只发生在玩家点击同一目标 Cell 确认并结束回合后。
- 结束回合后 G9 的点选撤销栈已经清空。

因此 Undo 仍只回退移动、跳跃、选中和高亮状态，不需要反向播放攻击或死亡表现。

---

## 8. 本阶段不处理

P3 不处理：

- 武器挂载。
- 箭矢、弹丸、法术轨迹。
- 攻击镜头追踪。
- 骑兵真实骑乘动画。
- 多攻击者严格战斗站位。
- 伤害数字或 UI 战报。
- 材质透明淡出。

---

## 9. 验收清单

- 编译通过。
- 无吃子的普通移动 / 跳跃行为保持 P2 结果。
- 确认一个有吃子的行动后，移动 / 跳跃表现之后触发吃子表现。
- 多个吃子按顺序逐个播放。
- 攻击棋子和先锋棋子会先面向被吃棋子。
- 被吃棋子会面向先锋棋子。
- 弓兵原地播放射箭动画。
- 主将原地播放施法动画。
- 步兵 / 骑兵跑入被吃棋子 Cell，攻击后跑回原位，并恢复攻击时面向被吃棋子的朝向。
- 被吃棋子播放受击和死亡动画，随后淡出并移除。
- P3 不影响 Gameplay 行动日志输出。
- P3 不影响 C6 行动镜头追踪；攻击过程镜头仍交给相机模块后续处理。
