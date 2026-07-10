# HISM Cell Tile 高亮设计稿

> 本稿用于实现当前 HISM 球面瓦片渲染模式下的点击 / 悬停高亮。
>
> 目标：不再使用旧 `PlanetBinder` 的球面射线转 Cell 查询，也不使用旧 `CellHighlightLUT` 整球材质高亮；改为直接命中 HISM 实例，反查 `CellId`，并通过 HISM `PerInstanceCustomData` 驱动瓦片材质在 UV 边缘一圈发光。

---

## 1. 目标效果

当前地形主视觉已经是三种地形各一个 HISM：

- `PlainTileHISMComp`
- `ForestTileHISMComp`
- `MountainTileHISMComp`

本方案在这三个组件上直接实现 tile 级高亮：

```text
鼠标射线
  -> 命中某个 HISM 组件的某个 InstanceIndex
  -> InstanceIndex 反查 CellId
  -> hover 状态或 Gameplay 容器输出逻辑高亮
  -> 给该实例写 PerInstanceCustomData
  -> 瓦片材质按 UV 圆心距离只高亮外圈
```

材质效果不是整块变亮，而是类似：

```text
UV 圆心 = (0.5, 0.5)
半径 >= 0.4 的区域高亮
形成直径约 0.8 的边缘环
```

由于高亮仍画在原 HISM 瓦片上，所以被其他实例自然遮挡的部分不会亮，最终视觉会接近一个被遮挡切出来的六边形 / 五边形边缘，整体类似螺帽形边框。

---

## 2. 不再使用的旧路径

本方案明确不使用：

- `PlanetBinder::OnHoverWorldPoint` 的 `WorldHit -> UnitDir -> FindNearestCell` 射线查询。
- `PlanetBinder::OnClickWorldPoint` 的旧 LUT select 切换。
- `UCellHighlightComponent` 的 `CellHighlightLUT` 高亮绘制。
- 旧整球 `ProceduralMesh` 材质中的 `ComputeHighlight` HLSL 描边。

仍保留：

- `APlanetBinder` 作为关卡中的交互桥接 Actor。
- `APlanetBinder::TessellatedMeshRef` 指向当前 `APlanetTessellatedMesh`。
- `PlanetBinder` 到 `APlanetTessellatedMesh` 的桥接关系。
- `HighlightHoverColor`、`HighlightStrength` 等 HISM 高亮参数语义。

---

## 3. C++ 侧职责

### 3.1 HISM 实例索引

每次 `APlanetTessellatedMesh::RebuildHISMTileInstances_()` 都会执行：

```text
ClearInstances()
重新 AddInstance()
```

因此每次重建都必须同步重建索引表。

新增索引：

```cpp
TArray<int32> PlainInstanceToCellId;
TArray<int32> ForestInstanceToCellId;
TArray<int32> MountainInstanceToCellId;

TArray<FTerraHISMCellInstanceRef> CellIdToHISMInstance;
```

其中 `FTerraHISMCellInstanceRef` 记录：

```cpp
UHierarchicalInstancedStaticMeshComponent* Component;
int32 InstanceIndex;
```

约定：

- `InstanceIndex -> CellId` 用于射线命中反查。
- `CellId -> Component + InstanceIndex` 用于按逻辑 Cell 写高亮数据。
- `CellIdToHISMInstance` 长度等于当前 `CellTopology->Cells.Num()`。
- 每次 `RebuildHISMTileInstances_()` 开始时全部清空并重新填充。

---

### 3.2 PerInstanceCustomData 通道

每个 HISM 组件设置：

```cpp
SetNumCustomDataFloats(4)
```

通道分配：

| Custom Data Index | 名称 | 含义 |
| --- | --- | --- |
| `0` | `FinalHighlightColor.R` | C++ 计算后的最终高亮颜色 R |
| `1` | `FinalHighlightColor.G` | C++ 计算后的最终高亮颜色 G |
| `2` | `FinalHighlightColor.B` | C++ 计算后的最终高亮颜色 B |
| `3` | `FinalHighlightIntensity` | C++ 计算后的最终高亮强度，通常为 `0..1` |

材质中读取：

