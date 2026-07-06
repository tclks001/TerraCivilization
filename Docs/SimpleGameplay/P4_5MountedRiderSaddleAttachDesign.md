# TerraCivilization SimpleGameplay P4.5 Rider 马背 Socket / 骨骼跟随设计稿

> 本稿对应 [PieceAnimationPresentationDesign.md](PieceAnimationPresentationDesign.md) 中 P4 骑兵双骨骼方案的第五个落地阶段。
> P4.5 建立在 P4.1-P4.4 之上：骑兵已经由 `HorseMesh + RiderMesh` 表现，马负责移动 / 跳跃，骑手可通过 AnimBP + Montage 播上半身攻击 / 受击。本阶段只解决 Rider 与马背骨骼不同步的问题；武器挂载不在本阶段实现。

---

## 1. 目标

P4.5 完成后应具备：

1. Idle / Move / Jump / Attack / Hit 时，`RiderAnchor` 始终挂到 `HorseMesh` 的马背骨骼或 Socket。
2. 默认挂点使用马骨骼树上的：

```text
Torso
```

3. 如果后续在 `Torso` 上创建 `SaddleSocket`，可把挂点改成：

```text
SaddleSocket
```

4. `P4RiderRelativeLocation` / `P4RiderRelativeRotation` 从本阶段开始表示“相对马背骨骼 / Socket 的微调”，需要重新调。
5. Death 时，`RiderAnchor` 脱离马背挂点，重新挂回 Actor 根节点，并使用 `P4RiderDeathRelativeLocation` / `P4RiderDeathRelativeRotation` 落地播放整身死亡动画。
6. 不处理武器挂载。

---

## 2. 问题来源

P4.1-P4.4 中的结构是：

```text
ATerraPieceActor
  RootScene
    HorseMesh
    RiderAnchor
      RiderMesh
```

这意味着 Rider 只跟随骑兵 Actor 的整体位移，不跟随马骨骼动画内部的马背起伏。

马跳跃时实际有两层运动：

1. Actor 沿 P2 跳跃弧线移动。
2. `HorseMesh` 的跳跃动画让马背骨骼在 SkeletalMesh 内部上下运动。

P4.4 之前 Rider 只跟随第 1 层，不跟随第 2 层，所以跳跃过程中会看起来脱离马背。

---

## 3. 新结构

P4.5 正常骑乘状态改为：

```text
ATerraPieceActor
  RootScene
    HorseMesh
      Torso / SaddleSocket
        RiderAnchor
          RiderMesh
```

死亡状态改为：

```text
ATerraPieceActor
  RootScene
    HorseMesh
    RiderAnchor
      RiderMesh
```

也就是说：

- 活着或受击 / 攻击时，Rider 跟随马背骨骼 / Socket。
- 死亡时，Rider 从马背脱离，落地播放整身死亡动画。

---

## 4. 资产与配置接口

继续在 `APlanetTessellatedMesh` Details 面板暴露字段，归入：

```text
PlanetTopology | Tess | SimpleGameplay P1 Piece Presentation
```

新增字段：

| 字段 | 默认值 | 用途 |
| --- | --- | --- |
| `P4SaddleAttachName` | `Torso` | `RiderAnchor` 挂到 `HorseMesh` 的骨骼名或 Socket 名 |

继续使用：

| 字段 | P4.5 语义 |
| --- | --- |
| `P4RiderRelativeLocation` | Rider 相对 `P4SaddleAttachName` 的位置微调 |
| `P4RiderRelativeRotation` | Rider 相对 `P4SaddleAttachName` 的旋转微调 |
| `P4RiderUniformScale` | RiderAnchor 缩放 |
| `P4RiderDeathRelativeLocation` | Rider 死亡时相对 Actor 根节点的落地位置 |
| `P4RiderDeathRelativeRotation` | Rider 死亡时相对 Actor 根节点的落地旋转 |

`P4SaddleAttachName` 可以填骨骼名，也可以填 Socket 名。C++ 侧统一作为 `FName` 传给 `AttachToComponent`。

---

## 5. 编辑器配置建议

### 5.1 首版直接使用 Torso

本阶段首版不要求创建 Socket，可直接使用马骨骼树上的：

```text
P4SaddleAttachName = Torso
```

然后在 PIE 中重新调整：

```text
P4RiderRelativeLocation
P4RiderRelativeRotation
P4RiderUniformScale
```

因为这些参数以前相对 `RootScene`，现在相对 `HorseMesh.Torso`，旧值通常不能直接复用。

### 5.2 后续可创建 SaddleSocket

如果直接挂 `Torso` 调参比较别扭，可以在 Horse Skeleton 中：

1. 打开 Horse Skeleton。
2. 选中 `Torso` 骨骼。
3. Add Socket。
4. 命名：

```text
SaddleSocket
```

5. 在 Skeleton 编辑器中调整 Socket 位置和旋转，使它位于马鞍附近。
6. 回到关卡 Actor，把：

```text
P4SaddleAttachName = SaddleSocket
```

这样 C++ 不需要改。

Socket 不是新骨骼，不会修改动画层级，也不会要求重新导入动画。

---

## 6. 动画状态规则

正常骑乘态会调用 Saddle 挂载：

| 状态 | RiderAnchor 父组件 | Rider 表现 |
| --- | --- | --- |
| Idle | `HorseMesh` 的 `P4SaddleAttachName` | Sitting |
| Move | `HorseMesh` 的 `P4SaddleAttachName` | Sitting |
| Jump | `HorseMesh` 的 `P4SaddleAttachName` | Sitting |
| Attack | `HorseMesh` 的 `P4SaddleAttachName` | P4.4 上半身 Montage 或 P4.3 回退 |
| Hit | `HorseMesh` 的 `P4SaddleAttachName` | P4.4 上半身 Montage 或 P4.3 回退 |

死亡态会调用 Root 挂载：

| 状态 | RiderAnchor 父组件 | Rider 表现 |
| --- | --- | --- |
| Death | `RootScene` | 落地整身死亡动画 |

---

## 7. 验收清单

- 编译通过。
- `P4SaddleAttachName = Torso` 时，骑兵 Idle 中 Rider 挂在马背附近。
- Move / Jump 时 Rider 跟随马背骨骼起伏，不再明显脱离马背。
- Attack / Hit 时 Rider 仍跟随马背，同时 P4.4 上半身 Montage 保持可用。
- Death 时 Rider 从马背脱离，落到地面播放整身死亡动画。
- 不配置 `SaddleSocket` 也可验收；后续改成 `SaddleSocket` 不需要改 C++。

---

## 8. 本阶段不处理

P4.5 不处理：

- 武器挂载。
- 马背 IK。
- Rider 双腿随马背姿态修正。
- 真实落马过渡动画。
- SaddleSocket 自动创建。
- 不同马模型的挂点资产规范化。
