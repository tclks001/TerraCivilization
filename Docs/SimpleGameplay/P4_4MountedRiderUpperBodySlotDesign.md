# TerraCivilization SimpleGameplay P4.4 Rider 上半身 Slot Montage 设计稿

> 本稿对应 [PieceAnimationPresentationDesign.md](PieceAnimationPresentationDesign.md) 中 P4 骑兵双骨骼方案的第四个落地阶段。
> P4.4 建立在 P4.1-P4.3 之上：骑兵已经由 `HorseMesh + RiderMesh` 表现，移动 / 跳跃由马播放，骑手保持坐姿，攻击 / 受击 / 死亡已有双骨骼分发。本阶段只解决骑手攻击 / 受击时“下半身保持坐姿，上半身播放动作”的最小可用方案。

---

## 1. 目标

P4.4 完成后应具备：

1. 骑兵 Idle / Move / Jump 时：
   - `HorseMesh` 继续按 P4.1 / P4.2 播马动画。
   - `RiderMesh` 由 Rider AnimBP 保持 Sitting。
2. 骑兵 Attack 时：
   - `RiderMesh` 下半身保持 Sitting。
   - `RiderMesh` 上半身通过 `MountedUpperBody` Slot 播攻击 Montage。
3. 骑兵 Hit 时：
   - `RiderMesh` 下半身保持 Sitting。
   - `RiderMesh` 上半身通过 `MountedUpperBody` Slot 播受击 Montage。
4. 骑兵 Death 时：
   - 继续沿用 P4.3：Rider 掉到地面，整身播放死亡动画。
   - Death 不做 Sitting 分层。
5. 如果未配置 Rider AnimBP 或 Montage，代码回退到 P4.3 的直接 `PlayAnimation` 行为。

---

## 2. 四层职责

| 层 | P4.4 职责 |
| --- | --- |
| Gameplay 规则层 | 不变 |
| 表现编排层 / Presentation Sequencer | 不变，继续复用 P3 攻击 / 受击 / 死亡时序 |
| 动画驱动层 / Animation State Driver | `ATerraPieceActor` 对 Rider 优先调用 `UTerraMountedRiderAnimInstance` 播 Montage |
| AnimBP / SkeletalMesh | 新增 Rider AnimBP：Sitting Base Pose + 上半身 Slot + Layered Blend Per Bone |

P4.4 的核心在 AnimBP。C++ 只负责把状态和 Montage 请求传进去。

---

## 3. C++ 接口

新增 C++ AnimInstance 基类：

```text
UTerraMountedRiderAnimInstance
```

它暴露：

| 字段 / 函数 | 用途 |
| --- | --- |
| `SittingAnimation` | AnimBP 中用来播放坐姿 Base Pose |
| `RiderState` | 当前状态：`Sitting` / `UpperBodyAction` / `Death` |
| `SetSittingAnimation` | Actor 每次应用配置时写入坐姿动画 |
| `EnterSitting` | Idle / Move / Jump 时保持坐姿 |
| `PlayUpperBodyMontage` | Attack / Hit 时播放上半身 Montage |
| `EnterDeath` | Death 前通知 AnimBP 退出上半身状态 |

`ATerraPieceActor` 的 Mounted 分发规则：

```text
Attack:
  有 Rider AnimBP + P4RiderUpperBodyAttackMontage -> 播 Montage
  否则 -> 回退到 P4.3 整身攻击动画

Hit:
  有 Rider AnimBP + P4RiderUpperBodyHitMontage -> 播 Montage
  否则 -> 回退到 P4.3 整身受击动画

Death:
  Rider 掉到地面
  RiderMesh 切回 SingleNode
  整身播放 P3DeathAnimation
```

---

## 4. 资产接口

继续在 `APlanetTessellatedMesh` Details 面板暴露字段，归入：

```text
PlanetTopology | Tess | SimpleGameplay P1 Piece Presentation
```

新增字段：

| 字段 | 用途 |
| --- | --- |
| `P4RiderAnimInstanceClass` | RiderMesh 使用的 AnimBP 类；父类必须是 `TerraMountedRiderAnimInstance` |
| `P4RiderUpperBodyAttackMontage` | 骑手上半身攻击 Montage |
| `P4RiderUpperBodyHitMontage` | 骑手上半身受击 Montage |

继续使用：

