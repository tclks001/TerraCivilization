# TerraCivilization SimpleGameplay 视角追踪与交互设计稿

> 本稿用于把当前 SimpleGameplay 已经实现的视角、鼠标交互、手动相机控制、撤销交互，以及后续更适合球面策略游戏的视角追踪方案统一整理为一个独立大模块。
>
> 本稿与 [PieceAnimationPresentationDesign.md](PieceAnimationPresentationDesign.md) 平级。棋子动画表现负责“棋子如何动”，本稿负责“玩家如何看见、选择、追踪和理解棋局”。
>
> 当前已有阶段稿：
>
> - [G2_5CameraAndSelectionOptimizationDesign.md](G2_5CameraAndSelectionOptimizationDesign.md)
> - [G8CameraAndInteractionDesign.md](G8CameraAndInteractionDesign.md)
> - [G9UndoDesign.md](G9UndoDesign.md)
> - [HISMCellHighlightOverlayDesign.md](HISMCellHighlightOverlayDesign.md)
>
> 当前待完成 / 暂搁置阶段：
>
> - **C7：对手回合观看与视角恢复**。目前项目尚未设计并落地简单 AI，无法稳定测试“对手行动时”的镜头切换与视角恢复，因此 C7 暂不实现；等简单 AI 落地后再恢复设计和实现。

---

## 1. 目标

视角追踪与交互模块要解决的是：

```text
Gameplay 规则状态
    -> 玩家当前该关注什么
    -> 镜头如何展示
    -> 玩家如何移动视角、寻找棋子、选择棋子、撤销误操作
    -> 玩家能稳定理解当前棋局
```

核心目标：

1. **让球面棋盘可读**
   - 玩家不应该因为棋子分布在球体背面而迷失。
   - 镜头需要提供阵营中心、选中棋子、行动目标、攻击对象等关注点。

2. **自动镜头和手动镜头共存**
   - 自动镜头负责回合开始、选中、行动演出等关键时刻。
   - 手动镜头负责玩家自由观察。
   - 玩家手动操作应能打断自动镜头，避免镜头抢控制权。

3. **交互反馈与规则状态一致**
   - hover、select、可移动格、可吃子预览、当前阵营提示都必须来自 Gameplay 状态或 HISM 命中结果。
   - 相机不参与规则判定。

4. **为后续动画表现和行动演出留接口**
   - P2/P3 后，棋子移动、跳跃、攻击、死亡都需要镜头追踪或框选。
   - 本模块应能逐步承接“行动演出镜头”，但不把动画逻辑塞进相机层。

---

## 2. 当前基础

### 2.1 场景与输入链路

当前输入链路：

```text
APlanetInteractionController
    -> 鼠标射线 / 键盘 / 滚轮输入
    -> APlanetBinder
    -> APlanetTessellatedMesh
    -> FTerraGameplayContainer
```

主要类型：

| 类型 | 当前职责 |
| --- | --- |
| `APlanetInteractionController` | 鼠标 hover/click、右键 undo、WSAD/滚轮相机输入 |
| `APlanetBinder` | 旧路径的球体 hover/click 桥接；当前 HISM 路径优先 |
| `APlanetTessellatedMesh` | HISM 拾取、高亮合成、Gameplay 宿主、相机桥接 |
| `FTerraGameplayContainer` | 选中、移动、跳跃、吃子、回合、Undo 规则状态 |

### 2.2 当前渲染交互基础

当前 Cell 主交互走 HISM：

- HISM tile rendering。
- HISM collision。
- 鼠标 hover / click 命中 HISM 实例。
- 通过 InstanceIndex 反查 CellId。
- Cell 高亮通过 HISM PerInstanceCustomData 输出最终颜色和强度。

旧的 ProceduralMesh / Binder 路径仍作为 fallback 和历史兼容存在，但当前 SimpleGameplay 交互应优先依赖 HISM。

---

