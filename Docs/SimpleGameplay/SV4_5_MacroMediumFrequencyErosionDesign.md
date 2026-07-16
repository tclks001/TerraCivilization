# SV4.5：宏观中频侵蚀设计稿

> 状态：实现稿。
>
> 编码：UTF-8，简体中文。
>
> 前置：SV4 山脊线低频高度已验收。本阶段位于 SV4 与 SV5 水文之间。

---

## 1. 目标与边界

SV4.5 为已有连续表面增加确定性的中频高度细节，消除等高、过于规整的山脊与大面积完全平坦的地表。它模拟侵蚀后的宏观轮廓，不尝试在运行时执行真实水力或热力侵蚀模拟。

本阶段不做：

- Gameplay 地形、移动规则、寻路、CellId、UI 或 WorldGen 地形类别修改；
- 河道、水面、湿润带和真实水文流向计算，留给 SV5；
- 高于 sub=7 可可靠表达频率的几何碎石、裂纹和草叶，留给 SV3 材质与 SV6 Decor；
- 相机位置/朝向或棋子朝向读取地表坡面法线。相机与棋子旋转继续使用标准球体径向方向。

## 2. 第三方依赖

从 PTG 插件复制 FastNoiseLite 1.0.1 单头文件与 MIT License 到：

```text
Source/TerrainVisual/Public/ThirdParty/FastNoiseLite/FastNoiseLite.h
Source/TerrainVisual/Public/ThirdParty/FastNoiseLite/LICENSE.txt
```

TerrainVisual 直接包含该文件，不依赖、链接或加载 `ProceduralTerrainGenerator`/`FastNoiseLite` 插件模块。未来删除 PTG 插件不会影响本阶段。

## 3. 高度模型

SV4.5 增量为：

```text
MediumFrequencyHeight =
    CrestVariation
  - MountainErosion
  + LowlandUndulation
```

所有噪声都以旋转后的 `UnitDirection` 为 3D 输入，禁止使用经纬度、Cell UV 或 HISM UV：

```text
NoisePosition = RotateBySeed(UnitDirection) * NoiseFrequency
```

这保证跨 Cell、跨球面经线连续，并在相同 `GlobalVisualSeed` 下稳定复现。

### 3.1 山脊纵向起伏

使用 OpenSimplex2S fBm，取值范围约为 `[-1, 1]`。其影响由当前山脊 SDF 权重约束：

```text
CrestVariation = CrestAmplitude * fBm3D * ridgeWeight
```

默认 `CrestAmplitude=420cm`。它使同一条山脊的高度沿线有起伏，同时在两侧自然消退，不改变山脊线的连续性。

### 3.2 山坡侵蚀沟槽

使用 OpenSimplex2S 的 progressive domain warp 后，再以 Ridged fBm 生成细长沟谷候选：

```text
warped = DomainWarp3D(NoisePosition)
ridgeNoise = RidgedFBm3D(warped)        // [-1, 1]
valleyMask = pow(1 - saturate((ridgeNoise + 1) * 0.5), ValleySharpness)
MountainErosion = ErosionAmplitude * ridgeWeight * slopeMask * valleyMask
```

`slopeMask = 4 * ridgeWeight * (1 - ridgeWeight)`，限制侵蚀主要发生在山体中上坡，避免破坏峰顶和山脚的连续边界。默认 `ErosionAmplitude=260cm`、`ValleySharpness=2.2`。

SV5 的真实河道 SDF 将覆盖其中少数主沟谷；SV4.5 的沟槽不承诺水文正确性。

### 3.3 低地缓起伏

平原和森林只使用低八度的 OpenSimplex2S fBm：

```text
LowlandUndulation = LowlandAmplitude * fBm3D * (1 - mountainWeight)
```

默认 `LowlandAmplitude=55cm`，森林可沿用该值。它只打破完全规则的球面，不应影响玩法地形边界的可读性。

## 4. 参数

参数位于 `APlanetTessellatedMesh` 的 `Terrain Visual|SV4.5 Medium Frequency` 分类，并传入 `FTerrainVisualConfig`。

| 参数 | 默认值 | 说明 |
| --- | ---: | --- |
| `bEnableMediumFrequencyErosion` | true | 总开关。 |
| `CrestNoiseAmplitudeCM` | 420 cm | 山脊纵向起伏幅度。 |
| `ErosionAmplitudeCM` | 260 cm | 山坡沟槽最大下切量。 |
| `LowlandNoiseAmplitudeCM` | 55 cm | 平原/森林的微弱宏观起伏。 |
| `MediumFrequencyNoiseFrequency` | 5.5 | 3D 球面噪声频率。 |
| `ErosionDomainWarpAmplitude` | 0.42 | 域扭曲强度。 |
| `ErosionValleySharpness` | 2.2 | 沟谷掩码锐度。 |

## 5. 性能与法线

FastNoiseLite 对每个 `TerrainVisualField` 查询仅进行常数次 3D 噪声采样。sub=7 表面在编辑器重建、PIE 初始化或低频视觉事件中重算即可；不得逐帧重建 Procedural Mesh。

高度场法线继续由相邻切线方向采样得出，因此新形变会同步影响光照、碰撞表面和棋子高度。中频波长必须保持至少约 3 至 4 个 sub=7 三角形边长，防止锯齿、闪烁和碰撞锯齿。

## 6. 验收

1. 同一山脊不再等高，峰顶有稳定但不过分锯齿的纵向变化。
2. 山体两侧出现不规则、非平行的中频沟槽；主山脊线仍连续，不被噪声切断。
3. 平原和森林不再完全平整，但玩法三地形边界、高亮和 CellId 查询位置不改变。
4. 同一 seed 重开地图结果一致；改变 `GlobalVisualSeed` 只改变视觉形态。
5. 点击、移动、Undo、棋子高度、相机稳定性和 UI 均保持 SV4 行为。
6. 删除或禁用 PTG 插件后，TerrainVisual 仍可独立编译与运行。

## 7. 后续

SV5 生成真实装饰性河网时，河道从 SV4.5 高度场的高处向低处求解，并在材质中覆盖、强化主要侵蚀沟谷。
