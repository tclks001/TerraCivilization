# TerraCivilization SimpleGameplay P7 阵营调色板换色设计稿

> 本稿对应 [PieceAnimationPresentationDesign.md](PieceAnimationPresentationDesign.md) 的 **P7：终局演出与职业差异化细化** 中的阵营换色部分。
> 本稿基于 [PieceMaterialPaletteInvestigation.md](PieceMaterialPaletteInvestigation.md) 的资产线索：当前 Adventurers 人物是单材质槽 SkeletalMesh，材质实例只通过 `DiffuseColorMap` 指向一张 1024 x 1024、8 x 4 色块式贴图，部位语义由模型 UV 决定。

---

## 1. 目标

P7 阵营换色完成后应具备：

1. 12 个阵营的棋子一眼可分辨。
2. 同一兵种仍保留原本职业读感，例如 Knight 仍像近战、Ranger 仍像弓兵、Mage 仍像主将。
3. 不修改 Gameplay 规则，不让 Gameplay 关心材质、贴图或颜色。
4. 不直接覆盖原始 `knight_texture.png`、`mage_texture.png`、`ranger_texture.png` 等源贴图。
5. 不依赖“每个身体部位一个材质槽”，因为当前人物模型不是这种资产结构。
6. 首版优先对盔甲、披风、帽子、外衣等高可见区域换色，避免把皮肤、金属边缘、阴影块误换成阵营色。

本稿推荐首版使用：

```text
单个 P7 Palette Replace 母材质
    + 每个棋子一个 Dynamic Material Instance
    + 按 OwnerFactionId 写入 FactionPrimaryColor / FactionSecondaryColor
    + 按角色类型写入需要换色的 8 x 4 色块 Mask
```

你提出的“每个阵营一个材质，调整其中一个或几个纹理块的颜色”的思路是可行的；本稿把它工程化为动态材质参数版。这样视觉结果等价于“不同阵营使用不同材质颜色”，但不需要维护 `12 阵营 x 3 角色 x 多个职业` 的材质实例矩阵。

---

## 2. 现有资产模式结论

当前经文档和 UE 5.8 只读查询确认，主要人物模型使用如下模式：

| SkeletalMesh | 材质槽 | 材质实例 | DiffuseColorMap |
| --- | --- | --- | --- |
| `Knight1` | `knight` | `/Game/Animations/Adventurers/Characters/knight` | `/Game/Animations/Adventurers/Assets/knight_texture` |
| `Mage1` | `mage` | `/Game/Animations/Adventurers/Characters/mage` | `/Game/Animations/Adventurers/Assets/mage_texture` |
| `Ranger1` | `ranger` | `/Game/Animations/Adventurers/Characters/ranger` | `/Game/Animations/Adventurers/Assets/ranger_texture` |
| `Barbarian1` | `barbarian` | `/Game/Animations/Adventurers/Characters/barbarian` | `/Game/Animations/Adventurers/Assets/barbarian_texture` |

这些材质实例继承自：

```text
/InterchangeAssets/Materials/FBXLegacyPhongSurfaceMaterial
```

当前可用参数主要是：

```text
DiffuseColorMap
DiffuseColorMapWeight
Shininess
RefractionDepthBias
AmbientColor
SpecularColor
```

即使查询 `FactionColor`、`SkinColor`、`ArmorColor`、`ClothColor` 这类名字，当前 legacy 材质也没有真正可用的部位换色机制。它们不能作为 P7 的实现基础。

贴图模式：

```text
1024 x 1024
8 列 x 4 行
每块约 128 x 256
共 32 个色块
```

关键结论：

```text
材质实例不知道“胸甲 / 披风 / 帽子 / 发色”。
SkeletalMesh 只有一个材质槽。
某个身体部位用哪个色块，由该部位三角形的 UV0 落在哪个 128 x 256 色块决定。
```

---

## 3. 非目标

P7 首版不做：

- 重新拆分 SkeletalMesh 材质槽。
- 修改或重导所有人物模型。
- 直接烘焙 12 套永久源贴图并替换原始贴图。
- 运行时改变 Gameplay 兵种判定。
- 复杂队伍旗帜、布料模拟或角色徽章 Mesh 制作。
- 自动识别“胸甲 / 披风 / 头发”的语义到 100% 正确。

部位识别首版接受“工具辅助 + 人工确认”的流程。原因是当前资源没有语义标注，纯算法只能推断色块和 UV 岛，不可能可靠知道某块一定是“帽子”还是“护腕”。

---

## 4. 总体方案

### 4.1 为什么不按身体部位换材质

不可直接做：

```text
胸甲材质槽 -> 换阵营色
披风材质槽 -> 换阵营色
帽子材质槽 -> 换阵营色
```

因为当前 `Knight1`、`Mage1`、`Ranger1` 等模型都是单材质槽。UE 只知道这整个角色用一个材质实例，不知道哪些三角形属于胸甲、袖子、帽子。

正确抓手是：

```text
UV0 所在色块
```

也就是把角色贴图当作 32 个可寻址的 palette tile：

```text
TileIndex = Row * 8 + Col
Col = floor(U * 8)
Row = floor((1 - V) * 4)
```

### 4.2 推荐实现

运行时结构：

```text
ATerraPieceActor
  HumanMesh / RiderMesh
    Material Slot 0
      Dynamic Material Instance: MI_PiecePaletteReplace_Runtime
        BaseColorTexture = 原角色 DiffuseColorMap
        FactionPrimaryColor
        FactionSecondaryColor
        PaletteMaskRows / PaletteMaskBits
```

材质执行：

