# SL4：视觉层级精修与行星大气边缘设计稿

> 父稿：[SpacePlanetLightingDesign.md](SpacePlanetLightingDesign.md) §10 的 **SL4：视觉层级精修与可选大气边缘**。
>
> 前置稿：[SL2_SpaceSkyBackgroundDesign.md](SL2_SpaceSkyBackgroundDesign.md) 与 [SL3_SpaceAmbientAndExposureDesign.md](SL3_SpaceAmbientAndExposureDesign.md)。本稿开始前，地表材质法线、Static Mesh 绕序和切线空间法线贴图必须已经修复；SL4 不用后处理掩盖任何基础法线、阴影或网格问题。

---

## 1. 阶段目标与边界

SL4 的目标是让已经“可读、稳定”的宇宙行星画面获得明确的视觉层级：

```text
第一层：棋子、选中态、行动范围、Hover / Target
第二层：受光地表、山脉轮廓、昼夜线
第三层：星空、低强度大气轮廓、有限 Bloom
```

完成画面应有清晰的太阳方向和球体曲率，星球从星空中分离出来，但不会变成蓝天 / 雾中的地球。黄色选中、蓝色行动范围、阵营棋子必须始终比普通地表与星点优先。

本阶段不处理：真实大气散射、体积云、日夜循环、可见太阳盘、太阳耀斑、行星环、星空动画，或为修复法线问题而修改灯光。

---

## 2. 前置检查与锁定基线

在开始调色、Bloom 或大气壳前，保存当前已通过 SL3 的地图，并记录以下基准：

| 项目 | 基准要求 |
| --- | --- |
| `DL_Sun` | 唯一有阴影的主太阳；受光半球与阴影方向一致 |
| `SL_SpaceAmbient` | 已确定最低可读性强度；不再随意抬高 |
| `DL_SpaceFill` | 若使用，仅为无阴影、低强度玩法填充；不在 SL4 增强 |
| `PPV_SpacePlanet` | 已 Unbound，曝光已固定，无镜头自动曝光泵动 |
| `SpaceSky` | 纯星空背景，无云、雾、蓝天或地平线 |
| Tile 材质 | `NormalDX`、法线贴图与最终网格法线已验证正确 |

执行一次基准截图：默认战略镜头、昼夜线镜头、最暗可玩夜面镜头各一张。SL4 的任何修改都要与这三张基准对比；若棋子、Cell 高亮或昼夜线变差，先回退本阶段参数，而不是修改 SL2 / SL3 基线。

---

## 3. 调参顺序

SL4 必须按以下顺序执行，每一项完成后保存并做局部验收：

```text
1. 固定太阳构图与阴影软硬度
2. 控制后处理 Bloom
3. 控制全局色彩层级
4. 验证棋子与 Cell 交互对比
5. 可选添加大气薄壳
6. 完整玩法回归
```

禁止反向调参。例如先做强 Bloom 或大气壳，再用曝光和补光修正，会使问题来源无法判断。

---

## 4. 太阳构图与阴影精修

### 4.1 DL_Sun 可调整范围

SL4 只允许调整 `DL_Sun` 的构图和阴影边缘，不改变“单太阳主导”的关系。

| 属性 | 初始参考 | 可调范围 | 验收意图 |
| --- | ---: | ---: | --- |
| Intensity | 保持 SL3 值 | 仅在曝光校准后微调 | 不让昼面过曝或发灰 |
| Temperature | `5600 K` | `5200 - 5800 K` | 轻微暖白，不做橙色夕阳 |
| Source Angle | `0.5 deg` | `0.3 - 1.0 deg` | 阴影清晰但山地边缘不出现刺眼锯齿 |
| Rotation | SL3 已验收方向 | 每次一个轴 `5 - 10 deg` | 让默认镜头同时看到受光面和昼夜线 |

### 4.2 操作步骤

