# TerraCivilization SimpleGameplay P6 远程投射物与法术特效设计稿

> 本稿对应 [PieceAnimationPresentationDesign.md](PieceAnimationPresentationDesign.md) 的 **P6：远程投射物、法术特效**。
> P6 建立在 P3 攻击/受击/死亡串行表现与 P5 武器挂载之上，只新增远程攻击过程中的飞行表现，不改变 Gameplay 吃子判定。

---

## 1. 目标

P6 完成后应具备：

1. 弓兵远程吃子时，在攻击动画起播时生成箭矢临时 Actor，并先让箭矢停留在挂点上。
2. 箭矢使用 `StaticMeshComponent` 显示，默认 mesh 为：
   ```text
   /Game/Animations/Adventurers/Assets/arrow_bow
   ```
3. 箭矢停留 `P6ArrowReleaseDelaySeconds` 后，从弓兵 `handslot_r` 附近释放，以被吃 Actor 当前坐标叠加 `P6ArrowTargetRelativeLocation` 为目标方向，并投影到与起点同高的落点。
4. 箭矢切向速度保持不变，法向速度按重力关系变化，飞行最高点由 `P6ArrowArcHeightCM` 控制。
5. 主将远程吃子时，在攻击动画起播时生成法术临时 Actor，并先让法术停留在挂点上。
6. 法术使用 `ChildActorComponent` 承载配置的法术 Actor，默认类为：
   ```text
   /Game/FXVarietyPack/Blueprints/BP_ky_fireBall
   ```
7. 法术停留 `P6SpellReleaseDelaySeconds` 后，从主将 `handslot_r` 附近释放，沿球面测地线飞向被吃棋子。
8. 箭矢 / 法术到达目标后直接销毁；本期不生成命中特效。
9. 投射物与法术都只属于 `PiecePresentation` 表现层，不参与 Gameplay 命中、伤害或日志判定。

当前 P6 时序规则：

- 箭矢 / 法术 Actor 在远程攻击动画起播时立即生成。
- 生成后先跟随攻击者 `AttachName` 对应 Socket / Bone 停留。
- 停留期间每帧读取攻击者当前挂点 Transform，并叠加配置的相对位置、旋转和缩放。
- 停留 `P6ArrowReleaseDelaySeconds` / `P6SpellReleaseDelaySeconds` 后，Actor 从当前挂点世界位置释放。
- 释放后飞行 `P6ArrowFlightSeconds` / `P6SpellFlightSeconds`，到达目标后销毁。
- `ReleaseDelaySeconds` 不再表示“延迟生成”，而是表示“已生成后在挂点上停留多久再释放”。

---

## 2. 非目标

P6 不处理：

- 箭矢命中火花、插入目标、停留在尸体上。
- 法术命中爆炸、冲击波、屏幕震动。
- 投射物碰撞检测。
- 阵营颜色 / 法术换色。
- 多段法术、追踪转弯、飞行中避障。
- 网络同步。

---

## 3. 架构

P6 使用统一临时 Actor：

```text
ATerraPieceProjectileActor
  RootScene
  ArrowMeshComponent       // 箭矢模式
  SpellActorComponent      // 法术模式，ChildActorComponent 承载 BP_ky_fireBall
```

选择临时 Actor 的原因：

- 箭矢 / 法术都有起点、终点、飞行时间、朝向和生命周期。
- P3 需要把它们和攻击动画、受击动画对齐。
- Manager 不需要持有裸 Mesh / 粒子的 Tick 细节。
- 后续 P7 或更晚可以在这个 Actor 上补拖尾、命中特效、颜色参数或回调。

裸粒子 / 裸 Mesh 只适合作为临时 Actor 内部视觉载体，不作为主生命周期实体。

---

## 4. C++ 落点

### 4.1 新增类型

```text
Source/PiecePresentation/Public/TerraPieceProjectileActor.h
Source/PiecePresentation/Private/TerraPieceProjectileActor.cpp
```

关键接口：

```cpp
void Launch(const FTerraPieceProjectileLaunchParams& Params);
```

`FTerraPieceProjectileLaunchParams` 包含：

- `VisualType`：Arrow / Spell
- `StartWorldLocation`
- `TargetWorldLocation`
- `PlanetCenterWorldLocation`
- `ArrowMesh`
- `SpellActorClass`
- `RelativeRotation`
- `UniformScale`
- `FlightSeconds`
- `ArcHeightCM`

### 4.2 Manager 接入

`UTerraPiecePresentationManager::RunP3ReturnAndFadeStep_` 中，在远程攻击者播放攻击动画的同一套时序里调度 P6：

```text
ProjectileStartDelay = AttackStartDelay
ProjectileReleaseTime = AttackStartDelay + P6ReleaseDelaySeconds
ProjectileArriveTime = AttackStartDelay + P6ReleaseDelaySeconds + P6FlightSeconds
```

只有以下兵种会生成 P6 projectile：

| 兵种 | 表现 |
| --- | --- |
| `Archer` | Arrow |
| `Commander` | Spell |

步兵 / 骑兵仍只走 P3 近战跑入、攻击、返回。

### 4.3 发射点

