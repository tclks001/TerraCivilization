# TerraCivilization SimpleGameplay 棋子材质调色板线索整理

> 本文记录对 `Content/Animations/Adventurers/Characters` 下人物模型、材质实例和贴图资产的初步检查结果。
>
> 目标是为后续“不同阵营棋子颜色区分”提供依据。本文只整理资产线索，不定义最终实现方案。

---

## 1. 检查范围

本次重点检查以下目录：

```text
Content/Animations/Adventurers/Characters/
Content/Animations/Adventurers/Characters/raw/
Content/Animations/Adventurers/Textures/
Content/Animations/Adventurers/Textures/raw/
Content/Animations/Adventurers/Assets/
Content/Animations/Adventurers/Assets/raw/
```

主要关注：

- 人物 SkeletalMesh 使用的材质槽。
- 材质实例的父材质和参数。
- 人物贴图是否为固定色块调色板。
- 当前资产中是否存在“按部位编辑颜色”的显式参数或蓝图。

---

## 2. SkeletalMesh 与材质槽

当前 `Characters` 下主要人物模型均为单材质槽结构。

| SkeletalMesh | 材质实例 | 材质槽名 |
| --- | --- | --- |
| `Barbarian1` | `barbarian` | `barbarian` |
| `Knight1` | `knight` | `knight` |
| `Mage1` | `mage` | `mage` |
| `Ranger1` | `ranger` | `ranger` |
| `Rogue1` | `rogue` | `rogue` |
| `Rogue_Hooded` | `rogue` | `rogue` |

结论：

- 当前不是“每个身体部位一个材质槽”的方案。
- 人物部位差异大概率由同一张贴图上的不同 UV 区域决定。

---

## 3. 材质实例结构

所有人物材质实例均继承自：

```text
/InterchangeAssets/Materials/FBXLegacyPhongSurfaceMaterial
```

材质实例中可见的主要参数为：

```text
DiffuseColorMapWeight = 1.0
Shininess = 25.0
RefractionDepthBias = 0.0
AmbientColor
SpecularColor
DiffuseColorMap
```

其中 `DiffuseColorMap` 指向对应人物贴图，例如：

| 材质实例 | DiffuseColorMap |
| --- | --- |
| `knight` | `/Game/Animations/Adventurers/Assets/knight_texture` |
| `mage` | `/Game/Animations/Adventurers/Assets/mage_texture` |
| `ranger` | `/Game/Animations/Adventurers/Assets/ranger_texture` |
| `barbarian` | `/Game/Animations/Adventurers/Assets/barbarian_texture` |
| `rogue` | `/Game/Animations/Adventurers/Assets/rogue_texture` |

未发现以下类型的显式参数：

```text
SkinColor
HairColor
ArmorColor
ClothColor
PartColor_0..31
PaletteIndex
FactionColor
```

结论：

- 当前导入材质只是 FBX legacy phong 材质实例。
- 没有看到可直接在材质实例中编辑“某个人物部位颜色”的机制。
- 若要做阵营换色，需要新增项目自定义材质或运行时替换贴图方案。

---

## 4. 人物贴图色块结构

`Characters/raw` 下人物贴图均为：

```text
1024 x 1024
8 列 x 4 行
共 32 个色块
每个色块约 128 x 256
```

这说明当前人物资源使用调色板式贴图：

```text
模型 UV 岛落在某个色块区域
    -> 该部位显示该色块颜色 / 渐变
```

因此“某个色块对应哪个人物部位”的信息不在材质参数里，而在 SkeletalMesh 的 UV 布局里。

---

## 5. 部分贴图色块中心色

以下颜色来自每个 128 x 256 色块中心点采样，用于确认贴图布局，不代表完整渐变范围。

### 5.1 Knight

```text
row 0: #F6C09C #DAAE7D #13191B #818C91 #4A5155 #B17052 #9B5A45 #818C91
row 1: #D3252A #D4DBDE #818C91 #818C91 #E8B95C #9B5A45 #6B5C53 #4A5155
row 2: #D4DBDE #978F86 #D3252A #9B5A45 #978F86 #978F86 #8E8E8D #8E8E8D
row 3: #8E8E8D #8E8E8D #8E8E8D #8E8E8D #8E8E8D #8E8E8D #8E8E8D #8E8E8D
```

### 5.2 Mage

```text
row 0: #F6C09C #1B191B #13191B #818C91 #818C91 #B45829 #9B5A45 #6B5C53
row 1: #484471 #484471 #A41A5A #818C91 #4A5155 #9B5A45 #6B5C53 #2B294B
row 2: #D4DBDE #A41A5A #2F6C73 #9B5A45 #978F86 #978F86 #55B66A #F6C09C
row 3: #F9AA4E #B45829 #D09745 #4D6C82 #ED1F24 #DAAE7D #F6C09C #F6C09C
```

