# TerraCivilization SimpleGameplay G12 装备掉落与棋子升变设计稿

> G12 在既有 G4/G5 捕获结算与 P1-P7 棋子表现基础上引入“弓”和“活马”两类可拾取掉落物，以及 `ArcherCavalry` 弓骑兵。主玩法设计稿只保留摘要，本稿定义规则、C++ 状态、表现资产和验收。

---

## 0. 目标与范围

G12 让兵种死亡留下可争夺的能力来源：

```text
弓兵死亡     -> Cell 留弓     -> 步兵拾取为弓兵 / 骑兵拾取为弓骑兵
骑兵死亡     -> Cell 留活马   -> 步兵拾取为骑兵 / 弓兵拾取为弓骑兵
弓骑兵死亡   -> Cell 同时留弓和活马
```

本期实现：Gameplay 掉落与升变、掉落模型同步、弓骑兵规则与基础双骨骼表现接口。

本期不实现：拾取专属特效、掉落物碰撞、骑乘上马过渡、弓骑兵专属射击蒙太奇资产本体。专属射击资产的制作和挂载接口在本文固定，缺失时安全回退到既有弓兵射击动画。

---

## 1. 规则

### 1.1 掉落与占格

| 被吃兵种 | 掉落 | 是否占用 Cell |
| --- | --- | --- |
| `Infantry` / `Commander` | 无 | 否 |
| `Archer` | 弓 | 否 |
| `Cavalry` | 活马 | 否 |
| `ArcherCavalry` | 弓 + 活马 | 否 |

- 掉落记录在 `EquipmentDropsByCellId`，与 `CellToPieceId` 分离；同一个 Cell 可以同时有弓和马，但只能有一枚棋子。
- 被吃子先按照既有规则离开棋盘，再把其对应掉落写入被吃 Cell。多次掉落合并为该 Cell 的布尔状态。
- 掉落物不会阻止普通移动、跳跃落点、连跳、二吃一或弓兵射线；它们也不是棋子，不能作为跳跃中间支点或攻击目标。

### 1.2 拾取与升变

拾取只在棋子完成一次落点后发生：普通移动的目标 Cell，或连跳路径的每一个落点。拾取立即生效，后续连跳、吃子预览与最终攻击均使用升变后的兵种。

| 进入者 | Cell 有弓 | Cell 有马 | 结果 |
| --- | --- | --- | --- |
| 步兵 | 是 | 否 | 拾弓，升为弓兵 |
| 步兵 | 否 | 是 | 骑马，升为骑兵 |
| 步兵 | 是 | 是 | 同时拾取，升为弓骑兵 |
| 骑兵 | 是 | 任意 | 拾弓，升为弓骑兵；马保持原状态 |
| 弓兵 | 任意 | 是 | 骑马，升为弓骑兵；弓保持原状态 |
| 弓骑兵 / 主将 / 中立单位 | 任意 | 任意 | 不拾取，掉落保持 |

若进入者不需要某件装备，则该件装备不消失。例如骑兵经过只有马的 Cell，马仍留在原地；弓兵经过只有弓的 Cell，弓仍留在原地。

### 1.3 弓骑兵能力

`ArcherCavalry` 同时满足“骑兵能力”和“弓兵能力”：

- 可普通移动、标准跳跃、连续跳跃与骑兵的第二落点远跳。
- 可进入山脉，也可越过山脉；原生骑兵的山脉封锁只保留给 `Cavalry`。
- 可作为弓兵发起既有 `A' A - B` 远程吃子；站在山脉时最大射程为 4，继承山地弓兵的延长射程。
- 森林仍保护目标并使远程扫描继续穿过该 Cell，不会被弓骑兵忽略。
- 作为 P3/P6 远程攻击者时使用弓兵类别；作为移动与跳跃者时使用骑兵类别。

---

## 2. Gameplay 数据与时序

### 2.1 新状态

```cpp
enum class ETerraGameplayPieceType : uint8
{
    Commander,
    Infantry,
    Cavalry,
    Archer,
    ArcherCavalry,
};

struct FTerraGameplayEquipmentDropState
{
    int32 CellId = INDEX_NONE;
    bool bHasBow = false;
    bool bHasHorse = false;
};
```