```text
PerInstanceCustomData(0) -> FinalHighlightColor.R
PerInstanceCustomData(1) -> FinalHighlightColor.G
PerInstanceCustomData(2) -> FinalHighlightColor.B
PerInstanceCustomData(3) -> FinalHighlightIntensity
```

新版职责划分：

```text
Gameplay：负责棋子选择、移动阶段、回合状态，并输出逻辑高亮颜色。
C++ 渲染桥：负责 hover、Gameplay 高亮优先级覆盖与最终强度写入。
材质：只负责读取 FinalHighlightColor 与 FinalHighlightIntensity，并按 UV 边缘 mask 输出自发光。
```

G2 起不再使用 `HISMSelectedCellIds` / `SelectedNow`。当前 C++ 合成规则：

```cpp
float HoverIntensity = (HISMCurrentHoverCellId == CellId) ? 1.0f : 0.0f;

FLinearColor FinalHighlightColor = FLinearColor::Black;
float FinalHighlightIntensity = 0.0f;

FTerraGameplayCellHighlight GameplayHighlight;
if (GameplayContainer.IsValid() && GameplayContainer->GetHighlightForCell(CellId, GameplayHighlight))
{
    // Gameplay 高亮优先：选中棋子脚下黄色、行走后可停下蓝色、G3 下一步落点淡蓝色。
    // 如果 Cell 是 G3 下一步落点且正在 hover，则渲染桥把淡蓝色加深。
    FinalHighlightColor = GameplayContainer->IsCurrentActionTargetCell(CellId) && HoverIntensity > 0.0f
        ? G3ActionTargetHoverColor
        : GameplayHighlight.Color;
    FinalHighlightIntensity = GameplayHighlight.Intensity;
}
else if (HoverIntensity > KINDA_SMALL_NUMBER)
{
    FinalHighlightColor = HighlightHoverColor;
    FinalHighlightIntensity = HoverIntensity;
}
```

新版通道中 `FinalHighlightColor` 与 `FinalHighlightIntensity` 分开传输：

```text
无高亮：Color=(0,0,0), Intensity=0
普通 Hover：Color=HighlightHoverColor, Intensity=1
G2.5 当前阵营棋子底色：Color=(1,0.45,0.68), Intensity=1
G2.5 hover 当前阵营棋子：Color=(1,0.22,0.32), Intensity=1
G3 可行走 / 可跳跃落点：Color=(0.35,0.80,1), Intensity=1
G3 hover 可行走 / 可跳跃落点：Color=(0.08,0.45,1), Intensity=1
G2/G3 选中棋子脚下：Color=(1,1,0), Intensity=1
G2/G3 行走后可停下：Color=(0,0.35,1), Intensity=1
Gameplay 行动高亮 + Hover 同时存在：Gameplay 行动高亮覆盖 Hover / 当前阵营底色，G3 落点 hover 例外加深
```

后续如果要显示“绿色可移动格”“红色攻击目标”“紫色技能范围”，只需要 Gameplay/C++ 写入对应最终颜色，不需要继续修改材质图。

---

### 3.3 Hover 防抖状态机

复用旧 `UCellHighlightComponent` 的语义，但不复用它的 LUT：

- 当前最多只有一个 hover Cell。
- 鼠标从一个 HISM 实例移动到另一个实例时，立即切换高亮。
- 鼠标离开所有 HISM 实例时，进入短暂 fade-out 防抖。
- 防抖结束后清掉 hover。
- 鼠标在防抖期间回到原 Cell，不重复写数据。

新增字段：

```cpp
int32 HISMCurrentHoverCellId = INDEX_NONE;
int32 HISMPendingHoverCellId = INDEX_NONE;
float HISMHoverFadeTimer = 0.0f;
```

新增参数：

```cpp
float HISMHoverFadeDuration = 0.5f;
```

因为 `APlanetTessellatedMesh` 原本不 Tick，本方案需要让它在 HISM 高亮启用时 Tick，用于倒计时清 hover。

---

### 3.4 G2 Gameplay 点击状态

点击命中的 HISM 实例后：

```text
CellId -> FTerraGameplayContainer::HandleCellClick(CellId, DirtyCellIds)
```

G2 点击语义：

