# SL3：太空环境补光与固定曝光设计稿

> 父稿：[SpacePlanetLightingDesign.md](SpacePlanetLightingDesign.md) §10 的 **SL3：环境补光与固定曝光**。
>
> 前置稿：[SL2_SpaceSkyBackgroundDesign.md](SL2_SpaceSkyBackgroundDesign.md)。本稿假定 `SpaceSky`、`MI_SpaceSky` 与唯一的 `DL_Sun` 已经完成，且当前地图已呈现黑色星空中的受光行星。

---

## 1. 阶段目标与边界

SL2 解决了“行星在宇宙中”的第一眼读感，但背光面可能接近纯黑，镜头从星空转向行星时也可能因自动曝光发生亮度泵动。对黑底、稀疏星点背景而言，`Sky Light` 捕捉到的平均亮度可能接近零；因此 SL3 使用 Sky Light 作为首选环境补光，并允许一个无阴影的弱填充直射光作为可读性兜底。SL3 只解决这两个问题：

```text
DL_Sun：塑造明亮昼面与昼夜线
       +
SL_SpaceAmbient：极弱冷色补光，保留夜面玩法信息
       +
DL_SpaceFill：仅在 Sky Light 无法读出夜面时启用的无阴影冷色填充
       +
PPV_SpacePlanet：固定曝光，禁止自动适应
       =
昼夜关系明确、背光侧仍可操作、镜头亮度稳定
```

本阶段不做大气薄壳、最终色彩分级、最终 Bloom 或棋子材质改造。若棋子在夜面仍不够醒目，先把全局补光调整到本稿规定上限；仍不够时记录问题，留给独立棋子表现任务处理。

---

## 2. 实现选择

父稿的最终方向是“深蓝黑指定 Cubemap”。当前项目已具备可用的 `Texture2D` 星空背景，但没有已指定的独立深色 Cubemap 资产，因此 SL3 初版采用以下可直接落实的路径：

| 项目 | SL3 初版选择 | 原因 |
| --- | --- | --- |
| Sky Light Source Type | `SLS Captured Scene` | 不要求先导入 / 制作 TextureCube |
| Capture 内容 | 已完成的 `SpaceSky` 和当前太空场景 | 提供极弱、统一的上半球环境色 |
| Real Time Capture | 关闭 | 星空和太阳方向在本阶段不动态变化；避免每帧成本和亮度漂移 |
| Lower Hemisphere | Solid Color，近黑冷蓝 | 阻止“地面方向”产生灰色环境填充 |
| Intensity Scale | `0.06` 起步，先测试至 `1.00` | 判断捕捉环境是否具有可用环境能量 |
| 弱填充直射光 | `DL_SpaceFill`，默认关闭 | 当黑色星空捕捉无法照亮地表时，提供稳定玩法可读性 |

这不是把 `qwantani_night_puresky_4k` 直接赋给 Sky Light，也不是把它当第二盏太阳。Sky Light 只读取当前场景捕捉；`StarIntensity` 仍在 `MI_SpaceSky` 中单独控制。若捕捉画面平均接近黑，Sky Light 无论小幅提高多少都不会形成有效地表补光，此时按第 4.6 节启用 `DL_SpaceFill`。

后续若新增专用的深蓝黑 Cubemap，可将 Source Type 切换为 `SLS Specified Cubemap`，并复用本稿的强度、后处理与验收部分；这属于资源替换，不改变玩法可读性目标。

---

## 3. 前置检查

1. 打开 `/Game/Map/TessellatedMeshTestMap`。
2. 确认只有一盏可见的 Directional Light，名称为 `DL_Sun`。
3. 确认 `SpaceSky` 使用 `MI_SpaceSky`，并且星空在编辑器与 PIE 均可见。
4. 在 World Outliner 搜索 `SkyLight` 和 `PostProcessVolume`，记录已有 Actor。已有 Actor 可以复用，但本地图中不应同时存在多个开启的环境补光 Sky Light 或多个会改曝光的无限范围 PPV。
5. 不修改 `DL_Sun` 的方向；SL3 以 SL2 已验收的昼夜线为基准调补光与曝光。