## 3. 已实现能力整理

### 3.1 G2.5：回合开始与选中相机辅助

详见 [G2_5CameraAndSelectionOptimizationDesign.md](G2_5CameraAndSelectionOptimizationDesign.md)。

当前行为：

1. 回合开始时：
   - 自动切到当前阵营大本营 / 主将相关 Cell 正上方。
   - 相机朝向该 Cell。

2. 点击当前阵营棋子时：
   - 相机不移动位置。
   - 只旋转视角对准该棋子所在 Cell。

3. 当前阵营棋子提示：
   - 当前阵营棋子所在 Cell 显示淡粉色。
   - hover 当前阵营棋子时变红粉色。

当前局限：

- 回合开始视角过于垂直向下，空间感弱。
- 镜头关注主将/大本营，但玩家真正需要关注的经常是前线和可行动棋子。
- 点击棋子时强制旋转可能打断玩家原本观察方向。
- 没有判断棋子是否已经在屏幕内。

### 3.2 G8：WSAD + 滚轮手动球面轨道

详见 [G8CameraAndInteractionDesign.md](G8CameraAndInteractionDesign.md)。

当前行为：

| 输入 | 当前语义 |
| --- | --- |
| `W/S` | 改变相机纬度 |
| `A/D` | 改变相机经度 |
| 滚轮上 | 拉近 |
| 滚轮下 | 拉远 |

当前相机状态：

```text
LongitudeDeg
LatitudeDeg
HeightOffsetCM
```

当前实现特点：

- 相机位置由球面经纬度 + 高度还原。
- 相机朝向球心。
- 自动聚焦后会同步 G8 轨道参数，避免下一次 WSAD 把相机拉回旧位置。

当前局限：

- WSAD 改的是相机经纬度，不是“观察焦点”。
- 相机始终看球心，容易变成绕球观察工具，而不是策略游戏里的斜俯视焦点相机。
- 在极区、背面和大范围棋子分布场景中，玩家仍然容易迷路。
- 滚轮缩放只改高度，但当前视角姿态仍被“看球心”逻辑限制。

### 3.3 G9：右键撤销当前点选

详见 [G9UndoDesign.md](G9UndoDesign.md)。

当前行为：

- 右键撤销当前未提交行动的上一次成功点击。
- 使用 Gameplay 操作栈回退。
- 撤销棋子位置、选中状态、高亮状态、可移动/可跳跃目标、行动路径缓存。
- 不回滚相机。

当前相机语义：

- Undo 后保持当前视角。
- 这是合理的初版行为：撤销是纠错操作，不应额外抢镜头。

### 3.4 HISM Hover / Click / Highlight

详见 [HISMCellHighlightOverlayDesign.md](HISMCellHighlightOverlayDesign.md)。

当前行为：

- 鼠标 hover HISM Cell：写入 hover 高亮。
- 鼠标 click HISM Cell：转交 Gameplay 处理。
- 鼠标离开：hover 进入短暂 fade / 清理流程。
- Gameplay 高亮、当前阵营棋子底色、hover、高亮预览按优先级合成最终颜色。

当前高亮优先级大体为：

```text
Gameplay 行动高亮
    > G3/G4 行动目标 hover 加深
    > 当前阵营棋子 hover
    > 当前阵营棋子底色
    > 普通 hover
    > 无高亮
```

---

## 4. 模块边界

### 4.1 本模块负责

视角追踪与交互模块负责：

- 自动镜头目标选择。
- 自动镜头平滑过渡。
- 手动相机控制。
- 输入打断自动镜头。
- 鼠标 hover / click 的体验层组织。
- 当前阵营棋子导航。
- 选中棋子导航。
- 行动过程中镜头追踪。
- 非玩家/其他阵营行动的观看镜头。

### 4.2 本模块不负责

本模块不负责：