1. 采样原始 `BaseColorTexture`。
2. 根据当前像素 UV 判断它落在哪个 8 x 4 色块。
3. 判断这个色块是否在当前角色的“可阵营换色色块列表”里。
4. 如果不在列表里，输出原始颜色。
5. 如果在列表里，保留原贴图明暗关系，将色相替换为阵营色。

这样可以做到：

- Knight 的胸甲 / 披风色块跟随阵营变色。
- Ranger 的外衣 / 兜帽色块跟随阵营变色。
- Mage 的袍子 / 帽子色块跟随阵营变色。
- 皮肤、黑色眼睛、金属灰、阴影和武器贴图不被误改。

---

## 5. 如何判断色块映射到哪个部位

### 5.1 必须接受的事实

当前资产里没有这样的表：

```text
row 1 col 0 = Knight 披风
row 2 col 2 = Knight 盾牌标志
row 1 col 1 = Ranger 兜帽
```

所以 P7 要先建立一份项目内的“角色 palette tile 标注表”。

推荐产物：

```text
Content/PiecePresentation/Data/DA_P7PiecePaletteMap_Knight
Content/PiecePresentation/Data/DA_P7PiecePaletteMap_Mage
Content/PiecePresentation/Data/DA_P7PiecePaletteMap_Ranger
```

或者首版先用 C++/DataTable 固化：

```text
Mage:    PrimaryTiles = [(1,2)], SecondaryTiles = [(1,1)]
Knight:  PrimaryTiles = [(1,0)], SecondaryTiles = [(1,1)]
Ranger:  PrimaryTiles = [(1,0)], SecondaryTiles = [(1,6)]
```

以上为当前人工色块预览验证后的首版配置。暂时每个角色只启用一个 Primary 色块和一个 Secondary 色块。

### 5.2 自动导出 UV 色块覆盖

首选流程是在 UE 编辑器里导出 SkeletalMesh LOD0 的 UV 覆盖统计。

建议新增 Editor-only Python 或 C++ 工具：

```text
Scripts/P7/export_piece_palette_uv_coverage.py
```

输入：

```text
/Game/Animations/Adventurers/Characters/Knight1
/Game/Animations/Adventurers/Characters/Mage1
/Game/Animations/Adventurers/Characters/Ranger1
```

输出：

```text
Saved/P7Palette/Knight1_uv_tiles.json
Saved/P7Palette/Mage1_uv_tiles.json
Saved/P7Palette/Ranger1_uv_tiles.json
Saved/P7Palette/Knight1_uv_tiles_overlay.png
```

JSON 建议格式：

```json
{
  "mesh": "/Game/Animations/Adventurers/Characters/Knight1",
  "texture_grid": { "columns": 8, "rows": 4 },
  "tiles": [
    {
      "row": 1,
      "col": 0,
      "triangle_count": 128,
      "area_ratio": 0.18,
      "dominant_bones": ["spine_03", "upperarm_l", "upperarm_r"],
      "notes": ""
    }
  ]
}
```

核心计算：

```text
for each triangle in LOD0:
    uv_center = average(triangle.uv0)
    col = clamp(floor(uv_center.u * 8), 0, 7)
    row = clamp(floor((1 - uv_center.v) * 4), 0, 3)
    accumulate triangle area / triangle count / bone weights into tile[row][col]
```

如果 UE Python 当前拿不到完整 LOD 顶点 UV，则走 raw FBX：

```text
Content/Animations/Adventurers/Characters/raw/Knight.fbx
Content/Animations/Adventurers/Characters/raw/Mage.fbx
Content/Animations/Adventurers/Characters/raw/Ranger.fbx
```

用 Blender Python 导入 FBX 后读取 mesh loop UV：

```text
blender --background --python Scripts/P7/export_piece_palette_uv_coverage_blender.py
```

Blender 路线的优点是对 FBX UV 读取更稳定，且可以直接按 mesh object 名、材质名、顶点组推断大致部位。

### 5.3 生成 32 色块预览图

仅有 JSON 不够直观。必须生成每个角色一张“色块预览图”：

```text
Saved/P7Palette/Knight1_palette_contact_sheet.png
```

推荐样式：

```text
8 x 4 格
每格显示：
    Row/Col/Index
    原贴图中心色
    UV 覆盖面积
    Dominant Bones
    是否候选阵营色块
```

人工确认时，打开 UE 或 Blender，逐个测试候选色块：

1. 在 overlay 图上选择一个高覆盖、颜色鲜明的色块。
2. 在材质实例里临时把该色块替换成纯亮红或亮青。
3. 在 SkeletalMesh 预览窗口旋转模型。
4. 记录它对应的身体部位。

### 5.4 快速人工确认法

不写工具也可以手动确认，但效率较低：

1. 复制一张 raw PNG，例如 `knight_texture.png`。
2. 把某个 128 x 256 色块整块涂成高饱和测试色，例如纯品红。
3. 重新导入为临时贴图 `T_Debug_Knight_Tile_R1_C0`。
4. 临时赋给 Knight 材质实例的 `DiffuseColorMap`。
5. 打开 `Knight1` 预览，观察哪些部位变成品红。
6. 记录：
   ```text
   Knight row 1 col 0 -> 披风 / 胸前布料，适合作为 FactionPrimary
   ```
7. 换下一个色块继续。

注意：这个方法只用于调研，不能把测试贴图提交为正式资产。

### 5.5 色块命名规范

建议所有记录都用同一坐标语义：

```text
Row = 0..3，从贴图顶部到下方
Col = 0..7，从贴图左侧到右侧
Index = Row * 8 + Col
```

示例：

```text
R1C0 = Row 1, Col 0, Index 8
```

不要同时混用“图片坐标 row”和“UV v 从下到上 row”，否则材质和工具会对不上。

---

## 6. 色块选择原则

优先选择：

