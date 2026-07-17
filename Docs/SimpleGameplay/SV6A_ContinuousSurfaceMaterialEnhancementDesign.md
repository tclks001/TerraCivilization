# SV6-A：连续表面材质增强设计稿

> 编码：UTF-8，简体中文。前置：SV3/SV4/SV4.5/SV5 已验收。

## 1. 目标与边界

SV6-A 用三平面 PBR 纹理和参数化 Tint 增强连续基础表面，使没有 HISM 岩脊覆盖的山地也具备连续岩层、碎石和植被质感。它是 SV6 HISM Decor 的前置视觉层，不生成 HISM、不改变连续网格、碰撞、CellId、Gameplay 或河网。

细节职责：

```text
SV4/SV4.5 几何高度：山脉轮廓、侵蚀沟谷
SV6-A 材质：岩层、碎石、苔原/树冠覆盖、PBR 细节
SV6 HISM Decor：局部清晰岩脊、尖峰、巨石、草与树
```

## 2. 纹理方案

使用 `Content/Textures` 内的 1K PBR 纹理，每组都包含 `Color`、`NormalDX`、`Roughness`：

| 组 | 资产 | 地貌角色 |
| --- | --- | --- |
| Gravel042 | `Gravel042_1K-PNG_*` | 平原基础、山脚碎石、侵蚀沟谷、河岸。 |
| Moss002 | `Moss002_1K-PNG_*` | 森林树冠层/苔原与湿润覆盖。 |
| Rock038 | `Rock038_1K-PNG_*` | 山脊、陡坡和裸露岩层。 |

三平面投影使用 `Absolute World Position` 与 `VertexNormalWS`。权重必须来自真实连续表面法线，不能使用 Cell SDF 边界噪声方向，避免 Cell 内产生投影接缝。`TerrainTileScaleCM` 默认 `450cm`，`TerrainTriplanarSharpness` 默认 `4`。

## 3. C++ 注入与 Actor 设置

`APlanetTessellatedMesh` 在 `PlanetTopology|Terrain Visual|SV6-A Surface Enhancement` 暴露 9 张纹理和以下参数：

| 参数 | 默认值 |
| --- | --- |
| `PlainTint` | `(0.72, 0.82, 0.48)` |
| `ForestTint` | `(0.18, 0.42, 0.20)` |
| `MountainTint` | `(0.48, 0.42, 0.34)` |
| `TerrainTileScaleCM` | `450` |
| `TerrainTriplanarSharpness` | `4` |
| `TerrainNormalStrength` | `0.55` |

将上述 9 张 `Content/Textures/*` 资产分别填入 Actor 属性。C++ 以 MID 参数名 `GravelColor`、`GravelNormal`、`GravelRoughness`、`Moss*`、`Rock*` 注入。对应 Texture Object Parameter 节点必须在节点 Details 的 **Texture** 槽也填写同类默认纹理，确保材质编译期 sampler 类型正确。

## 4. 主 Custom 节点

继续使用 SV5 的主 Custom 节点和所有输入。新增以下输入：

| 输入 | 类型 | 来源 |
| --- | --- | --- |
| `BaseNormalWS` | Float3 | VertexNormalWS |
| `TerrainTileScaleCM` | Float1 | Scalar Parameter |
| `TerrainTriplanarSharpness` | Float1 | Scalar Parameter |

新增 Additional Outputs：

| 输出 | 类型 | 用途 |
| --- | --- | --- |
| `OutPlainMask` | Float1 | Gravel 权重。 |
| `OutForestMask` | Float1 | Moss 权重。 |
| `OutRockMask` | Float1 | Rock 权重。 |
| `OutWetMask` | Float1 | 河岸湿润影响。 |
| `OutWaterMask` | Float1 | 沿用 SV5 水面遮罩。 |
| `OutFlowWorld` | Float3 | 沿用 SV5 水流方向。 |
| `OutFlowCoord` | Float1 | 沿用 SV5 波相位。 |

主节点不采样 PBR 贴图，也不直接输出 Normal。以下 HLSL 可替换 SV5 主节点；它继续计算 Cell SDF、河流和高亮，但将实际 PBR 颜色交给后续小节点。

