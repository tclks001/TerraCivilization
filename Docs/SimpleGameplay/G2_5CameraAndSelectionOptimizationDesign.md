# TerraCivilization SimpleGameplay G2.5 视角变化和选中逻辑优化设计稿

> 本稿承接 G2：`Gameplay` 模块已经负责棋子、Cell 逻辑、普通移动和回合结束。
>
> G2.5 目标是在不改变 G2 行动规则的前提下，优化“轮到谁行动”和“当前指向/选中哪个棋子”的视觉反馈与相机反馈。

---

## 1. G2.5 目标

新增 3 类体验优化：

1. **回合开始相机切换**
   - 当某个阵营回合开始时，摄像机自动切到该阵营大本营正上方一定距离。
   - 摄像机朝向大本营中心，方向以球面参考系计算。

2. **当前阵营棋子底色提示**
   - 当前阵营所有棋子所在 Cell 显示淡粉色高亮。
   - 这表示“这些是当前玩家可关注的棋子”。

3. **hover / select 反馈优化**
   - hover 到当前阵营棋子时，该 Cell 从淡粉色变成更红一些。
   - 选中当前阵营棋子时，摄像机只旋转对准该棋子所在位置，不移动摄像机。
   - G2 已有选中 / 移动颜色保持不变：
     - 选中棋子脚下 Cell：黄色。
     - 移动后可停下 Cell：蓝色。

G2.5 不新增新行动规则，不改变普通移动、回合结束和棋子占用逻辑。

---

## 2. 高亮来源与优先级

G2.5 后同一个 Cell 可能同时具有多个高亮来源。

优先级从高到低：

| 优先级 | 来源 | 颜色 |
| --- | --- | --- |
| 400 | G2 移动后可停下 | 蓝色 `(0, 0.35, 1)` |
| 300 | G2 选中棋子脚下 | 黄色 `(1, 1, 0)` |
| 200 | hover 到当前阵营棋子 | 红粉色 `(1, 0.22, 0.32)` |
| 150 | 当前阵营棋子底色 | 淡粉色 `(1, 0.45, 0.68)` |
| 100 | 普通 hover 非当前阵营 / 空 Cell | 原 `HighlightHoverColor` 暖金 |
| 0 | 无高亮 | 黑色，强度 `0` |

关键规则：

- `FTerraGameplayContainer::GameplayHighlights` 继续表示 G2 行动高亮，优先级最高。
- 当前阵营棋子底色不写入 `GameplayHighlights`，由渲染桥根据 Gameplay 容器查询结果动态合成。
- 这样可以保证：
  - 当前阵营所有棋子默认淡粉。
  - hover 当前阵营棋子时变红。
  - 被选中的棋子仍显示黄色，不被红粉 hover 覆盖。
  - 移动后的棋子仍显示蓝色，不被淡粉/红粉覆盖。

---

## 3. Gameplay 容器新增查询接口

G2.5 不让渲染层直接遍历内部占用表推断阵营逻辑，而是在 `FTerraGameplayContainer` 增加只读查询接口：

```cpp
int32 GetCurrentFactionBaseCellId() const;
bool IsCurrentFactionPieceCell(int32 CellId) const;
bool CollectFactionPieceCellIds(int32 FactionId, TArray<int32>& OutCellIds) const;
bool CollectCurrentFactionPieceCellIds(TArray<int32>& OutCellIds) const;
bool TryGetPieceCellId(int32 PieceId, int32& OutCellId) const;
```

用途：

| 接口 | 用途 |
| --- | --- |
| `GetCurrentFactionBaseCellId` | 回合开始相机切到大本营上方 |
| `IsCurrentFactionPieceCell` | HISM 高亮合成时判断淡粉 / 红粉 |
| `CollectFactionPieceCellIds` | 回合切换时刷新上一阵营与新阵营棋子 Cell |
| `CollectCurrentFactionPieceCellIds` | Gameplay 初始化后刷新当前阵营棋子底色 |
| `TryGetPieceCellId` | 选中棋子后让相机旋转对准棋子 |

---

## 4. 相机定位算法

### 4.1 回合开始：移动并朝向大本营