- 移动是否合法。
- 吃子是否合法。
- 胜负判定。
- 棋子动画状态机。
- 骨骼动画播放。
- 地形生成。

对应边界：

| 模块 | 职责 |
| --- | --- |
| `Gameplay` | 规则真值、回合、合法行动、胜负 |
| `PiecePresentation` | 棋子 Actor、位移插值、动画状态 |
| `APlanetTessellatedMesh` | 地形/HISM 宿主、相机/交互桥接 |
| 视角追踪与交互模块 | 如何看、如何找、如何选择、如何追踪 |

---

## 5. 推荐相机模型

当前 G8 是“相机绕球心轨道”。未来建议升级为“观察焦点 + 斜俯视姿态”的相机模型。

### 5.1 当前模型

```text
CameraLongitudeDeg
CameraLatitudeDeg
CameraHeightOffsetCM
LookAt = PlanetCenter
```

优点：

- 简单。
- 调试稳定。
- 不需要维护焦点。

缺点：

- 策略游戏体验偏硬。
- WSAD 和滚轮不像“在地图上移动视角”。
- 过于容易变成垂直俯视或看球心的全局观察。

### 5.2 目标模型

建议未来相机状态改为：

```text
FocusWorldPoint / FocusCellId
FocusUnitDir
Distance
TiltDeg
YawAroundFocusDeg
```

含义：

| 字段 | 含义 |
| --- | --- |
| `FocusCellId` | 当前镜头关注的 Cell，可为空 |
| `FocusUnitDir` | 当前球面焦点方向 |
| `Distance` | 相机离焦点距离 |
| `TiltDeg` | 相机斜俯视角，不再固定看球心 |
| `YawAroundFocusDeg` | 绕焦点的水平观察方向 |

目标行为：

- WSAD 移动焦点，而不是直接移动相机经纬度。
- 滚轮只改 Distance。
- Tilt/Yaw 保持玩家当前姿态。
- 自动镜头只改变焦点和必要的视角参数，不强制垂直俯视。

---

## 6. 自动镜头建议

### 6.1 回合开始

当前行为：

```text
切到当前阵营主将/大本营正上方
```

推荐未来行为：

```text
切到当前阵营战区中心的斜俯视
```

战区中心初版可取：

```text
当前阵营所有活棋子 UnitCenter 的归一化平均方向
```

或更进一步：

```text
当前阵营所有可行动棋子 + 附近敌方棋子 的加权中心
```

视角建议：

- 高度：中高空。
- 俯仰：约 45°~60°。
- 方位：从主将方向看向前线，或尽量保持玩家上一回合的方位。
- 切换：0.5~0.8 秒平滑过渡。

原则：

- 玩家需要看“这回合能动谁”，不是只看主将。
- 不要垂直向下。
- 不要每回合都把玩家视角重置到完全相同的死板角度。

### 6.2 点击己方棋子

当前行为：

```text
只旋转当前相机，强制对准该棋子
```

推荐未来行为：

```text
如果棋子已在舒适视野内：不动相机
如果棋子在屏幕边缘：轻微移动/旋转到可读区域
如果棋子完全不在屏幕内：平滑切到棋子附近斜俯视
```

点击棋子后镜头目标：

- 选中棋子可见。
- 可移动/可跳跃目标可见。
- 尽量保持玩家原来的观察方位。

原则：

- 玩家已经看得到的东西，不要替玩家重切镜头。
- 镜头服务于“看清可行动范围”，不是单纯看棋子本身。

### 6.3 棋子移动 / 跳跃

普通一步移动：

- 如果起点和终点都在屏幕内：相机不动。
- 如果终点会出屏：轻微跟随，让终点留在画面内。

跳跃 / 连跳：

- 可启用轻量跟随。
- 关注区域应包含：
  - 当前棋子位置。
  - 下一跳可能落点。
  - 当前可吃子预览。

确认回合后的攻击 / 吃子演出：