| 字段 | 用途 |
| --- | --- |
| `P4RiderSittingAnimation` | AnimBP Base Pose |
| `P3CavalryMeleeAttackAnimation` | 未配置 Montage 时的回退动画来源 |
| `P3HitAnimation` | 未配置 Montage 时的回退动画来源 |
| `P3DeathAnimation` | 骑手落地整身死亡动画 |

---

## 5. 编辑器配置步骤

### 5.1 创建 Rider AnimBP

1. 在 Content Browser 中找到 `Rig_Medium_General_Skeleton`。
2. 右键创建 Animation Blueprint。
3. Parent Class 选择：

```text
TerraMountedRiderAnimInstance
```

4. Skeleton 选择：

```text
Rig_Medium_General_Skeleton
```

5. 命名建议：

```text
ABP_TerraMountedRider
```

### 5.2 创建 Slot

1. 打开 `ABP_TerraMountedRider`。
2. 进入 AnimGraph。
3. 新增一个 `Slot` 节点。
4. Slot Name 设置为：

```text
MountedUpperBody
```

如果编辑器提示需要 Slot Group，可使用默认组，或新建：

```text
Mounted
```

关键是 Montage 里的 Slot 名必须和 AnimGraph 中一致。

### 5.3 配置 Sitting Base Pose

P4.4 的 Sitting Base Pose 是 Rider 的“默认全身姿势”。后续攻击 / 受击 Montage 只覆盖上半身，下半身仍来自这个 Base Pose。

推荐先用最稳定的固定动画方式配置，不要一开始追求动态资产输入。

#### 5.3.1 最小稳定配置：固定 Sitting 动画

1. 打开 `ABP_TerraMountedRider`。
2. 进入 `AnimGraph`。
3. 在空白处右键搜索你的坐姿动画，例如：

```text
Rig_Medium_GeneralSitting
```

4. 选择类似下面的节点：

```text
Play Rig_Medium_GeneralSitting
```

5. 把该节点先直接连到 `Output Pose`。
6. 点击 Compile。
7. 在预览窗口中确认 Rider 是坐姿。

此时 AnimGraph 最简单结构是：

```text
Play Rig_Medium_GeneralSitting -> Output Pose
```

如果这一步不对，先不要继续配 Slot。先确认：

- `ABP_TerraMountedRider` 使用的 Skeleton 是 `Rig_Medium_General_Skeleton`。
- `Rig_Medium_GeneralSitting` 也是同一个 Skeleton。
- 预览 Mesh 使用骑兵 Rider 的人形 SkeletalMesh。

#### 5.3.2 将 Sitting 节点命名为 Base Pose

为了后面接 `Layered Blend Per Bone` 时不混乱，建议整理节点：

1. 选中 `Play Rig_Medium_GeneralSitting` 节点。
2. 在节点旁边添加 Comment，命名为：

```text
Sitting Base Pose
```

3. 后续所有基础姿势都从这个节点输出。

此阶段不要把 `Slot` 直接接到最终输出，否则 Montage 会覆盖全身，达不到“下半身坐姿、上半身攻击”的效果。

#### 5.3.3 可选进阶配置：使用 C++ 写入的 SittingAnimation 变量

C++ 已经会向 `UTerraMountedRiderAnimInstance::SittingAnimation` 写入 `P4RiderSittingAnimation`。但在 AnimGraph 中动态驱动 Sequence Player 对 UE 版本和节点类型有要求，P4.4 不强制使用。

如果你的 AnimGraph 支持动态动画资产输入，可以尝试：

1. 在 AnimGraph 中创建可接收 Animation Asset 变量的 Sequence Player / Sequence Evaluator。
2. 将 `SittingAnimation` 变量接到该节点的 Animation / Sequence 输入。
3. 将该节点输出作为 `Sitting Base Pose`。
4. Compile 后确认预览仍然是坐姿。

如果找不到可接变量的 Sequence Player，保持 5.3.1 的固定动画配置即可。P4.4 的 C++ 回退和验收都支持固定 Sitting 动画。

#### 5.3.4 常见错误

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 预览没有坐下 | AnimBP Skeleton 或坐姿动画 Skeleton 不一致 | 重新确认 `Rig_Medium_General_Skeleton` |
| Rider 在游戏里 T Pose | `P4RiderAnimInstanceClass` 没挂，或 AnimBP 没 Compile | 挂到 `APlanetTessellatedMesh` 并 Compile AnimBP |
| 攻击时全身都动 | Slot 没经过 `Layered Blend Per Bone` | 继续按 5.4 配分层 |
| 下半身也被攻击动画拉动 | Blend 起始骨骼太低 | 5.4 中把 `spine_01` 改为 `spine_02` |