给定大本营 `BaseCellId`：

```cpp
UnitDir = CellTopology->Cells[BaseCellId].UnitCenter;
SurfaceWorld = ActorTransform.TransformPosition(UnitDir * GlobeRadiusCM);
CameraWorld = ActorTransform.TransformPosition(UnitDir * (GlobeRadiusCM + G2_5TurnStartCameraHeightCM));
LookRotation = (SurfaceWorld - CameraWorld).Rotation();
```

然后：

```text
PlayerController->GetViewTarget()->SetActorLocation(CameraWorld)
PlayerController->GetViewTarget()->SetActorRotation(LookRotation)
PlayerController->SetControlRotation(LookRotation)
```

如果当前 ViewTarget 不存在，则只设置 `ControlRotation`。

### 4.2 选中棋子：只旋转不移动

给定棋子所在 `CellId`：

```cpp
TargetWorld = ActorTransform.TransformPosition(UnitDir * GlobeRadiusCM);
CameraWorld = CurrentViewTarget->GetActorLocation();
LookRotation = (TargetWorld - CameraWorld).Rotation();
```

然后只执行：

```text
ViewTarget->SetActorRotation(LookRotation)
PlayerController->SetControlRotation(LookRotation)
```

不改变相机 / Pawn / ViewTarget 的位置。

---

## 5. `APlanetTessellatedMesh` 新增字段

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlanetTopology|Tess|SimpleGameplay G2.5")
bool bEnableG2_5CameraAssist = true;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlanetTopology|Tess|SimpleGameplay G2.5")
float G2_5TurnStartCameraHeightCM = 8000.0f;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlanetTopology|Tess|SimpleGameplay G2.5")
FLinearColor G2_5CurrentFactionPieceColor = FLinearColor(1.0f, 0.45f, 0.68f, 1.0f);

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlanetTopology|Tess|SimpleGameplay G2.5")
FLinearColor G2_5CurrentFactionPieceHoverColor = FLinearColor(1.0f, 0.22f, 0.32f, 1.0f);
```

---

## 6. 点击后的事件判断

`HandleHISMClickHit` 调用 Gameplay 容器前记录：

```cpp
PrevFactionId = GameplayContainer->GetCurrentFactionId();
PrevSelectedPieceId = GameplayContainer->GetSelectedPieceId();
```

调用后读取：

```cpp
NewFactionId = GameplayContainer->GetCurrentFactionId();
NewSelectedPieceId = GameplayContainer->GetSelectedPieceId();
NewPhase = GameplayContainer->GetInteractionPhase();
```

然后：

1. 如果 `NewFactionId != PrevFactionId`：
   - 刷新上一阵营和新阵营所有棋子 Cell 高亮。
   - 调用 `FocusCameraOnCurrentFactionBase_(true)`，移动相机到新阵营大本营上方。
2. 如果 `NewSelectedPieceId != PrevSelectedPieceId` 且 `NewPhase == PieceSelected`：
   - 调用 `FocusCameraOnCell_(SelectedPieceCellId, false)`，只旋转视角对准棋子。

---

## 7. 验收清单

### 7.1 初始化 / 回合开始

- PIE 开始后当前阵营棋子所在 Cell 全部显示淡粉色。
- 摄像机自动切到当前阵营大本营上方，并朝向大本营。

### 7.2 Hover

- hover 到当前阵营棋子所在 Cell：淡粉色变成更红。
- hover 离开后恢复淡粉色。
- hover 到非当前阵营棋子或空 Cell：仍使用普通 hover 暖金色。

### 7.3 选中

- 点击当前阵营可移动棋子：
  - 该棋子脚下 Cell 显示 G2 黄色。
  - 摄像机不移动，只旋转对准该棋子。
- 选中颜色不被红粉 hover 覆盖。

### 7.4 移动与结束回合

- 普通移动后：
  - 新脚下 Cell 显示 G2 蓝色。
  - 其他当前阵营棋子仍显示淡粉色。
- 点击蓝色 Cell 结束回合后：
  - 旧阵营淡粉色消失。
  - 新阵营棋子显示淡粉色。
  - 摄像机切到新阵营大本营上方并朝下。
