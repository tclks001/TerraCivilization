# TerraCivilization SimpleGameplay P4.1 骑兵双骨骼 Idle 表现设计稿

> 本稿对应 [PieceAnimationPresentationDesign.md](PieceAnimationPresentationDesign.md) 中 P4 骑兵双骨骼方案的第一个落地阶段。
>
> P4.1 的目标很窄：只把游戏中的骑兵 Actor 从“单个枪兵 / Knight 占位模型”替换为“马 + 坐姿枪兵骑手”的双骨骼结构，并只播放 Idle 表现。移动、跳跃、攻击、受击、死亡、武器挂载、马背 IK、上半身分层动画都不在本阶段实现。

---

## 1. 目标

P4.1 完成后应具备：

1. Gameplay 层仍然只有一枚 `Cavalry` 棋子，不拆分马和人。
2. 表现层中骑兵仍然是一个 `ATerraPieceActor`。
3. `Cavalry` 棋子使用双骨骼组件：
   - `HorseMesh`
   - `RiderMesh`
4. `HorseMesh` 站在棋子 Actor 根节点上，播放马 Idle。
5. `RiderMesh` 挂在马背上的 `RiderAnchor` / `SaddleAnchor` 下，播放坐姿 Idle。
6. 骑手使用现有枪兵 / Knight 人物模型，坐姿动画使用 `Rig_Medium_GeneralSitting`。
7. 本阶段只验证静态站位和 Idle，不要求行走、跳跃、攻击、受击或死亡正确。

---

## 2. 四层职责

| 层 | P4.1 职责 |
| --- | --- |
| Gameplay 规则层 | 不变；`Cavalry` 仍是一枚普通棋子 |
| 表现编排层 / Presentation Sequencer | 仍按 P1 快照生成 / 更新骑兵 Actor，不产生新 Gameplay 事件 |
| 动画驱动层 / Animation State Driver | 对骑兵拆分 Idle：马播 HorseIdle，骑手播 Sitting |
| AnimBP / SkeletalMesh | 本阶段仍可直接 `PlayAnimation`，不要求 AnimBP 分层 |

P4.1 不改变规则层，也不让 Gameplay 关心马和骑手的存在。

---

## 3. 当前资产依据

当前项目已有以下相关资产：

```text
Content/Animations/Adventurers/Animations/Rig_Medium_GeneralSitting
Content/Animations/Adventurers/Characters/Knight...
Content/Animations/Horse/Horse
Content/Animations/Horse/HorseIdle
```

P4.1 推荐资产映射：

| 表现对象 | 推荐资产 |
| --- | --- |
| 马模型 | `Content/Animations/Horse/Horse` |
| 马 Idle | `Content/Animations/Horse/HorseIdle` 或 `HorseAnimalArmature_Idle` |
| 骑手模型 | 当前 P1 Cavalry Mesh，默认仍用 `Knight1` |
| 骑手坐姿 Idle | `Rig_Medium_GeneralSitting` |

说明：

- 骑手仍先复用 P1 的 `P1CavalryMesh`。
- 如果 `Rig_Medium_GeneralSitting` 与 `Knight1` 使用同一 Skeleton，可直接播放。
- 如果坐姿动画播放异常，再单独处理骨架绑定问题；P4.1 设计上不引入 Retargeter 流程。

---

## 4. Actor 组件结构

P4.1 后 `ATerraPieceActor` 建议支持两种表现模式：

```text
Humanoid
Mounted
```

普通棋子：

```text
ATerraPieceActor
  RootScene
  HumanMesh
```

骑兵棋子：

```text
ATerraPieceActor
  RootScene
  HorseMesh
  RiderAnchor
  RiderMesh
```

组件关系建议：

```text
RootScene
  HorseMesh
  RiderAnchor
    RiderMesh
```

`RiderAnchor` 不依赖马骨骼 socket。P4.1 先用普通 `USceneComponent` 暴露相对位移 / 旋转 / 缩放调参。

原因：

- 当前阶段只验收静态骑乘站位。
- 不要求马动画过程中骑手跟随马背骨骼摆动。
- 先用 `RiderAnchor` 能快速调出可接受的骑乘位置。
- 后续 P4.2 / P4.3 若需要跟随马背骨骼，再升级为 Horse Skeleton Socket。

---

## 5. 资产与参数接口

P4.1 建议继续在 `APlanetTessellatedMesh` Details 面板暴露参数，归入：

```text
PlanetTopology | Tess | SimpleGameplay P1 Piece Presentation
```

新增字段建议：

| 字段 | 默认 / 推荐资产 | 用途 |
| --- | --- | --- |
| `P4HorseMesh` | `/Game/Animations/Horse/Horse` | 骑兵马模型 |
| `P4HorseIdleAnimation` | `/Game/Animations/Horse/HorseIdle` | 马 Idle |
| `P4RiderSittingAnimation` | `/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralSitting` | 骑手坐姿 Idle |
| `P4HorseRelativeLocation` | `Zero` | 马相对 Actor 根节点的位置修正 |
| `P4HorseRelativeRotation` | `Zero` | 马导入朝向修正 |
| `P4HorseUniformScale` | `1.0` | 马模型缩放 |
| `P4RiderRelativeLocation` | 需 PIE 调整 | 骑手相对马背锚点的位置 |
| `P4RiderRelativeRotation` | 需 PIE 调整 | 骑手相对马背锚点的旋转 |
| `P4RiderUniformScale` | `1.0` | 骑手相对缩放 |

