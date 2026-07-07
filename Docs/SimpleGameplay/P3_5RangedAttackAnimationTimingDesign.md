# TerraCivilization SimpleGameplay P3.5 远程攻击动画表现设计稿

> 本稿对应 [PieceAnimationPresentationDesign.md](PieceAnimationPresentationDesign.md) 中 P3 攻击、受击与死亡表现之后的补充阶段。
> P3.5 建立在 [P3AttackHitDeathPresentationDesign.md](P3AttackHitDeathPresentationDesign.md) 和 [P6ProjectileAndSpellVfxDesign.md](P6ProjectileAndSpellVfxDesign.md) 之上，只调整弓兵 / 主将远程攻击动画的播放速率，不改变 Gameplay 吃子判定、近战跑入返回、受击和死亡规则。

---

## 1. 目标

P3.5 完成后应具备：

1. 弓兵射箭动画可以在固定目标时长内播放，不再完全受动画资产原始长度影响。
2. 主将施法动画可以在固定目标时长内播放，不再完全受动画资产原始长度影响。
3. 弓兵 / 主将各自提供 `PlayRateScale` 微调项，模仿 P4.2 马跳跃动画的速率适配方式。
4. 通过调节远程攻击动画时长、`PlayRateScale`、P6 `ReleaseDelaySeconds` 和 P6 `FlightSeconds`，对齐：
   - 攻击动作释放帧
   - 箭矢 / 法术出手帧
   - 箭矢 / 法术抵达帧
   - 被吃棋子的受击动画起播帧
5. 步兵 / 骑兵近战攻击动画暂不改速率，仍沿用 P3 原逻辑。

---

## 2. 范围

本阶段处理：

- `Commander` 主将远程施法攻击动画播放速率。
- `Archer` 弓兵远程射箭攻击动画播放速率。
- P3 捕获表现阶段时长计算中，对远程攻击动画使用 P3.5 目标时长。

本阶段不处理：

- 近战攻击动画统一时长。
- 受击 / 死亡动画统一时长。
- P6 投射物路径、命中特效、换色。
- Gameplay 伤害、行动日志、Undo。

---

## 3. 参数接口

P3.5 在 `APlanetTessellatedMesh` Details 面板中继续归入：

```text
PlanetTopology | Tess | SimpleGameplay P1 Piece Presentation
```

新增字段：

| 字段 | 默认值 | 用途 |
| --- | --- | --- |
| `P35CommanderAttackDurationSeconds` | `0.35` | 主将施法攻击动画目标播放时长 |
| `P35CommanderAttackPlayRateScale` | `1.0` | 主将施法攻击动画播放速率缩放 |
| `P35ArcherAttackDurationSeconds` | `0.35` | 弓兵射箭攻击动画目标播放时长 |
| `P35ArcherAttackPlayRateScale` | `1.0` | 弓兵射箭攻击动画播放速率缩放 |

已有字段继续参与远程攻击同步：

| 字段 | 用途 |
| --- | --- |
| `P3CommanderAttackToHitSeconds` | 主将攻击起播到受击起播的同步时间 |
| `P3ArcherAttackToHitSeconds` | 弓兵攻击起播到受击起播的同步时间 |
| `P3AttackAnimationStartOffsetSeconds` | 攻击动画起播偏移，用于跳过动画开头空帧 |
| `P6SpellReleaseDelaySeconds` | 主将攻击起播后，法术 Actor 在挂点停留多久再释放 |
| `P6ArrowReleaseDelaySeconds` | 弓兵攻击起播后，箭矢 Actor 在挂点停留多久再释放 |
| `P6SpellFlightSeconds` | 法术飞到目标的时长 |
| `P6ArrowFlightSeconds` | 箭矢飞到目标的时长 |

---

## 4. 播放速率规则

P3.5 使用和 P4.2 马跳跃相同的“固定表现窗口 + 播放速率适配”思路。