`SurfaceTerrainLUT.r` 是离散类别编码：Plain=`0`、Forest=`127/255`、Mountain=`1`。因此，绝不能先将三个 Cell 的 `r` 值按 `w0/w1/w2` 混合后再用阈值分类。必须先把每个 Cell 解码为 one-hot 类别，再混合类别权重；这样 Plain/Forest/Mountain 的视觉过渡才会准确位于球面 Cell SDF 边界，并和高亮边界一致。以下完整样板已按此规则修正。

```hlsl
int c0=(int)round(CellContextUV.x),c1=(int)round(CellContextUV.y);int c2=(int)round(CellContextColor.r*255)*256+(int)round(CellContextColor.g*255);
float3 dir=normalize(WorldPos-PlanetCenter);float3 v0=normalize(SurfaceCellDirectionLUT.Load(int3(c0,0,0)).rgb),v1=normalize(SurfaceCellDirectionLUT.Load(int3(c1,0,0)).rgb),v2=normalize(SurfaceCellDirectionLUT.Load(int3(c2,0,0)).rgb);
float a0=acos(clamp(dot(dir,v0),-1,1)),a1=acos(clamp(dot(dir,v1),-1,1)),a2=acos(clamp(dot(dir,v2),-1,1));float d0=a0-min(a1,a2),d1=a1-min(a0,a2),d2=a2-min(a0,a1);float bw=max(TerrainCellBlendRad,1e-5);float w0=smoothstep(-bw,bw,-d0),w1=smoothstep(-bw,bw,-d1),w2=smoothstep(-bw,bw,-d2),ws=max(w0+w1+w2,1e-5);w0/=ws;w1/=ws;w2/=ws;
float r0=SurfaceTerrainLUT.Load(int3(c0,0,0)).r,r1=SurfaceTerrainLUT.Load(int3(c1,0,0)).r,r2=SurfaceTerrainLUT.Load(int3(c2,0,0)).r;
float p0=1-step(.25,r0),m0=step(.75,r0),f0=saturate(1-p0-m0);float p1=1-step(.25,r1),m1=step(.75,r1),f1=saturate(1-p1-m1);float p2=1-step(.25,r2),m2=step(.75,r2),f2=saturate(1-p2-m2);
float plain=p0*w0+p1*w1+p2*w2,forest=f0*w0+f1*w1+f2*w2,mountain=m0*w0+m1*w1+m2*w2;
float slope=saturate(1-abs(dot(normalize(BaseNormalWS),dir)));float rock=saturate(max(mountain,mountain*slope*1.45));float moss=forest*(1-rock*.55);float gravel=saturate(1-moss-rock*.72);
float riverSdf=1e6,flowCoord=0;float3 flowDir=0;
[loop]for(int i=0;i<256;i++){if(i>=min((int)RiverSegmentCount,256))break;float4 sa=SurfaceRiverSegmentLUT.Load(int3(i*2,0,0)),sb=SurfaceRiverSegmentLUT.Load(int3(i*2+1,0,0));float3 A=normalize(sa.rgb),B=normalize(sb.rgb),N=normalize(cross(A,B)),q=normalize(dir-N*dot(dir,N));float arc=acos(clamp(dot(A,B),-1,1)),aq=acos(clamp(dot(A,q),-1,1)),qb=acos(clamp(dot(q,B),-1,1)),onArc=step(aq+qb,arc+1e-4),da=acos(clamp(dot(dir,A),-1,1)),db=acos(clamp(dot(dir,B),-1,1)),dist=lerp(min(da,db),acos(clamp(dot(dir,q),-1,1)),onArc),t=saturate(aq/max(arc,1e-5)),sdf=dist-lerp(sa.a,sb.a,t);if(sdf<riverSdf){riverSdf=sdf;flowCoord=aq;flowDir=normalize(B-dir*dot(B,dir));}}
[loop]for(int i=0;i<16;i++){if(i>=min((int)RiverLakeCount,16))break;float4 lc=SurfaceRiverLakeLUT.Load(int3(i*2,0,0)),la=SurfaceRiverLakeLUT.Load(int3(i*2+1,0,0));float3 C=normalize(lc.rgb),X=normalize(la.rgb),Y=normalize(cross(C,X));float ca=acos(clamp(dot(dir,C),-1,1)),gate=step(ca,max(lc.a,la.a)*2.5);float2 uv=float2(asin(clamp(dot(dir,X),-1,1))/max(lc.a,1e-5),asin(clamp(dot(dir,Y),-1,1))/max(la.a,1e-5));float sdf=lerp(1e6,(length(uv)-1)*min(lc.a,la.a),gate);if(sdf<riverSdf){riverSdf=sdf;flowDir=0;}}
float water=saturate(1-smoothstep(-.0005,.0008,riverSdf)),wet=saturate(1-smoothstep(0,max(.0001,RiverWetWidth*.002),riverSdf));
float4 h0=SurfaceHighlightLUT.Load(int3(c0,0,0)),h1=SurfaceHighlightLUT.Load(int3(c1,0,0)),h2=SurfaceHighlightLUT.Load(int3(c2,0,0));float pad=max(HighlightPaddingRad,1e-5),e0=(1-smoothstep(0,pad,-d0))*step(0,-d0),e1=(1-smoothstep(0,pad,-d1))*step(0,-d1),e2=(1-smoothstep(0,pad,-d2))*step(0,-d2);float3 hi=e0*h0.rgb*h0.a+e1*h1.rgb*h1.a+e2*h2.rgb*h2.a;
OutPlainMask=gravel;OutForestMask=moss;OutRockMask=rock;OutWetMask=wet;OutWaterMask=water;OutFlowWorld=flowDir;OutFlowCoord=flowCoord;OutEmissive=hi*HighlightStrength+water*RiverColor*RiverHighlightStrength;OutRoughness=lerp(.82,.18,water);OutMetallic=0;return float3(1,1,1);
```

