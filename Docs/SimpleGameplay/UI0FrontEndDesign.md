# TerraCivilization UI0 骨架与前台设计稿

> UI0 是 [UIDesign.md](UIDesign.md) 的第一个实现里程碑。它建立可替换的 UMG 前台、局部玩家 UI 路由和新游戏真实 WorldGen 配置通路；不实现存档、教学、历史、局内 HUD、暂停、科技选择或结算。

---

## 1. 范围

UI0 在当前 `/Game/Map/NowMap` 上以全屏前台覆盖层启动。星球棋盘保留为菜单背景，但 `APlanetInteractionController` 在前台路由激活时停止把鼠标、相机、Tab 或右键输入送入棋盘。

已实现路由：

```text
MainMenu -> NewGameSetup -> InGame(None)
              ^              |
              +---- Back ----+
```

主页面包含新游戏、继续游戏、教学关、历史记录、设置、退出六个入口。其中 UI0 只启用新游戏和退出；其余入口保留为禁用项，避免在尚无存档/教程目录/设置数据时创建虚假的流程。UI1 以后依次接通它们。

新游戏页面已接入的真实字段为：随机种子、山脉条带数量、森林块数量。点击开始后，`UTerraUISubsystem::StartNewGame` 将它们写入场景中 `APlanetTessellatedMesh::WorldGenSettings`，再调用现有 `Rebuild()`。这复用既有 `FWorldGenSettings`、WorldGen 和 Gameplay 初始化流程。

本阵营颜色属于 UI0 启动配置 `FTerraNewGameConfig`，已被保存并广播，但当前未写入棋子表现：项目尚未提供可按阵营覆写的运行时调色板入口。UI1 或专门的阵营表现里程碑应接入 `PiecePresentation` 后才将其视为实际生效。

## 2. C++ 结构

| 类型 | 文件 | 职责 |
| --- | --- | --- |
| `UTerraUISettings` | `Public/UI/TerraUISettings.h` | 项目设置中的 Widget 挂载接口与启动开关。 |
| `UTerraUISubsystem` | `Public/UI/TerraUISubsystem.h` | `ULocalPlayerSubsystem`；管理当前前台路由、单个全屏 Widget、输入模式和新游戏请求。 |
| `UTerraMainMenuWidget` | `Public/UI/TerraMainMenuWidget.h` | 主页面默认 C++ fallback。 |
| `UTerraNewGameSetupWidget` | `Public/UI/TerraNewGameSetupWidget.h` | 新游戏页面默认 C++ fallback。 |
| `APlanetInteractionController` | `Private/Interaction/PlanetInteractionController.cpp` | BeginPlay 打开前台；路由激活期间跳过棋盘输入。 |

Subsystem 是唯一调用 `SetInputMode` 的 UI0 类。进入前台使用 `UI Only`；开始新游戏时清除 Widget 后还原为 `Game Only`，但始终保持鼠标可见，因为当前 HISM 棋盘交互依赖鼠标悬停和点击。恢复 `Game Only` 时必须调用 `SetConsumeCaptureMouseDown(false)`，否则 UE 会吞掉离开 UI 后用于重新捕获视口的第一次左/右键，表现为棋盘点击或 Undo 需要双击。Widget 仅调用 Subsystem 的 `Show...` 和 `StartNewGame` 接口。

## 3. 默认资产与编辑器挂载

UI0 即使没有任何 `.uasset` 也可运行：若设置没有指定 Widget，它使用原生 fallback。最终项目资产必须放在以下位置：

```text
Content/UI/
  Widgets/
    WBP_MainMenu.uasset
    WBP_NewGameSetup.uasset
  Styles/
    DA_TerraUIStyle.uasset            (UI1 开始使用)
  Fonts/
  Icons/
```

在 Unreal Editor 中创建并挂载：

1. 在 `/Game/UI/Widgets/` 新建 Widget Blueprint，父类分别选 `TerraMainMenuWidget` 与 `TerraNewGameSetupWidget`。前者命名 `WBP_MainMenu`，后者命名 `WBP_NewGameSetup`。
2. 打开 **Project Settings -> Terra UI**。
3. 在 **Front End** 分类把 `Main Menu Widget Class` 指向 `WBP_MainMenu`，把 `New Game Setup Widget Class` 指向 `WBP_NewGameSetup`。
4. 保持 `Show Front End On Startup` 启用。若需要直接 PIE 调试棋盘，将它关闭即可。

这两个字段来自 `UTerraUISettings` 的 `Config=Game` 属性，因此编辑器保存后写入 `Config/DefaultGame.ini`；不需改 C++ 默认硬路径。若所填 Widget 无法加载，Subsystem 会记录错误，且不会产生半激活路由。

UI0 的原生 fallback 在 `NativeOnInitialized` 中通过 C++ 创建基础控件，确保 Slate 根节点生成前已有完整 WidgetTree；它主要用于验证路由和真实 WorldGen 通路。最终 WBP 应在 Designer 中自行排版；不要依赖 fallback 的 Widget 名称或层级。Blueprint 可以从父类调用以下可用接口：

| 接口 | 用途 |
| --- | --- |
| `ShowMainMenu()` | 回到主页面。 |
| `ShowNewGameSetup()` | 打开新游戏配置。 |
| `StartNewGame(Config)` | 用 `FTerraNewGameConfig` 开始并重建。 |
| `CloseFrontEnd()` | 关闭前台，进入棋盘输入状态。 |
| `OnRouteChanged` | 监听页面切换。 |
| `OnNewGameStarted` | 监听有效的新局配置。 |

## 4. 验收

- 启动 `NowMap` 后出现主页面，鼠标和键盘不会驱动棋盘或相机。
- 主页面“新游戏”能进入配置页；返回能回到主页面。
- 输入任意非负地形数量和整数种子后点击开始，`APlanetTessellatedMesh` 重新生成对应地形并恢复棋盘输入。
- 非整数或负地形数量不会调用 `Rebuild()`，并在日志中报告字段错误。
- Project Settings 中挂载的 Widget Blueprint 优先于原生 fallback；不配置资产时 fallback 仍可启动。
- 关闭 `Show Front End On Startup` 后，PIE 保持原有棋盘启动与输入行为。

## 5. 后续接口

UI1 接通 Settings、Continue 和 ESC 时，应在 `UTerraUISubsystem` 增加路由，而不是把页面状态写进 `APlanetInteractionController`。UI2 的 Tutorial/Agent/科技和 UI3 的存档/历史也沿用同一页面栈。

`FTerraNewGameConfig` 需要在 UI3 迁移到完整可序列化的 Match Start Config，并写入对局快照。阵营颜色只有在 PiecePresentation 提供按阵营运行时配置接口后才可应用。