1. 使用默认战略镜头，选中 `DL_Sun`。
2. 先只调整 Rotation，使可见球面约有 `60% - 75%` 处于直射光下。
3. 确认昼夜线是连续弧线，不穿过主要选中棋子或行动区域的中心。
4. 观察山脉投影：影子必须从山体沿远离太阳的方向延伸，且同一局部区域内方向一致。
5. 只在山体阴影边缘过硬、锯齿明显时，把 `Source Angle` 由 `0.5` 逐步提高至 `0.7` 或 `1.0 deg`。
6. 不通过关闭 `Cast Shadows`、增加第二个投影光或提高环境光来处理阴影问题。

### 4.3 通过标准

- 太阳受光、山地亮面与山地投影方向符合直觉。
- 默认镜头不呈现正面全亮的材质样球，也不呈现正面全黑的剪影。
- 山地阴影有助于判断高度，但不会遮没棋子、Cell 高亮或主要行动区域。

---

## 5. Bloom 与全局色彩层级

### 5.1 Bloom

在 `PPV_SpacePlanet -> Bloom` 中设置并覆盖：

| 属性 | 起始值 | 可调范围 | 说明 |
| --- | ---: | ---: | --- |
| Intensity | `0.20` | `0.10 - 0.35` | 只给亮星、少量高光和大气边缘轻微辉光 |
| Threshold | `1.50` | `1.20 - 2.00` | 普通地表和交互颜色不应泛光 |

操作方法：

1. 先将 Intensity 设为 `0.20`、Threshold 设为 `1.50`。
2. 在星空、昼面高光、黄色选中和蓝色行动范围同时可见的镜头检查。
3. 若星点形成成片雾光、选中黄色边缘膨胀，先降低 Intensity。
4. 若大气薄壳启用后完全没有轮廓感，优先微降 Threshold，而不是把 Intensity 提到 `0.35` 以上。

### 5.2 全局色彩分级

在 `PPV_SpacePlanet -> Color Grading -> Global` 中设置：

| 属性 | 起始值 | 可调范围 | 说明 |
| --- | ---: | ---: | --- |
| Saturation | `0.95` | `0.90 - 1.00` | 收住岩石 / 草地饱和度，保留阵营色空间 |
| Contrast | `1.00` | `0.95 - 1.05` | 只做小幅对比校正 |
| Gamma | `1.00` | 固定优先 | 不用 Gamma 伪造夜面补光 |
| Shadows Tint | 极弱蓝青 | 低于肉眼显著染色 | 与太空环境呼应，不变成紫蓝滤镜 |

不使用整屏蓝色 LUT，也不把 Global Saturation 拉高来“让森林更绿”。地形色彩由材质负责；SL4 只给玩法色留出对比空间。

### 5.3 通过标准

- 星点存在但不抢过棋子和交互高亮。
- 黄色、蓝色、各阵营主色与地表色保持足够区分。
- 画面不会出现整体雾化、奶白高光或大面积蓝紫染色。

---

## 6. 棋子与 Cell 交互可读性验收

SL4 不直接修改棋子材质，但必须验证当前全局光照没有吞没既有交互表现。

### 6.1 检查顺序

1. 在太阳受光面选择任意阵营棋子。
2. 在昼夜线附近选择同一类型棋子。
3. 在最暗但仍可玩背光面选择棋子。
4. 分别触发 Hover、当前选中、普通移动范围、跳跃范围和目标提示。
5. 每种状态保持至少三秒，观察 Bloom、阴影与颜色是否改变其边界或造成误判。

### 6.2 判断表

| 现象 | 判断 | SL4 内处理 |
| --- | --- | --- |
| 黄色选中被太阳高光吞没 | 全局高光 / Bloom 过强 | 降 Bloom 或略调太阳构图 |
| 蓝色行动范围与夜面混淆 | 夜面色相与蓝色过近 | 先减弱 Shadows Tint；不提高环境光 |
| 阵营棋子在夜面仅剩自发光 | 棋子本体可读性不足 | 记录为棋子材质任务；不提高全局填充光 |
| 山体阴影遮住 Cell 提示 | 太阳角度影响玩法区域 | 小幅调整太阳构图或 Source Angle |
| 背景星点穿插 / 遮挡拾取 | SpaceSky 资产配置错误 | 返回 SL2，检查 `NoCollision` 与阴影关闭 |