### 5.4 配置 Layered Blend Per Bone

`Layered Blend Per Bone` 的作用是：

- Base Pose 使用完整 Sitting。
- Blend Pose 0 使用 Sitting 经过 `MountedUpperBody` Slot 后的姿势。
- 只从脊柱某个骨骼开始混合 Blend Pose 0。
- 因此下半身继续来自 Sitting，上半身接受 Montage 覆盖。

#### 5.4.1 先解决 Sitting Base Pose 只有一个出口的问题

AnimGraph 里的一个 Pose 输出理论上可以被多条连线复用；但实际编辑时容易拖线混乱，也不利于后续维护。推荐使用 UE 的缓存姿势节点。

推荐结构是：

```text
Play Rig_Medium_GeneralSitting
  -> Save Cached Pose
       Cache Name = RiderSittingBase

Use Cached Pose RiderSittingBase
  -> Layered Blend Per Bone Base Pose

Use Cached Pose RiderSittingBase
  -> Slot MountedUpperBody
  -> Layered Blend Per Bone Blend Pose 0

Layered Blend Per Bone -> Output Pose
```

具体操作：

1. 在 `AnimGraph` 中找到 5.3 创建的 `Play Rig_Medium_GeneralSitting`。
2. 右键搜索并创建：

```text
Save Cached Pose
```

3. 把 `Play Rig_Medium_GeneralSitting` 的 Pose 输出接到 `Save Cached Pose`。
4. 选中 `Save Cached Pose` 节点，把 Cache Name 改成：

```text
RiderSittingBase
```

5. 右键搜索并创建第一个：

```text
Use Cached Pose
```

6. 在该节点 Details 里选择缓存名：

```text
RiderSittingBase
```

7. 再创建第二个 `Use Cached Pose`，同样选择：

```text
RiderSittingBase
```

这两个 `Use Cached Pose` 就是同一个 Sitting Base Pose 的两个出口：

- 第一个接 `Layered Blend Per Bone` 的 `Base Pose`。
- 第二个先接 `Slot MountedUpperBody`，再接 `Layered Blend Per Bone` 的 `Blend Poses 0`。

#### 5.4.2 创建 Slot 节点

1. 右键创建：

```text
Slot
```

2. 选中 Slot 节点，在 Details 中设置：

```text
Slot Name = MountedUpperBody
```

3. 把第二个 `Use Cached Pose RiderSittingBase` 接到 Slot 的输入。
4. Slot 的输出接到 `Layered Blend Per Bone` 的 `Blend Poses 0`。

注意：Slot 不要直接接 `Output Pose`。直接接 Output 会让 Montage 覆盖全身。

#### 5.4.3 创建 Layered Blend Per Bone 节点

1. 右键创建：

```text
Layered Blend Per Bone
```

2. 如果节点只有 `Base Pose`，还没有 `Blend Poses 0`：
   - 选中节点。
   - 在 Details 面板找到 `Config` 或 `Settings`。
   - 将 `Blend Poses` 数组增加到 1 个元素。
   - 节点上会出现 `Blend Poses 0` 输入。

3. 连接：

```text
Use Cached Pose RiderSittingBase -> Base Pose
Slot MountedUpperBody -> Blend Poses 0
Layered Blend Per Bone -> Output Pose
```

#### 5.4.4 配置骨骼分层

UE 里不一定直接显示 `Branch Filter Bone Name` 这个完整名字。通常路径是：

```text
Layered Blend Per Bone Details
  -> Config
    -> Layer Setup
      -> [0]
        -> Branch Filters
          -> [0]
            -> Bone Name
            -> Blend Depth
```

也有版本会显示为：

```text
Layer Setup
  -> Index [0]
    -> Branch Filters
      -> Index [0]
        -> Bone Name
        -> Blend Depth
```

也就是说，设计稿里说的：

- `Branch Filter Bone Name`
- `Blend Depth`

在 UE 面板里的具体字段通常叫：

| 设计稿说法 | UE Details 具体字段名 |
| --- | --- |
| Branch Filter Bone Name | `Layer Setup -> [0] -> Branch Filters -> [0] -> Bone Name` |
| Blend Depth | `Layer Setup -> [0] -> Branch Filters -> [0] -> Blend Depth` |

建议配置：

| 配置项 | 建议 |
| --- | --- |
| `Layer Setup -> [0] -> Branch Filters -> [0] -> Bone Name` | 先试 `spine` |
| `Layer Setup -> [0] -> Branch Filters -> [0] -> Blend Depth` | `10` |
| `Mesh Space Rotation Blend` | 开启 |