### 5.3 Ranger

```text
row 0: #F6C09C #9B5A45 #13191B #818C91 #4A5155 #E7CDB4 #E7CDB4 #7E5A49
row 1: #257EBC #6B5C53 #D4DBDE #818C91 #978F86 #B17052 #9B5A45 #6B5C53
row 2: #D4DBDE #257EBC #E7CDB4 #7E5A49 #978F86 #978F86 #8E8E8D #7E5A49
row 3: #8E8E8D #8E8E8D #8E8E8D #8E8E8D #8E8E8D #8E8E8D #8E8E8D #8E8E8D
```

### 5.4 Barbarian

```text
row 0: #F6C09C #978F86 #13191B #818C91 #4A5155 #B17052 #9B5A45 #6B5C53
row 1: #AC665B #8E8E8D #D4DBDE #818C91 #978F86 #9B5A45 #7E5A49 #978F86
row 2: #D4DBDE #D09745 #4D6C82 #6B5C53 #978F86 #978F86 #8E8E8D #978F86
row 3: #8E8E8D #E7CDB4 #8E8E8D #8E8E8D #8E8E8D #8E8E8D #8E8E8D #8E8E8D
```

### 5.5 Rogue

```text
row 0: #F6C09C #9B5A45 #13191B #818C91 #4A5155 #B17052 #9B5A45 #6B5C53
row 1: #096253 #008353 #818C91 #818C91 #4A5155 #B17052 #9B5A45 #6B5C53
row 2: #E7CDB4 #008353 #55B66A #9B5A45 #B17052 #9B5A45 #6B5C53 #9B5A45
row 3: #55B66A #FF7C1B #BD0EF0 #08B9D1 #ED1F24 #DAAE7D #F6C09C #F6C09C
```

---

## 6. 当前不能直接确定的内容

当前检查不能直接得到：

```text
row 1 col 2 = 胸甲
row 2 col 0 = 袖子
row 0 col 5 = 头发
```

原因：

- 材质实例没有部位语义参数。
- SkeletalMesh 只有单材质槽。
- 部位到色块的关系由 UV 坐标决定。
- UE Python 暴露的当前 SkeletalMesh 查询能力不足以直接读出完整 LOD 顶点 UV。

要精确建立“色块 -> 部位”表，需要进一步解析或导出 Mesh UV。

---

## 7. 后续调查方向

### 7.1 解析 FBX / UV

推荐下一步从 raw FBX 入手：

```text
Content/Animations/Adventurers/Characters/raw/Knight.fbx
Content/Animations/Adventurers/Characters/raw/Mage.fbx
Content/Animations/Adventurers/Characters/raw/Ranger.fbx
```

处理流程：

1. 读取 mesh 顶点 / 三角形 / UV0。
2. 将 UV 映射到 8 x 4 色块：

```text
col = floor(U * 8)
row = floor((1 - V) * 4)
```

3. 按 mesh node 名、三角形、骨骼权重或空间位置聚类。
4. 推断每个色块主要覆盖的人物部位。
5. 生成项目内固定的“部位调色表”。

### 7.2 自定义材质换色

如果后续要做阵营颜色区分，推荐不要直接改原始贴图。

可选方向：

1. **Palette Replace 材质**
   - 采样原始 `DiffuseColorMap`。
   - 根据色块坐标或颜色索引替换局部颜色。
   - 暴露 `FactionPrimaryColor / FactionSecondaryColor` 等参数。

2. **运行时生成阵营贴图**
   - 以原贴图为模板。
   - 替换指定色块区域。
   - 每个阵营生成一张动态贴图或预烘焙贴图。

3. **离线复制贴图**
   - 为 12 个阵营提前生成不同版本贴图。
   - 实现简单，但资产数量较多，后续维护成本更高。

---

## 8. 初步结论

当前 Adventurers 人物资源的颜色机制更像：

```text
单材质槽 SkeletalMesh
    -> FBXLegacyPhongSurfaceMaterial 材质实例
    -> 一张 1024 x 1024 调色板式 DiffuseColorMap
    -> UV 岛落在 8 x 4 色块上决定部位颜色
```

因此：

- 不能直接通过现有材质实例编辑某个身体部位颜色。
- 可以通过替换贴图色块或自定义 Palette 材质实现阵营换色。
- 精确部位映射需要进一步解析 raw FBX 或导出 SkeletalMesh UV。
