# TerraCivilization SimpleGameplay 棋子动画表现设计稿

> 本稿用于在当前 G6 胜负闭环已经跑通的基础上，为 SimpleGameplay 增加一套**独立于地形渲染模块**的棋子动画表现方案。
>
> 目标不是把规则逻辑塞进 `APlanetTessellatedMesh`，而是在现有 `Gameplay` 规则容器与 `TerraCivilization` 输入 / 地形渲染链路之间，新增一个专门负责**棋子模型、骨骼、动画、表现状态机与表现事件**的模块。
>
> 本稿同时参考当前项目已经存在的资产目录：
>
> - `Content/Animations/Adventurers/...`
> - `Content/Animations/Horse/...`
>
> 因此文中的动画建议会优先贴合这些现成资产，而不是假设项目已经有完整骑兵、蒙太奇、BlendSpace 或 Control Rig 资源。

---

## 1. 目标

本阶段要解决的不是玩法规则，而是下面这条表现链路：

```text
Gameplay 规则状态
    -> 表现层事件 / 状态快照
    -> 棋子表现模块
    -> SkeletalMesh / AnimBP / 特效 / 朝向 / 位移插值
    -> 玩家看到“这枚棋子正在移动、跳跃、攻击、死亡、胜利待机”
```

核心目标：

1. **动画表现独立为新模块**
   - 不把棋子动画逻辑继续堆进 `Gameplay` 模块。
   - 不把棋子表现实现塞进当前地形 HISM 渲染逻辑。
   - 地形仍负责 Cell、瓦片、拾取、高亮；棋子表现模块只负责棋子自己。

2. **表现驱动来自 Gameplay，而不是反过来**
   - `FTerraGameplayContainer` 仍然是真实规则来源。
   - 动画层不参与合法性判定、不维护第二套真规则。
   - 表现层只消费“棋子从哪到哪、进行了什么动作、谁被移除、当前轮到谁”等结果。

3. **优先复用现有 Adventurers / Horse 资产**
   - 人形棋子优先用 Adventurers 资源。
   - 骑兵初版允许使用“人 + 马”双骨骼拼装，而不是等待完整骑兵专用动画。
   - 初版先保证**能看、能分辨、能闭环**，再追求更细腻的骑乘同步。

4. **阶段化落地**
   - 先做最小可用表现闭环。
   - 再补动作分类、受击 / 死亡、远程攻击、骑兵双骨骼、终局表现。

---

## 2. 当前基础与模块边界

### 2.1 已有规则层

当前项目已经有：

- `Source/Gameplay/Public/TerraGameplayTypes.h`
- `Source/Gameplay/Public/TerraGameplayContainer.h`
- `Source/Gameplay/Private/TerraGameplayContainer.cpp`

其中 `FTerraGameplayContainer` 已负责：

- 棋子初始化
- 当前回合阵营
- 普通移动 / 跳跃 / 连跳
- 普通吃子 / 弓兵远程吃子
- 主将失败与对局结束

这意味着动画层**不应该重复实现一份移动 / 吃子 / 胜负规则**。

### 2.2 已有场景层

当前 `APlanetTessellatedMesh` 已负责：

- 地形 mesh 与 HISM 瓦片渲染
- HISM 命中拾取
- Gameplay 容器初始化
- Cell 高亮与回合辅助镜头
- G1 调试球绘制

因此它现在更像：

```text
地形承载 actor + 交互桥 + 当前 Gameplay 宿主
```

### 2.3 本稿拍板的模块边界

动画表现模块应当独立，不与地形 HISM 模块混在一起。

推荐分层：

| 层 | 职责 |
| --- | --- |
| `Gameplay` 模块 | 规则真值、回合、棋子状态、胜负 |
| `PiecePresentation` 模块 | 棋子 actor / component、动画状态、位移插值、表现事件 |
| `TerraCivilization` 现有地形层 | 球面地形、HISM 瓦片、鼠标拾取、Cell 高亮、相机 |