```text
Idle：点击当前阵营可移动棋子 -> 选中，脚下 Cell 高亮黄色
PieceSelected：点击相邻空 Cell -> 普通移动 1 格，新脚下 Cell 高亮蓝色
PieceMovedCanEndTurn：再次点击脚下蓝色 Cell -> 停下并结束回合
```

旧 `HISMSelectedCellIds` 多选集合和 `SelectedNow` 日志已移除。HISM 高亮只负责显示 Gameplay 容器输出的最终逻辑颜色。

---

### 3.5 当前命中 CellId

后台记录：

```cpp
int32 LastHISMPickedCellId;
int32 LastHISMClickedCellId;
```

用途：

- Debug 屏幕输出。
- 后续 SimpleGameplay 查询当前选中 Cell。
- 蓝图或 C++ 可通过 getter 获取。

---

## 4. 输入层接线

`APlanetInteractionController::PlayerTick` 仍然负责从鼠标发射一根 trace，但流程改为：

```text
GetHitResultUnderCursorByChannel
  -> 如果 Binder 有 TessellatedMeshRef，先调用 Tess->HandleHISMHoverHit(Hit)
      -> 成功：不再调用 PlanetBinder::OnHoverWorldPoint
      -> 失败：调用 Tess->ClearHISMHover()
  -> 如果 HISM 路径未处理，再 fallback 到旧 PlanetBinder 路径
```

左键点击同理：

```text
如果当前 Hit 可解析为 HISM 实例：
    Tess->HandleHISMClickHit(Hit)
否则：
    fallback 到 Binder->OnClickWorldPoint(Hit.ImpactPoint)
```

这样可以保证：

- 新 HISM 高亮优先生效。
- 不需要让 `PlanetBinder` 用旧射线方式解 Cell。

---

## 5. 材质图要求

C++ 负责把每个实例的 `FinalHighlightColor.RGB` 与 `FinalHighlightIntensity` 写入 GPU。材质不再判断 hover / select，也不再接收 `HoverColor` / `SelectColor`；材质只读取最终颜色和最终强度，并在 UV 边缘环输出高亮自发光。

每个 Plain / Forest / Mountain tile 材质都需要加入相同逻辑。如果三种地形共享父材质，只改父材质即可。

### 5.1 必需节点总览

在材质中添加：

| 节点类型 | 建议命名 / 参数名 | 说明 |
| --- | --- | --- |
| `TextureCoordinate` | `UV0` | 使用瓦片 UV0 |
| `ComponentMask` | `UV0_RG` | 只取 `R,G`，输出 `float2` UV |
| `PerInstanceCustomData` | `PICD_HighlightR` | `Data Index = 0` |
| `PerInstanceCustomData` | `PICD_HighlightG` | `Data Index = 1` |
| `PerInstanceCustomData` | `PICD_HighlightB` | `Data Index = 2` |
| `PerInstanceCustomData` | `PICD_HighlightIntensity` | `Data Index = 3` |
| `AppendVector` | `HighlightRG` | `PICD_HighlightR` + `PICD_HighlightG` |
| `AppendVector` | `HighlightRGB` | `HighlightRG` + `PICD_HighlightB` |
| `ScalarParameter` | `HighlightStrength` | 推荐默认 `1.5`，C++ 会写入同名参数 |
| `ScalarParameter` | `HighlightInnerRadius` | 推荐默认 `0.40`，C++ 会写入同名参数 |
| `ScalarParameter` | `HighlightOuterRadius` | 推荐默认 `0.50`，C++ 会写入同名参数 |
| `Custom` | `MF_HISM_FinalHighlightEmissive` | 输入 UV、最终颜色、强度、半径，输出自发光颜色 |
| `Add` | `BaseEmissivePlusHighlight` | 如果原材质已有自发光，用 `Add` 叠加 |

### 5.2 PerInstanceCustomData 节点设置

创建 4 个 `PerInstanceCustomData` 节点：

| 节点命名 | Data Index | 输出含义 |
| --- | --- | --- |
| `PICD_HighlightR` | `0` | `FinalHighlightColor.R` |
| `PICD_HighlightG` | `1` | `FinalHighlightColor.G` |
| `PICD_HighlightB` | `2` | `FinalHighlightColor.B` |
| `PICD_HighlightIntensity` | `3` | `FinalHighlightIntensity` |

