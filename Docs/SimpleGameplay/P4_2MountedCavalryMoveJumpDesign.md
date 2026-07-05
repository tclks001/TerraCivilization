# TerraCivilization SimpleGameplay P4.2 骑兵移动与跳跃表现设计稿

> 本稿对应 [PieceAnimationPresentationDesign.md](PieceAnimationPresentationDesign.md) 中 P4 骑兵双骨骼方案的第二个落地阶段。
>
> P4.2 建立在 [P4_1MountedCavalryIdleDesign.md](P4_1MountedCavalryIdleDesign.md) 之上：骑兵已经是一个 Actor 内的 `HorseMesh + RiderMesh`。本阶段只处理骑兵移动 / 跳跃时的动画分发，不改变 Gameplay 规则，不处理攻击、受击、死亡和武器。

---

## 1. 目标

P4.2 完成后应具备：

1. 骑兵仍然是一个 `ATerraPieceActor`，Gameplay 仍只看到一枚 `Cavalry` 棋子。
2. 骑兵 Actor 的球面位移、跳跃弧线、转向仍复用 P2 逻辑。
3. 骑兵普通移动时：
   - `HorseMesh` 播放马 Walk / Gallop 类动画。
   - `RiderMesh` 继续播放 `Rig_Medium_GeneralSitting`。
4. 骑兵跳跃时：
   - `HorseMesh` 播放马 Jump 类动画。
   - `RiderMesh` 继续播放 `Rig_Medium_GeneralSitting`。
5. 骑兵移动 / 跳跃结束后：
   - `HorseMesh` 回到马 Idle。
   - `RiderMesh` 保持 Sitting。
6. 非骑兵棋子的 P2 移动、跳跃表现不变。

---

## 2. 四层职责

| 层 | P4.2 职责 |
| --- | --- |
| Gameplay 规则层 | 不变；仍只产出 Move / Jump 后的棋子位置 |
| 表现编排层 / Presentation Sequencer | 继续消费现有 `FTerraPiecePresentationMoveEvent` |
| 动画驱动层 / Animation State Driver | 对 Mounted 模式分发 Horse Move / Jump，Rider 保持 Sitting |
| AnimBP / SkeletalMesh | 本阶段仍直接 `PlayAnimation`，不引入 AnimBP 分层 |

P4.2 的核心是“动画分发”，不是新事件系统。

---

## 3. 资产接口

P4.2 继续在 `APlanetTessellatedMesh` Details 面板暴露字段，归入：

```text
PlanetTopology | Tess | SimpleGameplay P1 Piece Presentation
```

新增字段：

| 字段 | 默认 / 推荐资产 | 用途 |
| --- | --- | --- |
| `P4HorseMoveAnimation` | `/Game/Animations/Horse/HorseWalk` | 骑兵普通移动时马播放 |
| `P4HorseJumpAnimation` | `/Game/Animations/Horse/HorseGallop_Jump` | 骑兵跳跃时马播放 |
| `P4HorseJumpPlayRateScale` | `1.0` | 马跳跃动画播放速率缩放；用于让马跳动画在固定跳跃位移时间内播完 |

已有 P4.1 字段继续使用：

| 字段 | 用途 |
| --- | --- |
| `P4HorseIdleAnimation` | 移动 / 跳跃结束后回 Idle |
| `P4RiderSittingAnimation` | 骑手移动 / 跳跃期间保持坐姿 |
| `P4HorseRelative*` | 马站位和朝向调参 |
| `P4RiderRelative*` | 骑手马背位置调参 |

---

## 4. 动画分发规则

普通 Humanoid 模式保持 P2 原逻辑：

```text
Move -> HumanMesh 播 P2MoveAnimation
Jump -> HumanMesh 播 P2JumpAnimation
Finish -> HumanMesh 播 P2IdleAnimation
```

Mounted 模式改为：

