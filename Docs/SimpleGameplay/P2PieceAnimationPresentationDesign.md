# TerraCivilization SimpleGameplay P2 基础移动与跳跃表现设计稿

> 本稿对应 [PieceAnimationPresentationDesign.md](PieceAnimationPresentationDesign.md) 中的 **P2：基础 locomotion 与跳跃**。
>
> P2 建立在 P1 的 `PiecePresentation` 独立模块之上。本阶段只验收普通移动、跳跃、连跳的视觉效果，不实现角色脚底贴地的 HISM 查询，也不实现攻击、受击、死亡、武器、马匹或骑兵双骨骼。

---

## 1. 目标

P2 完成后应具备：

1. 棋子从一个 Cell 到另一个 Cell 时不再瞬移。
2. 普通移动使用球面短弧插值，并驱动 `Move` 动画状态。
3. 跳跃使用球面短弧插值 + 径向抬高曲线，并驱动 `Jump` 动画状态。
4. 连跳时每次点击落点都播放一段跳跃动作，表现上不会回退成瞬移。
5. P1 增量同步框架继续成立，仍维护：

```text
PieceId -> ATerraPieceActor
```

6. 本阶段不做 HISM 射线脚底贴地，棋子仍站在 P1 的球面固定半径上。
7. 棋子方向遵循总稿的统一模型方向规则：初始背向本阵营主将，移动 / 跳跃时面向动作方向，停止后保持方向。

---

## 2. 四层职责

| 层 | P2 职责 |
| --- | --- |
| Gameplay 规则层 | 仍只负责规则真值；点击后给出最终棋子快照 |
| 表现编排层 / Presentation Sequencer | 将宿主传入的移动事件附着到本次同步快照 |
| 动画驱动层 / Animation State Driver | 在移动过程中输出 `Idle / Move / Jump`、速度、进度和朝向 |
| AnimBP / SkeletalMesh | 可选播放 Idle / Move / Jump 动画资产；未挂动画时仍显示位移效果 |

P2 不让 `Gameplay` 模块依赖动画类型，也不让 AnimBP 读取 Gameplay 内部状态。

---

## 3. 事件来源

当前 `FTerraGameplayContainer` 已经能在点击后给出最终规则快照，但还没有专门的表现事件队列。

P2 采用低侵入桥接方案：

1. `APlanetTessellatedMesh::HandleHISMClickHit` 在调用 `HandleCellClick` 前记录：
   - 旧选中棋子 ID
   - 旧选中棋子的 CellId
   - 旧交互阶段
2. 点击处理完成后读取：
   - 新选中棋子 ID
   - 新选中棋子的 CellId
   - 新交互阶段
3. 如果同一枚棋子从 `FromCellId` 变到 `ToCellId`，宿主生成一条 P2 移动事件：

```text
Move：旧阶段 PieceSelected -> 新阶段 PieceMovedCanEndTurn
Jump：新阶段 PieceJumpingCanContinue
```

4. 宿主把这条事件和最终快照一起传给 `UTerraPiecePresentationManager`。

这个方案的目的：

- 不改 Gameplay 合法性判定。
- 不把动画事件写进规则容器内部。
- P3 需要攻击 / 死亡时再把“行动结算事件”提升为更正式的 Gameplay 输出结构。

---

## 4. 表现数据结构

### 4.1 动作类型

P2 在 `PiecePresentation` 中新增表现移动类型：

```text
None
Move
Jump
```

### 4.2 单段移动事件

P2 事件只描述一枚棋子的一个位移段：

```text
PieceId
FromCellId
ToCellId
MoveType
FromWorldTransform
ToWorldTransform
```

其中 `FromWorldTransform` 和 `ToWorldTransform` 都由 `APlanetTessellatedMesh` 通过当前球面 Cell 计算得到。

---

## 5. 普通移动表现

普通移动使用：

```text
StartTransform -> TargetTransform
```

插值规则：

1. 位置沿球面方向做短弧插值。
2. 半径保持 P1 棋子半径，不做地形查询。
3. `Up` 始终取当前位置相对星球中心的外法线。
4. `Forward` 取本段移动方向投影到当前位置切平面后的方向。
5. 转向从动作开始时的当前朝向向移动方向做 `slerp`，避免方向突变。
6. 动作结束后保持结束朝向，后续同 Cell 快照同步不重置方向。
7. 动画驱动状态：

