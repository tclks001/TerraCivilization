# TerraCivilization SimpleGameplay C4 智能选中聚焦设计稿

> 本稿从 [CameraTrackingAndInteractionDesign.md](CameraTrackingAndInteractionDesign.md) 的 C4 阶段拆出。
>
> C4 改善“点击己方棋子后镜头是否需要移动”的判断。它不改变 Gameplay 规则，只改变选中棋子后的相机反馈。

---

## 1. 目标

C4 要达成：

1. 点击已经在当前视角中心附近的棋子时，相机不移动、不旋转。
2. 点击距离当前视角中心较远的棋子时，相机平滑把视角中心转移到该棋子。
3. 聚焦后继续保留 C3/C3.5/C3.6/C3.7 的手动相机状态，不被下一帧拉回旧焦点。
4. 自动聚焦尽量保留当前观察方位、距离和 C3.7 自动倾角逻辑。

---

## 2. 当前问题

G2.5 的选中逻辑是：

```text
点击己方棋子
    -> FocusCameraOnCell_(SelectedCellId, false)
    -> 不移动相机，只强制旋转看向棋子
```

问题：
- 玩家已经在观察棋子附近时，镜头仍可能强制旋转，打断观察方位。
- 棋子距离当前视角中心较远时，只旋转不移动，后续手动操作仍不自然。
- C3.5 后相机每帧由 Controller 的 `C3FocusUnitDir` 锚定；如果 Tess 直接改 ViewTarget，下一帧会被 Controller 拉回旧焦点。

---

## 3. 舒适区判定

C4 舒适区不再使用屏幕投影矩形，也不再使用 `C4ComfortScreenMarginFraction`。

新参数：

| 参数 | 默认值 | 作用 |
| --- | --- | --- |
| `bEnableC4SmartSelectionFocus` | `true` | 是否启用 C4 智能选中聚焦 |
| `C4ComfortFocusAngleDeg` | `18` | 所选单位与当前视角中心球面方向的最大舒适角距离 |
| `C4SelectedPieceFocusBlendSeconds` | `0.35` | 选中舒适区外棋子时的焦点 Blend 时长 |

已移除/重命名：

| 旧参数 | 新参数 | 原因 |
| --- | --- | --- |
| `C4ComfortScreenMarginFraction` | `C4ComfortFocusAngleDeg` | 旧名暗示屏幕边距，已不符合当前角距离判定 |

判定方式：

```text
CurrentFocusUnitDir = 当前视角中心所在球面方向
SelectedUnitDir = SelectedCell.UnitCenter
AngleDeg = acos(dot(CurrentFocusUnitDir, SelectedUnitDir))

若 AngleDeg <= C4ComfortFocusAngleDeg：
    认为在舒适区，相机不动
否则：
    请求 C4 自动聚焦到 SelectedUnitDir
```

`CurrentFocusUnitDir` 由当前相机位置与旋转通过 `SyncFocusCameraStateFromView(...)` 反解得到，与 C3 的焦点式相机几何保持一致。

---

## 4. 聚焦方式

C4 不直接设置相机 Actor。

正确链路：

```text
APlanetTessellatedMesh
    -> 判断选中 Cell 与当前视角中心的球面角距离是否舒适
    -> 若不舒适，向 APlanetInteractionController 请求 C4 自动聚焦
    -> Controller 平滑修改 C3FocusUnitDir
    -> Controller 每帧 ApplyFocusCameraState
```

这样可以保证：
- C3.5 每帧稳态锁定不会把镜头拉回旧位置。
- C3.7 自动倾角仍由距离派生。
- 聚焦结束后玩家继续用 `WSAD/Q/E/滚轮` 接管同一套 C3 状态。

---

## 5. 自动聚焦 Blend

初版只 Blend 焦点方向：

```text
StartFocusUnitDir = 当前 C3FocusUnitDir
TargetFocusUnitDir = SelectedCell.UnitCenter
FocusUnitDir = Slerp(Start, Target, Alpha)
```

保持：
- `C3DistanceToFocusCM`
- `C3YawAroundFocusDeg`
- C3.7 自动倾角派生规则

如果玩家在 Blend 过程中输入 `WSAD/Q/E/滚轮`，取消自动聚焦，交还手动控制。

---

## 6. 验收

1. 点击与当前视角中心球面角距离不超过 `C4ComfortFocusAngleDeg` 的己方棋子，镜头不移动、不旋转。
2. 点击超过 `C4ComfortFocusAngleDeg` 的己方棋子，镜头平滑把视角中心移动到该棋子。
3. 调小 `C4ComfortFocusAngleDeg` 会让 C4 更容易触发聚焦。
4. 调大 `C4ComfortFocusAngleDeg` 会让更多选中操作保持镜头不动。
5. 自动聚焦结束后，`WSAD/Q/E/滚轮` 继续基于新的焦点工作。
6. 自动聚焦过程中手动输入会取消聚焦。

---

## 7. 暂不处理

C4 暂不实现：
- 根据可移动/可跳跃目标自动扩大构图。
- 屏幕边缘箭头提示。
- 复杂相机 Blend 曲线。
- 行动过程跟随。
> Implementation note (2026-07-10): C4 selection comfort checks and focus
> requests are implemented in `UPlanetCameraComponent`. `APlanetTessellatedMesh`
> still triggers the behavior after Gameplay selection changes; the serialized C4
> tuning fields now live on `UPlanetCameraComponent`.