### 6.3 通过标准

- 三种常用镜头亮度下，玩家无需改变相机即可识别当前选中棋子和可行动 Cell。
- 交互颜色的视觉优先级高于普通岩石、草地纹理、星点和大气轮廓。
- 不以增大全局环境光或打开第二套阴影来换取可读性。

---

## 7. 可选大气薄壳

### 7.1 启用前提

只有 §4 - §6 已通过时，才创建大气薄壳。若当前视觉已经足够清楚，大气壳可以永久不启用；它是“加强星球与背景分离”的可选表现，不是太空感的基础依赖。

### 7.2 新建资产

| 类型 | 名称 | 推荐位置 |
| --- | --- | --- |
| Material | `M_PlanetAtmosphere` | `/Game/Sky/M_PlanetAtmosphere` |
| Material Instance | `MI_PlanetAtmosphere` | `/Game/Sky/MI_PlanetAtmosphere` |
| Static Mesh Actor | `PlanetAtmosphere` | `TessellatedMeshTestMap` |

使用 `/Engine/BasicShapes/Sphere`，Location 与行星中心一致。若 `GlobeRadiusCM = 15000`，基础球半径为 `50 cm`，可从以下值开始：

```text
Location = (0, 0, 0)
Rotation = (0, 0, 0)
Scale    = (303, 303, 303)
```

其半径约为 `15150 cm`。实际值应按公式确定：

```text
AtmosphereRadius = GlobeRadiusCM * 1.003 - 1.010
SphereScale = AtmosphereRadius / 50
```

若行星实际半径并非 `15000 cm`，必须按当前 `GlobeRadiusCM` 重算，不能照抄示例 Scale。

### 7.3 M_PlanetAtmosphere 材质

创建 `M_PlanetAtmosphere`，设置：

| 属性 | 值 |
| --- | --- |
| Material Domain | `Surface` |
| Shading Model | `Unlit` |
| Blend Mode | 先用 `Additive` |
| Two Sided | 开启 |

材质节点使用一个纯 Fresnel 轮廓，不采样天空、不使用 `SkyAtmosphere`：

```text
Fresnel
    Exponent = Scalar Parameter RimPower (default 5.0)
    Base Reflect Fraction = 0.0
        ↓
Multiply
    A = Fresnel
    B = Vector Parameter AtmosphereColor (default #7FC8FF)
        ↓
Multiply
    B = Scalar Parameter AtmosphereIntensity (default 0.03)
        ↓
Emissive Color
```

创建参数：

| 参数 | 默认值 | 调整范围 | 用途 |
| --- | ---: | ---: | --- |
| `RimPower` | `5.0` | `3.0 - 8.0` | 数值越大，边缘越窄 |
| `AtmosphereIntensity` | `0.03` | `0.01 - 0.08` | 控制轮廓亮度，必须保持克制 |
| `AtmosphereColor` | `#7FC8FF` | 冷蓝白附近 | 仅作边缘分离，不为地表染色 |

将 `MI_PlanetAtmosphere` 挂入 `PlanetAtmosphere` 的 Element 0，并设置：

| 分类 | 属性 | 值 |
| --- | --- | --- |
| Collision | Collision Presets | `NoCollision` |
| Rendering | Cast Shadow | 关闭 |
| Rendering | Visible in Ray Tracing | 关闭，如该项存在 |
| Mobility | Static | 固定 |

### 7.4 大气壳调参步骤

1. 首先保持 `AtmosphereIntensity = 0.03`、`RimPower = 5.0`。
2. 在完整行星镜头中观察轮廓，只允许在最外缘出现薄亮边。
3. 若正面出现明显蓝色滤镜，先提高 `RimPower`，再降低 Intensity。
4. 若轮廓太宽，逐级提高 `RimPower`；不要靠降低 SphereScale 让壳体贴入地表。
5. 若轮廓完全不可见，在确认 Bloom 参数后，将 Intensity 以 `0.01` 递增，最大不超过 `0.08`。
6. 在昼面、昼夜线、背光侧、镜头缩放和相机绕球时检查透明排序与闪烁。