把前三个 float 组成 `float3`：

```text
PICD_HighlightR ┐
                 ├─ AppendVector -> HighlightRG
PICD_HighlightG ┘

HighlightRG      ┐
                 ├─ AppendVector -> HighlightRGB
PICD_HighlightB ┘
```

### 5.3 Custom 节点：自发光边缘环

创建一个 `Custom` 节点：

```text
节点名：MF_HISM_FinalHighlightEmissive
Output Type：CMOT Float 3
Description：HISM tile final highlight emissive ring
```

Custom 输入列表：

| 输入名 | 类型 | 连接来源 |
| --- | --- | --- |
| `UV` | `float2` | `TextureCoordinate` 经过 `ComponentMask R,G` |
| `HighlightColor` | `float3` | `HighlightRGB` |
| `HighlightIntensity` | `float` | `PICD_HighlightIntensity` |
| `HighlightStrength` | `float` | `ScalarParameter HighlightStrength` |
| `HighlightInnerRadius` | `float` | `ScalarParameter HighlightInnerRadius` |
| `HighlightOuterRadius` | `float` | `ScalarParameter HighlightOuterRadius` |

可直接复制粘贴的 HLSL：

```hlsl
float3 finalColor = saturate(HighlightColor);
float finalIntensity = saturate(HighlightIntensity);

float innerRadius = saturate(HighlightInnerRadius);
float outerRadius = saturate(HighlightOuterRadius);
outerRadius = max(outerRadius, innerRadius + 0.0001);

float2 centeredUV = UV - float2(0.5, 0.5);
float radius = length(centeredUV);

float ringMask = smoothstep(innerRadius, outerRadius, radius);
float emissiveMask = ringMask * finalIntensity * HighlightStrength;

return finalColor * emissiveMask;
```

### 5.4 节点连线

#### 5.4.1 输入组装

```text
TextureCoordinate
  -> ComponentMask R,G
  -> Custom.UV

PerInstanceCustomData(Data Index 0)
  -> AppendVector.A
PerInstanceCustomData(Data Index 1)
  -> AppendVector.B
AppendVector(R,G)
  -> AppendVector.A
PerInstanceCustomData(Data Index 2)
  -> AppendVector.B
AppendVector(RG,B)
  -> Custom.HighlightColor

PerInstanceCustomData(Data Index 3)
  -> Custom.HighlightIntensity

ScalarParameter HighlightStrength
  -> Custom.HighlightStrength

ScalarParameter HighlightInnerRadius
  -> Custom.HighlightInnerRadius

ScalarParameter HighlightOuterRadius
  -> Custom.HighlightOuterRadius
```

#### 5.4.2 输出到材质

如果原材质没有自发光：

```text
Custom.MF_HISM_FinalHighlightEmissive
  -> Material.Emissive Color
```

如果原材质已有自发光：

```text
OriginalEmissive
  -> Add.A
Custom.MF_HISM_FinalHighlightEmissive
  -> Add.B
Add
  -> Material.Emissive Color
```

### 5.5 可选：BaseColor 轻微染色

如果只接自发光在强光环境下不够明显，可以再添加一个 `Custom` 节点或普通节点，把高亮轻微叠加到 `BaseColor`。但初版推荐先只接 `Emissive Color`，避免改变瓦片原本地形颜色。

如果需要 BaseColor 染色，可用下面这个独立 Custom：

```text
节点名：MF_HISM_FinalHighlightTintedBaseColor
Output Type：CMOT Float 3
```

Custom 输入列表：

| 输入名 | 类型 | 连接来源 |
| --- | --- | --- |
| `BaseColor` | `float3` | 原材质 BaseColor 结果 |
| `UV` | `float2` | `TextureCoordinate` 经过 `ComponentMask R,G` |
| `HighlightColor` | `float3` | `HighlightRGB` |
| `HighlightIntensity` | `float` | `PICD_HighlightIntensity` |
| `HighlightInnerRadius` | `float` | `ScalarParameter HighlightInnerRadius` |
| `HighlightOuterRadius` | `float` | `ScalarParameter HighlightOuterRadius` |
| `TintBlend` | `float` | 可用 `ScalarParameter HighlightBaseTintBlend`，推荐默认 `0.25` |