`ATerraPieceActor` 新增：

```cpp
FVector ResolveAttachmentWorldLocation(FName AttachName, const FVector& RelativeLocation) const;
```

规则：

1. 普通人形棋子从 `HumanMesh` 查 Socket / Bone。
2. 骑兵若未来使用该接口，则从 `RiderMesh` 查 Socket / Bone。
3. 找不到 `AttachName` 时回退到对应 mesh 的组件 Transform。
4. 再叠加 `RelativeLocation` 得到世界发射点。

---

## 5. 轨迹

### 5.1 箭矢

箭矢位置：

```text
TargetActorPoint = TargetActorTransform.TransformPosition(P6ArrowTargetRelativeLocation)
TargetSameHeight = normalize(TargetActorPoint - PlanetCenter) * length(Start - PlanetCenter) + PlanetCenter
Base = SlerpOnSphere(Start, TargetSameHeight, Alpha)
ExtraHeight = 4 * P6ArrowArcHeightCM * Alpha * (1 - Alpha)
Position = Base + Up * ExtraHeight
```

规则：

1. `Start` 为箭矢释放瞬间的挂点世界位置。
2. `TargetActorPoint` 为被吃 Actor 当前坐标系下的 3D 偏移点：`TargetActorTransform.TransformPosition(P6ArrowTargetRelativeLocation)`。
3. `TargetSameHeight` 会把 `TargetActorPoint` 投影到与 `Start` 相同的球心半径，保证落点与起点同高。
4. `Alpha = FlightElapsedSeconds / P6ArrowFlightSeconds`，所以切向速度恒定。
5. 法向高度使用 `4H * Alpha * (1 - Alpha)`，其中 `H = P6ArrowArcHeightCM`。
6. 该高度曲线等价于同高起落点下的恒定重力运动：法向速度线性变化，最高点出现在 `Alpha = 0.5`。

由 `P6ArrowArcHeightCM` 和 `P6ArrowFlightSeconds` 可推导等效重力关系：

```text
Gravity = 8 * P6ArrowArcHeightCM / (P6ArrowFlightSeconds ^ 2)
InitialNormalVelocity = 4 * P6ArrowArcHeightCM / P6ArrowFlightSeconds
NormalHeight(t) = InitialNormalVelocity * t - 0.5 * Gravity * t^2
```

P6 不显式暴露重力参数，避免同一条轨迹出现高度、时长、重力三者互相冲突；编辑器只调最高点高度和飞行时长。

### 5.2 法术

法术位置：

```text
Position = SlerpOnSphere(Start, Target, Alpha)
```

P6 暂不加高度抛物线，让 `BP_ky_fireBall` 本身作为沿测地线滑行的特效 Actor。

### 5.3 朝向

每帧使用：

```text
Up = normalize(Position - PlanetCenter)
ArrowForward = normalize(NextPosition - Position)
SpellForward = project(NextPosition - Position, plane normal Up)
Rotation = MakeFromXZ(Forward, Up) * RelativeRotation
```

箭矢使用完整飞行速度方向作为 Forward，因此姿态会随重力抛物线的上升 / 下落发生俯仰变化。法术仍沿球面测地线滑行，Forward 投影到球面切平面。

`RelativeRotation` 用于在“指向飞行方向”的基础上校准箭矢模型或法术 Actor 的导入朝向。

---

## 6. 可调参数

参数暴露在 `APlanetTessellatedMesh` 的：

```text
PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation
```

| 字段 | 默认 | 用途 |
| --- | --- | --- |
| `P6ArrowProjectileMesh` | `/Game/Animations/Adventurers/Assets/arrow_bow` | 箭矢飞行 Mesh |
| `P6SpellProjectileActorClass` | `/Game/FXVarietyPack/Blueprints/BP_ky_fireBall` | 法术飞行 Actor |
| `P6ArrowAttachName` | `handslot_r` | 箭矢释放挂点 |
| `P6SpellAttachName` | `handslot_r` | 法术释放挂点 |
| `P6ArrowRelativeLocation` | `(0,0,0)` | 箭矢释放点偏移 |
| `P6ArrowRelativeRotation` | `(0,0,0)` | 箭矢朝向校准 |
| `P6ArrowUniformScale` | `1.0` | 箭矢缩放 |
| `P6ArrowTargetRelativeLocation` | `(0,0,0)` | 箭矢落点相对被吃 Actor 坐标系的 3D 偏移 |
| `P6SpellRelativeLocation` | `(0,0,0)` | 法术释放点偏移 |
| `P6SpellRelativeRotation` | `(0,0,0)` | 法术 Actor 朝向校准 |
| `P6SpellUniformScale` | `1.0` | 法术 Actor 缩放 |
| `P6ArrowReleaseDelaySeconds` | `0.18` | 弓兵攻击动画起播后多久释放箭矢 |
| `P6SpellReleaseDelaySeconds` | `0.18` | 主将攻击动画起播后多久释放法术 |
| `P6ArrowFlightSeconds` | `0.28` | 箭矢飞行时长 |
| `P6SpellFlightSeconds` | `0.35` | 法术飞行时长 |
| `P6ArrowArcHeightCM` | `350.0` | 箭矢重力抛物线最高额外高度；与 `P6ArrowFlightSeconds` 一起决定等效重力和初始法向速度 |