主输出 `Base Color` 可先连接，但后续 PBR 小节点输出会覆盖其结果。`OutEmissive`、`OutRoughness`、`OutMetallic` 保持连接。

## 5. 三平面 PBR 小 Custom

创建一个独立 Custom 节点，输出 `Float3`，连接 `Base Color`。输入：`WorldPos`(Float3)、`NormalWS`(Float3)、三组 `Color` Texture2D、`PlainMask`、`ForestMask`、`RockMask`、`WetMask`、`WaterMask`、`PlainTint`、`ForestTint`、`MountainTint`、`TileScaleCM`、`Sharpness`、`RiverColor`。纹理对象使用 Texture Object Parameter。

```hlsl
float3 n=abs(normalize(NormalWS));n=pow(n,max(Sharpness,1));n/=max(n.x+n.y+n.z,1e-5);float3 uvX=float3(WorldPos.yz/max(TileScaleCM,1),0),uvY=float3(WorldPos.xz/max(TileScaleCM,1),0),uvZ=float3(WorldPos.xy/max(TileScaleCM,1),0);
float3 g=GravelColor.SampleLevel(GravelColorSampler,uvX,0).rgb*n.x+GravelColor.SampleLevel(GravelColorSampler,uvY,0).rgb*n.y+GravelColor.SampleLevel(GravelColorSampler,uvZ,0).rgb*n.z;float3 m=MossColor.SampleLevel(MossColorSampler,uvX,0).rgb*n.x+MossColor.SampleLevel(MossColorSampler,uvY,0).rgb*n.y+MossColor.SampleLevel(MossColorSampler,uvZ,0).rgb*n.z;float3 r=RockColor.SampleLevel(RockColorSampler,uvX,0).rgb*n.x+RockColor.SampleLevel(RockColorSampler,uvY,0).rgb*n.y+RockColor.SampleLevel(RockColorSampler,uvZ,0).rgb*n.z;
float sum=max(PlainMask+ForestMask+RockMask,1e-5);float3 land=(g*PlainTint*PlainMask+m*ForestTint*ForestMask+r*MountainTint*RockMask)/sum;land=lerp(land,land*float3(.62,.72,.58),WetMask*.35);return lerp(land,RiverColor,WaterMask);
```

## 6. 法线与粗糙度小节点

保持 **Tangent Space Normal 开启**。不要关闭它，也不要把世界空间法线直接接到 `Normal`。SV6-A 的地表法线使用 UE 标准 `WorldAlignedNormal`，水波继续使用 SV5 的切线空间小 Custom；空间变换全部交给 UE 节点。

### 6.1 地表法线节点

以下步骤以材质图中主 Custom 的输出 `OutForestMask`、`OutRockMask`、`OutWaterMask` 已可用为前提。所有 `WorldAlignedNormal` 输出与 SV5 水波节点输出均是**切线空间法线**，可以安全连接到材质 `Normal`。