- 如果发生吃子，镜头应保证攻击者和被吃目标同时可见。
- 如果没有吃子，保持当前玩家视角即可。

### 6.4 别人的回合

未来若有 AI 或联网对手：

```text
对手开始回合：不立刻强制切镜头
对手实际行动：切到行动区域
对手行动结束：恢复或半恢复玩家之前视角
```

建议保存：

```text
PlayerPreferredCameraState
```

用于在对手行动演出结束后恢复玩家观察位。

调试模式可保留：

```text
bAutoFocusEveryFactionTurn
```

用于本地多阵营轮流操作时仍自动切换到当前阵营。

---

## 7. 手动交互建议

### 7.1 WSAD

当前：

```text
W/S 改纬度
A/D 改经度
```

推荐未来：

```text
WSAD 按屏幕方向移动球面焦点
```

含义：

- `W`：焦点向屏幕上方对应的球面方向移动。
- `S`：焦点向屏幕下方。
- `A`：焦点向屏幕左侧。
- `D`：焦点向屏幕右侧。

优势：

- 手感接近 RTS / 4X 地图平移。
- 避免极区经纬度控制退化。
- 保持当前斜俯视姿态。

### 7.2 滚轮缩放

推荐规则：

- 滚轮只改变 `Distance`。
- 不改变焦点。
- 不改变 Tilt。
- 不改变 YawAroundFocus。

目标：

```text
拉近/拉远，但保持原本视角构图
```

### 7.3 鼠标拖拽

未来可增加：

| 输入 | 建议作用 |
| --- | --- |
| 鼠标中键拖拽 | 旋转观察方向或平移焦点 |
| 鼠标右键拖拽 | 旋转观察方向 |
| `Q/E` | 绕焦点旋转镜头 |
| `R` | 回到推荐斜俯视角 |
| `Home` | 回到当前阵营战区中心 |

注意：

- 当前右键已经用于 Undo。
- 如果未来使用右键拖拽，需要区分“短按右键 Undo”和“右键拖拽旋转”。

### 7.4 自动镜头打断

推荐统一规则：

```text
AutoCameraBlend 正在进行
玩家输入 WSAD / 滚轮 / 拖拽 / Q/E
=> 立即取消自动镜头
=> 进入 ManualCameraMode
```

原因：

- 玩家手动操作优先级应高于自动镜头。
- 不允许相机和玩家抢控制权。

---

## 8. 棋子导航建议

球面地图中，己方棋子可能分布在整个球上。仅靠相机旋转不够。

### 8.1 Tab 循环己方棋子

建议新增：

| 输入 | 作用 |
| --- | --- |
| `Tab` | 选中/聚焦下一个当前阵营可行动棋子 |
| `Shift + Tab` | 上一个 |

行为：

- 如果当前没有选中棋子：选中第一个可行动棋子。
- 如果已有选中棋子：切到下一个。
- 镜头平滑移动到该棋子附近斜俯视。
- 高亮逻辑复用当前 G2/G3 选中与移动目标。

### 8.2 屏幕外提示

未来可为屏幕外己方棋子显示边缘提示：

```text
屏幕边缘箭头 / 小点
```

第一版可以只显示当前阵营可行动棋子的数量和 Tab 索引。

### 8.3 阵营中心快捷键

建议：

| 输入 | 作用 |
| --- | --- |
| `Home` | 回到当前阵营战区中心 |
| `Ctrl + Home` | 回到当前阵营主将 |

---

## 9. 与动画表现模块的衔接

棋子动画表现模块会逐步提供：

- 移动段。
- 跳跃段。
- 攻击事件。
- 受击 / 死亡事件。

视角追踪模块未来可消费这些表现事件，但不直接驱动规则：

```text
Gameplay 规则
    -> PiecePresentation 表现事件
    -> CameraTracking 选择镜头关注区域
```

建议事件映射：