结论：

- **不建议**把棋子动画表现并进当前地形 HISM 渲染模块。
- **也不建议**让 `Gameplay` 模块直接依赖 `USkeletalMeshComponent` 或 `AnimInstance`。
- 应新增一个独立模块，例如：

```text
Source/PiecePresentation/
```

初版由 `TerraCivilization` 依赖它，和当前 `Gameplay` 并列。

---

## 3. 现有资产盘点与可用性结论

以下结论基于当前可见资产目录。

### 3.1 人形角色资产

当前可见：

```text
Content/Animations/Adventurers/Characters/
    Barbarian...
    Knight...
    Mage...
    Ranger...
    Rogue...

Content/Animations/Adventurers/Animations/
    Rig_Medium_GeneralIdle_A/B
    Rig_Medium_GeneralHit_A/B
    Rig_Medium_GeneralDeath_A/B
    Rig_Medium_MovementBasicWalking_A/B/C
    Rig_Medium_MovementBasicRunning_A/B
    Rig_Medium_MovementBasicJump_Start
    Rig_Medium_MovementBasicJump_Idle
    Rig_Medium_MovementBasicJump_Land
    Rig_Medium_MovementBasicJump_Full_Short/Long
```

推断：

- 当前已经具备一套**中体型通用人形骨架动作**。
- 这些动作很适合作为：
  - 步兵移动
  - 弓兵移动
  - 法师移动
  - 枪兵移动
  - 通用受击 / 死亡
- 目前**没有看到明显按职业拆好的专用攻击动画命名**。
- 也**没有看到现成 AnimBP / BlendSpace / Montage 资产**。

因此初版建议：

- 先把 Adventurers 角色视为**共享人形骨架 + 通用 locomotion / jump / hit / death 动作包**。
- 若职业专属攻击动画在别处还没导入，初版先允许用：
  - 通用 attack 占位
  - 或“位移 + 朝向 + 特效 + 受击 / 死亡”撑起表现闭环。

### 3.2 马匹资产

当前可见：

```text
Content/Animations/Horse/
    Horse_Skeleton
    Horse.uasset
    HorseWalk
    HorseGallop
    HorseGallop_Jump
    HorseIdle
    HorseIdle_2
    HorseJump_toIdle
    HorseAttack_Kick
    HorseAttack_Headbutt
    HorseDeath
```

结论：

- 马的独立骨架与基础 locomotion 很完整。
- 已经足够支持：
  - 待机
  - 行走 / 奔跑
  - 跳跃
  - 死亡
- 马也有攻击动画，但这并不等同于“骑兵持枪冲刺 / 骑兵挥砍”。

### 3.3 武器与外观资产

当前可见：

```text
Content/Animations/Adventurers/Assets/
    sword_1handed
    sword_2handed
    staff
    wand
    bow / bow_withString
    quiver
    crossbow...
    shield...
    dagger...
    axe...
```

这意味着：

- 职业区分可以先通过**模型外形 + 武器挂件**完成。
- 即使攻击动作暂时不全，玩家也仍能一眼看出：
  - 战士
  - 枪兵
  - 弓兵
  - 法师
  - 骑兵

### 3.4 对本项目最重要的现实结论

1. **人形资源足够支持“落地、移动、跳跃、受击、死亡”的首版闭环。**
2. **马资源足够支持“独立马体 locomotion”。**
3. **最大缺口是“骑手与马一体化的专用骑兵动画”。**
4. **因此骑兵初版最合理的方向不是等完整骑兵动画，而是做“双 SkeletalMesh 拼装方案”。**

---

## 4. 动画表现模块的职责

新模块建议命名：

```text
PiecePresentation
```

建议职责只做下面四件事。

### 4.1 棋子可视实例管理

负责：

- 为每个 `PieceId` 创建对应表现对象。
- 把逻辑棋子映射到：
  - `AActor` 或 `USceneComponent`
  - `USkeletalMeshComponent`
  - 武器挂件
  - 骑兵的附加马体