- 面积大、玩家远景可见的色块。
- 当前已经是职业主色的色块，例如 Knight 红、Ranger 蓝/绿、Mage 紫。
- 分布在胸甲、披风、兜帽、袍子、帽檐、肩甲等位置的色块。
- 同一角色最多 1 到 3 个主色块，避免全身都被阵营色吞掉。

避免选择：

- 皮肤色块。
- 眼睛、嘴、阴影、黑色描边。
- 金属高光灰色块，除非明确想把盔甲整体染色。
- 木柄、皮革、弓弦等职业道具色块。
- 多个小零件共享且语义混乱的色块。

推荐首版策略：

| 角色 | 用途 | 候选优先级 |
| --- | --- | --- |
| Knight | 步兵 / 骑兵 Rider | 盔甲布面、披风、盾面标志优先；保留金属灰和皮肤 |
| Ranger | 弓兵 | 兜帽、外衣、披肩优先；保留弓、皮革、皮肤 |
| Mage | 主将 | 袍子、帽子、肩披优先；主将可额外使用更亮的 SecondaryColor |

### 6.1 已验证首版色块配置

以下结果来自色块预览图人工验证。P7 首版先按每个角色一个 Primary、一个 Secondary 落地。

| Piece | Primary 色块 | Primary 参考部位 | Secondary 色块 | Secondary 参考部位 |
| --- | --- | --- | --- | --- |
| `Mage` | `R1C2` | 披风颜色 | `R1C1` | 帽子 |
| `Knight` | `R1C0` | 披风颜色 | `R1C1` | 奖章绶带 |
| `Ranger` | `R1C0` | 披风颜色 | `R1C6` | 箭袋 |

实现时对应 mask：

```text
Mage:
  PrimaryTiles   = [(Row=1, Col=2)]
  SecondaryTiles = [(Row=1, Col=1)]

Knight:
  PrimaryTiles   = [(Row=1, Col=0)]
  SecondaryTiles = [(Row=1, Col=1)]

Ranger:
  PrimaryTiles   = [(Row=1, Col=0)]
  SecondaryTiles = [(Row=1, Col=6)]
```

骑兵 Rider 当前使用 Knight 模型，因此骑兵阵营换色复用 `Knight` 的色块配置。

---

## 7. 材质实现方案

### 7.1 新增母材质

建议新增：

```text
Content/PiecePresentation/Materials/M_TerraPiece_PaletteReplace
```

材质参数：

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `BaseColorTexture` | Texture2D | 原角色 DiffuseColorMap |
| `PaletteGridColumns` | Scalar | 默认 `8` |
| `PaletteGridRows` | Scalar | 默认 `4` |
| `FactionPrimaryColor` | Vector | 阵营主色 |
| `FactionSecondaryColor` | Vector | 阵营辅色 |
| `PrimaryTileMaskRGBA8` | FColor / RGBA8 | 32 bit mask，标记哪些 tile 用主色 |
| `SecondaryTileMaskRGBA8` | FColor / RGBA8 | 32 bit mask，标记哪些 tile 用辅色 |
| `FactionColorStrength` | Scalar | 替换强度，默认 `1.0` |
| `PreserveValueStrength` | Scalar | 保留原贴图明暗强度，默认 `1.0` |
| `MinSaturationToReplace` | Scalar | 低饱和灰色保护阈值，默认 `0.05` |

`PrimaryTileMaskRGBA8` 和 `SecondaryTileMaskRGBA8` 分别用一个 `FColor` 承载 32 个 tile bit：

```text
bit index = Row * 8 + Col
R channel = bits 0..7   = row 0, col 0..7
G channel = bits 8..15  = row 1, col 0..7
B channel = bits 16..23 = row 2, col 0..7
A channel = bits 24..31 = row 3, col 0..7
```

当前文档和配置里的 Row 坐标使用“预览图 row”：`R0` 是预览图顶部，`R3` 是预览图底部。UE 材质里的 `TexCoord.y` 方向与预览图 row 相反，因此 C++ 写入材质 mask 前必须做一次 row 翻转：

```text
MaterialRow = 3 - PreviewRow
```

实际验证结果：只写垂直镜像 row 才能命中模型上的正确部位；同时写原 row 会扩大换色范围。因此 `FTerraPiecePaletteTile.Row` 仍填写人工验证得到的预览图 row，`PackP7TileMaskBits` 内部负责转换到材质 row。

示例：

```text
R0C0 -> bit 0
R0C7 -> bit 7
R1C0 -> bit 8
R1C6 -> bit 14
R3C7 -> bit 31
```

C++ 端 pack 成 `FColor`：

```cpp
const FColor PackedMask(
    Mask & 255,
    (Mask >> 8) & 255,
    (Mask >> 16) & 255,
    (Mask >> 24) & 255);
```

材质 / Custom HLSL 端还原成 `uint` mask：

```hlsl
uint4 v = (uint4)round(tex * 255.0);
uint Mask =
      v.r
    | (v.g << 8)
    | (v.b << 16)
    | (v.a << 24);
```

注意：UE 材质参数系统不直接暴露 `FColor` 作为 Vector Parameter。如果直接走 Dynamic Material Instance，C++ 端仍以 `FColor` 作为打包数据源，再显式按 `R/255.0f, G/255.0f, B/255.0f, A/255.0f` 写入材质 Vector 参数；不要依赖可能带颜色空间语义的隐式转换。材质侧语义仍按 `FColor` / RGBA8 处理，不按浮点 mask 处理。若后续做 Material Parameter Collection 或小尺寸 mask texture，也必须保持 sRGB 关闭、Nearest 采样和 0..255 精确还原。

### 7.2 材质节点逻辑

概念伪代码：