---

## 4. 配置 SL_SpaceAmbient

### 4.1 添加或复用 Sky Light

1. 在 World Outliner 搜索 `SkyLight`。
2. 若没有 Sky Light：从 Place Actors 面板拖入 `Sky Light`，名称改为 `SL_SpaceAmbient`。
3. 若已有一个仅服务于当前地图的 Sky Light：重命名为 `SL_SpaceAmbient` 并按下表重设。
4. 若有多个 Sky Light：除 `SL_SpaceAmbient` 外，其余先在 Outliner 关闭可见性并在 Details 禁用，确认画面无依赖后移出或删除。不能让两个环境光叠加，否则夜面亮度无法归因。

### 4.2 Details 初始参数

选中 `SL_SpaceAmbient`，在 Details 中设置：

| 分类 | 属性 | 初始值 | 说明 |
| --- | --- | ---: | --- |
| Transform | Mobility | `Movable` | 后续若需要动态太阳，仍可重捕捉 |
| Light | Source Type | `Captured Scene` | 使用当前完成的太空场景 |
| Light | Intensity Scale | `0.06` | 本期唯一允许的起始补光值 |
| Light | Light Color | 白色 | 冷色由捕捉环境和下半球颜色承担，不叠加明显染色 |
| Light | Cast Shadows | 开启 | 保留环境遮蔽与地形关系 |
| Sky Light | Real Time Capture | 关闭 | 本阶段不允许每帧重捕捉 |
| Sky Light | Lower Hemisphere Is Solid Color | 开启 | 消除下方灰色环境光 |
| Sky Light | Lower Hemisphere Color | `#050B16` | 接近黑的冷蓝；保持很低亮度 |
| Sky Light | Volumetric Scattering Intensity | `0` | 当前地图不使用体积雾 |

属性组名称会随 UE 小版本略有差异；以属性含义为准。若 `Volumetric Scattering Intensity` 不存在，可忽略该行。

### 4.3 首次捕捉与检查

1. 确认 `SpaceSky` 已可见、`DL_Sun` 已按 SL2 配置、没有蓝天 / 云 / 雾 Actor。
2. 在 `SL_SpaceAmbient` Details 中执行 `Recapture Sky`。
3. 等待视口更新完成，再把相机转向行星背光侧。
4. 预期结果是：背光侧从纯黑变为很暗的冷色轮廓，仍明显暗于昼面。

若 `Recapture Sky` 后夜面突然非常亮，首先把 `Intensity Scale` 降到 `0.03`，其次把 `MI_SpaceSky.StarIntensity` 降到 `0.01 - 0.03` 后再次 `Recapture Sky`。不要立即增加或删除太阳光。

### 4.4 先验证 Sky Light 是否有效

在决定是否启用 `DL_SpaceFill` 前，先完成一次可重复的 Sky Light 验证：

1. 执行一次 `Recapture Sky`。
2. 将 `SL_SpaceAmbient.Intensity Scale` 临时设为 `1.00`。
3. 把相机移至当前最暗的背光地表，观察地表是否出现可辨认的瓦片、岩石或颜色层次。
4. 若 `1.00` 时地表仍接近纯黑，或只出现极微弱星点反射，说明黑色星空的捕捉平均亮度不足；不要再把 Sky Light 提高到大于 `1.00`，直接进入 §4.6。
5. 若 `1.00` 时地表可读，再按 §4.5 将强度降至刚好满足玩法可读性的最低值。

### 4.5 补光强度调参法

只用以下离散步骤调整 `Intensity Scale`：

```text
0.03 -> 0.06 -> 0.10 -> 0.20 -> 0.30
```

每次修改强度后，分别在昼夜线附近和最暗背光面检查：