### 7.5 大气壳失败处理

出现以下任一情况，关闭 `PlanetAtmosphere`，仍可判定 SL4 通过：

- 与水面或半透明特效发生明显排序错误。
- TAA 下出现闪烁、锯齿或重影。
- 整颗行星被蓝色 / 白色雾化。
- 大气边缘与黄色、蓝色交互高亮发生竞争。

---

## 8. SL4 验收成果

### 8.1 静态视觉验收

1. 从默认战略镜头观察完整行星：受光面、昼夜线、山地层次和星空分离清楚。
2. 缩放至中近景：地表纹理和法线正确响应太阳，不出现此前的哑铃亮暗、反向山影或大面积法线断层。
3. 绕球一周：背景始终保持宇宙感，球体轮廓不被雾、蓝天或过强 Bloom 吞没。
4. 若启用大气壳：它只在轮廓边缘出现，不在地表正面形成蓝色覆盖。

### 8.2 玩法验收

1. 在昼面、昼夜线和背光可玩面，分别执行 hover、棋子选择、普通移动、跳跃与回合切换。
2. 确认黄色选中、蓝色行动范围和阵营棋子均优先于地表、阴影、星点和大气壳。
3. 相机聚焦、手动绕球、缩放、快速切换星空 / 行星时，不出现曝光泵动、背景穿帮或交互丢失。
4. PIE 退出后，确认 HISM、材质实例、`PPV_SpacePlanet` 与可选大气壳仍保持正确状态。

### 8.3 阶段完成定义

满足以下条件时，SL4 完成：

- 场景稳定地读成“黑色宇宙中的可操作行星”，而不是蓝天中的岩石球。
- 太阳是唯一主导阴影和昼夜结构的光源，基础光照与材质法线一致。
- 星点、Bloom 与可选大气轮廓提供气氛，但不降低棋子和 Cell 交互的优先级。
- 可选大气壳无透明渲染问题；若关闭它，核心视觉和玩法验收仍完全成立。

---

## 9. 排错表

| 症状 | 优先排查 | 修复 |
| --- | --- | --- |
| 昼面又出现反向亮暗 / 阴影错位 | 材质 Normal、Static Mesh 重建、DL_Sun 数量 | 返回 Tile 法线与单太阳检查；不要用后处理掩盖 |
| 山影过硬、锯齿明显 | `DL_Sun.Source Angle` | 在 `0.3 - 1.0 deg` 内提高，保持单太阳 |
| 星点变成白色雾层 | Bloom Intensity 或 `StarIntensity` | 先降 Bloom，再回到 SL2 降星图亮度 |
| 黄色 / 蓝色高亮泛光 | Bloom Threshold 过低或 Intensity 过高 | 提高 Threshold 或降低 Intensity |
| 夜面变灰 | SL3 环境光 / 填充光被提高 | 回到 SL3 的最低可读性值 |
| 大气壳覆盖地表 | RimPower 太低、Intensity 太高、SphereScale 错误 | 提高 RimPower、降 Intensity、按半径公式重算 |
| 大气壳闪烁或排序错误 | 半透明 / Additive 渲染限制 | 关闭大气壳；不阻塞 SL4 |
| 背景遮挡点击 | `SpaceSky` 或 `PlanetAtmosphere` 开启碰撞 | 两者都设为 `NoCollision` |

---

## 10. 后续衔接

若需要远景朦胧感，SL4 通过后先进入 [SL4_5_PlanetAerialPerspectiveDesign.md](SL4_5_PlanetAerialPerspectiveDesign.md)：该阶段以地表专属后处理空气透视建立近清远朦的层次。SL4.5 通过后再进入 SL5，统一固化 `DL_Sun`、`SL_SpaceAmbient`、`PPV_SpacePlanet`、`MI_SpaceSky`、可选 `MI_PlanetAtmosphere` 与空气透视材质实例的最终参数。

后续若需要真实大气、云层、可见太阳盘或动态日夜循环，应建立独立设计稿；不得直接把开放世界模板的 `SkyAtmosphere`、体积云和高度雾重新加入当前主地图。