```hlsl
float2 uv = TexCoord0;
float3 baseColor = Texture2DSample(BaseColorTexture, BaseColorTextureSampler, uv).rgb;

int col = clamp((int)floor(uv.x * 8.0), 0, 7);
int row = clamp((int)floor((1.0 - uv.y) * 4.0), 0, 3);
int tileIndex = row * 8 + col;

uint primaryBits = UnpackRGBA8Mask(PrimaryTileMaskRGBA8);
uint secondaryBits = UnpackRGBA8Mask(SecondaryTileMaskRGBA8);

uint tileBit = 1u << tileIndex;
float primaryMask = (primaryBits & tileBit) != 0u ? 1.0 : 0.0;
float secondaryMask = (secondaryBits & tileBit) != 0u ? 1.0 : 0.0;
float mask = saturate(primaryMask + secondaryMask);

float3 targetHueColor = primaryMask > 0.5 ? FactionPrimaryColor.rgb : FactionSecondaryColor.rgb;
float value = max(max(baseColor.r, baseColor.g), baseColor.b);
float3 recolored = targetHueColor * lerp(1.0, value, PreserveValueStrength);

float saturation = max(baseColor.r, max(baseColor.g, baseColor.b)) - min(baseColor.r, min(baseColor.g, baseColor.b));
float protectedMask = step(MinSaturationToReplace, saturation);

float finalMask = mask * protectedMask * FactionColorStrength;
return lerp(baseColor, recolored, finalMask);
```

说明：

- `PreserveValueStrength` 用来保留原贴图的深浅变化，否则整块会像纯色贴片。
- `MinSaturationToReplace` 用于保护灰色金属、黑色阴影和白色高光。
- 如果某个候选 tile 本身是灰色盔甲但确实要染色，可以对该角色降低阈值，或为这个 tile 单独设置 `bIgnoreSaturationGuard`。首版可以先不做单 tile 例外。

### 7.3 纹理覆盖这一块材质的方法

这里的“覆盖”不是给模型局部换一个材质槽，而是在同一个材质里按 UV 局部覆盖颜色：

```text
原贴图采样结果
    -> 判断 UV 是否在目标色块
    -> 目标色块内输出阵营色
    -> 非目标色块输出原贴图
```

因此不需要修改 SkeletalMesh 材质槽，也不需要创建新的 mesh section。

如果后续确实要用“贴图覆盖”而不是“材质内换色”，也可以走离线生成贴图：

```text
T_Knight_Faction00
T_Knight_Faction01
...
T_Ranger_Faction11
```

生成逻辑同样是对指定 128 x 256 色块做色相替换，然后把生成贴图设为材质实例的 `BaseColorTexture`。但这会增加资源数量，推荐只作为移动端优化或最终烘焙路线。

---

## 8. 阵营颜色配置

建议在 `PiecePresentation` 中新增阵营颜色配置，而不是散落在 `APlanetTessellatedMesh` 字段里。

新增类型：

```cpp
USTRUCT(BlueprintType)
struct FTerraPieceFactionPalette
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor PrimaryColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor SecondaryColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ColorStrength = 1.0f;
};
```

在 `FTerraPieceVisualConfig` 中新增：

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
TObjectPtr<UMaterialInterface> P7PaletteReplaceMaterial;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
TArray<FTerraPieceFactionPalette> P7FactionPalettes;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
TObjectPtr<UTexture2D> P7CommanderBaseTexture;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
TObjectPtr<UTexture2D> P7InfantryBaseTexture;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
TObjectPtr<UTexture2D> P7CavalryRiderBaseTexture;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
TObjectPtr<UTexture2D> P7ArcherBaseTexture;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Piece Presentation|P7")
TArray<FTerraPiecePaletteMask> P7PaletteMasksByPieceType;
```

首版默认 12 阵营颜色建议使用明确区分的 hue，不用全靠红蓝：

| FactionId | Primary | Secondary |
| --- | --- | --- |
| 0 | Red | Gold |
| 1 | Blue | White |
| 2 | Green | Tan |
| 3 | Purple | Silver |
| 4 | Orange | Black |
| 5 | Cyan | Navy |
| 6 | Yellow | Brown |
| 7 | Magenta | Dark Gray |
| 8 | Teal | Copper |
| 9 | White | Red |
| 10 | Black | Emerald |
| 11 | Lime | Violet |

具体色值应在场景光照下调整，避免黄色和白色过亮、黑色阵营丢失细节。

---

## 9. 绑定到模型上的方法

### 9.1 接入点

当前真实棋子表现链路是：

```text
APlanetTessellatedMesh::SyncP1PiecePresentation_
    -> BuildP1PieceVisualConfig_
    -> UTerraPiecePresentationManager::SyncPieces
    -> ATerraPieceActor::ApplyPresentationSnapshot
    -> ATerraPieceActor::ApplyVisualConfig_
        -> HumanMesh / RiderMesh SetSkeletalMesh
        -> ApplyWeaponAttachments_
```

P7 应接在 `ATerraPieceActor::ApplyVisualConfig_` 里，放在 `SetSkeletalMesh` 之后、`ApplyWeaponAttachments_` 之前或之后都可以。推荐在 mesh 可见性和相对 transform 设置完成后调用：

```cpp
ApplyP7FactionMaterials_(VisualConfig);
ApplyWeaponAttachments_();
```

### 9.2 Actor 内新增运行时材质缓存

在 `ATerraPieceActor` 中新增：

```cpp
UPROPERTY(Transient)
TObjectPtr<UMaterialInstanceDynamic> P7HumanMID;

