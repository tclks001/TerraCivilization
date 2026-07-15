# TerraCivilization UI1 局内 HUD、暂停与行动日志设计稿

> UI1 承接 [UI0FrontEndDesign.md](UI0FrontEndDesign.md)，实现局内的最小信息层：右侧可开合的行动日志和 ESC 暂停菜单。它不实现存档、设置、科技选择、教学、历史记录、胜负结算或 Agent 面板。

---

## 1. 目标与边界

- 玩家从 UI0 的“开始游戏”进入棋盘时，创建一个不阻塞棋盘输入的 HUD。
- HUD 右侧显示“行动日志”，默认展开，可随时收起；它只添加已经确认结束回合的 G7 行动。
- 每项日志使用简体中文自然语言，描述行动阵营、行动兵种、跳跃段数、升变、击杀对象和阵营击败；不做策略评价或自由文本推理。
- 玩家在局内按 ESC 打开暂停菜单；暂停菜单阻断输入并暂停世界，点击“继续”恢复世界和棋盘输入。
- UI1 不读取 JSONL 文件来驱动当前 HUD，不复制或解析 `Saved/Logs`；文件日志与 HUD 同由同一份已确认行动数据产生。

## 2. UI 路由与输入

```text
InGame
  -> HUD (Viewport Z=10, 非阻塞)
  -> ESC -> Paused (Viewport Z=100, UI Only, World Paused)
  -> Continue / ESC -> InGame (Game Only, World Unpaused)
```

`UTerraUISubsystem` 继续是唯一调用 `SetInputMode`、`SetGamePaused` 的类型。HUD 不调用这些函数；它只显示数据。暂停菜单和主页面使用高层级全屏 Widget，HUD 始终保留在暂停菜单下方。

HUD 自身、根 Canvas 与日志容器必须使用 `SelfHitTestInvisible`：只有“行动日志”开合按钮接收 UI 点击，右侧面板的空白区和整个屏幕其余区域都必须穿透到 `APlanetInteractionController` 的 HISM hover/click 射线。`InGame` 是非阻塞路由，`IsBlockingGameInput()` 只能在主菜单、新游戏配置和暂停时返回 true；否则会让 Controller 提前退出 `PlayerTick`，使所有棋盘 hover/click 失效。该 Controller 同时永久忽略 UE 默认 Look 输入；相机只接受既有 WSAD/QE/滚轮路径，避免离开 UI 后移动鼠标改变视角。

UI0 前台关闭并进入新局时，Subsystem 调用既有 `FocusCameraOnCurrentFactionBase()`，清除菜单期间默认 Look 输入留下的偏移并将当前阵营基地重新置于屏幕焦点。暂停恢复不调用该接口，避免打断玩家已建立的镜头位置。

### 2.1 暂停相机状态同步

未被 C3 主动写入时，画面的最终权威不是某个 UI 状态，而是 `PlayerCameraManager` 的最终相机位置/旋转；它会结合 `PlayerController::ControlRotation` 和 `ViewTarget` Actor 的变换。C3 则保存 `FocusUnitDir`、距离和 Yaw，供下一帧 `ApplyFocusCameraState` 写回。

暂停前必须调用 `APlanetInteractionController::CaptureCurrentViewForPause()`：以 `PlayerCameraManager` 的最终渲染视图同步 C3，取消未完成的自动聚焦 Blend，再用同步后的 C3 状态立即写回 ViewTarget/ControlRotation。之后才调用 `SetGamePaused(true)` 和显示暂停菜单。这样暂停冻结的是当前看到的画面，不会在 Controller Tick 被停止后短暂回落到旧的 C3 焦点状态。

## 3. G7 到 HUD 的数据桥

### 3.1 权威记录

`FTerraGameplayContainer::EmitActionLog_` 在原有 JSONL/`UE_LOG` 输出前，同时追加一份 `FTerraGameplayActionLogEntry` 到内存数组。字段与 G7 JSON 一致：