| 结果 | 判断 | 下一步 |
| --- | --- | --- |
| 背光面仍无法区分棋子和 Cell | 捕捉有效时为补光不足 | 上调一级 |
| 可看见棋子轮廓、阵营色和 Cell 高亮，但地表细节仍较弱 | 正确 | 保持当前值 |
| 背光面地表与昼面接近，昼夜线消失 | 补光过强 | 下调一级 |
| 夜面整体偏灰、没有深空感 | 下半球颜色或强度过亮 | 降低强度，确认 `#050B16` 近黑 |

`0.30` 是捕捉有效时的常规上限。若到 `1.00` 仍没有可用地表层次，按 §4.6 启用弱填充直射光；不要用曝光、Bloom 或无限提高 Sky Light 伪造夜面可读性。

### 4.6 黑色星空场景的稳定兜底：DL_SpaceFill

当截图所示“地表全黑、只有单位自发光可见”出现，并且 §4.4 的 `Intensity Scale = 1.00` 测试仍无法读出地形时，创建一个独立的玩法填充光。它不是太阳，不投射阴影，也不参与 Atmosphere；其职责是为太阳背面提供约 `3% - 6%` 的漫反射可见度。

1. 从 Place Actors 放入一个 `Directional Light`，命名为 `DL_SpaceFill`。
2. 以 `DL_Sun` 的照射方向为基准，将 `DL_SpaceFill` 旋转到大致相反方向。视觉标准是：它照亮当前太阳背面，而不会在太阳受光面产生可见的第二层阴影。
3. 在 Details 中设置：

| 属性 | 值 |
| --- | --- |
| Mobility | `Movable` |
| Intensity | `0.25 lux` 起步，范围 `0.15 - 0.30 lux` |
| Use Temperature | 开启 |
| Temperature | `7500 K` |
| Cast Shadows | 关闭 |
| Atmosphere Sun Light | 关闭 |
| Volumetric Scattering Intensity | `0`，如该项存在 |

4. 将相机停在最暗背光地表；每次以 `0.05 lux` 调整强度，直到能分辨地形纹理轮廓、Cell 边界和非自发光单位轮廓。
5. 回到太阳受光面检查：若受光面出现明显蓝灰第二光源感，先降低 `DL_SpaceFill`，不要通过提高主太阳或曝光抵消。
6. 保持 `DL_SpaceFill` 不投影。开启它会在地表和棋子上产生相反方向的第二套阴影，使“太空中的单太阳”读感崩坏。

推荐起始组合：

```text
DL_Sun       = 5.00 lux, 5600 K, Cast Shadows = On
DL_SpaceFill = 0.25 lux, 7500 K, Cast Shadows = Off
SL_SpaceAmbient = 0.03 - 0.10（若捕捉仍有微弱贡献则保留）
```

若 `DL_SpaceFill = 0.30 lux` 仍无法看到地表，先检查地表材质是否为 `Default Lit`、法线是否朝外、以及 PPV 曝光是否被别的 Volume 覆盖；不要继续加到接近太阳强度。

### 4.7 何时需要再次 Recapture Sky

在以下任意操作后，执行一次 `Recapture Sky`，然后再判断夜面：

- 修改了 `MI_SpaceSky` 的 `StarIntensity` 或星空纹理。
- 修改了 `SpaceSky` 的可见性或材质。
- 新增 / 删除可被天空捕捉到的大型背景对象。
- 将 `DL_Sun` 旋转到全新构图并决定保留。

仅修改 `SL_SpaceAmbient.Intensity Scale`、`DL_SpaceFill` 或 `PPV_SpacePlanet` 曝光时，不需要重新捕捉。

---

## 5. 配置 PPV_SpacePlanet 固定曝光

### 5.1 添加或复用 Post Process Volume

