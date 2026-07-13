# SL2：Texture2D 星空背景与单太阳定向光设计稿

> 父稿：[SpacePlanetLightingDesign.md](SpacePlanetLightingDesign.md) §10 的 **SL2：星空背景与单太阳定向光**。
>
> 本稿使用已导入的普通纹理 `/Game/Sky/qwantani_night_puresky_4k` 作为星空背景。该资源是 `Texture2D`，本期不转换为 `TextureCube`，不新建 C++ 类，也不把它用作 Sky Light 的环境光源。

---

## 1. 阶段目标与完成画面

完成 SL1 后，地图应只有黑色背景和现有行星 / 玩法对象。SL2 在此基础上加入：

```text
Texture2D 全景星空
        ↓
M_SpaceSky（Unlit，Emissive）
        ↓
大尺寸球形背景网格
        ↓
DL_Sun（唯一 Directional Light）
        ↓
中心行星的受光半球与昼夜分界
```

完成后的第一眼效果应为：黑色宇宙背景包围着中心行星，星球至少有一侧被太阳照亮，另一侧仍然较暗；背景没有蓝天、云、雾或地平线。

本阶段不处理夜面玩法可读性、固定曝光、Sky Light、大气薄壳或最终 Bloom。这些属于 SL3 和 SL4。

---

## 2. 前置条件与命名

开始前确认：

- SL1 已通过：当前地图中没有可见的 `VolumetricCloud`、`ExponentialHeightFog`、`SkyAtmosphere` 或模板天空球。
- 当前地图为 `/Game/Map/TessellatedMeshTestMap`。
- 星空纹理已存在：`/Game/Sky/qwantani_night_puresky_4k`。
- 行星、HISM 瓦片、相机、棋子和交互功能均已正常。

本稿统一使用下列新资产与 Actor 名称：

| 类型 | 名称 | 建议位置 |
| --- | --- | --- |
| Material | `M_SpaceSky` | `/Game/Sky/M_SpaceSky` |
| Material Instance | `MI_SpaceSky` | `/Game/Sky/MI_SpaceSky` |
| Static Mesh Actor | `SpaceSky` | `TessellatedMeshTestMap` |
| Directional Light | `DL_Sun` | `TessellatedMeshTestMap` |

如果项目已有同名资产，不覆盖已有内容；以项目实际路径为准，并在关卡中保持 Actor 职责唯一。

---

## 3. 检查星空 Texture2D

在 Content Browser 打开 `/Game/Sky/qwantani_night_puresky_4k`，确认它的资源类型是 `Texture2D`。因为它来自 EXR，亮部很可能有高动态范围数值；本期通过材质中的亮度参数压低它，而不是修改原纹理内容。

在纹理 Details 中检查：

| 项目 | 初始建议 | 原因 |
| --- | --- | --- |
| Compression Settings | 保持导入后的 HDR / 默认 HDR 配置 | 避免把 HDRI 错压成普通颜色纹理 |
| sRGB | 保持导入结果 | EXR 通常以线性 HDR 数据导入；不要为了“变亮”随意切换 |
| Mip Gen Settings | 保持默认 | 远距离星空需要稳定采样，初版不关闭 Mip |
| Address X / Y | `Wrap` | 经度方向的星图需要连续循环；避免接缝边缘拉伸 |

不要在这里把纹理设成 Normal Map、Masks 或 UI；也不要创建 `Sky Light` 并把此 HDRI 直接指定给它。背景亮度与玩法补光必须在后续 SL3 分离控制。

---

## 4. 创建 M_SpaceSky 材质

### 4.1 新建材质设置

1. 在 `/Game/Sky` 右键创建 Material，命名为 `M_SpaceSky`。
2. 打开材质，在 Details 中设置：

| 属性 | 值 |
| --- | --- |
| Material Domain | `Surface` |
| Blend Mode | `Opaque` |
| Shading Model | `Unlit` |
| Two Sided | 开启 |
| Cast Shadow | 材质不需要单独设置；后续在 Actor 关闭阴影 |