```text
Move:
  HorseMesh -> P4HorseMoveAnimation
  RiderMesh -> P4RiderSittingAnimation

Jump:
  HorseMesh -> P4HorseJumpAnimation
  RiderMesh -> P4RiderSittingAnimation

Finish:
  HorseMesh -> P4HorseIdleAnimation
  RiderMesh -> P4RiderSittingAnimation
```

骑兵跳跃动画播放速率需要特殊处理：

- 非骑兵继续使用 `P2JumpDurationSeconds`。
- 骑兵也继续使用 `P2JumpDurationSeconds` 作为 Actor 从起点跳到终点的固定总时长。
- 马跳跃动画通过播放速率适配固定跳跃位移时间，而不是拉长棋子的空间位移。
- 播放速率为：

```text
HorseJumpPlayRate =
    P4HorseJumpAnimation.Length / P2JumpDurationSeconds
    * P4HorseJumpPlayRateScale
```

- 跳跃位移结束后直接切回 `P4HorseIdleAnimation`。
- 如果马跳跃动作仍显得过快或过慢，只调整 `P4HorseJumpPlayRateScale`。
- 该方案保持棋盘移动节奏稳定，避免为了适配某个资产而拖慢连跳和镜头追踪节奏。

骑手在 P4.2 阶段不播放跑步、跳跃或攻击动画。原因是骑手没有骑乘移动动画时，全身播放人形移动 / 跳跃会让腿部离开坐姿，造成明显穿模。

---

## 5. 位移与朝向

P4.2 不改 P2 的空间逻辑：

- 普通移动仍沿球面短弧插值。
- 跳跃仍沿球面短弧 + 径向抬高曲线。
- 骑兵整体 Actor 的 Forward 仍面向移动 / 跳跃方向。
- 动作结束后保持 P2 当前方向规则。

马和骑手的相对位置仍由 P4.1 参数控制。P4.2 不引入马背 socket，也不让骑手随马骨骼起伏。

---

## 6. 与 P3 的关系

P4.2 不处理骑兵攻击。

因此：

- P3 中骑兵攻击仍可能不是最终效果。
- P4.2 不改变攻击、受击、死亡接口。
- 骑兵攻击 / 受击 / 死亡的 Mounted 特化留给 P4.3。

---

## 7. 本阶段不处理

P4.2 不处理：

- 骑手攻击、受击、死亡。
- 马死亡。
- 上半身 / 下半身分层。
- 马背 socket。
- 骑手随马背骨骼摆动。
- 武器挂载。
- 骑兵攻击站位。
- 马移动速度与位移距离的严格匹配。

---

## 8. 验收清单

- 编译通过。
- 非骑兵棋子的移动 / 跳跃表现不变。
- 骑兵 Idle 仍保持 P4.1 的马 + 坐姿骑手。
- 骑兵普通移动时，Actor 沿 P2 路径移动，马播放 Walk / Gallop 类动画，骑手保持 Sitting。
- 骑兵跳跃时，Actor 沿 P2 跳跃弧线移动，马播放 Jump 类动画，骑手保持 Sitting。
- 骑兵普通移动结束后，马立即回到 Idle，骑手仍保持 Sitting。
- 骑兵跳跃位移总时长仍使用 `P2JumpDurationSeconds`。
- 骑兵跳跃时，马跳跃动画会按 `P4HorseJumpAnimation.Length / P2JumpDurationSeconds * P4HorseJumpPlayRateScale` 自动调整播放速率。
- 骑兵跳跃位移结束后，马直接回到 Idle，不应再出现到达目的地后原地播放剩余跳跃动画。
- 不要求攻击、受击、死亡表现正确。

---

## 9. 后续阶段衔接

P4.2 完成后，后续建议：

| 阶段 | 目标 |
| --- | --- |
| P4.3 | 骑兵攻击 / 受击 / 死亡：Rider 播对应动作，Horse 播死亡 |
| P4.4 | Rider AnimBP：上半身攻击 / 受击，下半身固定 Sitting |
| P4.5 | 马背 socket、骑手随马背骨骼、武器挂载和细化调参 |