1. 在 World Outliner 搜索 `PostProcessVolume`。
2. 若不存在：从 Place Actors 放置 `Post Process Volume`，名称设为 `PPV_SpacePlanet`。
3. 若已有只服务当前地图的 PPV：复用并命名为 `PPV_SpacePlanet`。
4. 选中它，在 Details 开启 `Infinite Extent (Unbound)`，确保相机在行星四周、远离行星或处于背光侧时都使用同一套后处理。
5. 若已有另一个 Unbound PPV，先检查其 Priority 与 Exposure 项。SL3 期间只允许一个 PPV 负责曝光；其余 PPV 必须禁用 Exposure Override，或暂时关闭 / 降低优先级。

### 5.2 Exposure 初始配置

在 `PPV_SpacePlanet -> Exposure` 分类中设置：

| 属性 | 值 | 目的 |
| --- | --- | --- |
| Metering Mode | `Manual` | 禁止自动曝光适应 |
| Exposure Compensation | `0.0` | 从中性基准开始校准 |
| Exposure Compensation Curve | 无 | 不随亮度变化 |
| Apply Physical Camera Exposure | 关闭，如该项存在 | 防止相机物理参数成为隐含亮度变量 |

项目已启用扩展曝光亮度范围。不同 UE 小版本中，若 `Metering Mode = Manual` 后仍显示 `Min EV100` / `Max EV100`：保持它们不驱动自动范围即可；不要为了“锁定”同时开启另一种 Auto Exposure 覆盖。

如果当前 UE 版本或项目模板没有可用的 `Manual` 模式，使用下面的兼容替代方案：

```text
Metering Mode = Auto Exposure Basic
Min EV100 = Max EV100 = 当前画面合适的同一数值
Speed Up = 0
Speed Down = 0
```

该替代方案的验收标准与 Manual 相同：相机朝星空、昼面和夜面移动时，不发生随场景亮度变化的自动适应。

### 5.3 固定曝光校准顺序

严格按下列顺序做，不在同一轮同时改两个变量：

1. 先把 `SL_SpaceAmbient.Intensity Scale` 固定在第 4 节已经验收的值。
2. 从默认战略视角观察受光面，`Exposure Compensation` 先保持 `0.0`。
3. 若受光面的地表材质明显泛白、棋子细节消失，将 Compensation 每次降低 `0.25`。
4. 若受光面整体过暗、无法读出平原 / 森林 / 山脉，将 Compensation 每次提高 `0.25`。
5. 每调整一次，立即把镜头转向纯星空、昼夜线和最暗背光面各观察一次。
6. 停在“受光面无溢白，夜面仍保留最低轮廓，切换视角无亮度泵动”的值上。

初版允许的 `Exposure Compensation` 建议范围为 `-1.0 - +1.0`。超过该范围优先回头检查 `DL_Sun.Intensity`、`StarIntensity` 与 Sky Light 强度，而不是持续用曝光修正掩盖光照比例问题。

### 5.4 Bloom 暂定设置

SL3 不做最终后期精修，但应避免默认 Bloom 让星空干扰玩法。在 `PPV_SpacePlanet -> Bloom` 中：

| 属性 | 临时值 |
| --- | ---: |
| Intensity | `0.2` |
| Threshold | `1.5` |

如果当前项目没有覆盖 Bloom，先保持引擎默认值也可；只要星点、太阳高光或地表不会形成明显的大面积光晕。最终视觉微调留给 SL4。

---

## 6. SL3 联合验收步骤

### 6.1 编辑器静态验收

1. 保存 `MI_SpaceSky`、`TessellatedMeshTestMap` 和所有 External Actor 提示的包。
2. 在默认战略视角观察昼面：地表不能溢白，棋子和地形类型仍可区分。
3. 绕球移动至昼夜线：应看到明显弧形明暗过渡，而非被均匀环境光抹平。
4. 移到背光侧：应能辨认棋子轮廓、阵营色、Cell 基础边界和现有 hover / 高亮，但地表整体明显暗于昼面。
5. 镜头在纯星空、昼面和夜面之间快速切换三次：亮度必须保持稳定，不能有数秒渐亮 / 渐暗过程。