`Two Sided` 允许相机位于普通球体网格内部时看到内表面，因此本稿不要求制作反向法线球体。

### 4.2 材质节点连接

在空白材质图中创建并连接以下节点：

```text
TextureCoordinate (Coordinate Index = 0)
        ↓ UVs
Texture Sample Parameter 2D
    Parameter Name = SpaceSkyTexture
    Texture = /Game/Sky/qwantani_night_puresky_4k
        ↓ RGB
Multiply
    A = Texture Sample RGB
    B = Scalar Parameter: StarIntensity (default = 0.05)
        ↓
Emissive Color
```

创建一个 `Scalar Parameter`：

| 参数名 | 默认值 | 调整范围 | 用途 |
| --- | ---: | ---: | --- |
| `StarIntensity` | `0.05` | `0.01 - 0.15` | 压低 EXR HDR 亮度，控制星点与背景明度 |

除 `Emissive Color` 外，材质主节点的 `Base Color`、`Normal`、`Roughness`、`Specular` 和 `Opacity` 全部不连接。编译并保存，确认没有材质报错。

### 4.3 纹理方向校正

先保持上述最简单的直接 UV 接法。将材质挂入关卡后，按第 6 节检查方向：

- 若星空上下颠倒：在 `TextureCoordinate` 与 Texture Sample 之间加入 `CustomRotator`，中心为 `(0.5, 0.5)`、旋转角度设为 `0.5`，或在材质中翻转 V 坐标。
- 若主要星带 / 亮星位置只是左右偏移：这是经度零点不同，不是错误；用 `CustomRotator` 小幅旋转，直到构图自然。
- 若接缝位于镜头常用视野中心：只旋转星图采样，不旋转行星、相机或 `DL_Sun`。
- 若纹理显示为严重拉伸或只显示局部：确认背景使用的是带经纬度 UV 的球体，且 `TextureCoordinate` 使用 `Coordinate Index = 0`。

不要在初版使用 `Reflection Vector` 采样这张 `Texture2D`；该方法适用于 `TextureCube`，不是当前资源类型。

### 4.4 创建材质实例

1. 右键 `M_SpaceSky`，创建 Material Instance，命名 `MI_SpaceSky`。
2. 在实例中确认 `SpaceSkyTexture` 指向 `/Game/Sky/qwantani_night_puresky_4k`。
3. 勾选并设置 `StarIntensity = 0.05`。
4. 保存实例。后续只在 `MI_SpaceSky` 调参数，不直接改母材质。

---

## 5. 在关卡中配置 SpaceSky 背景

### 5.1 放置球形背景网格

1. 打开 `TessellatedMeshTestMap`。
2. 如果 Content Browser 未显示引擎网格，开启 `Settings -> Show Engine Content`。
3. 从 `/Engine/BasicShapes/Sphere` 拖入关卡，Actor 命名为 `SpaceSky`。
4. 将 Location 设为行星中心。当前项目默认行星位于世界中心时，填写：

```text
Location = (0, 0, 0)
Rotation = (0, 0, 0)
Scale    = (20000, 20000, 20000)
```

`/Engine/BasicShapes/Sphere` 的基础半径约为 `50 cm`，上述缩放约对应 `1,000,000 cm` 背景半径。它远大于设计稿中 HISM 行星默认的 `15,000 cm` 半径，也能覆盖常规绕球与缩放相机距离。

若你的相机可离开行星中心超过约 `1,000,000 cm`，按下式增加缩放：

```text
SpaceSkyRadius > GlobeRadiusCM + MaxCameraDistanceFromCenter
SphereScale = SpaceSkyRadius / 50
```

### 5.2 SpaceSky Actor Details

选中 `SpaceSky`，在 Details 中设置：