它维护的是：

```text
PieceId -> PresentationInstance
```

而不是：

```text
CellId -> 规则状态
```

### 4.2 表现状态机

负责决定一个棋子当前处于什么表现态：

```text
Idle
Move
JumpStart
JumpLoop / Air
JumpLand
AttackMelee
AttackRanged
HitReact
Death
VictoryIdle
DefeatDisappear
```

注意：

- 这是**表现状态**，不是规则状态。
- 它由 Gameplay 结果驱动，但不反向改规则。

### 4.3 空间对齐与插值

负责：

- 计算棋子站在球面某个 Cell 上时的世界位置
- 让棋子法线朝外贴合球面
- 根据前进方向旋转角色朝向
- 在 Cell A 到 Cell B 之间插值移动
- 跳跃时加一条局部抛物线 / 高度曲线

### 4.4 表现事件消费

负责消费如下事件：

- 棋子生成
- 棋子开始移动
- 棋子完成移动
- 棋子开始跳跃
- 棋子结束跳跃
- 棋子攻击结算
- 棋子被吃 / 被移除
- 当前阵营切换
- 对局胜利 / 失败

这部分是模块与 Gameplay 的真正接口核心。

---

## 5. 推荐的驱动方式

### 5.1 不用“动画直接读底层规则细节”

不建议让 AnimBP 直接去读：

- `PendingCapturePieceIds`
- `InteractionPhase` 的所有内部细枝末节
- `ActionTargetCellIdToCaptureCellIds`

原因：

- 会把表现层强耦合到规则容器实现细节。
- 以后规则微调时，动画层会跟着脆弱。

### 5.2 用“表现事件 + 表现快照”驱动

推荐接口分两类：

1. **稳定快照**
   - 当前每个活棋子在哪个 Cell
   - 是什么兵种
   - 属于哪个阵营
   - 当前轮到哪个阵营
   - 对局是否结束

2. **一次性事件**
   - 某棋子从 A 移到 B
   - 某棋子从 A 跳到 B
   - 某棋子本次行动形成攻击
   - 某棋子被移除
   - 某阵营失败
   - 某阵营获胜

推荐理解为：

```text
快照负责“现在是什么”
事件负责“刚刚发生了什么”
```

### 5.3 首版接口形态建议

初版不要求一上来就做复杂委托系统。

可以先在 `Gameplay` 模块中新增一层轻量“表现事件结构”，例如：

```cpp
FTerraPiecePresentationDelta
FTerraPieceActionEvent
```

由 `FTerraGameplayContainer` 在一次点击处理后输出给表现层。

推荐事件粒度：

| 事件 | 含义 |
| --- | --- |
| `Spawn` | 初始化或重建时生成棋子表现 |
| `Relocate` | 棋子瞬时逻辑位置改变，需要表现层播放位移 |
| `Jump` | 本次位移是跳跃 |
| `Attack` | 某棋子刚发动了近战或远程动作 |
| `Removed` | 某棋子被吃，播放死亡 / 消散 |
| `FactionTurnStarted` | 当前阵营切换，可播待机强调 |
| `MatchEnded` | 终局进入，胜者 / 败者进入特殊态 |

### 5.4 为什么这是本项目最稳的方式

因为当前项目已经有一条类似桥接链：

```text
GameplayContainer -> DirtyCellIds -> HISM Highlight 刷新
```

棋子动画完全可以沿用这个思路：

```text
GameplayContainer -> PiecePresentationEvents -> Piece actors / AnimBP 刷新
```

这样不会破坏现有模块分工。

---

## 6. 模块结构建议

建议新增 Unreal 模块：

```text
Source/PiecePresentation/
    Public/
    Private/
```

### 6.1 推荐核心类型