#### 6.1.1 保持材质空间设置

1. 点击材质图空白处，打开主材质 Details。
2. 确认 **Tangent Space Normal** 为勾选状态。
3. 不要连接 `VertexNormalWS` 或任何世界空间向量到材质 `Normal` 引脚。
4. 删除旧版 SV5 中可能残留的 `OutNormal` 连线；它会覆盖整颗球的正确顶点法线。

#### 6.1.2 创建公共三平面尺度

1. 创建 `Scalar Parameter`，名称必须为 `TerrainTileScaleCM`，默认值 `450`。
2. 创建 `AppendVector` 节点。
3. 将同一个 `TerrainTileScaleCM` 节点连接到 `AppendVector` 的 `A` 与 `B`。
4. 创建第二个 `AppendVector` 节点：将第一个 `AppendVector` 输出连接到第二个的 `A`，再将 `TerrainTileScaleCM` 连接到第二个的 `B`。
5. 第二个 `AppendVector` 输出即为 `(TileScaleCM, TileScaleCM, TileScaleCM)`，下文称为 `TriSize`。

不要使用 `Absolute World Position` 直接接到 `Normal`；`WorldAlignedNormal` 自己负责世界坐标投影。

#### 6.1.3 创建三组 WorldAlignedNormal

依次创建三个 `WorldAlignedNormal` 材质函数调用节点。创建方式：在材质图空白处右键，搜索 `WorldAlignedNormal`，选择材质函数调用。

**A. Gravel 法线**

1. 创建 `Texture Object Parameter`，名称 `GravelNormal`。
2. 在该节点 Details 的 **Texture** 槽填写 `Gravel042_1K-PNG_NormalDX`；Sampler Type 选择 `Normal`。
3. 将 `GravelNormal` 输出连接到 `WorldAlignedNormal` 的 `TextureObject`。
4. 将 `TriSize` 连接到 `TextureSize`。
5. 创建 `VertexNormalWS` 节点，连接到 `Normal` 输入。
6. 将 `WorldAlignedNormal` 的 `XYZ` 输出命名为 `N_Gravel`。

**B. Moss 法线**

重复 A 的 1 至 6 步，仅替换：

```text
Texture Object Parameter: MossNormal
默认纹理: Moss002_1K-PNG_NormalDX
输出别名: N_Moss
```

**C. Rock 法线**

重复 A 的 1 至 6 步，仅替换：

```text
Texture Object Parameter: RockNormal
默认纹理: Rock038_1K-PNG_NormalDX
输出别名: N_Rock
```

`WorldAlignedNormal` 的其他输入保持默认即可。若节点版本显示 `ProjectionTransitionContrast` 输入，创建 `Vector Parameter`，名称 `TerrainTriplanarContrast`、默认 `(4,4,4)`，同时连接到三个节点；该参数与 C++ 注入的 `TerrainTriplanarSharpness` 视觉语义一致，但材质节点不能直接把一个标量自动扩成三分量。

#### 6.1.4 按地貌掩码混合法线

1. 创建 `LinearInterpolate`，命名为 `Lerp_GroundNormal`：
   - `A` 接 `N_Gravel`。
   - `B` 接 `N_Moss`。
   - `Alpha` 接主 Custom 的 `OutForestMask`。
2. 创建第二个 `LinearInterpolate`，命名为 `Lerp_LandNormal`：
   - `A` 接 `Lerp_GroundNormal` 输出。
   - `B` 接 `N_Rock`。
   - `Alpha` 接主 Custom 的 `OutRockMask`。

主 Custom 内已使 Forest 与 Rock 掩码大体互斥，因此此顺序会得到：Plain/沟谷偏 Gravel、Forest 偏 Moss、Mountain/陡坡偏 Rock。

#### 6.1.5 控制连续地表法线强度

1. 创建 `Scalar Parameter`，名称 `TerrainNormalStrength`，默认 `0.55`。
2. 创建 `OneMinus`，输入接 `TerrainNormalStrength`。
3. 创建 `FlattenNormal` 节点：
   - `Normal` 接 `Lerp_LandNormal` 输出。
   - `Flatness` 接 `OneMinus` 输出。
4. 将输出命名为 `N_Land`。