弓兵 / 主将远程攻击动画的最终播放速率为：

```text
RemoteAttackPlayRate =
    (AttackAnimation.Length - P3AttackAnimationStartOffsetSeconds)
    / P35RemoteAttackDurationSeconds
    * P35RemoteAttackPlayRateScale
```

其中：

- `P35RemoteAttackDurationSeconds` 对主将取 `P35CommanderAttackDurationSeconds`。
- `P35RemoteAttackDurationSeconds` 对弓兵取 `P35ArcherAttackDurationSeconds`。
- `P35RemoteAttackPlayRateScale` 对主将取 `P35CommanderAttackPlayRateScale`。
- `P35RemoteAttackPlayRateScale` 对弓兵取 `P35ArcherAttackPlayRateScale`。

当 `PlayRateScale = 1.0` 时，动画剩余片段会尽量在目标时长内播完。

当 `PlayRateScale > 1.0` 时，动画会更快结束。

当 `PlayRateScale < 1.0` 时，动画会更慢结束。

因此远程攻击动画的实际表现时长为：

```text
RemoteAttackActualDuration =
    P35RemoteAttackDurationSeconds / P35RemoteAttackPlayRateScale
```

P3 后续等待、远程攻击回正等依赖“动画结束”的时序，都应使用这个实际表现时长，而不是只使用目标播放时长。

---

## 5. 与 P3 同步时序的关系

P3 原有同步规则保持：

```text
SynchronizedHitDelay = max(
    P3HitReactDelaySeconds,
    ResolveAttackToHitSeconds(Attacker),
    ResolveAttackToHitSeconds(Vanguard)
)

AttackStartDelay(piece) =
    SynchronizedHitDelay - ResolveAttackToHitSeconds(piece.PieceType)
```

P3.5 不改变“攻击起播到受击起播”的同步时刻。也就是说：

- `P3CommanderAttackToHitSeconds` 仍决定主将攻击起播后多久触发受击。
- `P3ArcherAttackToHitSeconds` 仍决定弓兵攻击起播后多久触发受击。
- P3.5 只控制攻击动画本身如何在这段窗口中播放。

远程攻击的 P3 捕获表现阶段等待时间不再使用动画资产原始长度，而使用实际表现时长：

```text
AttackStartDelay + P35RemoteAttackDurationSeconds / P35RemoteAttackPlayRateScale
```

这样较长的远程动画资产不会把整段吃子表现拖长。

---

## 6. 与 P6 投射物 / 法术的关系

P6 Actor 现在按攻击起播时刻生成；生成后先跟随攻击者的 `AttachName` 挂点停留 `ReleaseDelaySeconds`，再脱离挂点开始飞行：

```text
ProjectileStartDelay =
    AttackStartDelay

ProjectileReleaseTime =
    AttackStartDelay + P6ReleaseDelaySeconds

ProjectileArriveTime =
    AttackStartDelay + P6ReleaseDelaySeconds + P6FlightSeconds
```

推荐调参目标：

```text
P6ReleaseDelaySeconds + P6FlightSeconds
    ~= P3RemoteAttackToHitSeconds

P6ReleaseDelaySeconds
    <= P35RemoteAttackDurationSeconds
```

含义：

- 投射物 / 法术抵达目标时，被吃棋子开始受击。
- 投射物 / 法术从攻击起播时已经可见，并在释放前跟随手部挂点。
- 释放时刻落在攻击动画播放窗口内。
- 如果释放帧看起来偏早 / 偏晚，优先调 `P6ReleaseDelaySeconds`。
- 如果投射物飞得太快 / 太慢，优先调 `P6FlightSeconds`。
- 如果角色动作本身太快 / 太慢，调 `P35*AttackPlayRateScale` 或 `P35*AttackDurationSeconds`。

---

## 7. C++ 落地

### 7.1 配置层

`FTerraPieceVisualConfig` 新增：