UPROPERTY(Transient)
TObjectPtr<UMaterialInstanceDynamic> P7RiderMID;
```

并新增私有函数：

```cpp
void ApplyP7FactionMaterials_(const FTerraPieceVisualConfig& VisualConfig);
void ApplyP7MaterialToMesh_(USkeletalMeshComponent* Mesh, UTexture2D* BaseTexture, const FTerraPiecePaletteMask& Mask, const FTerraPieceFactionPalette& Palette, UMaterialInterface* ParentMaterial, TObjectPtr<UMaterialInstanceDynamic>& InOutMID);
```

辅助函数建议：

```cpp
static uint32 PackP7TileMaskBits(const TArray<FTerraPiecePaletteTile>& Tiles)
{
    uint32 Bits = 0;
    for (const FTerraPiecePaletteTile& Tile : Tiles)
    {
        if (Tile.Row >= 0 && Tile.Row < 4 && Tile.Col >= 0 && Tile.Col < 8)
        {
            const int32 MaterialRow = 3 - Tile.Row;
            const uint32 BitIndex = static_cast<uint32>(MaterialRow * 8 + Tile.Col);
            Bits |= (1u << BitIndex);
        }
    }
    return Bits;
}

static FColor PackP7TileMaskRGBA8(const TArray<FTerraPiecePaletteTile>& Tiles)
{
    const uint32 Mask = PackP7TileMaskBits(Tiles);
    return FColor(
        Mask & 255,
        (Mask >> 8) & 255,
        (Mask >> 16) & 255,
        (Mask >> 24) & 255);
}

static FLinearColor NormalizeP7TileMaskColor(const FColor& Color)
{
    return FLinearColor(
        Color.R / 255.0f,
        Color.G / 255.0f,
        Color.B / 255.0f,
        Color.A / 255.0f);
}
```

### 9.3 普通人形绑定

普通步兵、弓兵、主将：

```cpp
USkeletalMeshComponent* TargetMesh = HumanMesh;
UTexture2D* BaseTexture = VisualConfig.ResolveP7BaseTexture(PieceType);
const FTerraPiecePaletteMask* Mask = VisualConfig.ResolveP7PaletteMask(PieceType);
const FTerraPieceFactionPalette* Palette = VisualConfig.ResolveP7FactionPalette(OwnerFactionId);

P7HumanMID = UMaterialInstanceDynamic::Create(VisualConfig.P7PaletteReplaceMaterial, this);
P7HumanMID->SetTextureParameterValue(TEXT("BaseColorTexture"), BaseTexture);
P7HumanMID->SetVectorParameterValue(TEXT("FactionPrimaryColor"), Palette->PrimaryColor);
P7HumanMID->SetVectorParameterValue(TEXT("FactionSecondaryColor"), Palette->SecondaryColor);
P7HumanMID->SetScalarParameterValue(TEXT("FactionColorStrength"), Palette->ColorStrength);

const FColor PrimaryMaskColor = PackP7TileMaskRGBA8(Mask->PrimaryTiles);
const FColor SecondaryMaskColor = PackP7TileMaskRGBA8(Mask->SecondaryTiles);
P7HumanMID->SetVectorParameterValue(TEXT("PrimaryTileMaskRGBA8"), NormalizeP7TileMaskColor(PrimaryMaskColor));
P7HumanMID->SetVectorParameterValue(TEXT("SecondaryTileMaskRGBA8"), NormalizeP7TileMaskColor(SecondaryMaskColor));
TargetMesh->SetMaterial(0, P7HumanMID);
```

### 9.4 骑兵绑定

骑兵现在是：

```text
HorseMesh
RiderMesh
```

P7 首版只给 `RiderMesh` 做阵营换色，不给马换色：

```cpp
if (IsMountedCavalry_())
{
    ApplyP7MaterialToMesh_(RiderMesh, VisualConfig.P7CavalryRiderBaseTexture, CavalryMask, Palette, ParentMaterial, P7RiderMID);
}
```

原因：

- 阵营归属应该看骑手盔甲 / 披风 / 上身，而不是马的毛色。
- 当前 Horse 资产不是 Adventurers 这套 8 x 4 人物 palette 贴图结构，不能直接套同一套 tile mask。
- 后续若要马具阵营色，应单独给 Horse 做材质参数或马鞍/旗帜配件。

骑兵 `HumanMesh` 当前会隐藏，`RiderMesh` 才是显示的人物模型。因此不能只给 `HumanMesh` 设置材质，否则骑兵看不到阵营色。

### 9.5 武器和盾牌

P7 首版武器不强制换色，避免过度复杂。

但步兵盾牌很适合做阵营识别。推荐 P7.1 再做：

```text
Shield StaticMesh -> Dynamic Material Instance -> FactionPrimaryColor
```

首版优先完成角色本体换色；盾牌、箭袋、披风 Mesh、旗帜属于后续增强。

---

## 10. C++ 字段和函数建议

### 10.1 新增数据结构

建议放在：

```text
Source/PiecePresentation/Public/TerraPiecePresentationTypes.h
```

```cpp
USTRUCT(BlueprintType)
struct FTerraPiecePaletteTile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Row = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Col = 0;
};

USTRUCT(BlueprintType)
struct FTerraPiecePaletteMask
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETerraGameplayPieceType PieceType = ETerraGameplayPieceType::Infantry;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FTerraPiecePaletteTile> PrimaryTiles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FTerraPiecePaletteTile> SecondaryTiles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinSaturationToReplace = 0.05f;
};
```

### 10.2 配置生成

`APlanetTessellatedMesh::BuildP1PieceVisualConfig_()` 中补：

```cpp
VisualConfig.P7PaletteReplaceMaterial =
    P7PaletteReplaceMaterial.Get()
        ? P7PaletteReplaceMaterial.Get()
        : LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/PiecePresentation/Materials/M_TerraPiece_PaletteReplace.M_TerraPiece_PaletteReplace"));