| 字段 | HUD 用途 |
| --- | --- |
| `TurnIndex` | 显示“第 N 回合”。 |
| `PlayerId` | 显示“阵营 N”。 |
| `PieceId`、`ActionPieceType` | 显示哪个阵营移动了什么兵种。 |
| `PathCellIds` | 显示起点、终点和跳跃段数。 |
| `bPromoted`、`PromotedPieceType` | 显示升变后的兵种。 |
| `CapturedPieces` | 显示被击杀棋子的原阵营与原兵种。 |
| `DefeatedFactionIds` | 显示本次结算击败的阵营。 |
| `CapturedPieceIds`、`AttackerPieceIds`、`VanguardPieceIds` | 保留给 G7 文件核对和后续详情/回放。 |

`UPlanetGameplayComponent` 在一次 `HandleGameplayCellClick` 前后比较已提交记录数量；新记录出现时广播 `OnActionLogCommitted`。这样玩家点击、教学脚本和 NPC MCP 通过同一合法行动入口完成的 G7 行动都会被 HUD 收到。

### 3.2 中文摘要规则

普通行动：

```text
第 12 回合：阵营 3 移动步兵 41，从地块 128 到地块 145。
```

有连跳：

```text
第 12 回合：阵营 3 移动骑兵 41，从地块 128 到地块 145，进行了 2 段跳跃。
```

有捕获：

```text
第 12 回合：阵营 3 移动步兵 41，从地块 128 到地块 145，升变成弓骑兵，击杀了阵营 5 的弓兵，击杀了阵营 7 的主将，击败了阵营 7。
```

UI1 故意保留 CellId、PieceId 和阵营编号，方便当前调试与 G7 文件互相核对。捕获目标和阵营失败在 `ResolvePendingCaptures_` 前冻结其原始阵营/兵种，避免 G11 转归后日志错误地把被击杀棋子显示成征服方单位。阵营展示名和 Cell 可读名称在后续数据资产稳定后替换。

## 4. 默认资产与编辑器挂载

UI1 的 C++ fallback 没有任何 Widget Blueprint 时也可运行。最终资产按以下路径创建：

```text
Content/UI/
  Widgets/
    WBP_InGameHUD.uasset
    WBP_PauseMenu.uasset
```

在编辑器中创建 `Widget Blueprint`，父类分别选择 `TerraInGameHUDWidget` 与 `TerraPauseMenuWidget`，然后进入 **Project Settings -> Terra UI -> In Game**：

- `In Game HUD Widget Class` 挂载 `WBP_InGameHUD`。
- `Pause Menu Widget Class` 挂载 `WBP_PauseMenu`。

Widget Blueprint 可以重做布局和美术，但必须在其事件图调用/绑定父类的现有路由入口：暂停继续调用 `UTerraUISubsystem::ResumeGame()`；日志应使用 `UPlanetGameplayComponent::OnActionLogCommitted` 或 `CollectCommittedActionLogEntries`，不得读取 JSONL 轮询。

## 5. 验收

- 开始一局游戏后，右侧出现“行动日志”标题；其展开/收起不改变相机和棋盘输入。
- 完成一次普通移动并结束回合后，HUD 追加一条中文日志，且 G7 JSONL 同时只追加一行。
- 连跳和捕获动作仍各只追加一条 HUD 日志，摘要中的连跳/捕获数量与 G7 数组一致。
- 新行动追加后，HUD 在后续 3 个 Slate Tick 中读取 `GetScrollOffsetOfEnd()` 并直接调用 `SetScrollOffset()`。自动换行的最终高度会在这些布局 pass 中稳定，因此最后一条多行日志会完整可见；收起时同样保持最新位置。
- ESC 后世界暂停且棋盘不能交互；点击“继续”后恢复。
- NPC MCP 和教学脚本导致的已确认行动同样出现在 HUD。
- HUD 日志不因 Undo、选中、移动预览或失败的点击而追加。

## 6. 后续接口

UI2 可在本 HUD 上增加回合概览、选中棋子、教学、Agent 和科技模态。UI3 将 `FTerraGameplayActionLogEntry` 写入对局快照和历史记录，用于安全恢复与回放。