```text
ActionState = Move
MoveSpeed > 0
NormalizedPhase = 0..1
```

到达终点后切回：

```text
ActionState = Idle
MoveSpeed = 0
```

方向保持不变，直到下一次移动、攻击或受击动作发生。

---

## 6. 跳跃表现

跳跃使用同样的球面短弧路径，但额外叠加径向抬高：

```text
HeightAlpha = sin(Progress * PI)
Position = ArcPosition + Up * JumpHeightCM * HeightAlpha
```

跳跃方向规则与普通移动一致：

- 起跳时从当前朝向向跳跃方向 `slerp`。
- 空中持续面向跳跃方向。
- 落地后保持落地朝向。

推荐默认值：

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `P2JumpDurationSeconds` | `0.55` | 单跳时长 |
| `P2JumpHeightCM` | `650` | 跳跃最高额外外抬高度 |

动画驱动状态：

```text
ActionState = Jump
NormalizedPhase = 0..1
```

连跳通过每次点击落点生成一段新的 `Jump` 事件。已有跳跃未播完时，下一段跳跃从当前视觉位置或上段目标位置继续播放，避免视觉硬切。

---

## 7. 动画资产接口

P2 在 `APlanetTessellatedMesh` Details 面板继续沿用 P1 分类，并新增可选动画资产槽：

```text
PlanetTopology | Tess | SimpleGameplay P1 Piece Presentation
```

新增字段：

| 字段 | 推荐资产 |
| --- | --- |
| `P2IdleAnimation` | `/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralIdle_A` |
| `P2MoveAnimation` | `/Game/Animations/Adventurers/Animations/Rig_Medium_MovementBasicWalking_A` |
| `P2JumpAnimation` | `/Game/Animations/Adventurers/Animations/Rig_Medium_MovementBasicJump_Full_Short` |

说明：

- 如果挂了 AnimBP，AnimBP 可以读取 `UTerraPieceAnimInstance::DriveState` 自行切状态。
- 如果未挂 AnimBP，P2 会直接让 `USkeletalMeshComponent` 播上述 Animation Sequence。
- 如果动画资产为空，位移 / 跳跃弧线仍然生效，只是角色姿态保持当前骨骼默认表现。

---

## 8. 可调参数

P2 暴露以下参数：

| 字段 | 默认值 | 作用 |
| --- | --- | --- |
| `P2MoveDurationSeconds` | `0.35` | 普通移动单段时长 |
| `P2JumpDurationSeconds` | `0.55` | 跳跃单段时长 |
| `P2JumpHeightCM` | `650` | 跳跃最高额外外抬高度 |

这些参数放在 `APlanetTessellatedMesh` 上，便于 PIE 调试。

---

## 9. Undo 行为

G9 Undo 会把 Gameplay 回退到上一次点击前的状态。

P2 对 Undo 使用快照同步而不是播放倒放动画：

1. 右键 Undo 后，表现层立即同步到恢复后的棋子位置。
2. 当前正在播放的移动 / 跳跃段被取消。
3. 高亮状态仍由 G9 原逻辑恢复。
4. 若恢复到另一个 Cell，方向使用该 Cell 的初始朝向规则；若仍在同一个 Cell，则保留当前方向。

理由：

- Undo 是调试和操作纠错工具，优先要求状态准确。
- 倒放移动会增加表现队列复杂度，等后续需要时再做。

---

## 10. 本阶段不处理

P2 不处理：

- HISM 射线脚底贴地。
- 地形高度差导致的脚 IK。
- 武器挂载。
- 马匹和骑兵双骨骼。
- 攻击、受击、死亡、消失延迟。
- 远程投射物。
- 行动日志回放。

---

## 11. 验收清单

- 编译通过。
- PIE 中 P1 棋子仍能正常生成。
- 点击普通移动落点后，棋子沿球面移动到目标 Cell，不再瞬移。
- 点击跳跃落点后，棋子沿球面跳到目标 Cell，有明显起落高度。
- 连续跳跃时，每次跳跃都有独立视觉段。
- 移动 / 跳跃完成后棋子回到 Idle 状态。
- 移动 / 跳跃完成后棋子保持动作结束朝向，不被下一次同 Cell 快照同步重置。
- 移动 / 跳跃转向使用 slerp，视觉上不出现瞬间旋转。
- 右键 Undo 后棋子位置立即回到规则状态，不残留旧移动。
- 不依赖 HISM 脚底射线查询即可完成本阶段验收。