如果 `Branch Filters` 为空：

1. 展开 `Layer Setup`。
2. 展开元素 `[0]`。
3. 找到 `Branch Filters`。
4. 点击 `+` 添加一个元素。
5. 再设置 `Bone Name` 和 `Blend Depth`。

#### 5.4.5 骨骼名调试建议

当前 `Rig_Medium_General` 骨骼树的关键层级是：

```text
root
  hips
    spine
      chest
        head
        upperarm_l
        upperarm_r
    upperleg_l
    upperleg_r
```

因此 P4.4 不应该再使用 `spine_01` / `spine_02`。这两个名字在当前骨骼树里不存在，填进去不会得到正确的上半身分层。

推荐选择：

| Bone Name | 效果 | 建议用途 |
| --- | --- | --- |
| `spine` | 从腰 / 脊柱开始覆盖，攻击幅度更完整，可能轻微影响坐姿稳定性 | 首选尝试 |
| `chest` | 从胸腔开始覆盖，腰和下半身更稳，但攻击幅度可能变小 | 如果 `spine` 导致腰部扭动明显，改用它 |

首选配置：

```text
Bone Name = spine
Blend Depth = 10
```

如果攻击动作把腰部拉得太厉害，改成：

```text
Bone Name = chest
Blend Depth = 10
```

如果攻击幅度太小，或者手臂动作不完整，再改回：

```text
Bone Name = spine
Blend Depth = 10
```

必须使用骨骼树里的精确名字。当前项目里应使用小写 `spine` / `chest`，不要写成 `Spine` / `Chest`。

#### 5.4.6 最终检查

最终 AnimGraph 应该符合：

```text
Play Sitting
  -> Save Cached Pose RiderSittingBase

Use Cached Pose RiderSittingBase
  -> Layered Blend Per Bone Base Pose

Use Cached Pose RiderSittingBase
  -> Slot MountedUpperBody
  -> Layered Blend Per Bone Blend Poses 0

Layered Blend Per Bone
  -> Output Pose
```

如果播放攻击 Montage 时还是全身动，优先检查：

1. Montage 的 Slot 是否真的是 `MountedUpperBody`。
2. AnimGraph 里的 Slot Name 是否也是 `MountedUpperBody`。
3. Slot 是否接在 `Layered Blend Per Bone` 的 `Blend Poses 0`，而不是直接接 Output。
4. `Layer Setup -> Branch Filters -> Bone Name` 是否填了有效脊柱骨骼。

### 5.5 创建攻击 Montage

1. 找到骑兵攻击动画序列，例如：

```text
Rig_Medium_GeneralUpward_Thrust
```

2. 右键创建 AnimMontage。
3. 命名建议：

```text
AM_TerraRider_UpperBody_Attack
```

4. 打开 Montage。
5. 在 Montage 编辑器中设置 Slot。

Slot 通常在 Montage 编辑器中间的时间轴区域，而不是右侧普通 Details 面板里。打开 Montage 后，找到类似下面的轨道：

```text
DefaultGroup.DefaultSlot
```

或：

```text
Slot
  DefaultSlot
```

具体操作：

1. 在 Montage 时间轴区域找到 Slot Track。
2. 点击当前 Slot 名称，通常默认是：

```text
DefaultSlot
```

3. 在下拉列表里选择：

```text
MountedUpperBody
```

4. 如果下拉列表里没有 `MountedUpperBody`，先打开 Anim Slot Manager：

```text
Window -> Anim Slot Manager
```

或在 Montage 编辑器顶部 / 右侧面板中找到：

```text
Anim Slot Manager
```

5. 在 Slot Manager 中添加 Slot：

```text
Slot Name = MountedUpperBody
```

Group 可以先使用默认组。如果你已经创建了 Mounted 组，也可以使用：

```text
Group Name = Mounted
Slot Name = MountedUpperBody
```

6. 回到 Montage 时间轴，把 Montage 的 Slot Track 改为：

```text
MountedUpperBody
```

最终要确认 Montage 时间轴上的轨道名字包含：

```text
MountedUpperBody
```

而不是仍然停留在：

```text
DefaultSlot
```

7. 保存。

### 5.6 创建受击 Montage

1. 找到受击动画序列，例如：

```text
Rig_Medium_GeneralHit_A
```

2. 右键创建 AnimMontage。
3. 命名建议：

```text
AM_TerraRider_UpperBody_Hit
```