VisualConfig.P7CommanderBaseTexture =
    LoadObject<UTexture2D>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/mage_texture.mage_texture"));

VisualConfig.P7InfantryBaseTexture =
    LoadObject<UTexture2D>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/knight_texture.knight_texture"));

VisualConfig.P7CavalryRiderBaseTexture =
    LoadObject<UTexture2D>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/knight_texture.knight_texture"));

VisualConfig.P7ArcherBaseTexture =
    LoadObject<UTexture2D>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/ranger_texture.ranger_texture"));
```

其中 `P7FactionPalettes` 和 tile mask 可以先作为 `UPROPERTY` 暴露在 `APlanetTessellatedMesh`，等稳定后迁移到 DataAsset。

### 10.3 参数写入

把 `PrimaryTiles` / `SecondaryTiles` 转成 `FColor` RGBA8 mask：

```cpp
static uint32 PackP7TileMaskBits(const TArray<FTerraPiecePaletteTile>& Tiles)
{
    uint32 Bits = 0;
    for (const FTerraPiecePaletteTile& Tile : Tiles)
    {
        if (Tile.Row >= 0 && Tile.Row < 4 && Tile.Col >= 0 && Tile.Col < 8)
        {
            const int32 MaterialRow = 3 - Tile.Row;
            Bits |= 1u << static_cast<uint32>(MaterialRow * 8 + Tile.Col);
        }
    }
    return Bits;
}

static FColor PackP7TileMaskRGBA8(const TArray<FTerraPiecePaletteTile>& Tiles)
{
    const uint32 Mask = PackP7TileMaskBits(Tiles);
    return FColor(
        Mask & 255,
        (Mask >> 8) & 255,
        (Mask >> 16) & 255,
        (Mask >> 24) & 255);
}

static FLinearColor NormalizeP7TileMaskColor(const FColor& Color)
{
    return FLinearColor(
        Color.R / 255.0f,
        Color.G / 255.0f,
        Color.B / 255.0f,
        Color.A / 255.0f);
}
```

示例结果：

| 配置 | 32 bit Mask | RGBA8 |
| --- | --- | --- |
| `Mage` Primary `R1C2` | `1 << 18` | `(0, 0, 4, 0)` |
| `Mage` Secondary `R1C1` | `1 << 17` | `(0, 0, 2, 0)` |
| `Knight` Primary `R1C0` | `1 << 16` | `(0, 0, 1, 0)` |
| `Knight` Secondary `R1C1` | `1 << 17` | `(0, 0, 2, 0)` |
| `Ranger` Primary `R1C0` | `1 << 16` | `(0, 0, 1, 0)` |
| `Ranger` Secondary `R1C6` | `1 << 22` | `(0, 0, 64, 0)` |

写入材质时：

```cpp
const FColor PrimaryMaskColor = PackP7TileMaskRGBA8(Mask.PrimaryTiles);
MID->SetVectorParameterValue(TEXT("PrimaryTileMaskRGBA8"), NormalizeP7TileMaskColor(PrimaryMaskColor));