可直接复制粘贴的 HLSL：

```hlsl
float3 finalColor = saturate(HighlightColor);
float finalIntensity = saturate(HighlightIntensity);

float innerRadius = saturate(HighlightInnerRadius);
float outerRadius = saturate(HighlightOuterRadius);
outerRadius = max(outerRadius, innerRadius + 0.0001);

float2 centeredUV = UV - float2(0.5, 0.5);
float radius = length(centeredUV);

float ringMask = smoothstep(innerRadius, outerRadius, radius);
float blend = saturate(ringMask * finalIntensity * TintBlend);

return lerp(BaseColor, saturate(BaseColor + finalColor), blend);
```

连线：

```text
原 BaseColor 结果
  -> Custom.BaseColor

Custom.MF_HISM_FinalHighlightTintedBaseColor
  -> Material.Base Color
```

### 5.6 推荐初始参数

| 参数 | 推荐值 |
| --- | --- |
| `HighlightInnerRadius` | `0.40` |
| `HighlightOuterRadius` | `0.50` |
| `HighlightStrength` | `1.5` |
| `HighlightBaseTintBlend` | `0.25`，仅在启用 BaseColor 染色时需要 |

颜色不再由材质参数提供，而由 C++ 写入：

| 逻辑状态 | C++ 写入颜色 | C++ 写入强度 |
| --- | --- | --- |
| 无高亮 | `(0,0,0)` | `0` |
| 普通 Hover | `HighlightHoverColor`，默认 `(1.0, 0.85, 0.10)` | `1` |
| G2.5 当前阵营棋子底色 | 淡粉色 `(1,0.45,0.68)` | `1` |
| G2.5 hover 当前阵营棋子 | 红粉色 `(1,0.22,0.32)` | `1` |
| G3 可行走 / 可跳跃落点 | 淡蓝色 `(0.35,0.80,1)` | `1` |
| G3 hover 可行走 / 可跳跃落点 | 深淡蓝色 `(0.08,0.45,1)` | `1` |
| G2/G3 选中棋子脚下 | 黄色 `(1,1,0)` | `1` |
| G2/G3 行走后可停下 | 蓝色 `(0,0.35,1)` | `1` |
| Gameplay 行动高亮 + Hover | Gameplay 行动高亮颜色，G3 落点 hover 时加深 | `1` |

如果瓦片 UV 并非中心对称，需要先检查烘焙出的 tile 资产 UV 是否以 `(0.5,0.5)` 为中心。

---

## 6. 自然遮挡语义

本方案不额外画 Overlay Mesh，也不关闭深度测试。

高亮仍是原 HISM 实例材质的一部分，因此：

- 该瓦片被相邻森林 / 山脉实例遮挡的区域不会亮。
- 山脉凸起遮挡平原边缘时，高亮自然被挡住。
- 如果瓦片之间有轻微重叠，最终边缘形状会由深度测试自然裁切。

这正是本方案想要的“螺帽形 / 自然六边形边缘”效果。

---

## 7. 验收步骤

### Step 1：C++ 编译

重新编译项目，确认无 C++ 编译错误。

### Step 2：确认关卡引用

在关卡中确认：

- 存在 `BP_PlanetBinder` 或 `APlanetBinder`。
- `PlanetBinder.TessellatedMeshRef` 指向场景里的 `APlanetTessellatedMesh`。
- PlayerController 使用 `APlanetInteractionController` 或它的蓝图子类。

### Step 3：确认 HISM 瓦片渲染启用

在 `APlanetTessellatedMesh` 上确认：

- `bEnableHISMTileRendering = true`
- `bEnableHISMTileCollision = true`
- `bEnableHISMInstanceHighlight = true`
- 平原 / 森林 / 山脉三个 StaticMesh 已挂载

### Step 4：配置 HISM tile 材质

给 Plain / Forest / Mountain 三个 StaticMesh 使用的材质都接入：

