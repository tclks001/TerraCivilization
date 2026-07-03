# TerraCivilization SimpleGameplay P1 棋子模型表现设计稿

> 本稿对应 [PieceAnimationPresentationDesign.md](PieceAnimationPresentationDesign.md) 中的 **P1：棋子真实模型替换 G1 调试球**。
>
> P1 的目标是搭出后续动画系统的完整框架，并让真实 SkeletalMesh 棋子 Actor 站到 Gameplay 棋子所在 Cell 上。
> 本阶段不实现武器、马匹、移动插值、攻击、死亡或复杂 AnimBP 状态机。

---

## 1. 目标

P1 完成后应具备：

1. 新增独立 UE Runtime 模块：

```text
Source/PiecePresentation/
```

2. 棋子表现按四层结构组织：

| 层 | P1 职责 |
| --- | --- |
| Gameplay 规则层 | `FTerraGameplayContainer` 继续提供棋子真值快照 |
| 表现编排层 / Presentation Sequencer | P1 先把快照翻译成 Spawn / Update / Remove 同步步骤 |
| 动画驱动层 / Animation State Driver | P1 先提供 Idle 驱动状态，为 P2 移动 / 跳跃参数预留入口 |
| AnimBP / SkeletalMesh | P1 使用 `ATerraPieceActor` + `USkeletalMeshComponent` 显示人物模型 |

3. 棋子表现使用 Actor。

4. 刷新策略采用增量同步框架：

```text
PieceId -> ATerraPieceActor
```

已有棋子更新位置和模型；新增棋子创建 Actor；死亡 / 消失棋子销毁 Actor。

5. P1 只映射主人物模型：

| 玩法兵种 | P1 模型 |
| --- | --- |
| 主将 | Mage |
| 步兵 | Knight |
| 骑兵 | Knight，占位；不挂马 |
| 弓兵 | Ranger |

---

## 2. 模块边界

### 2.1 Gameplay 规则层

规则层仍然只负责：

- 棋子是否存在。
- 棋子属于哪个阵营。
- 棋子是什么兵种。
- 棋子在哪个 Cell。

P1 不改动规则判定，也不让规则层直接依赖 `USkeletalMeshComponent`。

### 2.2 PiecePresentation 模块

`PiecePresentation` 模块负责：

- 定义表现层快照与同步步骤。
- 管理 `PieceId -> ATerraPieceActor`。
- 根据宿主提供的 Cell 世界 Transform 摆放棋子。
- 维护每个棋子的 P1 Idle 动画驱动状态。

### 2.3 TerraCivilization 地形宿主

`APlanetTessellatedMesh` 只做桥接：

- 持有 `FTerraGameplayContainer`。
- 暴露 P1 棋子模型资产接口。
- 根据 CellId 计算球面世界 Transform。
- 把 Gameplay 棋子快照同步给 `UTerraPiecePresentationManager`。

---

## 3. P1 资产接口

P1 不要求用户新建 DataAsset，先在 `APlanetTessellatedMesh` Details 面板暴露一组直接资产槽：

```text
PlanetTopology | Tess | SimpleGameplay P1 Piece Presentation
```

需要挂载：

| 字段 | 推荐资产 |
| --- | --- |
| Commander Mesh | `Content/Animations/Adventurers/Characters/Mage1` |
| Infantry Mesh | `Content/Animations/Adventurers/Characters/Knight1` |
| Cavalry Mesh | `Content/Animations/Adventurers/Characters/Knight1` |
| Archer Mesh | `Content/Animations/Adventurers/Characters/Ranger1` |

同时暴露：

| 字段 | 作用 |
| --- | --- |
| `bEnableP1PiecePresentation` | 是否启用真实棋子 Actor 表现 |
| `Piece Radius Offset CM` | 棋子相对球面外抬高度 |
| `Piece Uniform Scale` | 棋子统一缩放 |
| `Mesh Relative Location` | 模型相对 Actor 根节点的位置修正 |
| `Mesh Relative Rotation` | 模型导入朝向不一致时的旋转修正 |
| `bHideG1DebugPiecesWhenP1IsActive` | PIE 中启用 P1 后隐藏旧调试球，避免重叠 |

---

## 4. 资产挂载流程

1. 打开关卡中的 `APlanetTessellatedMesh` 实例。

2. 在 Details 面板找到：

```text
PlanetTopology | Tess | SimpleGameplay P1 Piece Presentation
```

3. 确认：

```text
bEnableP1PiecePresentation = true
```

4. 挂载四个 SkeletalMesh：

```text
Commander Mesh -> Mage1
Infantry Mesh  -> Knight1
Cavalry Mesh   -> Knight1
Archer Mesh    -> Ranger1
```

5. PIE 运行。

6. 期望效果：

- 每个活棋子生成一个 `ATerraPieceActor`。
- 模型站在对应 Cell 上。
- 主将、步兵、骑兵占位、弓兵能通过人物模型区分。
- 行动、Undo、吃子后，相关 Actor 会复用、移动或销毁，而不是整盘重建。

7. 如果模型躺倒或侧向错误，调整：

```text
Mesh Relative Rotation
```

如果模型过大、过小或陷入地表，调整：

```text
Piece Uniform Scale
Piece Radius Offset CM
Mesh Relative Location
```

---

## 5. 增量同步规则

P1 同步逻辑：

1. 从 `FTerraGameplayContainer::GetPieces()` 读取当前快照。
2. 过滤 `bAlive=false` 的棋子。
3. 对每个活棋子：
   - 若 `PieceId` 没有对应 Actor，则创建。
   - 若已有 Actor，则复用并更新 Cell、兵种、阵营和 Transform。
4. 对表现层仍存在、但 Gameplay 快照中已不存在或已死亡的 `PieceId`：
   - 销毁对应 Actor。
5. 同步完成后记录日志：
   - 创建数量
   - 更新数量
   - 移除数量
   - 当前存活表现 Actor 数量

P1 不做“全量清空再重建”，否则 P2 做移动和攻击动画时会打断正在播放的表现。

---

## 6. 球面站位规则

P1 的棋子 Transform 由 `APlanetTessellatedMesh` 计算：

```text
Position = Cell.UnitCenter * (GlobeRadiusCM + PieceRadiusOffsetCM)
Up       = Cell.UnitCenter
Forward  = 投影到 Cell 切平面的稳定参考方向
```

Actor 约定：

- 本地 `+Z` 是角色 Up。
- 本地 `+X` 是角色 Forward。
- 如果导入模型不是这个约定，通过 `Mesh Relative Rotation` 修正，不改规则层。

---

## 7. 本阶段不处理

P1 不处理：

- 武器挂点。
- 马匹与骑兵双骨骼。
- 普通移动插值。
- 跳跃动画。
- 攻击、受击、死亡动画。
- 远程投射物。
- 终局演出。
- DataAsset 化配置。

这些内容从 P2 开始逐步落地。

---

## 8. 验收清单

- 新增 `PiecePresentation` Runtime 模块。
- 项目编译通过。
- PIE 启动后，棋子使用真实 SkeletalMesh Actor 显示。
- `PieceId -> ATerraPieceActor` 增量同步关系成立。
- 点击移动或 Undo 后，已有 Actor 更新位置，不整盘重建。
- 被吃棋子在同步后被销毁。
- 未挂资产时不会崩溃，会输出缺失 Mesh 警告。