| 类型 | 职责 |
| --- | --- |
| `FTerraPieceVisualConfig` | 兵种到模型 / 动画配置的静态描述 |
| `UTerraPiecePresentationSubsystem` 或管理器类 | 统一管理全部棋子表现实例 |
| `ATerraPieceActor` | 单个棋子的可视 actor |
| `UTerraPieceAnimInstance` | 人形棋子 AnimBP 的 C++ 基类 |
| `UTerraHorseAnimInstance` | 马的 AnimBP 的 C++ 基类 |
| `FTerraPiecePresentationEvent` | Gameplay 发来的表现事件 |

### 6.2 单个棋子 actor 的推荐组成

步兵 / 弓兵 / 法师 / 枪兵：

```text
ATerraPieceActor
  RootScene
  HumanMesh (USkeletalMeshComponent)
  Optional Weapon Static/Skeletal Mesh Components
```

骑兵：

```text
ATerraPieceActor
  RootScene
  HorseMesh
  RiderAnchor
  RiderMesh
  Optional Weapon Components
```

### 6.3 为什么骑兵建议仍是一个 actor

因为逻辑上骑兵仍然是**一枚棋子**。

如果拆成两个独立 actor：

- 同步成本高
- 选择 / 高亮 / 销毁要双份处理
- 未来镜头跟随和特效挂点更乱

所以推荐：

- **一个棋子 actor**
- 内含**两套 SkeletalMeshComponent**

---

## 7. 资产映射建议

### 7.1 兵种到现有 Adventurers 角色的建议映射

结合当前资源命名，首版推荐：

| 玩法兵种 | 推荐角色资产 | 原因 |
| --- | --- | --- |
| 主将 | `Mage` | 本次拍板：主将表现使用 Mage |
| 步兵 | `Knight` + `sword_1handed` + `shield_*` | 近战步兵辨识度最稳定 |
| 弓兵 | `Ranger` + `bow_withString` | 现有资源直接匹配 |
| 法师 | `Mage` + `staff` / `wand` | 若后续玩法重新启用法师单位，可沿用现有资源 |
| 枪兵 | `Knight` + 长柄武器替换 | 当前没有现成长枪资产时，先保留为扩展位 |
| 骑兵 | `Knight` + `Horse` + `axe_1handed` / `axe_2handed` | 本次拍板：骑兵使用 Knight 骑马并配斧类武器 |

### 7.2 主将的实现建议

主将表现本次拍板直接使用 `Mage`。

理由：

- 规则上主将不能移动。
- 但表现上可以直接使用 `Mage` 角色站桩待机，作为“主将”而不是“可行动法师”。
- 需要通过外观、UI 标识或后续特效明确它是主将，不是普通法师兵种。




推荐：

- 主将使用 `Mage` 模型与通用待机。
- 通过独立配色、头顶标识、底座光圈或专属特效区分于普通作战单位。

### 7.3 武器挂载建议

现有 `Assets/` 中武器资源很多，建议不要把“职业差异”全压在角色本体造型上。

推荐通过 socket / 挂点绑定：

- `sword_1handed`（步兵）
- `staff`（主将 / 法师）
- `wand`（主将 / 法师）
- `bow_withString`（弓兵）
- `shield_*`（步兵）

这样能用同一人形骨架快速组出不同职业。

---

## 8. 动画蓝图策略

### 8.1 人形棋子

推荐一套共享 Humanoid AnimBP 基类，按参数驱动。

最小参数：

| 参数 | 作用 |
| --- | --- |
| `MoveSpeed` | 0=Idle, >0=Walk/Run |
| `bIsJumping` | 切到 Jump 状态 |
| `bIsAttacking` | 播一次性攻击动作 |
| `bIsHitReact` | 受击动作 |
| `bIsDead` | 死亡动作 |
| `FacingMode` | 是否强制朝向目标 |
| `PieceArchetype` | 近战 / 远程 / 法师 |

### 8.2 马

马建议单独一套 AnimBP：

| 参数 | 作用 |
| --- | --- |
| `MoveSpeed` | Idle / Walk / Gallop |
| `bIsJumping` | Jump |
| `bIsDead` | Death |