容器新增：

```cpp
TMap<int32, FTerraGameplayEquipmentDropState> EquipmentDropsByCellId;
```

`FInteractionUndoSnapshot` 必须复制这张表，保证 G9 可以撤销普通移动、跳跃和拾取造成的全部变化。

### 2.2 捕获与落点顺序

```text
棋子移动 / 跳跃到目标 Cell
  -> TryCollectEquipmentAtCell
  -> 以升变后的兵种重建当前落点的吃子预览
  -> 玩家结束本回合
  -> ResolvePendingCaptures
  -> 被吃弓兵/骑兵/弓骑兵写入装备掉落
  -> G11 阵营转归、胜负判定、回合推进
```

普通移动即使移动前没有吃子预览，也必须在拾取后调用 `LockPendingCapturesForActionTarget_`，因为步兵在落点拾弓后可能立即形成新的弓兵远程吃子。

### 2.3 AI、日志与高亮

- 升变只修改 `PieceType`，所有现有 `CollectCurrentFactionSelectablePieceIds`、合法行动、局部战术卡和战略快照自然读取新能力。
- `ui_begin_turn_review` 继续只返回回合状态；其后续选择/查询接口会将升变后的棋子列为当前阵营原生可选棋子。
- Idle 高亮读取永久阵营归属，不因升变改变规则；升变棋子仍高亮为其所属阵营。
- 运行日志输出 `[Gameplay][G12] EquipmentCollected`，含棋子、Cell、升变前后兵种。掉落 Actor 由同步快照驱动，不额外写入 G7 行动日志格式。

---

## 3. 掉落表现

### 3.1 Actor 结构

新增 `ATerraEquipmentDropActor`，每个含有掉落的 Cell 对应一个 Actor：

```text
ATerraEquipmentDropActor
  RootScene
  BowMesh       (StaticMeshComponent)
  HorseMesh     (SkeletalMeshComponent, 只播放 Idle)
```

若同 Cell 同时有弓和马，两个组件同时显示。Actor 使用 Cell 的球面位置与朝向，掉落不会创建碰撞，不接收点击。

`UTerraPiecePresentationManager` 维护 `CellId -> DropActor` 映射；每次 `SyncPieces` 接收完整掉落快照并增量生成、更新或销毁 Actor。棋子移动经过并拾取装备后，下一次同步立即移除相应组件或整 Actor。

### 3.2 默认资产和 Details 挂载

在 `APlanetTessellatedMesh -> PlanetPiecePresentationComponent` 的分类：

```text
SimpleGameplay P12 Equipment Drop Presentation
```

新增字段与默认预期资产：

| 字段 | 默认预期资产 | 用途 |
| --- | --- | --- |
| `P12DroppedBowMesh` | `/Game/Animations/Adventurers/Assets/bow.bow` | 地面弓模型 |
| `P12DroppedHorseMesh` | `/Game/Animations/Horse/Horse.Horse` | 被吃骑兵留下的活马模型 |
| `P12DroppedHorseIdleAnimation` | `/Game/Animations/Horse/HorseIdle.HorseIdle` | 活马循环 Idle |
| `P12DroppedBowRelativeLocation/Rotation/Scale` | `Zero/Zero/0.01`，PIE 调整 | 弓相对 Cell 中心变换 |
| `P12DroppedHorseRelativeLocation/Rotation/Scale` | `Zero/(0,-90,0)/3.0`，PIE 调整 | 活马相对 Cell 中心变换 |

默认值直接由 `BuildVisualConfig()` 回退加载上述资源；关卡实例可在 Details 中覆盖。

---

## 4. 弓骑兵棋子表现

### 4.1 基础表现

弓骑兵沿用 P4 的 Mounted 结构：`HorseMesh + RiderAnchor + RiderMesh`。骑手模型和调色板优先使用现有骑兵 Rider 配置，武器主组件改为弓；不同时显示骑兵斧。

`IsMountedCavalry_()` 对 `Cavalry` 和 `ArcherCavalry` 都返回真。弓骑兵移动/跳跃沿用马移动和跳跃动画；Idle 沿用骑手 Sitting + 马 Idle。

### 4.2 弓骑兵射击蒙太奇资产

