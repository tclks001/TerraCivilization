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
  -> 后台记录当前 hover / selected CellId
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

但保留：

- `APlanetBinder` 作为关卡中的交互桥接 Actor。
- `APlanetBinder::TessellatedMeshRef` 指向当前 `APlanetTessellatedMesh`。
- 旧 `PlanetBinder` / `CellHighlightComponent` 作为 debug fallback，不删除。
- `HighlightHoverColor`、`HighlightSelectColor`、`HighlightStrength` 等现有颜色参数语义。

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
SetNumCustomDataFloats(2)
```

通道分配：

| Custom Data Index | 名称 | 含义 |
| --- | --- | --- |
| `0` | HoverIntensity | `0` 或 `1`，鼠标悬停高亮 |
| `1` | SelectIntensity | `0` 或 `1`，点击选中高亮 |

材质中读取：

```text
PerInstanceCustomData(0) -> HoverIntensity
PerInstanceCustomData(1) -> SelectIntensity
```

最终颜色建议：

```text
HighlightColor = HoverColor * HoverIntensity + SelectColor * SelectIntensity
```

如果 hover 与 select 同时存在，可以让 select 优先，或者两者相加后 saturate。

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

### 3.4 Click Select 状态

点击命中的 HISM 实例后：

```text
CellId -> Toggle Selected
```

初版允许多选，内部保存：

```cpp
TSet<int32> HISMSelectedCellIds;
```

如果后续玩法只允许单选棋子，可以上层在调用前先 `ClearHISMSelection()`，或者新增单选模式。

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
- 旧 debug ProceduralMesh / LUT 路径仍可保留。
- 不需要让 `PlanetBinder` 用旧射线方式解 Cell。

---

## 5. 材质图要求

C++ 只负责把每个实例的 `HoverIntensity` / `SelectIntensity` 写入 GPU。真正“UV 边缘环”需要 HISM 瓦片材质读取这些值。

每个 Plain / Forest / Mountain tile 材质都需要加入相同逻辑。

### 5.1 必需节点

在材质中添加：

- `TextureCoordinate`
- `ComponentMask R,G`
- `Subtract`：减去 `(0.5, 0.5)`
- `Length`
- `SmoothStep` 或等价节点
- `PerInstanceCustomData`，Data Index = `0`
- `PerInstanceCustomData`，Data Index = `1`
- `VectorParameter HoverColor`
- `VectorParameter SelectColor`
- `ScalarParameter HighlightStrength`
- `ScalarParameter HighlightInnerRadius`
- `ScalarParameter HighlightOuterRadius`

### 5.2 推荐公式

```hlsl
float2 d = UV0 - float2(0.5, 0.5);
float r = length(d);
float ring = smoothstep(HighlightInnerRadius, HighlightOuterRadius, r);

float hover = PerInstanceCustomData0;
float selected = PerInstanceCustomData1;

float3 color = HoverColor.rgb * hover + SelectColor.rgb * selected;
Emissive += color * ring * HighlightStrength;
BaseColor = lerp(BaseColor, saturate(BaseColor + color), ring * saturate(hover + selected));
```

推荐初始参数：

| 参数 | 推荐值 |
| --- | --- |
| `HighlightInnerRadius` | `0.40` |
| `HighlightOuterRadius` | `0.50` |
| `HighlightStrength` | `1.5` |
| `HoverColor` | `(1.0, 0.85, 0.10)` |
| `SelectColor` | `(0.20, 0.90, 1.00)` |

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

建议如果只验证 HISM 点击，先临时关闭旧碰撞：

- `bUseDebugProceduralCollision = false`

避免旧整球 ProceduralMesh 抢在 HISM 前被 trace 命中。

### Step 4：配置 HISM tile 材质

给 Plain / Forest / Mountain 三个 StaticMesh 使用的材质都接入：

- `PerInstanceCustomData(0)` 作为 hover 强度。
- `PerInstanceCustomData(1)` 作为 select 强度。
- UV 半径 `0.4` 以外作为边缘环 mask。
- 将 mask 乘高亮颜色输出到 `Emissive Color` 或叠加到 `BaseColor`。

如果三个地形材质不同，三份都要接；如果它们共享一个父材质，只改父材质即可。

### Step 5：PIE 验收 hover

运行 PIE：

- 鼠标移到某个 HISM tile 上。
- 该 tile UV 边缘环变成 hover 色。
- 屏幕左上输出当前 HISM hover `CellId`。
- 移到相邻 tile，高亮立即切换。
- 移出星球，高亮在 `HISMHoverFadeDuration` 后清除。

### Step 6：PIE 验收 click select

运行 PIE：

- 左键点击某个 tile。
- 该 tile 保持 select 色。
- 再次点击同一个 tile，select 取消。
- 点击多个 tile，可以看到多个 selected tile 同时保持高亮。

### Step 7：OnConstruction 重建索引验收

在编辑器中修改：

- `WorldGenSettings.RandomSeed`
- `MountainStripCount`
- `ForestPatchCount`
- 或重新挂载三个 StaticMesh

确认：

- HISM 实例重建后仍可点击。
- 屏幕输出的 `CellId` 不错乱。
- hover / select 状态被清空或重新映射，不出现旧实例残留高亮。

---

## 8. 风险与限制

- 材质必须读取 `PerInstanceCustomData`，否则 C++ 写入数据也不会有视觉变化。
- StaticMesh 必须有可 trace 的碰撞；若没有简单碰撞，可临时使用复杂碰撞验证，但长期建议做合理简单碰撞。
- `Hit.Item` 是当前 HISM 内部实例索引，只在下一次 `ClearInstances()` 前有效。
- 如果旧 debug ProceduralMesh 碰撞打开且比 HISM 先被命中，HISM 路径可能拿不到实例命中；验证阶段建议关闭 `bUseDebugProceduralCollision`。
- 如果 tile UV 不是中心对称，`UV(0.5,0.5)` 半径环不会正好贴边，需要修正资产 UV 或材质 mask。

---

## 9. 后续扩展

后续 SimpleGameplay 可以继续扩展更多通道：

| Custom Data Index | 未来用途 |
| --- | --- |
| `2` | MoveRangeIntensity |
| `3` | JumpRangeIntensity |
| `4` | AttackTargetIntensity |
| `5` | CurrentFactionMask |

初版先只实现 hover / select 两个通道，避免材质复杂度过早上升。