### 8.3 初版不强求的资产

当前没看到这些现成资产，因此初版不应把它们当成前提：

- BlendSpace
- Montage
- AimOffset
- Control Rig
- IK Rig / Retargeter

推荐首版：

- 状态机 + 少量 Sequence 直接切换
- 真正需要攻击时再补 Montage

---

## 9. 棋子在球面上的空间表现

### 9.1 站位来源

站位真值仍来自 `CellId`。

当前 `APlanetTessellatedMesh` 已经有：

```text
GetCellSurfaceWorldPosition_(CellId, RadiusOffsetCM, OutWorldPosition)
```

动画模块不必重新发明球面定位公式。

推荐做法：

- 由宿主层提供“某 CellId 的球面世界位置和外法线方向”。
- 棋子表现模块只负责把 actor 放上去。

### 9.2 姿态对齐

每个棋子都需要：

- `Up` 对齐球面外法线
- `Forward` 按表现动作决定，不再每次快照同步都重置为 Cell 默认方向

因此角色根节点推荐始终做两步：

```text
1. Up 对齐 Cell.UnitCenter
2. 再绕 Up 轴旋转到面朝目标方向
```

方向规则统一如下：

1. 初始生成时，除主将外，所有模型都面朝“背向本阵营主将”的方向；主将方向任意，使用稳定默认方向即可。
2. 行走、跳跃或攻击时，模型面向动作方向；当前 P2 的跳跃按移动方向处理。
3. 受击时，模型面朝受击来源方向。
4. 动作停止后，模型方向保持在动作结束时的方向；后续静态快照同步不改变方向。
5. 只有发生新的移动、攻击、受击等动作时才更新方向。
6. 模型转向使用 `slerp`，避免方向突变。

资源导入朝向修正单独处理，不混入棋子逻辑方向：

- `ATerraPieceActor` 的逻辑约定仍是本地 `+X = Forward`、本地 `+Z = Up`。
- 当前 Adventurers 人物资源存在“逻辑朝前实际朝右”的模型参考系偏差。
- 该偏差通过 `APlanetTessellatedMesh::P1MeshRelativeRotation` 修正，默认值为 `Yaw=90`。
- 调试时只调整 `P1MeshRelativeRotation`，不要改移动、攻击、受击等逻辑朝向计算。
- 若未来不同兵种或资源包朝向不一致，再把该字段升级为兵种级或资产级配置。

### 9.3 移动表现

普通移动：

- 使用 Cell A 到 Cell B 的球面弧线插值
- 角色持续朝向前进方向
- 人形可播 Walk / Run
- 马匹可播 Walk / Gallop

### 9.4 跳跃表现

跳跃不是直线瞬移。

建议：

- 位置底层仍沿球面弧线走
- 额外叠加一条离球面法线方向抬起的高度曲线
- 起跳 / 落地各留少量缓冲时间

这样会比简单 `Lerp` 更像“从一个 Cell 跃到另一个 Cell”。

---

## 10. 骑兵方案

这是本稿最关键的专项。

### 10.1 不等专用骑兵动画，先做双骨骼拼装

当前没有看到“骑手骑马一体”的专用骑兵动作时，推荐方案是：

```text
HorseMesh 播马自己的 locomotion
RiderMesh 作为子组件挂在 Horse 上
RiderMesh 播一个简化的人形骑乘 / 战斗姿态
```

### 10.2 首版骑兵分层

推荐拆成三层：

1. **马负责下半身和整体位移感**
   - Idle
   - Walk
   - Gallop
   - Jump
   - Death

2. **骑手负责上半身职业辨识**
   - 持枪 / 持剑 / 持盾
   - 攻击挥动
   - 受击

3. **棋子 actor 负责整体移动轨迹**
   - 真正从 Cell A 到 Cell B 的空间插值由整个 actor 完成

### 10.3 骑手没有骑乘动画怎么办

如果没有 seated rider 动画，初版可按下面优先级处理：