P4.1 不建议立刻做 DataAsset。当前项目 P1-P3 已经沿用 `APlanetTessellatedMesh` 暴露资产槽，本阶段继续保持一致。

---

## 6. 表现模式选择

`ATerraPieceActor` 根据 `PieceType` 选择表现模式：

| PieceType | 表现模式 |
| --- | --- |
| `Commander` | Humanoid |
| `Infantry` | Humanoid |
| `Archer` | Humanoid |
| `Cavalry` | Mounted |

进入 Mounted 模式时：

1. 隐藏或停用 `HumanMesh`。
2. 显示 `HorseMesh`。
3. 显示 `RiderMesh`。
4. `HorseMesh` 设置为 `P4HorseMesh`。
5. `RiderMesh` 设置为 `P1CavalryMesh`。
6. `HorseMesh` 播放 `P4HorseIdleAnimation`。
7. `RiderMesh` 播放 `P4RiderSittingAnimation`。

普通 Humanoid 模式保持 P1-P3 当前逻辑，不应受 P4.1 影响。

---

## 7. 空间与朝向规则

骑兵 Actor 的根节点 Transform 仍由 P1/P2.5 决定：

- 站在 Gameplay `CellId` 对应的球面位置。
- Up 沿 Cell 外法线。
- Forward 仍遵循当前统一方向规则。
- `P1MeshRelativeRotation` 仍只用于人形资源导入朝向修正。

P4.1 中需要特别区分三层旋转：

| 层 | 作用 |
| --- | --- |
| Actor Rotation | 玩法层朝向，和普通棋子一致 |
| `P4HorseRelativeRotation` | 修正马资源导入朝向 |
| `P4RiderRelativeRotation` | 调整骑手坐到马背上的局部方向 |

不要用 `P1MeshRelativeRotation` 同时修正马和骑手。马和骑手应该有各自的相对旋转参数，否则调参会互相污染。

---

## 8. P4.1 不处理

本阶段不处理：

- 马移动 / 跳跃动画。
- 骑手攻击、受击、死亡动画。
- 马死亡动画。
- 上半身 / 下半身分层动画。
- 武器挂载。
- 马背 socket。
- 骑手随马背骨骼起伏。
- 骑手腿部 IK。
- 骑兵专属攻击站位。
- P3 攻击流程中的 Mounted 特化。

也就是说，P4.1 只解决“看起来已经是骑兵，而不是单个枪兵”的问题。

---

## 9. 与 P2/P3 的临时关系

P4.1 只要求 Idle 正确，因此需要接受以下临时行为：

- 如果骑兵移动，P2 可能仍按旧逻辑驱动 Actor 位移；马和骑手是否正确切换移动动画不作为验收项。
- 如果骑兵攻击，P3 是否正确播放骑手攻击动画不作为验收项。
- 如果骑兵死亡，马和骑手是否分别播放死亡动画不作为验收项。

实现时应尽量不破坏 P2/P3 当前流程：

- P4.1 只在 `Cavalry` 的 `ApplyVisualConfig` / Idle 初始化路径中分支。
- P2/P3 的移动和攻击函数可以先维持现状，后续 P4.2/P4.3 再扩展 Mounted 分发。

---

## 10. 调参流程

实现后推荐验收调参步骤：

1. 在 `APlanetTessellatedMesh` Details 面板挂载：
   - `P4HorseMesh`
   - `P4HorseIdleAnimation`
   - `P4RiderSittingAnimation`
2. 进入 PIE。
3. 找到任意 `Cavalry` 棋子。
4. 调整 `P4HorseUniformScale`，先让马尺寸合理。
5. 调整 `P4HorseRelativeRotation`，确认马头方向与骑兵 Actor Forward 一致。
6. 调整 `P4RiderRelativeLocation`，让骑手盆骨落在马鞍附近。
7. 调整 `P4RiderRelativeRotation`，让骑手面向马头。
8. 调整 `P4RiderUniformScale`，让骑手和马的比例合理。
9. 记录最终参数，作为 P4.2 的默认值。

P4.1 调参重点不是动画流畅，而是静态读感：

- 一眼看出这是骑兵。
- 骑手坐在马背上，不明显悬空。
- 骑手不严重穿入马身体。
- 马头方向与棋子朝向一致。

---

## 11. 验收清单

- 编译通过。
- PIE 中所有非骑兵棋子表现不变。
- PIE 中 `Cavalry` 棋子从单个 Knight / 枪兵占位变为马 + 骑手。
- 骑兵仍然只有一个 Actor，对应一个 `PieceId`。
- 马和骑手都跟随同一个棋子 Actor 站在正确 Cell 上。
- 马播放 Idle 动画。
- 骑手播放 `Rig_Medium_GeneralSitting` 坐姿 Idle。
- 可通过 Details 参数调整马和骑手的相对位置、旋转、缩放。
- 本阶段不要求骑兵移动、跳跃、攻击、受击、死亡表现正确。

---

## 12. 后续阶段衔接

P4.1 完成后，后续建议拆分：

| 阶段 | 目标 |
| --- | --- |
| P4.2 | [骑兵移动 / 跳跃时马播放 Walk / Gallop / Jump，骑手保持 Sitting](P4_2MountedCavalryMoveJumpDesign.md) |
| P4.3 | 骑兵攻击 / 受击 / 死亡时 Rider 播对应动作，Horse 播死亡 |
| P4.4 | 引入 Rider AnimBP，上半身攻击 / 受击，下半身固定坐姿 |
| P4.5 | 马背 socket、骑手随马背骨骼、武器挂载和细化调参 |