| 表现事件 | 镜头行为 |
| --- | --- |
| `Move` | 起终点都在屏幕内则不动；终点出屏则轻跟随 |
| `Jump` | 跟随选中棋子和下一跳候选范围 |
| `AttackMelee` | 框住攻击者与目标 |
| `AttackRanged` | 框住射手与目标；未来可追踪投射物 |
| `Death` | 不单独抢镜，除非目标是主将 |
| `MatchEnded` | 切到胜者主将或战区中心 |

---

## 10. 推荐阶段目标

### C1：现状整理与文档基准

目标：

- 将 G2.5 / G8 / G9 / HISM 交互现状统一整理到本稿。
- 明确当前问题和未来方向。

验收：

- 本稿存在并作为视角交互大模块基准。

### C2：游戏开始斜俯视战区中心

阶段稿：[C2TurnStartWarZoneCameraDesign.md](C2TurnStartWarZoneCameraDesign.md)。

目标：

- 游戏开始时不再垂直俯视主将。
- 只在本局开始之初硬设置一次镜头到当前阵营战区中心斜俯视。
- C2 初版已落地为当前阵营所有活棋子 Cell 方向的归一化平均值。
- 距离使用 `C3InitialDistanceToFocusCM`，倾角使用 C3.7 自动倾角逻辑。

验收：

- PIE / 游戏开始时镜头硬设置到当前阵营整体棋子附近。
- 后续回合开始不再执行硬设置，交给 C2.5 平滑切焦点。
- 视角有斜俯视空间感。
- `WSAD / Q/E / 滚轮` 可在初始镜头后继续接管。

### C2.5：回合开始平滑切战区焦点

阶段稿：[C2_5TurnStartWarZoneFocusBlendDesign.md](C2_5TurnStartWarZoneFocusBlendDesign.md)。

目标：

- 每次回合开始时，平滑把 C3 视角中心切到当前阵营战区中心。
- 不改变摄像机到视角中心的距离。
- 不改变当前 `YawAroundFocusDeg`，尽量保留玩家观察方位。
- C3.7 继续按当前距离派生倾角。

验收：

- 第二个及后续回合开始时，镜头焦点平滑滑向当前阵营整体棋子附近。
- 切换过程中相机距离不发生跳变。
- 手动输入可按 C3/C4 既有规则接管自动 Blend。

### C3：焦点式手动相机

阶段稿：[C3FocusCameraManualControlDesign.md](C3FocusCameraManualControlDesign.md)。

目标：

- WSAD 移动观察焦点。
- 滚轮只改变距离。
- 保持当前 Tilt / Yaw。
- C3 初版在当前球面视野中心点建立局部切平面坐标系，沿切线移动视野中心，保持视线倾角和到焦点距离。

验收：

- 手动控制不再直接变成垂直看球心。
- 极区操作不再明显退化。
- `WSAD` 不改变视野中球体的相对屏幕位置，只沿当前视野中心的局部切平面移动焦点。

### C3.5：无输入帧相机稳态

阶段稿：[C3_5IdleFrameCameraStabilityDesign.md](C3_5IdleFrameCameraStabilityDesign.md)。

目标：

- 修复 C3 收尾发现的一个副作用：在 C3 手动模式下松开 W/S/A/D、无任何输入的帧里，相机会缓慢漂移，只有再次按下按键触发 Apply 才被拉回焦点。
- 让相机在无输入帧仍严格锚定当前 `(FocusUnitDir, Yaw, Tilt, Distance)`，不接受来自 UE 默认 Pawn / PlayerController 的隐式姿态回写。

验收：

- 进入 C3 手动模式后，若玩家不做任何输入，相机在数秒内不发生任何可见位移与旋转。
- 松开 W/S/A/D 的瞬间与松开前的最后一帧姿态完全一致，且此后保持不动。
- 与 C4 智能选中聚焦兼容：C3.5 的稳态锁只对 C3 手动状态生效，不影响未来自动镜头 Blend。