1. **最稳方案**：制作一个简单的骑乘静态姿势作为基准待机
   - 可来自 DCC 手工摆 pose 后导回
   - 或在 UE 中用一次性骨骼调整烘成 Pose Asset / 单帧姿态

2. **次稳方案**：用现有人形 Idle 做轻量修正
   - 把 RiderMesh 缩放 / 旋转 / 抬腿近似放到马背上
   - 允许略 stylized，但要避免穿模太严重

3. **攻击时只做上半身动作**
   - 骑手上半身做枪刺 / 挥砍
   - 下半身尽量保持坐姿稳定

也就是说：

- **不要强行让骑手完整跑步动画和马的奔跑一起播。**
- 否则会非常假，而且穿模概率高。

### 10.4 骑手挂点建议

建议在 Horse skeleton 上建立或查找：

```text
Saddle / RiderRoot socket
```

若当前资产里没有现成 saddle socket，首版可在 `HorseMesh` 上手工创建。

骑手挂载方式：

```text
RiderMesh AttachToComponent(HorseMesh, SaddleSocket)
```

### 10.5 骑兵攻击建议

如果缺完整 mounted attack 动画，推荐：

- 骑手播短促上半身枪刺 / 挥砍动作
- 马继续当前 locomotion，不强切攻击停顿

这比“马突然播 Headbutt / Kick，当作骑兵攻击”更合理。

马的 `HorseAttack_Kick` / `HorseAttack_Headbutt` 更适合：

- 野兽单位
- 特殊演出
- 或非常临时的 debug 占位

不建议作为骑兵常规攻击主动作。

### 10.6 骑兵首版验收标准要放低但明确

初版骑兵不要求：

- 骑手腿部与马鞍完全严丝合缝
- 双方骨骼 100% 节拍同步
- 真正 AAA 级 mounted combat

初版只要求：

- 一眼看出这是骑兵
- 移动时马在动，人稳定骑在马背上
- 攻击时骑手有上半身动作
- 跳跃 / 死亡时整体不出大破绽

---

## 11. 表现事件与动作映射建议

### 11.1 普通移动

Gameplay 事件：

```text
Relocate(FromCell, ToCell, MoveType=Ordinary)
```

表现：

- 播 walk / run
- 沿球面短弧移动
- 移动中通过 `slerp` 转向行走方向
- 到达后回 idle，并保持移动结束时的朝向

### 11.2 跳跃 / 连跳

Gameplay 事件：

```text
Relocate(FromCell, ToCell, MoveType=Jump)
```

表现：

- 播 jump start
- 空中移动
- 落地 blend 到 idle 或继续下一跳
- 若连跳继续，保持动作链不断开
- 朝向规则与移动一致，面向本段跳跃方向，转向使用 `slerp`

### 11.3 近战吃子

Gameplay 事件：

```text
Attack(AttackerPieceId, TargetPieceIds, AttackType=Melee)
Removed(TargetPieceId)
```

表现：

- 攻击者朝向最近目标
- 播一次近战攻击
- 目标播 hit / death
- 被吃棋子延迟一小段再消失

### 11.4 弓兵远程吃子

Gameplay 事件：

```text
Attack(AttackerPieceId, TargetPieceIds, AttackType=Ranged)
Removed(TargetPieceId)
```

表现：

- 弓兵转向目标方向
- 播射击 / 施法 / 发射动作
- 可选生成简化投射物或轨迹线
- 目标播受击 / 死亡

### 11.5 阵营失败与胜利

Gameplay 事件：

```text
FactionEliminated
MatchEnded
```

表现：

- 胜方切到 VictoryIdle 或强调待机
- 败方剩余棋子可：
  - 直接消散
  - 或统一下沉 / 淡出
- 初版不需要复杂过场

---

## 12. 对现有源码的接入建议

### 12.1 不改动的原则

以下系统不应被动画模块污染：