4. 打开 Montage。
5. 确认 Slot 设置为：

```text
MountedUpperBody
```

6. 保存。

### 5.7 挂到关卡 Actor

选中关卡中的 `APlanetTessellatedMesh`，在 Details 面板设置：

```text
P4RiderAnimInstanceClass = ABP_TerraMountedRider
P4RiderUpperBodyAttackMontage = AM_TerraRider_UpperBody_Attack
P4RiderUpperBodyHitMontage = AM_TerraRider_UpperBody_Hit
```

已有字段继续保持：

```text
P4RiderSittingAnimation = Rig_Medium_GeneralSitting
P3CavalryMeleeAttackAnimation = Rig_Medium_GeneralUpward_Thrust
P3HitAnimation = Rig_Medium_GeneralHit_A
P3DeathAnimation = Rig_Medium_GeneralDeath_A
```

---

## 6. 验收清单

- 编译通过。
- 不配置 P4.4 字段时，骑兵表现与 P4.3 一致。
- 配置 `P4RiderAnimInstanceClass` 后，骑兵 Idle / Move / Jump 时 Rider 保持坐姿。
- 配置攻击 Montage 后，骑兵攻击时 Rider 下半身仍坐在马背上，上半身播放攻击。
- 配置受击 Montage 后，骑兵受击时 Rider 下半身仍坐在马背上，上半身播放受击。
- 骑兵死亡时 Rider 仍按 P4.3 掉到地面，整身播放死亡动画，马播放死亡动画。

---

## 7. 本阶段不处理

P4.4 不处理：

- 动态换 Sequence Player 资产的复杂蓝图封装。
- Death 的上下半身分层。
- Rider 随马背骨骼起伏。
- 武器挂载。
- 攻击 Notify。
- Root Motion 清理。
- 马受击动画。

这些内容留给 P4.5 及后续阶段。

---

## 8. 调试记录

### 问题一：骑兵攻击时 Rider 上半身攻击、下半身不动的效果始终不生效（全身都动）

**现象**

配置 AnimBP + 攻击 Montage 后，骑兵攻击时 Rider 全身播放攻击动画，下半身未保持坐姿。即使在 AnimBP 中将 Sitting Base Pose 直连 Output Pose，攻击时仍然全身动。日志显示：

```
[P4.4 Debug] Rider AI=AnimSingleNodeInstance_0, Montage=AM_TerraRider_UpperBody_Attack, MontagePlayed=0
[P4.4 Debug] GetMountedRiderAnimInstance_: Raw=ABP_TerraMountedRider_C_0, CastOK=1, AnimMode=0
[P4.4 Debug] GetMountedRiderAnimInstance_: Raw=AnimSingleNodeInstance_0, CastOK=0, AnimMode=1
```

**根因**

`ATerraPieceActor::PlayMoveAnimation` 中骑兵 Move 分支（PresentationOnlyMove 路径）对 `RiderMesh` 直接调用了 `PlayAnimationOnMesh_`，该方法内部调用 `USkeletalMeshComponent::PlayAnimation`，强制将 Mesh 切为 `AnimationSingleNode` 模式并销毁 AnimBP 实例。此后攻击阶段 `GetMountedRiderAnimInstance_()` Cast 失败，Montage 无法播放，回退到整身动画。

`PlayAnimationOnMesh_` 原本有 `Cast<UTerraPieceAnimInstance>` 守卫来拦截 AnimBP 的 Mesh，但 `UTerraMountedRiderAnimInstance` 继承自 `UAnimInstance`（非 `UTerraPieceAnimInstance`），因此守卫未生效。

此外，`PlayMountedMoveAnimation_` / `PlayMountedIdleAnimations_` 已经使用了正确的「AnimBP 优先 → 回退 SingleNode」模式，但 `PlayMoveAnimation` 中遗漏了这一判断。

**调试步骤**

1. 在 `PlayAttackAnimation` 中打印 `RiderMesh` 的 AnimInstance 类型和 Montage 播放结果 → 发现 `AnimSingleNodeInstance_0`、`MontagePlayed=0`。
2. 在 `ApplyVisualConfig_` 中确认 `RiderAnimInstanceClass` 已正确传入 `ABP_TerraMountedRider_C`。
3. 在 `GetMountedRiderAnimInstance_` 中追加日志，发现第 1 次调用返回 ABP（CastOK=1）、第 2 次返回 `AnimSingleNodeInstance`（CastOK=0）→ 锁定 AnimBP 在两次调用间被踢掉。
4. 全局搜索对 `RiderMesh` 调用 `PlayAnimationOnMesh_` 的位置 → 在 `PlayMoveAnimation` 第 303 行找到无判断的调用。