- `PerInstanceCustomData(0)` 作为 `FinalHighlightColor.R`。
- `PerInstanceCustomData(1)` 作为 `FinalHighlightColor.G`。
- `PerInstanceCustomData(2)` 作为 `FinalHighlightColor.B`。
- `PerInstanceCustomData(3)` 作为 `FinalHighlightIntensity`。
- UV 半径 `0.4` 以外作为边缘环 mask。
- 将 `FinalHighlightColor * FinalHighlightIntensity * ringMask * HighlightStrength` 输出到 `Emissive Color`，可选叠加到 `BaseColor`。

如果三个地形材质不同，三份都要接；如果它们共享一个父材质，只改父材质即可。

### Step 5：PIE 验收 hover

运行 PIE：

- 鼠标移到某个 HISM tile 上。
- 该 tile UV 边缘环变成 hover 色。
- 屏幕左上输出当前 HISM hover `CellId`。
- 移到相邻 tile，高亮立即切换。
- 移出星球，高亮在 `HISMHoverFadeDuration` 后清除。

### Step 6：PIE 验收 G2 / G2.5 Gameplay 点击

运行 PIE：

- 回合开始时，摄像机切到当前阵营大本营上方并朝向大本营，当前阵营棋子所在 Cell 显示淡粉色。
- hover 到当前阵营棋子所在 tile，淡粉色变为更红的红粉色；hover 离开后恢复淡粉色。
- 左键点击当前阵营的可移动棋子所在 tile，该棋子脚下 Cell 高亮黄色，摄像机不移动但旋转对准该棋子。
- 再点击相邻空 tile，棋子普通移动 1 格，原黄色消失，新脚下 Cell 高亮蓝色。
- 再次点击蓝色脚下 Cell，蓝色消失并结束回合，当前阵营切到下一个阵营，新阵营棋子变淡粉色，摄像机切到新阵营大本营上方。

### Step 7：OnConstruction 重建索引验收

在编辑器中修改：

- `WorldGenSettings.RandomSeed`
- `MountainStripCount`
- `ForestPatchCount`
- 或重新挂载三个 StaticMesh

确认：

- HISM 实例重建后仍可点击。
- 屏幕输出的 `CellId` 不错乱。
- hover / Gameplay 高亮状态被清空或重新映射，不出现旧实例残留高亮。

---

## 8. 风险与限制

- 材质必须读取 `PerInstanceCustomData`，否则 C++ 写入数据也不会有视觉变化。
- StaticMesh 必须有可 trace 的碰撞；若没有简单碰撞，可临时使用复杂碰撞验证，但长期建议做合理简单碰撞。
- `Hit.Item` 是当前 HISM 内部实例索引，只在下一次 `ClearInstances()` 前有效。
- 如果 tile UV 不是中心对称，`UV(0.5,0.5)` 半径环不会正好贴边，需要修正资产 UV 或材质 mask。

---

## 9. 后续扩展

新版材质通道保持固定：

| Custom Data Index | 固定用途 |
| --- | --- |
| `0` | `FinalHighlightColor.R` |
| `1` | `FinalHighlightColor.G` |
| `2` | `FinalHighlightColor.B` |
| `3` | `FinalHighlightIntensity` |

后续 SimpleGameplay 不再通过继续增加材质强度通道扩展状态，而是在 C++ 侧扩展高亮来源与优先级：

| 高亮来源 | 推荐颜色 | 推荐优先级 |
| --- | --- | --- |
| `Hover` | 暖金 | `100` |
| `CurrentFactionPiece` | 淡粉 | `150` |
| `CurrentFactionPieceHover` | 红粉 | `200` |
| `SelectedPiece` / `SelectedCell` | 黄色 | `300` |
| `MovedCanEndTurn` | 蓝色 | `400` |
| `MoveTarget` | 绿色 | `500` |
| `JumpTarget` | 蓝绿色或紫色 | `520` |
| `AttackTarget` | 红色 | `600` |
| `Debug` | 任意颜色 | 按调试需要指定 |

C++ 每次交互变化时只对受影响 Cell 重新合成最终颜色：

```text
收集该 Cell 的所有高亮来源
  -> 取 Priority 最高的一项
  -> 写入 FinalHighlightColor.RGB
  -> 写入 FinalHighlightIntensity
```

这样材质图无需再随着玩法状态增加而修改，后续想用任意颜色高亮任意 Cell，只需要 C++ 写入不同最终颜色即可。