- `FTerraGameplayContainer` 的规则判定主体
- HISM 地形实例逻辑
- `UCellHighlightComponent` 的高亮 LUT 逻辑

### 12.2 当前最自然的宿主接入点

当前最自然的接入点仍然是：

```text
APlanetTessellatedMesh
```

但它只作为**宿主与桥接者**，不作为“动画逻辑本体”。

也就是：

- `APlanetTessellatedMesh` 继续拥有 `GameplayContainer`
- 再新增一个 `PiecePresentationManager`
- 每次 Gameplay 状态变化后，把事件 / 快照推给它

### 12.3 为什么不直接把棋子做成 HISM

因为 HISM 适合：

- 静态瓦片
- 石头、树木、装饰

但不适合高频播放：

- 骨骼动画
- 每单位不同状态机
- 受击 / 死亡 / 武器挂点
- 骑兵双骨骼

因此棋子表现必须走独立 actor / skeletal component 路线。

---

## 13. 资源组织建议

推荐在 `Content` 下新增独立目录，而不是继续全部放在 `Animations` 根下混着。

建议：

```text
Content/PiecePresentation/
    Characters/
    Horses/
    Blueprints/
    AnimBP/
    Data/
    VFX/
```

其中：

| 目录 | 用途 |
| --- | --- |
| `Characters/` | 复制或整理可正式使用的人形 skeletal mesh |
| `Horses/` | 骑兵马体相关资源 |
| `Blueprints/` | `BP_TerraPieceActor_*` |
| `AnimBP/` | `ABP_TerraHumanoid`, `ABP_TerraHorse` |
| `Data/` | 兵种到资源映射 DataAsset |
| `VFX/` | 箭矢、法术、消散、胜利特效 |

原 `Content/Animations/...` 可作为原始导入区保留。

---

## 14. 分阶段目标

### P1：棋子真实模型替换 G1 调试球

详细阶段设计见：[P1PieceAnimationPresentationDesign.md](P1PieceAnimationPresentationDesign.md)。

目标：

- 用真实棋子 actor 替换当前 `DrawDebugSphere`
- 先不要求完整动画
- 只要能正确站在各 Cell 上、朝向球面外法线即可
- 采用增量同步框架维护 `PieceId -> ATerraPieceActor`
- 在 `APlanetTessellatedMesh` 上暴露主将 / 步兵 / 骑兵 / 弓兵的 SkeletalMesh 资产槽

验收：

- 12 阵营棋子都能正确生成
- 每个兵种能从外观上区分
- 主将单独表现
- 行动或 Undo 后复用既有 Actor 更新位置，不整盘销毁重建

### P2：基础 locomotion 与跳跃

详细阶段设计见：[P2PieceAnimationPresentationDesign.md](P2PieceAnimationPresentationDesign.md)。

目标：

- 普通移动播放 walk / run
- 跳跃 / 连跳播放 jump
- 棋子移动沿球面插值
- 本阶段不做 HISM 射线脚底贴地，只验收移动、跳跃和连跳的视觉效果

验收：

- 普通移动不再瞬移
- 跳跃有起落感
- 连跳不断态

### P2.5：HISM 碰撞高度修正

详细阶段设计见：[P2_5PieceHISMHeightTraceDesign.md](P2_5PieceHISMHeightTraceDesign.md)。

目标：

- 从角色位置外侧沿球心方向做射线检测。
- 只使用 HISM 碰撞命中点修正棋子 Actor Transform 的球面高度。
- 不使用碰撞法线，棋子 Up 仍沿 Cell 球面外法线。
- `Piece Radius Offset CM` 作为 HISM 命中高度之后的额外外抬微调。

验收：

- 棋子站位高度贴近 HISM 瓦片碰撞面。
- 调整 `Piece Radius Offset CM` 可在碰撞高度基础上微调模型高度。
- HISM 未命中或关闭时回退到 P1 固定半径站位。

### P3：攻击、受击与死亡表现

详细阶段设计见：[P3AttackHitDeathPresentationDesign.md](P3AttackHitDeathPresentationDesign.md)。