- `P35CommanderAttackDurationSeconds`
- `P35CommanderAttackPlayRateScale`
- `P35ArcherAttackDurationSeconds`
- `P35ArcherAttackPlayRateScale`

`APlanetTessellatedMesh::BuildPieceVisualConfig_()` 将 Details 面板字段写入 `VisualConfig`，并对时长和缩放做最小值钳制。

### 7.2 Actor 播放层

`ATerraPieceActor::PlayAttackAnimation` 新增重载：

```text
PlayAttackAnimation(AttackAnimation, StartOffsetSeconds, PlayRate)
```

远程攻击由 Manager 传入计算后的 `PlayRate`。

原有两参数版本保留，并默认 `PlayRate = 1.0`，保证旧调用和近战路径行为不变。

### 7.3 Manager 编排层

`UTerraPiecePresentationManager` 新增远程攻击辅助函数：

- `IsP35RemoteAttackPiece_`
- `GetP35RemoteAttackDurationSeconds_`
- `GetP35RemoteAttackPlayRateScale_`
- `ResolveAttackAnimationPlayRate_`
- `ResolveAttackAnimationDurationSeconds_`

调度攻击动画时：

- Commander / Archer 使用 P3.5 播放速率。
- Infantry / Cavalry 返回 `PlayRate = 1.0`。

计算 P3 攻击阶段总等待时间时：

- Commander / Archer 使用 P3.5 目标时长。
- Infantry / Cavalry 继续使用原始动画资产长度。

---

## 8. 验收清单

- 编译通过。
- 步兵 / 骑兵近战攻击表现不变。
- 弓兵远程攻击时，射箭动画播放速率会随 `P35ArcherAttackDurationSeconds` 和 `P35ArcherAttackPlayRateScale` 改变。
- 主将远程攻击时，施法动画播放速率会随 `P35CommanderAttackDurationSeconds` 和 `P35CommanderAttackPlayRateScale` 改变。
- 调整 `P6ArrowReleaseDelaySeconds + P6ArrowFlightSeconds` 可让箭矢抵达接近弓兵攻击的受击时刻。
- 调整 `P6SpellReleaseDelaySeconds + P6SpellFlightSeconds` 可让法术抵达接近主将攻击的受击时刻。
- P3 吃子表现不会因为远程攻击动画资产原始长度较长而额外等待。

---

## 9. 排错表

| 现象 | 可能原因 | 处理 |
| --- | --- | --- |
| 弓兵动作太快 | `P35ArcherAttackDurationSeconds` 太小，或 `P35ArcherAttackPlayRateScale` 太大 | 增大目标时长，或降低缩放 |
| 弓兵动作太慢 | `P35ArcherAttackDurationSeconds` 太大，或 `P35ArcherAttackPlayRateScale` 太小 | 减小目标时长，或提高缩放 |
| 主将施法动作和火球释放不同步 | `P6SpellReleaseDelaySeconds` 未对齐施法动作释放帧 | 先调释放延迟，再调动画时长 / 缩放 |
| 箭矢 / 法术抵达和受击不同步 | `P6ReleaseDelaySeconds + P6FlightSeconds` 与 `P3*AttackToHitSeconds` 不匹配 | 调 P6 释放/飞行，或调 P3 AttackToHit |
| 远程攻击结束后等待很久才进入后续阶段 | 受击 / 死亡动画或淡出阶段仍占用总等待 | 检查 `P3DeathAfterHitDelaySeconds`、死亡动画长度和 `P3CapturedFadeSeconds` |

---

## 10. 后续衔接

P3.5 只解决远程攻击动作的播放速率。后续如果要做“所有攻击总时长统一”，建议独立进入 P6.5 或更高阶段，再统一考虑：

- 近战跑入 + 攻击 + 返回总窗口。
- 近战攻击动画目标时长。
- 受击 / 死亡动画是否也需要固定播放窗口。
- P6 projectile 到达和 hit VFX 的统一时序。
