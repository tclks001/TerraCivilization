# TerraCivilization UI2.1 科技界面设计稿

> UI2.1 承接 [UI1InGameHUDAndPauseDesign.md](UI1InGameHUDAndPauseDesign.md)，只实现 T0 科技成长的可视化与输入：科技三选一、阵营 0 成长进度、阵营存活总览及科技详情。它不实现科技效果、美术卡牌、教学、Agent、存档或历史。

---

## 1. 数据权威与边界

`FTerraGameplayContainer` 是积分、门槛、候选、已持有科技、存活阵营和回合推进的唯一权威。UI 通过 `UPlanetGameplayComponent` 的只读查询与 `ChoosePendingTechnology` 请求操作。

```text
行动确认
  -> Gameplay 结算积分/候选
  -> OnTechnologyStateChanged
  -> 当前阵营待选时 OnTechnologyChoiceRequested
  -> UI 读取候选并显示模态
  -> 玩家选一项
  -> Gameplay 校验并写入 OwnedTechnologies
  -> Gameplay 推进回合
```

UI 不根据分数自行生成候选，不自行修改 `UnlockCount`，也不在点击科技后自行推进回合。

## 2. 科技三选一

只有玩家控制的阵营 0 进入 `bWaitingForTechnologyChoice` 时，`UTerraUISubsystem` 才打开 `TechnologyChoice` 路由：`UI Only`、阻断棋盘输入，但不更改科技规则状态。NPC 阵营的待选科技由行为树调用 `ChoosePendingTechnology` 无界面结算，不能创建或保留科技模态；当科技状态变化后，若阵营 0 已不再处于待选状态，已有模态立即关闭。

T0 目前的三个候选映射为：

| 枚举 | 显示名 | 规则效果 |
| --- | --- | --- |
| `PlaceholderTraining` | 基础训练 | 无效果占位。 |
| `PlaceholderLogistics` | 战地后勤 | 无效果占位。 |
| `PlaceholderDoctrine` | 作战学说 | 无效果占位。 |

点击科技卡调用 `UPlanetGameplayComponent::ChoosePendingTechnology(FactionId, TechnologyId, OutError)`。成功后关闭模态并恢复局内输入；若未来选择后立即产生下一组候选，事件会重新打开下一份模态。

## 3. 阵营 0 成长进度

HUD 顶部只显示玩家阵营 0（界面显示为“阵营 1”）的成长：

```text
等级 = UnlockCount + 1
当前分数 = AccumulatedScore + ScoreEarnedThisTurn
本等级起点 = 0（首级）或 NextUnlockScore - TechnologyUnlockScoreIncrement
进度 = (当前分数 - 本等级起点) / (NextUnlockScore - 本等级起点)
```

进度条的终点是 `NextUnlockScore`。该计算只用于显示；真实升级仍由 Gameplay 结算决定。

## 4. 左侧阵营面板

HUD 左侧“阵营总览”默认展开，可收起。它列出所有 `FTerraGameplayFactionState`：阵营编号和存活/已败状态。点击任一阵营显示：

- 阵营编号和存活状态。
- 已结算累计分数。
- 已持有科技列表。

T0 没有科技 Data Asset，详情暂以稳定科技枚举编号显示；三选一弹窗使用本稿定义的中文占位名。后续科技定义资产建立后，两个位置统一改为名称、图标和描述。

## 5. 默认资产与编辑器挂载

UI2.1 原生 fallback 可直接运行。最终 Widget Blueprint 放置在：

```text
Content/UI/Widgets/WBP_TechnologyChoice.uasset
```

在 **Project Settings -> Terra UI -> In Game** 的 `Technology Choice Widget Class` 挂载该资产。父类选择 `TerraTechnologyChoiceWidget`。HUD 仍复用 `WBP_InGameHUD` / `TerraInGameHUDWidget`，无需另建 HUD 入口。

## 6. 验收

- 阵营 0 分数、等级和进度条随已确认行动的科技状态变化更新。
- 阵营 0 分数达到真实门槛时，棋盘被规则层阻断并显示三选一；选择一项后科技写入当前阵营并正常推进回合。NPC 达到门槛时由行为树无界面选择，玩家不会看到不可点击的 NPC 科技页。
- 左侧面板正确显示所有阵营存活状态；点击阵营可看到其持有科技。
- 选择不在候选中的科技、非当前阵营科技或无待选状态均由 Gameplay 拒绝，UI 不伪造成功。