| 分类 | 属性 | 值 |
| --- | --- | --- |
| Rendering | Materials Element 0 | `MI_SpaceSky` |
| Rendering | Cast Shadow | 关闭 |
| Rendering | Visible in Ray Tracing | 关闭，如该项存在 |
| Collision | Collision Presets | `NoCollision` |
| Navigation | Can Ever Affect Navigation | 关闭，如该项存在 |
| Mobility | Static | 固定；不需要跟随相机 |

此 Actor 只提供画面背景。它不能阻挡鼠标射线、不能投射阴影、不能参与导航，也不应影响 Lumen / 光追几何代理。

### 5.3 检查背景网格

在编辑器视口中拉远并绕行星旋转：

- 任何方向都应看到星空，而不是球体外部、蓝天或地平线。
- 星图不应随相机移动产生明显视差；它应像无限远背景。
- 行星、棋子与 Cell 拾取不应被 `SpaceSky` 遮挡。

若整个背景不可见，依次检查：材质是否 `Unlit`、是否启用 `Two Sided`、`MI_SpaceSky` 是否已挂到 Element 0、Actor 是否在 Hidden 状态。

---

## 6. 配置 DL_Sun 单太阳直射光

### 6.1 清理旧太阳的选择

在 World Outliner 搜索 `DirectionalLight`：

- 若已有一个模板 Directional Light，直接重命名为 `DL_Sun` 并复用，避免同时存在多个主太阳。
- 若存在多个 Directional Light，只保留一个作为 `DL_Sun`；其余先隐藏验证，再删除或移出当前地图。
- 不额外添加 Point Light、Spot Light 或第二盏太阳来“补亮”夜面；SL3 才添加受控的 Sky Light。

### 6.2 DL_Sun 初始参数

选中 `DL_Sun`，设置：

| 分类 | 属性 | 初始值 |
| --- | --- | --- |
| Transform | Rotation | `(25, -35, 0)`，仅作为起点 |
| Light | Mobility | `Movable` |
| Light | Intensity | `5 lux` |
| Light | Use Temperature | 开启 |
| Light | Temperature | `5600 K` |
| Light | Source Angle | `0.5 deg` |
| Light | Cast Shadows | 开启 |
| Atmosphere and Cloud | Atmosphere Sun Light | 关闭 |

UE 的 Directional Light 旋转含义会受项目坐标与视角影响，因此不要追求上述旋转数值完全一致；以“默认镜头里出现侧前方受光和弧形昼夜线”为视觉准则。

### 6.3 调太阳方向的操作法

1. 使用项目默认游戏相机或最常用战略视角，确保完整行星在画面中。
2. 选中 `DL_Sun`，每次只调整一个轴约 `10 - 15 deg`，观察行星受光面。
3. 停在能看到约 `60% - 75%` 受光可见面的方向；昼夜线应避开画面最中央，并呈圆弧。
4. 若行星正面几乎全亮，说明太阳过于接近相机后方；转向侧方。
5. 若行星正面几乎全黑，说明太阳过于接近相机前方；向相机侧回转。
6. 若局部瓦片在 Lit 模式下整片反黑或昼夜线呈明显三角台阶，不用加补光掩盖；先检查 HISM 瓦片法线是否朝外。

### 6.4 本阶段允许的亮度状态

SL2 的目标是建立太阳塑形，不是完成最终曝光。由于 SL3 尚未加入 `SL_SpaceAmbient` 和手动曝光：

- 背光面很暗甚至接近黑：允许。
- 镜头朝星空时整体亮度变化：允许。
- 星点过亮、出现少量 Bloom：允许，但记录 `StarIntensity`，SL3 再收敛。
- 受光面发白或完全过曝：不允许，应先降低 `DL_Sun Intensity` 或 `StarIntensity`。

---

## 7. SL2 验收步骤

### 7.1 编辑器视觉验收