当前项目仅有骑兵突刺上半身蒙太奇接口，尚无弓骑兵射击蒙太奇。G12 预留：

| 字段 | 挂载位置 | 默认预期资产 | 当前回退 |
| --- | --- | --- | --- |
| `P12ArcherCavalryUpperBodyAttackMontage` | `SimpleGameplay P12 Equipment Drop Presentation` | `/Game/PiecePresentation/Animations/AM_ArcherCavalry_ShootUpperBody.AM_ArcherCavalry_ShootUpperBody` | `nullptr`；使用既有 `P3ArcherRangedAttackAnimation` |
| `P12ArcherCavalryAttackToHitSeconds` | 同上 | `0.35` | 使用 `P3ArcherAttackToHitSeconds` |

制作方法，遵循 P4.4 的 Rider 上半身分层方案：

1. 复制与 `P4RiderAnimInstanceClass` 同骨架的弓兵射击动画，若源动画使用不同骨架，先通过 IK Retargeter 目标到 Rider Skeleton。
2. 新建 Animation Montage：`AM_ArcherCavalry_ShootUpperBody`，放到 `/Game/PiecePresentation/Animations/`。
3. 在 Montage 中建立或复用 `UpperBody` Slot；只保留从胸椎以上的射击动作。下半身必须继续保持 `UTerraMountedRiderAnimInstance` 的 Sitting 输出，避免骑手站起或穿模。
4. 把射击 Notify / 播放长度与 `P6ArrowReleaseDelaySeconds` 对齐，确保箭矢生成时落在拉弓释放帧。先在 PIE 使用 `P12ArcherCavalryAttackToHitSeconds=0.35`，再根据实际动画调节。
5. 在关卡 Actor 的 `PlanetPiecePresentationComponent` Details 中给 `P12ArcherCavalryUpperBodyAttackMontage` 挂载新 Montage；`BuildVisualConfig()` 传入 `FTerraPieceVisualConfig`，`ATerraPieceActor` 的 Mounted 弓骑兵攻击优先播放它。

### 4.3 活马掉落与骑兵死亡修正

P4.3 的当前骑兵死亡会让 `HorseMesh` 播放 `P4HorseDeathAnimation`，随后整个棋子 Actor 淡出。这与 G12 的“被吃后留下活马”不一致。

G12 的表现顺序固定为：

```text
骑兵被吃
  -> 原骑兵 Actor 的 HorseMesh 不播放死亡动画，保持/切回 HorseIdle
  -> RiderMesh 播放既有死亡动画并随棋子 Actor 淡出
  -> 同步掉落快照后，在原 Cell 生成独立 DropActor.HorseMesh
  -> DropActor.HorseMesh 循环 P12DroppedHorseIdleAnimation
```

由于原棋子 Actor 仍会在淡出后销毁，掉落马必须属于独立 `ATerraEquipmentDropActor`，不能复用死亡骑兵 Actor 的 HorseMesh。若需要更平滑的无缝交接，可后续增加“死亡 Actor 马隐藏延迟”和“掉落马淡入”，但不属于 G12 验收前提。

---

## 5. 验收

- 弓兵被吃后，其 Cell 出现弓；骑兵被吃后，其 Cell 出现循环 Idle 的活马；弓骑兵死亡同时出现两者。
- 掉落 Cell 仍可被普通移动和跳跃落入，掉落不会成为跳跃支点或捕获目标。
- 步兵、骑兵、弓兵按表格拾取并升变；连跳经过带装备的 Cell 时立刻升变，之后的继续跳跃与吃子使用新能力。
- 弓骑兵能进入、跨越山脉、使用远跳和远射；山脉延长射程，森林仍阻挡远程攻击。
- 非需要兵种经过掉落 Cell 时，掉落保持；G9 撤销能恢复棋子兵种和掉落状态。
- 未挂载弓骑兵专属 Montage 时，远程攻击能回退为弓兵现有表现；挂载后，Rider 上半身播放 `AM_ArcherCavalry_ShootUpperBody`，下半身继续 Sitting。
- 骑兵被吃时不再把“死亡马”当作遗留马；最终留在 Cell 的马由 `P12DroppedHorseIdleAnimation` 驱动。