`TerrainNormalStrength=0` 时，`N_Land` 接近平坦 `(0,0,1)`；`1` 时保留贴图法线完整强度。建议初次验收保持 `0.45–0.65`，避免连续基础面比未来 HISM 岩脊还要尖锐。

#### 6.1.6 与 SV5 水波法线合并

SV5 已有的水波小 Custom 输出命名为 `N_RiverWave`，它在 `OutWaterMask=0` 时返回 `(0,0,1)`。

1. 创建第三个 `LinearInterpolate`，命名为 `Lerp_FinalNormal`。
2. `A` 接 `N_Land`。
3. `B` 接 `N_RiverWave`。
4. `Alpha` 接主 Custom 的 `OutWaterMask`。
5. `Lerp_FinalNormal` 输出连接主材质 `Normal` 引脚。

最终连线为：

```text
N_Gravel --┐
           Lerp_GroundNormal --┐
N_Moss ----┘                    Lerp_LandNormal -> FlattenNormal -> N_Land --┐
N_Rock -------------------------┘                                               Lerp_FinalNormal -> Material Normal
SV5 N_RiverWave ---------------------------------------------------------------┘
OutForestMask -> Lerp_GroundNormal Alpha
OutRockMask   -> Lerp_LandNormal Alpha
OutWaterMask  -> Lerp_FinalNormal Alpha
```

非水区域始终使用 `N_Land`；河流区域才替换为 SV5 切线空间波纹。整个过程不关闭 `Tangent Space Normal`，因此不会破坏球面其他区域的受光。

### 6.2 粗糙度小 Custom

创建独立 Custom 节点，主输出为 `Float1`，连接材质 `Roughness`。输入如下：

| 输入 | 类型 |
| --- | --- |
| `WorldPos` | Float3，Absolute World Position |
| `NormalWS` | Float3，VertexNormalWS |
| `GravelRoughness` / `MossRoughness` / `RockRoughness` | Texture2D Object Parameter |
| `PlainMask` / `ForestMask` / `RockMask` / `WetMask` / `WaterMask` | Float1，来自主 Custom 输出 |
| `TileScaleCM` / `Sharpness` | Float1，SV6-A 参数 |

```hlsl
float3 n=abs(normalize(NormalWS));n=pow(n,max(Sharpness,1));n/=max(n.x+n.y+n.z,1e-5);
float3 ux=float3(WorldPos.yz/max(TileScaleCM,1),0),uy=float3(WorldPos.xz/max(TileScaleCM,1),0),uz=float3(WorldPos.xy/max(TileScaleCM,1),0);
float g=GravelRoughness.SampleLevel(GravelRoughnessSampler,ux,0).r*n.x+GravelRoughness.SampleLevel(GravelRoughnessSampler,uy,0).r*n.y+GravelRoughness.SampleLevel(GravelRoughnessSampler,uz,0).r*n.z;
float m=MossRoughness.SampleLevel(MossRoughnessSampler,ux,0).r*n.x+MossRoughness.SampleLevel(MossRoughnessSampler,uy,0).r*n.y+MossRoughness.SampleLevel(MossRoughnessSampler,uz,0).r*n.z;
float r=RockRoughness.SampleLevel(RockRoughnessSampler,ux,0).r*n.x+RockRoughness.SampleLevel(RockRoughnessSampler,uy,0).r*n.y+RockRoughness.SampleLevel(RockRoughnessSampler,uz,0).r*n.z;
float sum=max(PlainMask+ForestMask+RockMask,1e-5);float land=(g*PlainMask+m*ForestMask+r*RockMask)/sum;land=lerp(land,land*.72,WetMask);return lerp(land,.18,WaterMask);
```

三个 Roughness Texture Object Parameter 的 Sampler Type 设为 `Linear Grayscale`；Color 参数设为 `Color`，NormalDX 参数设为 `Normal`。

## 7. 验收

1. 没有 HISM 的山脊和山坡也读为连续岩层，不出现 Cell UV、经线或三平面接缝。
2. Forest 有 Moss002 覆盖，Plain/沟谷有 Gravel042，陡峭 Mountain 有 Rock038。
3. River 高亮、湿润带与 SV5 流水法线继续工作。
4. 关闭或移除所有 HISM 后，连续表面仍有可信的中距离材质细节。
