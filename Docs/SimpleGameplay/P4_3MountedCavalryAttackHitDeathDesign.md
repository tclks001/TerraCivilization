# TerraCivilization SimpleGameplay P4.3 骑兵攻击 / 受击 / 死亡表现设计稿

> 本稿对应 [PieceAnimationPresentationDesign.md](PieceAnimationPresentationDesign.md) 中 P4 骑兵双骨骼方案的第三个落地阶段。
> P4.3 建立在 [P4_1MountedCavalryIdleDesign.md](P4_1MountedCavalryIdleDesign.md) 与 [P4_2MountedCavalryMoveJumpDesign.md](P4_2MountedCavalryMoveJumpDesign.md) 之上：骑兵已经由一个 `ATerraPieceActor` 内的 `HorseMesh + RiderMesh` 表现，移动 / 跳跃时马和骑手已经分发到不同动画。本阶段只处理骑兵参与 P3 攻击、受击、死亡时的双骨骼动画分发。

---

## 1. 目标

P4.3 完成后应具备：

1. 骑兵作为攻击者时，仍复用 P3 近战攻击流程：先跑入被吃棋子 Cell，再播放攻击动画，再跑回原位。
2. 骑兵攻击时：
   - `RiderMesh` 播放 `P3CavalryMeleeAttackAnimation`。
   - `HorseMesh` 保持马 Idle。
3. 骑兵受击时：
   - `RiderMesh` 播放 `P3HitAnimation`。
   - `HorseMesh` 保持马 Idle。
4. 骑兵死亡时：
   - `RiderMesh` 从马背切换到落地相对位置，播放 `P3DeathAnimation`。
   - `HorseMesh` 播放 `P4HorseDeathAnimation`。
   - 整个骑兵 Actor 继续复用 P3 的死亡后淡出和销毁流程。
5. P4.3 暂不实现 Rider 下半身固定 Sitting；该问题留给 P4.4。

---

## 2. 四层职责

| 层 | P4.3 职责 |
| --- | --- |
| Gameplay 规则层 | 不变；仍只产出吃子归因和棋子结果 |
| 表现编排层 / Presentation Sequencer | 不变；继续复用 P3 的串行吃子流程、攻击 / 受击 / 死亡计时 |
| 动画驱动层 / Animation State Driver | 在 `ATerraPieceActor` 中对 Mounted Cavalry 的 Attack / Hit / Death 做双 Mesh 分发 |
| AnimBP / SkeletalMesh | 本阶段仍直接 `PlayAnimation`，不引入 Rider 上下半身分层 |

P4.3 的核心是 Actor 侧动画分发，不改变 P3 的吃子时序。

---

## 3. 资产接口

继续在 `APlanetTessellatedMesh` Details 面板暴露字段，归入：

```text
PlanetTopology | Tess | SimpleGameplay P1 Piece Presentation
```

新增字段：

| 字段 | 默认 / 推荐资产 | 用途 |
| --- | --- | --- |
| `P4HorseDeathAnimation` | `/Game/Animations/Horse/HorseDeath` | 骑兵死亡时马播放 |
| `P4RiderDeathRelativeLocation` | 需 PIE 调整 | 骑兵死亡时骑手落地相对位置 |
| `P4RiderDeathRelativeRotation` | 需 PIE 调整 | 骑兵死亡时骑手落地相对旋转 |

复用已有字段：

| 字段 | 用途 |
| --- | --- |
| `P3CavalryMeleeAttackAnimation` | 骑手攻击动画 |
| `P3HitAnimation` | 骑手受击动画 |
| `P3DeathAnimation` | 骑手死亡动画 |
| `P3CapturedFadeSeconds` | 死亡后整个骑兵 Actor 淡出 |
| `P4HorseIdleAnimation` | 攻击 / 受击时马保持 Idle |

---

## 4. 动画分发规则

普通 Humanoid 模式保持 P3 原逻辑：

```text
Attack -> HumanMesh 播 P3 对应攻击动画
Hit    -> HumanMesh 播 P3HitAnimation
Death  -> HumanMesh 播 P3DeathAnimation
```

Mounted Cavalry 模式改为：

```text
Attack:
  RiderAnchor -> 马背相对位置
  RiderMesh   -> P3CavalryMeleeAttackAnimation
  HorseMesh   -> P4HorseIdleAnimation

Hit:
  RiderAnchor -> 马背相对位置
  RiderMesh   -> P3HitAnimation
  HorseMesh   -> P4HorseIdleAnimation

Death:
  RiderAnchor -> P4RiderDeathRelativeLocation / Rotation
  RiderMesh   -> P3DeathAnimation
  HorseMesh   -> P4HorseDeathAnimation
```

骑兵死亡后仍由 P3 的 `StartDeathFade` 缩放淡出并销毁 Actor。P4.3 不拆分 Rider 和 Horse 的销毁生命周期。

---

## 5. Rider 落地策略

P4.3 不做真实落马物理，也不做从马背到地面的过渡动画。

实现策略：

1. 骑兵死亡触发时，立即把 `RiderAnchor` 从马背相对位置切到 `P4RiderDeathRelativeLocation / Rotation`。
2. `RiderMesh` 在该落地锚点下播放人形死亡动画。
3. `HorseMesh` 同时播放马死亡动画。

这样可以避免最差读感：骑手在马背上原地播放死亡动画。

验收时优先调整：

1. `P4RiderDeathRelativeLocation`
2. `P4RiderDeathRelativeRotation`
3. `P4RiderUniformScale`

---

## 6. 本阶段不处理

P4.3 不处理：

- Rider 下半身固定 Sitting。
- Rider 上半身攻击分层。
- 真实落马过渡动画。
- 马攻击动画。
- 马受击动画。
- 马背 socket。
- 武器挂载。
- 骑兵攻击时的人马联动细化。

这些内容后续进入 P4.4 / P4.5。

---

## 7. 验收清单

- 编译通过。
- 非骑兵 P3 攻击 / 受击 / 死亡表现不变。
- 骑兵攻击时，骑手播放骑兵攻击动画，马不播放人形攻击动画。
- 骑兵受击时，骑手播放受击动画，马保持 Idle。
- 骑兵死亡时，骑手落到地面相对位置播放死亡动画，马播放死亡动画。
- 骑兵死亡后仍按 P3 流程淡出并销毁。

---

## 8. 后续阶段衔接

| 阶段 | 目标 |
| --- | --- |
| P4.4 | Rider AnimBP：上半身攻击 / 受击，下半身固定 Sitting |
| P4.5 | 马背 socket、骑手随马背骨骼、武器挂载和细化调参 |
