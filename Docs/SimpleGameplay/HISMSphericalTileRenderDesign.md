# TerraCivilization SimpleGameplay HISM 球面瓦片渲染设计稿

> 本稿用于把当前 `APlanetTessellatedMesh` 的主地表渲染，从程序化整球网格切换为 **每个 Cell 一个烘焙 Static Mesh 实例** 的方案。
>
> 资产来源：`/Game/Generated/SphericalTiles` 中已经烘焙好的平原、森林、山脉 `StaticMesh` 与对应 `MaterialInstance`。
>
> 目标：主视觉和交互拾取都使用 `HISM`；旧球面 `ProceduralMesh` / LUT / SDF 材质链路不再属于 SimpleGameplay 主线。

---

## 1. 目标

SimpleGameplay 当前只需要三种地形：

| 地形 | 渲染资产 | HISM 组件 |
| --- | --- | --- |
| 平原 | 编辑器挂载的平原 `StaticMesh` | `PlainTileHISMComp` |
| 森林 | 编辑器挂载的森林 `StaticMesh` | `ForestTileHISMComp` |
| 山脉 | 编辑器挂载的山脉 `StaticMesh` | `MountainTileHISMComp` |

渲染模块从 `WorldGen` 获取每个 `Cell` 的 `ETerraSimpleTerrainType`，然后把对应地形的瓦片实例加入对应的 HISM 组件。

核心要求：

- **每种地形一个 HISM 组件**，而不是每个 Cell 一个组件。
- **每个 Cell 一个实例**，实例由 `WorldGen` 输出的地形类型决定。
- **空间指向正确**：烘焙瓦片资产的局部 `+Z` 指向该 Cell 的 `UnitCenter`。
- **偏转不重要**：绕法线方向的 roll/yaw 不参与初版验收。
- **HISM-only**：SimpleGameplay 只保留 HISM 拼出球面的逻辑，不再保留旧整球 `ProceduralMesh` debug 对照。

---

## 2. 烘焙瓦片资产的坐标约定

`TerraSphericalTileGenerator` 生成的瓦片不是以 Cell 中心为原点的平面 patch，而是一个以资产局部原点为球心的小球面 patch：

```text
RawDir = normalize(float3(sx, sy, 1))
Position = RawDir * radius
```

因此：

- 资产局部原点表示“球心”。
- 资产局部 `+Z` 是瓦片中心朝外方向。
- 资产中心顶点大约位于局部 `(0, 0, BaseRadius)`。
- 把资产放到大球上时，不应把实例平移到 `Cell.UnitCenter * GlobeRadiusCM`。
- 正确做法是：实例位置保持在 actor 局部原点，只做旋转和统一缩放。

实例变换：

```text
Rotation = FindBetweenNormals(Local +Z, Cell.UnitCenter)
Location = (0, 0, 0)
Scale = (GlobeRadiusCM + HISMTileRadiusOffsetCM) / HISMTileSourceRadiusCM * HISMTileAdditionalUniformScale
```

其中：

| 参数 | 含义 |
| --- | --- |
| `GlobeRadiusCM` | 目标星球半径，沿用现有 `APlanetTessellatedMesh` 字段 |
| `HISMTileSourceRadiusCM` | 生成瓦片资产时使用的 `BaseRadius`，默认 `100` |
| `HISMTileRadiusOffsetCM` | HISM 瓦片相对星球半径的额外偏移，默认 `0` |
| `HISMTileAdditionalUniformScale` | 额外统一缩放，默认 `1` |

---

## 3. 渲染模块接口

在 `APlanetTessellatedMesh` 上新增编辑器字段：

```cpp
UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
bool bEnableHISMTileRendering = true;

UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
bool bEnableHISMTileCollision = true;

UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
UStaticMesh* PlainTileStaticMesh;

UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
UStaticMesh* ForestTileStaticMesh;

UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
UStaticMesh* MountainTileStaticMesh;

UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
float HISMTileSourceRadiusCM = 100.0f;

UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
float HISMTileRadiusOffsetCM = 0.0f;

UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
float HISMTileAdditionalUniformScale = 1.0f;
```

---

## 4. 构建流程

`APlanetTessellatedMesh::RebuildAll_()` 的主流程调整为：

```text
RebuildTopologies_()
Run WorldGen
RebuildHISMTileInstances_()
ApplyRenderModeVisibility_()
RebuildGameplay_()
RebuildG1DebugPieces_()
LogTopologyStats_()
```

HISM 生成流程：