### 6.2 PIE 玩法回归

1. PIE 启动，重复 6.1 的昼面、昼夜线、背光侧检查。
2. 在背光侧选择棋子，确认黄色选中态不被夜面吞没。
3. 显示普通移动 / 跳跃范围，确认蓝色 Cell 高亮仍区别于冷蓝背景和阴影。
4. 完成一次移动、回合切换、相机聚焦和手动绕球，观察任何操作都不触发曝光泵动。
5. 退出 PIE，检查编辑器中 `SL_SpaceAmbient`、`PPV_SpacePlanet`、HISM 和棋子材质仍保持正确状态。

### 6.3 验收成果

- 场景存在唯一的 `SL_SpaceAmbient`。若黑色星空捕捉没有足够环境能量，则存在唯一的无阴影玩法填充光 `DL_SpaceFill`，强度位于 `0.15 - 0.30 lux`，背光面可操作但明显暗于昼面。
- `PPV_SpacePlanet` 以 Manual Exposure 覆盖整个地图；镜头在星空、昼面和夜面之间移动时无自动曝光泵动。
- 星空保持视觉背景职责，不会作为强环境光源把行星夜面抬亮。
- 昼夜线仍清晰；HISM、棋子、Cell hover、选择和移动在编辑器与 PIE 中均正常。

---

## 7. 排错表

| 症状 | 常见原因 | 修复 |
| --- | --- | --- |
| 夜面完全黑 | Sky Light 捕捉平均接近黑，或未完成首次捕捉 | 按 §4.4 以 `1.00` 验证；无效则启用 §4.6 的 `DL_SpaceFill` |
| 夜面像阴天白昼 | Sky Light / 填充光强度过高，或存在多个环境光 | 降低 Sky Light 或 `DL_SpaceFill`，并确保各自 Actor 唯一 |
| 夜面灰白无深空感 | Lower Hemisphere 未设为 Solid Color，或颜色太亮 | 开启 Solid Color，使用 `#050B16` 或更暗颜色 |
| 转镜头时亮度跳变 | PPV 未 Unbound、曝光仍是 Auto、另一个 PPV 覆盖 | 检查 §5.1 / §5.2 的 PPV 数量、Priority 与 Metering Mode |
| 受光面泛白 | 曝光 Compensation 或太阳强度过高 | 先每次降低 Compensation `0.25`；仍过曝再降低 `DL_Sun` |
| 星点像大面积雾光 | `StarIntensity` 过大或 Bloom 太强 | 先降 `StarIntensity`，再将 Bloom 设为 `0.2 / 1.5` |
| 修改星空后夜面颜色不变 | Sky Light 使用 Captured Scene 但没有重捕捉 | 对 `SL_SpaceAmbient` 执行 `Recapture Sky` |
| 选中黄 / 行动蓝在夜面不醒目 | 试图用全局光解决局部交互问题 | 填充光不超过 `0.30 lux`；仍不足则交给棋子 / 高亮材质任务 |
| Sky Light 捕捉到蓝天 | SL1 环境 Actor 未清干净 | 返回 SL1 排错，不能靠降低 Sky Light 遮掩 |

---

## 8. 本阶段退出条件

以下条件必须同时满足，SL3 才可结束并进入 SL4：

- 默认战略镜头和常用绕球镜头均不再发生自动曝光适应。
- 背光侧可以完成棋子 hover、选择与行动范围判断，但仍明显暗于昼面。
- `DL_Sun` 的昼夜线没有被补光抹平。
- 当前地图只有一个负责环境补光的 Sky Light、至多一个无阴影 `DL_SpaceFill`，以及一个负责固定曝光的无限范围 PPV。
- 编辑器与 PIE 的视觉和交互结果一致。

SL3 的成果是“可读、稳定”的太空行星；SL4 才处理大气边缘、最终色彩层级与表现精修。