目标：

- 消费 Gameplay 层已经产出的吃子归因，不在表现层重新推规则。
- 一次行动先执行已有 Move / Jump 表现，之后按吃子列表串行执行每个吃子表现。
- 每次吃子时，攻击棋子和先锋棋子面向被吃棋子，被吃棋子面向先锋棋子。
- 弓兵和主将原地播放射箭 / 施法动画。
- 步兵和骑兵跑入被吃棋子的格子内，播放对应近战攻击动画，再跑回原位。
- 被吃棋子播放受击动画和死亡动画，停在死亡末尾并淡出。

验收：

- 普通吃子、远程吃子都有清晰反馈。
- 多个吃子会按 Gameplay 归因顺序逐个播放。
- 被移除棋子不会直接硬切消失。
- 攻击表现不处理 Undo；Undo 仍只在回合内回退点选 / 移动 / 跳跃状态。

### P4：骑兵双骨骼方案

P4.1 静态骑乘 Idle 阶段设计见：[P4_1MountedCavalryIdleDesign.md](P4_1MountedCavalryIdleDesign.md)。

P4.2 骑兵移动 / 跳跃阶段设计见：[P4_2MountedCavalryMoveJumpDesign.md](P4_2MountedCavalryMoveJumpDesign.md)。

P4.3 骑兵攻击 / 受击 / 死亡阶段设计见：[P4_3MountedCavalryAttackHitDeathDesign.md](P4_3MountedCavalryAttackHitDeathDesign.md)。

目标：

- 马体独立 locomotion
- 骑手稳定挂到马背
- 骑兵移动 / 跳跃 / 死亡可用
- 有简化攻击动作

验收：

- 骑兵一眼可辨
- 移动时整体自然
- 不依赖专用骑兵整套动画也能成立

### P5：终局演出与细化

目标：

- 胜利待机
- 失败淡出
- 职业差异化加强
- 远程投射物 / 法术特效

验收：

- 对局终局有明确反馈
- 角色表现不再只是“能动”，而是“有读感”

---

## 15. 初版不处理

以下内容不纳入首版动画实现范围：

- 网络同步下的动画预测
- Root Motion 驱动规则位置
- 复杂 IK 贴地
- Control Rig 实时骑乘修正
- 多层蒙太奇混合的精细战斗系统
- 面部表情与嘴型
- 高级布料 / 毛发模拟
- 电影级终局演出

---

## 16. 推荐的首版实现结论

如果以“尽快做出可玩且可看的版本”为目标，推荐路线是：

1. **新增独立 `PiecePresentation` 模块**
2. **先做人形棋子 actor + 共享 Humanoid AnimBP**
3. **用 Gameplay 事件驱动位移、跳跃、攻击、死亡**
4. **主将表现使用 `Mage`，但规则上仍不可移动**
5. **骑兵采用 HorseMesh + RiderMesh 的双骨骼拼装方案**
6. **马负责 locomotion，骑手负责职业辨识与上半身动作**
7. **先接受一个可信的 stylized 近似版本，不等待完美骑兵专用资源**

这样最符合当前项目现状：

- 规则闭环已存在
- 地形层已经够复杂，不适合继续塞棋子动画
- 现有 Adventurers 与 Horse 资产正好能支撑这个方向
- 真正短缺的只是“骑兵专用整套动作”，而这条路可以先绕开

---

## 17. 后续落地时建议优先核查的资产问题

正式开始做之前，建议先用资产检查流程确认三件事：

1. **Adventurers 几个角色是否共享同一套可互通骨架**
   - 决定是否能共用一套 Humanoid AnimBP

2. **现有动作序列分别绑定到哪个 skeleton**
   - 决定是否要先做 retarget 或重定向

3. **Horse skeleton 上是否已有适合挂 rider 的 socket / saddle 位**
   - 决定骑兵拼装成本

如果这三项确认下来，后面的实现路径就会非常顺。