---

## 7. 时序建议

P3 仍以 `P3ArcherAttackToHitSeconds` / `P3CommanderAttackToHitSeconds` 作为“攻击起播到受击开始”的同步时刻。

P6 的视觉同步建议满足：

```text
P6ReleaseDelaySeconds + P6FlightSeconds ≈ P3AttackToHitSeconds
```

默认值：

```text
0.18 + 0.28 = 0.46
0.18 + 0.35 = 0.53
```

当前 P3 默认远程 AttackToHit 为 `0.35`，因此若希望 projectile 抵达严格早于受击帧，应在编辑器中调小释放延迟 / 飞行时长，或调大 P3 远程 AttackToHit。首版允许略晚到，优先保证有清晰飞行表现。

---

## 8. 已知换色信息（后续使用）

P6 暂不处理换色，但已知 `FXVarietyPack` 中 `BP_ky_fireBall` / `BP_ky_thunderBall` 相关信息如下：

1. `BP_ky_fireBall` 有 `LinearColor` 蓝图变量：
   ```text
   color = LinearColor(r=2.0, g=0.8, b=0.1, a=0.0)
   ```
2. `BP_ky_thunderBall` 继承自 `BP_ky_fireBall`，也有同名变量：
   ```text
   color = LinearColor(r=2.589149, g=2.7, b=0.632785, a=1.0)
   ```
3. 父类蓝图字节串中存在：
   ```text
   SetVectorParameter
   Conv_LinearColorToVector
   ParameterName
   ```
   说明它会把 `color` 作为向量参数推给粒子系统。
4. `BP_ky_fireBall` 有 `hitFx`：
   ```text
   /Game/FXVarietyPack/Particles/P_ky_explosion
   ```
5. `BP_ky_thunderBall` 覆盖 `hitFx` 为：
   ```text
   /Game/FXVarietyPack/Particles/P_ky_ThunderBallHit
   ```
6. `BP_ky_ThunderBallHit` 本身没有暴露同名 `color` 变量。

后续若做阵营换色，推荐在 projectile actor 创建法术 child actor 后，对 child actor 的 `color` 属性做反射写入或提供统一接口；命中特效若需要同步换色，需要额外改 hit 蓝图或制作带颜色参数的命中特效 Actor。

---

## 9. 验收清单

- 编译通过。
- 弓兵远程吃子时，攻击动画期间生成箭矢。
- 箭矢从 `P6ArrowAttachName` 附近释放。
- 箭矢按重力抛物线飞向被吃 Actor 当前坐标加 `P6ArrowTargetRelativeLocation` 的方向，落点与释放起点同高，到达后销毁。
- 主将远程吃子时，攻击动画期间生成 `BP_ky_fireBall` 法术 Actor。
- 法术从 `P6SpellAttachName` 附近释放。
- 法术沿球面测地线飞向被吃棋子，到达后销毁。
- P6 不改变 Gameplay 结算、行动日志、Undo 规则。
- P6 不影响步兵 / 骑兵近战 P3 表现。

---

## 10. 排错表

| 症状 | 根因 | 修复 |
| --- | --- | --- |
| 远程攻击没有箭矢 | `P6ArrowProjectileMesh` 为空且默认资源路径加载失败 | 确认 `/Game/Animations/Adventurers/Assets/arrow_bow` 存在，或手动指定 Mesh |
| 主将没有法术 | `P6SpellProjectileActorClass` 为空且默认 `BP_ky_fireBall` 不存在 / 未导入 | 确认 `FXVarietyPack` 内容存在，或手动指定 Actor Class |
| 箭矢从身体中心发射 | `P6ArrowAttachName` 在当前骨架上不存在 | 填写真实 Socket / Bone 名，或调 `P6ArrowRelativeLocation` |
| 法术从错误位置发射 | `P6SpellAttachName` 或相对位置未校准 | 填写真实手部挂点并调 `P6SpellRelativeLocation` |
| 箭矢朝向横着飞 | `arrow_bow` 导入朝向与逻辑 `+X Forward` 不一致 | 调 `P6ArrowRelativeRotation` |
| 法术到达和受击不同步 | `P6ReleaseDelaySeconds + P6FlightSeconds` 与 P3 AttackToHit 不匹配 | 调 P6 释放/飞行时长，或调 P3 远程 AttackToHit |
| Projectile 残留 | Actor 没到达或 manager 被提前清理 | `ClearPieces` 会销毁已登记 projectile；若自定义 actor 改了销毁逻辑，确认仍会结束生命周期 |

---

## 11. 与上下游关系

- 上游 P3：P6 复用 P3 CaptureEvent、攻击起播时刻和受击同步窗口。
- 上游 P5：P6 复用挂点思路，但 projectile 是临时 Actor，不跟随棋子长期存在。
- 下游 P7：可在 `ATerraPieceProjectileActor` 内补拖尾、命中特效、职业差异化、阵营色。
- Gameplay：无依赖回流，仍只负责规则结算。