### C3.6：Q/E 绕焦点法线旋转视角

阶段稿：[C3_6QERollAroundFocusDesign.md](C3_6QERollAroundFocusDesign.md)。

目标：

- 按下 `Q/E` 让相机绕当前视野中心的球面外法线旋转，玩家从另一个方位看向同一视野中心。
- 保持 `FocusUnitDir` / `TiltDeg` / `DistanceToFocusCM` 不变。
- 转向后 WSAD 仍以新视角为参考系（`W` 是屏幕上方方向、`A/D` 是屏幕左右方向），无需玩家心算换算。
- 语义上等价于只让 C3 内部真值里的 `YawAroundFocusDeg` 加减输入，不新增任何持久状态。

验收：

- 按住 `E`（或 `Q`）时视野中心地形在屏幕正中不动，相机绕焦点在同一水平圆上滑动。
- 转到任意方位后按 `W`，焦点向新的屏幕上方推进；按 `A/D` 同理。
- C3 §7、C3.5 §6 原有验收条款仍成立。

### C3.7：自动倾角跟随距离插值

阶段稿：[C3_7AutoTiltFromDistanceDesign.md](C3_7AutoTiltFromDistanceDesign.md)。

目标：

- 取消 C3 手动相机中独立维护的 `TiltDeg` 状态。
- 改为每帧从 `DistanceToFocusCM` 线性插值派生倾角。
- 玩家滚轮拉近 → 倾角自动降低（接近平视细节），拉远 → 倾角自动升高（接近俯瞰全局）。
- 消除 C3 中"无输入即冻结 Tilt"的死参数问题。

验收：

- 拉到最近距离时倾角接近 `C3AutoTiltAtMinDistanceDeg`（默认 10°），构图接近平视。
- 拉到最远距离时倾角接近 `C3AutoTiltAtMaxDistanceDeg`（默认 85°），构图接近俯瞰。
- 滚轮缩放中间档位倾角线性变化，无跳变。
- 缩放不影响 WSAD / Q/E 的原有行为。
- C3 §7、C3.5 §6、C3.6 §6 原有验收条款仍成立。

### C4：智能选中聚焦

阶段稿：[C4SmartSelectionFocusDesign.md](C4SmartSelectionFocusDesign.md)。

目标：

- 点击棋子时判断它与当前视角中心所在球面坐标的角距离是否在舒适阈值内。
- 只在角距离超过阈值时移动镜头焦点。
- C4 初版由 Tess 判断选中棋子是否在焦点角距离舒适区内，由 Controller 平滑修改 C3 焦点状态。

验收：

- 玩家点击角距离舒适区内的棋子时镜头不乱动。
- 点击角距离舒适区外的棋子时镜头平滑切过去。
- 聚焦结束后 C3/C3.5/C3.6/C3.7 手动相机状态继续工作，不被拉回旧焦点。

### C5：Tab 棋子导航

阶段稿：[C5TabPieceNavigationDesign.md](C5TabPieceNavigationDesign.md)。

目标：

- `Tab / Shift+Tab` 循环当前阵营可行动棋子。
- 支持快速找回分布在球体背面的己方棋子。
- 与鼠标点击己方棋子的选择逻辑共用同一条 Gameplay / 高亮 / C4 智能聚焦链路。

验收：

- 玩家无需手动绕球寻找所有己方棋子。
- 无选中棋子时按 `Tab` 等价于点击当前阵营可行动棋子中 PieceId 最小的棋子。
- 有选中棋子时按 `Tab / Shift+Tab` 可在当前阵营可行动棋子中前后循环。
- Tab 选中后按右键 Undo，仍按 G9 规则回退到本次点选前状态。

### C6：行动镜头追踪

阶段稿：[C6ActionCameraTrackingDesign.md](C6ActionCameraTrackingDesign.md)。

目标：