1. 保存 `M_SpaceSky`、`MI_SpaceSky` 和 `TessellatedMeshTestMap`。
2. 在编辑器中从默认战略视角观察行星：背景应为星空，不能再出现蓝天、云、雾或地平线。
3. 绕行星旋转一周：星空覆盖全部方向，网格没有可见边缘，星图没有明显拉伸或中心接缝。
4. 观察行星：应存在连续的受光半球和弧形昼夜线；地表不应因为主光方向而全部发黑或全部被照平。
5. 在 Material Instance 中从 `StarIntensity = 0.05` 开始，必要时在 `0.01 - 0.15` 内调整。目标是星空可见但不比受光地表、棋子或高亮更抢眼。

### 7.2 PIE 与交互回归

1. PIE 启动，确认星空背景在运行时仍可见，且没有模板天空被运行时蓝图重新显示。
2. 将鼠标移动到多个 Cell 上，确认 hover 射线继续命中 HISM 瓦片，而不是 `SpaceSky`。
3. 选择棋子并显示可行动范围；星空 Actor 不应产生碰撞、遮挡或额外阴影。
4. 通过相机绕球、缩放、聚焦不同棋子检查背景半径；相机始终在 `SpaceSky` 内部，不能穿出球体看到黑色空洞或网格外表面。
5. 退出 PIE，确认材质实例、HISM 和棋子没有变为默认棋盘格。

### 7.3 验收成果

- `SpaceSky` 以普通 `Texture2D` 星空纹理提供全方位黑色宇宙背景，无需 C++ 类或 TextureCube。
- `DL_Sun` 是当前地图唯一的太阳主光，能从默认镜头塑造行星受光面和昼夜分界。
- 背景不投影、不碰撞、不遮挡鼠标射线，不影响 HISM、棋子、Cell hover、选中和移动。
- 场景已摆脱“蓝天中漂浮岩石球”的读感；夜面补光、曝光稳定与大气边缘留给后续 SL3 / SL4。

---

## 8. 排错表

| 症状 | 原因 | 修复 |
| --- | --- | --- |
| 星图完全不可见 | 材质不是 Unlit、未开启 Two Sided、实例未挂入网格 | 检查 §4.1、§4.4 和 §5.2 |
| 背景很亮像白雾 | EXR 亮度未经压低，或 `StarIntensity` 太大 | 将 `StarIntensity` 降至 `0.01 - 0.05` |
| 背景仍是蓝天 | SL1 环境 Actor 仍可见或 PIE 中被重新生成 | 返回父稿的 SL1 排查，不能用星空球遮住它 |
| 星图上下颠倒 | 球体 UV 与全景图 V 方向相反 | 按 §4.3 翻转 V 或旋转采样 |
| 星图严重变形 | 使用了不带经纬度 UV 的网格，或错误 TextureCoordinate Index | 改用 `/Engine/BasicShapes/Sphere` 的 UV0 |
| 鼠标不能点到 Cell | `SpaceSky` 有碰撞，阻挡了射线 | `Collision Presets = NoCollision` |
| 行星正面全亮 | `DL_Sun` 太接近相机后方 | 将太阳绕侧方旋转 |
| 行星正面全黑 | `DL_Sun` 太接近相机前方，或瓦片法线反向 | 先侧向旋转太阳；再检查法线朝外 |
| 光照边界呈三角块 | 瓦片顶点法线不平滑 / 朝向错误 | 修正 HISM 瓦片法线；不以增加 Sky Light 规避 |
| 镜头拉远后穿出背景 | `SpaceSky` 半径小于相机最大距离 | 按 §5.1 公式增大 Scale |

---

## 9. 本阶段不处理的内容

- 把 `qwantani_night_puresky_4k` 转换为 Cubemap。
- 使用该星空 EXR 作为 `Sky Light` 光源。
- Sky Light、夜面可读性补光、手动曝光和 Local Exposure。
- 可见太阳盘、太阳耀斑、星空旋转或日夜循环。
- 大气薄壳、极光、云层、行星环或其他天体。
- 为棋子、Cell 高亮或地表单独增加 Emissive 补偿。

SL2 通过后，下一阶段是 SL3：以独立的深蓝黑环境源和固定曝光，让背光面重新达到可操作的可读性。