const FColor SecondaryMaskColor = PackP7TileMaskRGBA8(Mask.SecondaryTiles);
MID->SetVectorParameterValue(TEXT("SecondaryTileMaskRGBA8"), NormalizeP7TileMaskColor(SecondaryMaskColor));
```

材质侧按 `uint4 v = tex * 255` 还原，并用 `Row * 8 + Col` 判断目标 bit。

### 10.4 运行时刷新时机

每次 `ApplyPresentationSnapshot` 都会有 `OwnerFactionId` 和 `PieceType`。P7 可以每次 snapshot 都调用材质刷新，初版足够简单。

后续优化：

- 记录 `LastAppliedFactionId`
- 记录 `LastAppliedPieceType`
- 记录 `LastAppliedBaseTexture`
- 只有变化时重建 / 重写 MID

---

## 11. 离线贴图方案备选

如果后续想严格实现“每个阵营一张材质 / 贴图”，可以做预烘焙路线：

```text
Scripts/P7/bake_faction_piece_textures.py
```

输入：

```text
raw knight_texture.png / mage_texture.png / ranger_texture.png
角色 tile mask
12 阵营颜色表
```

输出：

```text
Content/PiecePresentation/Textures/P7/T_Knight_Faction00.png
Content/PiecePresentation/Textures/P7/T_Knight_Faction01.png
...
Content/PiecePresentation/Materials/P7/MI_Knight_Faction00
```

优点：

- 运行时材质更简单。
- 可在烘焙阶段精细处理边缘、渐变和手绘修正。

缺点：

- 资源数量快速膨胀。
- 调色改一次要重新生成和导入。
- 以后新增角色也要生成 12 套。

建议阶段：

```text
P7 首版：动态材质
P7.2 优化：必要时烘焙贴图
```

---

## 12. 编辑器操作流程

### 12.1 建立色块映射

1. 打开 `PieceMaterialPaletteInvestigation.md`，确认当前贴图 grid 为 `8 x 4`。
2. 对 `Knight1`、`Mage1`、`Ranger1` 跑 UV 色块覆盖导出工具。
3. 打开每个角色的 overlay/contact sheet。
4. 在 UE 或 Blender 里逐个把候选 tile 临时染成高亮测试色。
5. 记录映射：
   ```text
   Mage   Primary R1C2 -> 披风颜色；Secondary R1C1 -> 帽子
   Knight Primary R1C0 -> 披风颜色；Secondary R1C1 -> 奖章绶带
   Ranger Primary R1C0 -> 披风颜色；Secondary R1C6 -> 箭袋
   ```
6. 把最终确认结果填到 P7 mask DataAsset 或 `APlanetTessellatedMesh` 暴露字段。

### 12.2 创建材质

1. 在 Content Browser 创建文件夹和材质：
   ```text
   Content/PiecePresentation/Materials
   Content/PiecePresentation/Materials/M_TerraPiece_PaletteReplace
   ```
2. 打开 `M_TerraPiece_PaletteReplace`，在 Details 中设置：
   ```text
   Material Domain = Surface
   Blend Mode = Opaque
   Shading Model = Default Lit
   Two Sided = false
   ```
3. 创建 `Texture Sample Parameter2D` 节点：
   ```text
   Parameter Name = BaseColorTexture
   默认纹理 = /Game/Animations/Adventurers/Assets/knight_texture
   Sampler Type = Color
   ```
   说明：默认纹理只作为编译占位；运行时 C++ 会按兵种写入 `mage_texture` / `knight_texture` / `ranger_texture`。
4. 创建 `TextureCoordinate` 节点：
   ```text
   Coordinate Index = 0
   ```
5. 创建 2 个 `Vector Parameter` 节点：
   ```text
   FactionPrimaryColor    默认 (0.85, 0.08, 0.06, 1.0)
   FactionSecondaryColor  默认 (1.00, 0.72, 0.18, 1.0)
   ```
6. 创建 2 个 `Vector Parameter` 节点作为 RGBA8 mask：
   ```text
   PrimaryTileMaskRGBA8
   SecondaryTileMaskRGBA8
   ```
   默认值建议先填 0。C++ 会把 `FColor` 的 `R/G/B/A uint8` 显式除以 255 后写入这里。不要在材质里把它当普通颜色理解。
7. 创建 5 个 `Scalar Parameter` 节点：
   ```text
   PaletteGridColumns       默认 8
   PaletteGridRows          默认 4
   FactionColorStrength     默认 1
   PreserveValueStrength    默认 1
   MinSaturationToReplace   默认 0.05
   ```
8. 创建一个 `Custom` 节点，Details 设置：
   ```text
   Description = P7PaletteReplace_RGBA8Mask
   Output Type = CMOT Float3
   ```
   然后在 `Additional Outputs` 增加 1 个输出：
   ```text
   Output Name = PrimaryEmissiveOut
   Output Type = CMOT Float3
   ```
   说明：`Custom` 节点主输出仍然作为换色后的 Base Color；`PrimaryEmissiveOut` 只在 Primary tile 命中且通过饱和度保护时输出 Primary 自发光颜色，其余区域输出黑色。
9. 给 `Custom` 节点添加以下 Inputs，名字必须完全一致：
   ```text
   UV                       Float2
   BaseColor                Float3
   FactionPrimaryColor      Float4
   FactionSecondaryColor    Float4
   PrimaryTileMaskRGBA8     Float4
   SecondaryTileMaskRGBA8   Float4
   PaletteGridColumns       Float1
   PaletteGridRows          Float1
   FactionColorStrength     Float1
   PreserveValueStrength    Float1
   MinSaturationToReplace   Float1
   ```
10. 按以下方式连线：
    ```text
    TextureCoordinate.RG -> Custom.UV
    BaseColorTexture.RGB -> Custom.BaseColor
    FactionPrimaryColor -> Custom.FactionPrimaryColor
    FactionSecondaryColor -> Custom.FactionSecondaryColor
    PrimaryTileMaskRGBA8 -> Custom.PrimaryTileMaskRGBA8
    SecondaryTileMaskRGBA8 -> Custom.SecondaryTileMaskRGBA8
    PaletteGridColumns -> Custom.PaletteGridColumns
    PaletteGridRows -> Custom.PaletteGridRows
    FactionColorStrength -> Custom.FactionColorStrength
    PreserveValueStrength -> Custom.PreserveValueStrength
    MinSaturationToReplace -> Custom.MinSaturationToReplace
    Custom 主输出 -> Material Base Color
    Custom.PrimaryEmissiveOut -> Material Emissive Color
    ```
11. 其余材质输出建议首版先固定：
    ```text
    Roughness = Constant 0.65
    Specular = Constant 0.35
    Metallic = Constant 0
    ```
    `Emissive Color` 不再固定为 0，而是使用 `Custom.PrimaryEmissiveOut`。当前不新增 C++ 参数；自发光强度直接由 `FactionPrimaryColor.rgb` 决定，后续如果需要更强或更弱，再加 `PrimaryEmissiveStrength` 标量参数。
12. `Custom` 节点 Code 粘贴以下 HLSL：
    ```hlsl
    float GridColumns = max(PaletteGridColumns, 1.0);
    float GridRows = max(PaletteGridRows, 1.0);

    uint Col = (uint)clamp(floor(saturate(UV.x) * GridColumns), 0.0, GridColumns - 1.0);
    uint Row = (uint)clamp(floor((1.0 - saturate(UV.y)) * GridRows), 0.0, GridRows - 1.0);
    uint TileIndex = Row * (uint)GridColumns + Col;
    uint TileBit = 1u << TileIndex;

    uint4 PrimaryBytes = (uint4)round(saturate(PrimaryTileMaskRGBA8) * 255.0);
    uint PrimaryMask =
          PrimaryBytes.r
        | (PrimaryBytes.g << 8)
        | (PrimaryBytes.b << 16)
        | (PrimaryBytes.a << 24);

    uint4 SecondaryBytes = (uint4)round(saturate(SecondaryTileMaskRGBA8) * 255.0);
    uint SecondaryMask =
          SecondaryBytes.r
        | (SecondaryBytes.g << 8)
        | (SecondaryBytes.b << 16)
        | (SecondaryBytes.a << 24);

    float PrimaryHit = ((PrimaryMask & TileBit) != 0u) ? 1.0 : 0.0;
    float SecondaryHit = ((SecondaryMask & TileBit) != 0u) ? 1.0 : 0.0;
    SecondaryHit *= (1.0 - PrimaryHit);

    float TileHit = saturate(PrimaryHit + SecondaryHit);
    float3 TargetColor = lerp(FactionSecondaryColor.rgb, FactionPrimaryColor.rgb, PrimaryHit);

    float MaxChannel = max(BaseColor.r, max(BaseColor.g, BaseColor.b));
    float MinChannel = min(BaseColor.r, min(BaseColor.g, BaseColor.b));
    float Saturation = MaxChannel - MinChannel;
    float SaturationGuard = step(MinSaturationToReplace, Saturation);

    float ValueScale = lerp(1.0, MaxChannel, saturate(PreserveValueStrength));
    float3 Recolored = saturate(TargetColor * ValueScale);
    float FinalMask = TileHit * SaturationGuard * saturate(FactionColorStrength);

    float3 BaseColorOut = lerp(BaseColor, Recolored, FinalMask);
    PrimaryEmissiveOut = FactionPrimaryColor.rgb * PrimaryHit * SaturationGuard * saturate(FactionColorStrength);

    return BaseColorOut;
    ```
13. Apply / Save。保存后 PIE 中若 `APlanetTessellatedMesh.P7PaletteReplaceMaterial` 为空，C++ 会自动尝试加载这个路径；如果路径或资产名不同，必须手动把材质拖到该字段。

### 12.3 绑定到棋子

1. 在 `FTerraPieceVisualConfig` 和 `APlanetTessellatedMesh` 暴露 P7 参数。
2. 在 `BuildP1PieceVisualConfig_()` 填默认材质、贴图、阵营色和 tile mask。
3. 在 `ATerraPieceActor::ApplyVisualConfig_()` 调 `ApplyP7FactionMaterials_()`。
4. PIE 运行，观察 12 阵营初始棋子。
5. 用 `bEnableP1PiecePresentation` 开关确认关闭真实棋子时不会影响 G1 调试球。

---

## 13. 验收清单

- 12 个阵营棋子有稳定、可辨认的主色。
- Knight 步兵和骑兵 Rider 都能按阵营换色。
- Ranger 弓兵按阵营换色。
- Mage 主将按阵营换色。
- 皮肤色块不被误染。
- 黑色眼睛 / 阴影不被误染。
- 金属灰色块不被误染，除非明确配置为可换色。
- 骑兵的马不被错误套用人物 palette 材质。
- Move / Jump / Attack / Hit / Death 期间材质不丢失。
- 被吃淡出时材质仍跟随 Actor 缩放 / 隐藏，不生成残留 MID Actor。
- `OwnerFactionId` 越界时使用 fallback 颜色并输出一次 Warning。
- `P7PaletteReplaceMaterial` 为空时回退到原材质，不影响棋子生成。

---

## 14. 排错表

| 症状 | 根因 | 修复 |
| --- | --- | --- |
| 棋子全身变成阵营色 | Tile mask 过宽，或 RGBA8 mask pack / unpack 错误 | 检查 `PrimaryTileMaskRGBA8` / `SecondaryTileMaskRGBA8` 写入 |
| 棋子完全没换色 | 没有给 mesh slot 0 设置 MID，或 `P7PaletteReplaceMaterial` 为空 | 检查 `ApplyP7FactionMaterials_` 是否被调用 |
| 换到错误部位 | Row/Col 坐标上下颠倒，或 UV 公式和标注表不一致 | 统一使用 `row = floor((1 - V) * 4)` |
| 骑兵没颜色 | 只给 `HumanMesh` 设置材质，但骑兵显示的是 `RiderMesh` | 对 `RiderMesh` 应用 P7 MID |
| 主将贴图错成 Knight | `P7CommanderBaseTexture` 配错 | Commander 使用 `mage_texture` |
| 弓兵贴图错成 Knight | `P7ArcherBaseTexture` 配错 | Archer 使用 `ranger_texture` |
| Primary 区域发光过强 | `FactionPrimaryColor` 太亮，或后续新增的自发光强度参数过高 | 当前设计中 Primary tile 会输出同色自发光；先降低阵营 Primary 颜色亮度，后续可增加 `PrimaryEmissiveStrength` 单独控制 |
| 灰色金属也被染色 | `MinSaturationToReplace` 太低，或该 tile 本身被列入 mask | 提高阈值或移除该 tile |
| 阵营色看不清 | `FactionColorStrength` 太低，或目标 tile 面积太小 | 提高强度，换更大面积 tile |

---

## 15. 推荐落地顺序

1. 先做 `Knight1` 一个角色，确认材质能按 `R1C0` 等 tile 局部换色。
2. 接入 `ATerraPieceActor::ApplyVisualConfig_()`，让步兵 12 阵营换色。
3. 复制 mask 流程到 `Ranger1` 和 `Mage1`。
4. 给骑兵 `RiderMesh` 接同一套 Knight mask。
5. 调整 12 阵营颜色表，保证远景可读。
6. 再考虑盾牌、箭袋、披风 Mesh、法术颜色和终局胜败演出。

---

## 16. 与上下游关系

- 上游 P1：P7 复用真实棋子 Actor 和 `FTerraPiecePresentationSnapshot::OwnerFactionId`。
- 上游 P4：骑兵要对 `RiderMesh` 换色，不对 `HumanMesh` 换色。
- 上游 P5：武器挂载不参与首版人物 palette 换色。
- 上游 P6：法术和箭矢可在后续读取同一阵营颜色表做特效换色。
- Gameplay：无依赖回流，仍只负责阵营、兵种、位置和胜负。
- 下游 P7 终局演出：胜方可提高 `FactionColorStrength` 或叠加边缘光，败方淡出时保持阵营色直到隐藏。
