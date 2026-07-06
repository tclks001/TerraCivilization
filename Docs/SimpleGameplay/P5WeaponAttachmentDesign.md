# TerraCivilization SimpleGameplay P5 武器挂载设计稿

> 本稿是 `PieceAnimationPresentationDesign.md` 的 P5 阶段设计。目标是在不改变 Gameplay 判定的前提下，让棋子表现 Actor 根据兵种挂载静态武器模型。

---

## 1. 目标

P5 只做武器可视挂载：

- 主将：保持空手施法。
- 弓兵：挂弓，暂不处理箭袋。
- 步兵：挂剑和盾。
- 骑兵：挂斧，挂在 Rider 身上而不是 Horse 身上。

武器模型、Attach Name、相对位置、相对旋转、相对缩放全部通过 `UPROPERTY` 暴露，便于在编辑器中按当前资产校准。

---

## 2. 非目标

本阶段不做：

- 武器碰撞或伤害判定。
- 攻击帧命中盒。
- 箭矢投射物。
- 法术特效。
- 箭袋、披风、徽记等职业配饰。
- 武器在 Idle / Attack 间动态换手。

这些内容留给 P6 / P7。

---

## 3. 模块边界

武器属于 `PiecePresentation` 表现层。

```text
Gameplay 规则层
    -> 输出棋子类型 / 行动结果
PiecePresentation
    -> 根据 PieceType 选择武器资产
    -> 将武器 StaticMeshComponent Attach 到角色 SkeletalMesh 的 Socket / Bone
AnimBP / SkeletalMesh
    -> 角色骨骼移动时，武器自动跟随
```

Gameplay 不知道武器是否存在，也不读取武器碰撞。

---

## 4. 资产建议

当前项目已有可用武器资产：

```text
Content/Animations/Adventurers/Assets/bow
Content/Animations/Adventurers/Assets/sword_1handed
Content/Animations/Adventurers/Assets/shield_round
Content/Animations/Adventurers/Assets/axe_1handed
```

建议默认映射：

| 兵种 | 武器 |
| --- | --- |
| 主将 | 空手 |
| 弓兵 | `bow` |
| 步兵 | `sword_1handed` + `shield_round` |
| 骑兵 | `axe_1handed` |

如果后续发现 `bow_withString`、`shield_badge` 或 `axe_2handed` 视觉更好，可以只替换 UPROPERTY 资产，不需要改代码。

---

## 5. 运行时结构

`ATerraPieceActor` 内新增静态网格组件：

```text
WeaponPrimaryMesh
WeaponSecondaryMesh
```

组件用途：

- `WeaponPrimaryMesh`
  - 步兵：剑
  - 弓兵：弓
  - 骑兵：斧
- `WeaponSecondaryMesh`
  - 步兵：盾
  - 其他兵种：隐藏

挂载目标：

- 非骑兵：挂到 `HumanMesh`。
- 骑兵：挂到 `RiderMesh`。
- 主将：隐藏全部武器组件。

---

## 6. Attach Name 规则

每个武器暴露独立 Attach Name：

| 参数 | 用途 |
| --- | --- |
| `P5InfantrySwordAttachName` | 步兵剑挂点，默认 `handslot_r` |
| `P5InfantryShieldAttachName` | 步兵盾挂点，默认 `handslot_l` |
| `P5ArcherBowAttachName` | 弓兵弓挂点，默认 `handslot_l` |
| `P5CavalryAxeAttachName` | 骑兵斧挂点，默认 `handslot_r` |

Attach Name 可以是 SkeletalMesh 的 Socket，也可以是 Bone。

若找不到 Attach Name：

- 仍挂到对应 Mesh 根组件，避免组件丢失。
- 输出一次 Warning 日志，提示具体 PieceId、AttachName、SkeletalMesh。

推荐做法：

1. 优先使用当前人物骨骼已有挂点：
   - 右手武器：`handslot_r`
   - 左手盾 / 弓：`handslot_l`
2. 若现有挂点位置不适合，再在人物骨骼上添加清晰 Socket，例如：
   - `Weapon_R`
   - `Shield_L`
   - `Bow_L`
3. 在 `APlanetTessellatedMesh` 的 P5 参数中填这些 Socket 名。
4. 用 P5 相对 Transform 微调位置。

如果暂时不建 Socket，也可以直接填现有骨骼名，后续再迁移到 Socket。

---

## 7. 相对 Transform

每个武器暴露：

- Relative Location
- Relative Rotation
- Uniform Scale

这些 Transform 是相对 Attach Name 的局部偏移。

P5 代码不假设武器资源导入方向正确。若武器横竖反了，优先调对应 `Relative Rotation`，不要改角色整体 `P1MeshRelativeRotation`。

---

## 8. 生命周期

武器组件跟随 `ATerraPieceActor` 生命周期：

- Spawn / Snapshot：根据兵种刷新武器。
- Move / Jump：武器自动跟随角色骨骼。
- Attack / Hit：武器自动跟随动画骨骼。
- Death：武器继续跟随死亡动画和淡出。
- 兵种切换或配置为空：隐藏并清空对应 StaticMesh。

P5 不单独创建武器 Actor，避免生命周期和销毁顺序复杂化。

---

## 9. 编辑器配置流程

1. 选中场景中的 `APlanetTessellatedMesh`。
2. 找到分类：

```text
PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation
```

3. 配置 P5 资产：
   - `P5ArcherBowMesh`：建议 `bow`。
   - `P5InfantrySwordMesh`：建议 `sword_1handed`。
   - `P5InfantryShieldMesh`：建议 `shield_round`。
   - `P5CavalryAxeMesh`：建议 `axe_1handed`。
4. 配置 Attach Name：
   - 默认值已经按当前约定设置：剑 / 斧为 `handslot_r`，盾 / 弓为 `handslot_l`。
   - 若已在骨骼上建 Socket，填 Socket 名。
   - 若暂时没有 Socket，可先填骨骼名。
5. 进入 PIE，观察 Idle / Move / Jump / Attack / Hit。
6. 调整对应 Relative Location / Rotation / Scale，直到武器贴手可信。

主将没有武器配置项，保持空手。

---

## 10. 验收标准

- 主将空手施法。
- 弓兵显示弓。
- 步兵显示剑和盾。
- 骑兵 Rider 显示斧，且马移动 / 跳跃时斧跟随 Rider。
- 武器在 Idle / Move / Jump / Attack / Hit / Death 中不会留在原地。
- 武器配置为空时不报错，只是不显示该武器。

---

## 11. 后续扩展

P5 完成后，后续可以继续：

- P6：弓兵箭矢投射物。
- P6：主将法术特效。
- P7：箭袋、职业徽记、盾牌阵营色、武器攻击拖尾。