**解决方案**

1. 修改 `PlayMoveAnimation` 骑兵分支，将无条件 `PlayAnimationOnMesh_(RiderMesh, ...)` 改为与 `PlayMountedMoveAnimation_` 一致的「AnimBP 优先」模式：

   ```cpp
   if (UTerraMountedRiderAnimInstance* RiderAnimInstance = GetMountedRiderAnimInstance_())
   {
       RiderAnimInstance->SetSittingAnimation(CachedVisualConfig.RiderSittingAnimation);
       RiderAnimInstance->EnterSitting();
   }
   else
   {
       PlayAnimationOnMesh_(RiderMesh, CachedVisualConfig.RiderSittingAnimation, true, 0.0f);
   }
   ```

2. 在 `PlayAnimationOnMesh_` 中增加对 `UTerraMountedRiderAnimInstance` 的防御性守卫，防止未来任何路径误调用 `PlayAnimation` 踢掉骑手 AnimBP：

   ```cpp
   if (Cast<UTerraMountedRiderAnimInstance>(MeshComponent->GetAnimInstance()))
   {
       return;
   }
   ```

---

### 问题二：修复问题一后，骑兵死亡时 Rider 变为 T-Pose

**现象**

Horse 的 Death 动画正常播放，但 Rider 呈 T-Pose 状态。崩溃日志显示空指针访问（reading address 0x0000000000000018）发生在 `ApplyVisualConfig_()` 附近，但不稳定复现。

**根因**

问题一中添加的防御性守卫在 `PlayAnimationOnMesh_` 对 `UTerraMountedRiderAnimInstance` 的 Cast 检查过于激进。骑兵死亡路径的设计意图是主动将 RiderMesh 从 AnimBP 模式切回 `AnimationSingleNode`，整身播放死亡动画（见 §3 C++ 接口中的 Death 规则）。但原代码在 `SetAnimationMode(AnimationSingleNode)` 和 `SetAnimInstanceClass(nullptr)` 之后仍调用 `PlayAnimationOnMesh_`，由于 UE 内部 `InitializeAnimScriptInstance` 的初始化时序不确定，`GetAnimInstance()` 可能仍返回旧的 ABP 实例，导致守卫拦截了合法的死亡动画播放调用。

**调试步骤**

1. 检查 `PlayDeathAnimation` 中骑兵分支的 `SetAnimationMode` / `SetAnimInstanceClass(nullptr)` 之后调用的是 `PlayAnimationOnMesh_`（带守卫的包装）。
2. 确认 Death 路径的设计意图是切回 SingleNode（P4.4 设计稿 §3 明确规定 Death 不做分层），守卫不应拦截该路径。
3. 将调用改为绕过守卫，直接使用 `USkeletalMeshComponent::PlayAnimation`。

**解决方案**

将死亡分支中 `RiderMesh` 的调用从 `PlayAnimationOnMesh_` 改为直接调用原始 API，同时将 `SetAnimationMode` / `SetAnimInstanceClass` 移到 if 块外部，确保即使 AnimBP 未挂载也能正确切换：

```cpp
if (IsMountedCavalry_())
{
    ApplyMountedRiderDeathTransform_();
    if (UTerraMountedRiderAnimInstance* RiderAnimInstance = GetMountedRiderAnimInstance_())
    {
        RiderAnimInstance->EnterDeath();
    }
    RiderMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    RiderMesh->PlayAnimation(DeathAnimation, false);
    if (StartOffsetSeconds > 0.0f)
    {
        RiderMesh->SetPosition(FMath::Max(StartOffsetSeconds, 0.0f), false);
    }
    PlayAnimationOnMesh_(HorseMesh, CachedVisualConfig.HorseDeathAnimation, false, 0.0f);
}
```

---

### 各路径最终调用规则总结

| 路径 | RiderMesh 调用方式 | 原因 |
| --- | --- | --- |
| Idle / Move | `PlayAnimationOnMesh_` 的守卫拦截 → 走 `EnterSitting` | 保持 AnimBP 驱动坐姿 |
| Attack / Hit | `PlayMountedRiderUpperBodyMontage_` → AnimBP Slot Montage | 上半身分层，下半身坐姿 |
| Death | 直接 `RiderMesh->PlayAnimation`，绕过守卫 | 有意切回 SingleNode 整身播放 |