```text
RebuildHISMTileInstances_()
    1. 清空 Plain / Forest / Mountain 三个 HISM 组件实例
    2. 给三个 HISM 组件分别设置编辑器挂载的 StaticMesh
    3. 如果 bEnableHISMTileRendering=false，直接隐藏并返回
    4. 读取 Generator->GetCellData()
    5. 遍历 CellTopology->Cells
    6. 根据 CellGeoData[CellId].SimpleTerrainType 选择 HISM 组件
    7. 计算 Rotation = +Z -> Cell.UnitCenter
    8. 计算 Scale = GlobeRadiusCM / HISMTileSourceRadiusCM
    9. AddInstance(FTransform(Rotation, ZeroLocation, UniformScale))
    10. 输出平原 / 森林 / 山脉实例数量日志
```

---

## 5. 编辑器中需要做的事

### 5.1 挂载三个 StaticMesh 资产

打开关卡中的 `BP_PlanetTessellatedMesh` 或 `APlanetTessellatedMesh` 实例，在详情面板找到：

```text
PlanetTopology | Tess | HISM Tiles
```

设置：

| 字段 | 推荐挂载 |
| --- | --- |
| `PlainTileStaticMesh` | `/Game/Generated/SphericalTiles/SM_Tile_Plain_01` |
| `ForestTileStaticMesh` | `/Game/Generated/SphericalTiles/SM_Tile_Forest_01` |
| `MountainTileStaticMesh` | `/Game/Generated/SphericalTiles/SM_Tile_Mountain_01` |

如果你的资产名字不同，挂载你实际生成的 `SM` 即可。

### 5.2 设置半径缩放

如果生成瓦片时 `BaseRadius=100`，保持：

```text
HISMTileSourceRadiusCM = 100
HISMTileRadiusOffsetCM = 0
HISMTileAdditionalUniformScale = 1
```

如果生成资产时使用了其他 `BaseRadius`，把 `HISMTileSourceRadiusCM` 改成生成时的值。

### 5.3 打开 HISM 渲染

确认：

```text
bEnableHISMTileRendering = true
bEnableHISMTileCollision = true
```

## 6. 验收步骤

### 6.1 编译验收

1. 编译项目。
2. 确认 `APlanetTessellatedMesh` 无 C++ 编译错误。
3. 打开编辑器后确认详情面板出现：

```text
PlanetTopology | Tess | HISM Tiles
```

### 6.2 资产挂载验收

1. 打开关卡中的 `BP_PlanetTessellatedMesh` 或实例 actor。
2. 挂载平原、森林、山脉三个 `StaticMesh`。
3. 点击 `Rebuild` 或改动任意 HISM 参数触发 `OnConstruction`。
4. Output Log 应出现类似日志：

```text
[Tess] Rebuilt HISM spherical tiles: Plain=..., Forest=..., Mountain=..., Radius=15000.0cm, SourceRadius=100.0cm
```

### 6.3 视觉验收

1. 视口中应看到由静态网格瓦片拼出的球面。
2. 地形类型应与 `WorldGen` 结果一致：
   - 基地保护区附近大面积平原。
   - 山脉呈条状。
   - 森林呈块状。
3. 不应再看到旧 `TerrainMaterial` 的整球颜色分层。
4. 如果瓦片整体半径不对，优先检查：
   - `HISMTileSourceRadiusCM` 是否等于生成资产时的 `BaseRadius`。
   - `GlobeRadiusCM` 是否是目标星球半径。
   - `HISMTileRadiusOffsetCM` 是否被误设为过大或过小。

### 6.4 朝向验收

1. 每个瓦片都应朝外贴在球面上。
2. 不要求纹理或形状绕法线方向完全连续。
3. 如果瓦片像“竖起来”或“朝内”，说明资产生成坐标约定或 `+Z` 对齐约定不一致，需要检查烘焙资产是否仍使用：

```text
RawDir = normalize(float3(sx, sy, 1))
Position = RawDir * radius
```

### 6.5 交互验收

1. PIE 运行。
2. 鼠标移动到球面上，应继续出现 hover cell 调试信息。
3. 点击 cell，应进入 G2 Gameplay 点击流程：点击当前阵营棋子会选中并高亮黄色，移动后脚下 Cell 高亮蓝色，再次点击脚下 Cell 结束回合。
4. 如果 hover / click 不稳定：
   - 确认 HISM StaticMesh 有可用碰撞。
   - 再排查 HISM 碰撞资产设置。

---

## 7. 初版不处理的问题

以下内容暂不纳入本次实现：

- 每个 Cell 的随机绕法线旋转。
- 五边形 / 六边形使用不同资产。
- 每个实例的材质参数随机化。
- HISM LOD 分组或运行时流送。
- Nanite HISM 的性能 profiling。
- ProceduralMesh / SDF / LUT 整球材质回退链路。