- 棋子移动 / 跳跃时，若落点即将离开 C4 定义的焦点角距离舒适区，则镜头与棋子行动同步平滑移动到落点。
- Undo 已发生的移动 / 跳跃时，若回退后的原位置在舒适区外，则镜头同步平滑移动回原位置。
- 镜头追踪过程中，玩家任意有效输入都会立刻打断自动镜头。

验收：

- 棋子落点仍在 C4 舒适区内时，行动镜头不移动。
- 棋子落点超出 C4 舒适区时，棋子移动动画和镜头焦点 Blend 同时开始，镜头目标为落点。
- Undo 一次已移动 / 已跳跃的棋子时，模型按反向移动表现回退；若回退目标在舒适区外，镜头也同步回退。
- 自动追踪过程中按 `WSAD/QE/滚轮/鼠标点击/右键/Tab` 任一有效输入，镜头追踪立刻停止。

### C6.5：行动表现结束后再回合回正

阶段稿：[C6_5DelayedTurnStartFocusDesign.md](C6_5DelayedTurnStartFocusDesign.md)。

目标：

- 行动确认并切到下一回合后，如果上个棋子的移动、跳跃、攻击、受击、死亡或淡出表现仍在播放，不立刻执行 C2.5 回合战区回正。
- 等本次行动表现预计结束后，再调用既有 C2/C2.5 回合镜头入口。
- 本阶段只修改镜头表现，不延后 Gameplay 回合推进。

验收：

- 普通移动 / 跳跃 / 吃子行动提交后，镜头不会在表现开始前直接切到下一阵营战区中心。
- 表现结束后，镜头按 C2.5 规则平滑回正到当前阵营战区中心。
- 无行动表现的换回合路径保持原有立即回正。

### C7：对手回合观看与视角恢复（待完成 / 暂搁置）

状态：

- 暂搁置。
- 阻塞原因：当前项目尚未设计并落地简单 AI，无法稳定制造和验证“对手行动时”的镜头切换、行动观看和玩家视角恢复流程。
- 恢复条件：简单 AI 或其他可自动执行对手行动的机制落地后，再补 C7 阶段设计稿并实现 C++。

目标：

- 对手行动时切到行动区域。
- 行动结束后恢复玩家偏好视角。

验收：

- 玩家能看见对手关键行动。
- 不会因为对手回合丢失自己的观察位置。

---

## 11. 实现注意事项

1. **不要把相机状态塞进 Gameplay**
   - Gameplay 只提供阵营、棋子、Cell 状态。
   - 相机状态属于表现和输入层。

2. **自动镜头必须可被打断**
   - 这是体验底线。

3. **不要每次同步都重置视角**
   - 只有回合开始、明确选中、行动演出等事件才触发自动镜头。

4. **不要把“看球心”作为最终相机模型**
   - 它适合调试，不适合长期策略体验。

5. **HISM 高亮和相机关注应共享 CellId，但不要共享职责**
   - 高亮解决“哪里可点/已选”。
   - 相机解决“玩家该看哪里”。

6. **保留调试开关**
   - 当前自动聚焦、手动相机、未来行动跟随都应能独立开关。

---

## 12. 当前暂不落地

本稿只做大模块设计基准，暂不落地：

- 新相机组件。
- 焦点式相机状态结构。
- 平滑镜头 Blend。
- 屏幕边缘提示。
- 鼠标拖拽旋转。
- 对手回合视角恢复。

后续如果要实现，应先为对应阶段补充子设计稿，再改 C++。
> Implementation note (2026-07-10): camera behavior code now lives in
> `Source/TerraCivilization/Public/Render/PlanetCameraController.h` and
> `Source/TerraCivilization/Private/Render/PlanetCameraController.cpp`.
> `APlanetTessellatedMesh` keeps serialized camera UPROPERTY settings and public
> compatibility wrappers, but gameplay triggers delegate to `FPlanetCameraController`.
