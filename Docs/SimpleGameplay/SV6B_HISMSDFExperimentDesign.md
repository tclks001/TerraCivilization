# SV6-B：HISM + SDF 径向投影实验设计稿

> 编码：UTF-8，简体中文。前置：SV6-A 已验收。

## 1. 目标与边界

SV6-B 用当前组成球面的 Plain/Forest/Mountain 三种 HISM Static Mesh 继续承担可见几何、Nanite、碰撞和既有 Gameplay 命中，只把三种资产原有材质临时替换为一份共享的径向投影 SDF 材质。该材质复用 SV6-A 的地形分类、三平面 PBR、河网、高亮和受光参数，用于验证“高密度 HISM 拼球 + 统一动态 SDF 材质”是否能够消除原 HISM 地块的材质接缝。

本阶段是可随时撤销的并列实验，不废弃 `LegacyHISMDebug` 和 `ContinuousSurface`：

| 模式 | 可见几何 | 材质 | 点击与碰撞 |
| --- | --- | --- | --- |
| `LegacyHISMDebug` | 三种旧 Tile HISM | Static Mesh 原材质 | 既有 HISM 路径 |
| `ContinuousSurface` | sub=7 连续基础表面 | SV6-A 连续材质 | 连续表面路径 |
| `HISMSDFExperiment` | 三种旧 Tile HISM | `TerrainVisualHISMSDFMaterial` | 既有 HISM 路径 |

SV6-B 不修改 WorldGen、Cell 拓扑、实例 Transform、Static Mesh 资产、棋子高度、相机、UI 或 Gameplay。模式切回 Legacy/Continuous 后，Actor 恢复 Static Mesh 原材质。

## 2. 径向映射与接缝假设

对 HISM 表面任意世界位置 `WorldPos`：

```text
Dir = normalize(WorldPos - PlanetCenter)
ProjectionWorldPos = PlanetCenter + Dir * HISMSDFProjectionRadiusCM
ProjectionNormalWS = Dir
```

- Cell SDF、地形分类、河网和高亮只读取 `Dir`。
- BaseColor、Roughness 和三平面法线使用 `ProjectionWorldPos/ProjectionNormalWS`，不使用资产 UV 和实际半径。
- 因此同一球面方向上的不同 Tile 遮挡层得到相同的纹理坐标与三平面混合权重；山头高度不会导致纹理沿径向漂移。
- HISM 网格自身的真实几何法线仍参与最终光照和阴影，因此本实验只能保证材质场连续，不能预先保证几何法线、轮廓、AO 或阴影连续。

`HISMSDFProjectionRadiusCM` 由 C++ 设置为 `GlobeRadiusCM + HISMTileRadiusOffsetCM`。这不是 SV4 宏观高度表面的精确位置，而是本阶段拍板的规范球面径向映射。

## 3. 每实例 Cell 上下文

连续表面把三个候选 CellId 烘焙在三角形 UV/VertexColor 中；普通 Static Mesh 没有这份元数据。SV6-B 为每个 Tile 实例写入 Owner Cell 及其六个直接邻居，像素材质从七个候选中选择球面距离最近的三个 Cell，再执行和 SV6-A 一致的 SDF 混合。

| Per Instance Custom Data | 内容 |
| ---: | --- |
| `0..3` | 保留旧 HISM RGB + Intensity 高亮数据，SV6-B 主路径不依赖它。 |
| `4` | Owner CellId。 |
| `5..10` | `NeighborCellIds[0..5]`；五边形空位回填 Owner CellId。 |

一个 Cell Tile 的可见范围正常只覆盖 Owner 及其一环邻居，因此七候选足以解析最近三格。若某个 Static Mesh 横跨两环以上，材质可能在其远端选择不到真实最近 Cell；这是本实验明确要暴露的资产覆盖范围风险，不以全 642 Cell 像素循环掩盖。

## 4. C++ 与 Actor 设置

代码新增：

- `ETerrainVisualMode::HISMSDFExperiment`。
- Actor 材质槽 `TerrainVisualHISMSDFMaterial`。
- HISM PICD 从 4 个扩展为 11 个，并稳定写入 Owner+六邻居。
- 实验模式不生成连续网格，但仍初始化 `SurfaceCellDirectionLUT`、`SurfaceTerrainLUT`、`SurfaceHighlightLUT`、River LUT 和 SV6-A 纹理参数。
- Plain/Forest/Mountain 各创建一份 MID；三份 MID 参数一致，只因属于不同 HISM Component 而独立持有。

编辑器验收设置：

1. 新建主材质 `M_TerrainVisual_HISMSDFExperiment`，按第 5 至 8 节搭建。
2. 在材质 Usage 中启用 `Used with Instanced Static Meshes`；Nanite Usage 若编辑器未自动识别则一并启用。
3. 将材质填入 Actor 的 `Terrain Visual > SV6-B HISM SDF Experiment > TerrainVisualHISMSDFMaterial`。
4. `TerrainVisualMode` 选择 `HISMSDFExperiment`。
5. 保持 `bEnableHISMTileRendering=true`，三种 Tile Static Mesh 和碰撞设置保持原值。

不需要手动修改三个 Static Mesh 资产的材质槽；运行时/Construction MID 会覆盖 HISM Component 材质，切回其他模式时恢复原材质。

## 5. 主材质与节点输入

材质设置：`Surface=Opaque`、`Blend Mode=Opaque`、`Tangent Space Normal=true`。创建下列七个 `PerInstanceCustomData` 节点，Data Index 分别为 `4..10`，默认值均为 `0`；每个节点先接独立 `VertexInterpolator`，再将七个插值器输出接入主 Custom。输入名称必须为：

```text
OwnerCell
Neighbor0
Neighbor1
Neighbor2
Neighbor3
Neighbor4
Neighbor5
```

主 Custom 其余输入：

| 输入 | 类型与来源 |
| --- | --- |
| `WorldPos` | Float3，Absolute World Position（Absolute）。 |
| `PlanetCenter` | Float3，Vector Parameter。 |
| `BaseNormalWS` | Float3，VertexNormalWS。 |
| `SurfaceCellCount` | Float1，Scalar Parameter，由 C++ 注入。 |
| `HISMSDFProjectionRadiusCM` | Float1，Scalar Parameter，由 C++ 注入。 |
| `SurfaceCellDirectionLUT` | Texture2D Object Parameter，默认可为空，由 C++ 注入。 |
| `SurfaceTerrainLUT` | Texture2D Object Parameter，由 C++ 注入。 |
| `SurfaceHighlightLUT` | Texture2D Object Parameter，由 C++ 注入。 |
| `SurfaceRiverSegmentLUT` | Texture2D Object Parameter，由 C++ 注入。 |
| `SurfaceRiverLakeLUT` | Texture2D Object Parameter，由 C++ 注入。 |
| `TerrainCellBlendRad` | Float1，建议沿用 SV6-A 当前值。 |
| `HighlightPaddingRad` / `HighlightStrength` | Float1，由现有参数/C++ 注入。 |
| `RiverSegmentCount` / `RiverLakeCount` | Float1，由 C++ 注入。 |
| `RiverWetWidth` | Float1，沿用 SV6-A/SV5 当前值。 |
| `RiverColor` | Float3，沿用当前水体颜色。 |
| `RiverHighlightStrength` | Float1，沿用当前值。 |

Additional Outputs：

| 输出 | 类型 |
| --- | --- |
| `OutPlainMask` / `OutForestMask` / `OutRockMask` | Float1 |
| `OutWetMask` / `OutWaterMask` | Float1 |
| `OutFlowWorld` | Float3 |
| `OutFlowCoord` | Float1 |
| `OutEmissive` | Float3 |
| `OutProjectionWorldPos` | Float3 |
| `OutProjectionNormalWS` | Float3 |

主输出类型为 `Float3`。以下代码可完整复制：

```hlsl
float candidates[7]={OwnerCell,Neighbor0,Neighbor1,Neighbor2,Neighbor3,Neighbor4,Neighbor5};
int cellCount=max((int)round(SurfaceCellCount),1);int c0=-1,c1=-1,c2=-1;float a0=1e6,a1=1e6,a2=1e6;
float3 dir=normalize(WorldPos-PlanetCenter);
[unroll]for(int i=0;i<7;i++)
{
    int c=clamp((int)round(candidates[i]),0,cellCount-1);
    if(c==c0||c==c1||c==c2)continue;
    float3 v=normalize(SurfaceCellDirectionLUT.Load(int3(c,0,0)).rgb);
    float a=acos(clamp(dot(dir,v),-1,1));
    if(a<a0){a2=a1;c2=c1;a1=a0;c1=c0;a0=a;c0=c;}
    else if(a<a1){a2=a1;c2=c1;a1=a;c1=c;}
    else if(a<a2){a2=a;c2=c;}
}
if(c1<0){c1=c0;a1=a0;}if(c2<0){c2=c1;a2=a1;}
float d0=a0-min(a1,a2),d1=a1-min(a0,a2),d2=a2-min(a0,a1);float bw=max(TerrainCellBlendRad,1e-5);float w0=smoothstep(-bw,bw,-d0),w1=smoothstep(-bw,bw,-d1),w2=smoothstep(-bw,bw,-d2),ws=max(w0+w1+w2,1e-5);w0/=ws;w1/=ws;w2/=ws;
float r0=SurfaceTerrainLUT.Load(int3(c0,0,0)).r,r1=SurfaceTerrainLUT.Load(int3(c1,0,0)).r,r2=SurfaceTerrainLUT.Load(int3(c2,0,0)).r;
float p0=1-step(.25,r0),m0=step(.75,r0),f0=saturate(1-p0-m0);float p1=1-step(.25,r1),m1=step(.75,r1),f1=saturate(1-p1-m1);float p2=1-step(.25,r2),m2=step(.75,r2),f2=saturate(1-p2-m2);
float plain=p0*w0+p1*w1+p2*w2,forest=f0*w0+f1*w1+f2*w2,mountain=m0*w0+m1*w1+m2*w2;
float slope=saturate(1-abs(dot(normalize(BaseNormalWS),dir)));float rock=saturate(max(mountain,mountain*slope*1.45));float moss=forest*(1-rock*.55);float gravel=saturate(1-moss-rock*.72);
float riverSdf=1e6,flowCoord=0;float3 flowDir=0;
[loop]for(int i=0;i<256;i++){if(i>=min((int)RiverSegmentCount,256))break;float4 sa=SurfaceRiverSegmentLUT.Load(int3(i*2,0,0)),sb=SurfaceRiverSegmentLUT.Load(int3(i*2+1,0,0));float3 A=normalize(sa.rgb),B=normalize(sb.rgb),N=normalize(cross(A,B)),q=normalize(dir-N*dot(dir,N));float arc=acos(clamp(dot(A,B),-1,1)),aq=acos(clamp(dot(A,q),-1,1)),qb=acos(clamp(dot(q,B),-1,1)),onArc=step(aq+qb,arc+1e-4),da=acos(clamp(dot(dir,A),-1,1)),db=acos(clamp(dot(dir,B),-1,1)),dist=lerp(min(da,db),acos(clamp(dot(dir,q),-1,1)),onArc),t=saturate(aq/max(arc,1e-5)),sdf=dist-lerp(sa.a,sb.a,t);if(sdf<riverSdf){riverSdf=sdf;flowCoord=aq;flowDir=normalize(B-dir*dot(B,dir));}}
[loop]for(int i=0;i<16;i++){if(i>=min((int)RiverLakeCount,16))break;float4 lc=SurfaceRiverLakeLUT.Load(int3(i*2,0,0)),la=SurfaceRiverLakeLUT.Load(int3(i*2+1,0,0));float3 C=normalize(lc.rgb),X=normalize(la.rgb),Y=normalize(cross(C,X));float ca=acos(clamp(dot(dir,C),-1,1)),gate=step(ca,max(lc.a,la.a)*2.5);float2 uv=float2(asin(clamp(dot(dir,X),-1,1))/max(lc.a,1e-5),asin(clamp(dot(dir,Y),-1,1))/max(la.a,1e-5));float sdf=lerp(1e6,(length(uv)-1)*min(lc.a,la.a),gate);if(sdf<riverSdf){riverSdf=sdf;flowDir=0;}}
float water=saturate(1-smoothstep(-.0005,.0008,riverSdf)),wet=saturate(1-smoothstep(0,max(.0001,RiverWetWidth*.002),riverSdf));
float4 h0=SurfaceHighlightLUT.Load(int3(c0,0,0)),h1=SurfaceHighlightLUT.Load(int3(c1,0,0)),h2=SurfaceHighlightLUT.Load(int3(c2,0,0));float pad=max(HighlightPaddingRad,1e-5),e0=(1-smoothstep(0,pad,-d0))*step(0,-d0),e1=(1-smoothstep(0,pad,-d1))*step(0,-d1),e2=(1-smoothstep(0,pad,-d2))*step(0,-d2);float3 hi=e0*h0.rgb*h0.a+e1*h1.rgb*h1.a+e2*h2.rgb*h2.a;
OutPlainMask=gravel;OutForestMask=moss;OutRockMask=rock;OutWetMask=wet;OutWaterMask=water;OutFlowWorld=flowDir;OutFlowCoord=flowCoord;OutEmissive=hi*HighlightStrength+water*RiverColor*RiverHighlightStrength;OutProjectionWorldPos=PlanetCenter+dir*max(HISMSDFProjectionRadiusCM,1);OutProjectionNormalWS=dir;return float3(1,1,1);
```

## 6. Base Color 与 Roughness

Base Color Custom 与 SV6-A 相同，但 `WorldPos` 必须接 `OutProjectionWorldPos`，`NormalWS` 必须接 `OutProjectionNormalWS`，禁止接 Absolute World Position 或 VertexNormalWS。

```hlsl
float3 n=abs(normalize(NormalWS));n=pow(n,max(Sharpness,1));n/=max(n.x+n.y+n.z,1e-5);float3 uvX=float3(WorldPos.yz/max(TileScaleCM,1),0),uvY=float3(WorldPos.xz/max(TileScaleCM,1),0),uvZ=float3(WorldPos.xy/max(TileScaleCM,1),0);
float3 g=GravelColor.SampleLevel(GravelColorSampler,uvX,0).rgb*n.x+GravelColor.SampleLevel(GravelColorSampler,uvY,0).rgb*n.y+GravelColor.SampleLevel(GravelColorSampler,uvZ,0).rgb*n.z;float3 m=MossColor.SampleLevel(MossColorSampler,uvX,0).rgb*n.x+MossColor.SampleLevel(MossColorSampler,uvY,0).rgb*n.y+MossColor.SampleLevel(MossColorSampler,uvZ,0).rgb*n.z;float3 r=RockColor.SampleLevel(RockColorSampler,uvX,0).rgb*n.x+RockColor.SampleLevel(RockColorSampler,uvY,0).rgb*n.y+RockColor.SampleLevel(RockColorSampler,uvZ,0).rgb*n.z;
float sum=max(PlainMask+ForestMask+RockMask,1e-5);float3 land=(g*PlainTint*PlainMask+m*ForestTint*ForestMask+r*MountainTint*RockMask)/sum;land=lerp(land,land*float3(.62,.72,.58),WetMask*.35);return lerp(land,RiverColor,WaterMask);
```

输入名称与 SV6-A 保持一致：`GravelColor/MossColor/RockColor`、`PlainMask/ForestMask/RockMask/WetMask/WaterMask`、`PlainTint/ForestTint/MountainTint`、`TileScaleCM`、`Sharpness`、`RiverColor`。输出接 `Base Color`。

Roughness Custom 同样使用投影位置和投影法线，并采用 SV6-A.1 已验收的受光参数：

```hlsl
float3 n=abs(normalize(NormalWS));n=pow(n,max(Sharpness,1));n/=max(n.x+n.y+n.z,1e-5);
float3 ux=float3(WorldPos.yz/max(TileScaleCM,1),0),uy=float3(WorldPos.xz/max(TileScaleCM,1),0),uz=float3(WorldPos.xy/max(TileScaleCM,1),0);
float g=GravelRoughness.SampleLevel(GravelRoughnessSampler,ux,0).r*n.x+GravelRoughness.SampleLevel(GravelRoughnessSampler,uy,0).r*n.y+GravelRoughness.SampleLevel(GravelRoughnessSampler,uz,0).r*n.z;
float m=MossRoughness.SampleLevel(MossRoughnessSampler,ux,0).r*n.x+MossRoughness.SampleLevel(MossRoughnessSampler,uy,0).r*n.y+MossRoughness.SampleLevel(MossRoughnessSampler,uz,0).r*n.z;
float r=RockRoughness.SampleLevel(RockRoughnessSampler,ux,0).r*n.x+RockRoughness.SampleLevel(RockRoughnessSampler,uy,0).r*n.y+RockRoughness.SampleLevel(RockRoughnessSampler,uz,0).r*n.z;
g=lerp(PlainRoughnessMin,PlainRoughnessMax,saturate(g));m=lerp(ForestRoughnessMin,ForestRoughnessMax,saturate(m));r=lerp(RockRoughnessMin,RockRoughnessMax,saturate(r));
float sum=max(PlainMask+ForestMask+RockMask,1e-5);float land=(g*PlainMask+m*ForestMask+r*RockMask)/sum;land=saturate(land*lerp(1,WetRoughnessScale,WetMask));return lerp(land,RiverRoughness,WaterMask);
```

粗糙度输入与默认值直接沿用 SV6-A.1；`WorldPos=OutProjectionWorldPos`，`NormalWS=OutProjectionNormalWS`。输出接 `Roughness`。`TerrainLandSpecular` 与 `RiverSpecular` 继续通过 `Lerp(Alpha=OutWaterMask)` 接入材质 `Specular`。

## 7. 法线与水波

保持 `Tangent Space Normal=true`。复制 SV6-A 的 Gravel/Moss/Rock 三组 `WorldAlignedNormal` 与掩码混合节点，只修改一处：三个 `WorldAlignedNormal.Normal` 全部接主 Custom 的 `OutProjectionNormalWS`，不接 `VertexNormalWS`。`TextureSize` 仍使用 `(TerrainTileScaleCM, TerrainTileScaleCM, TerrainTileScaleCM)`。

```text
N_Gravel --┐
           Lerp_GroundNormal --┐
N_Moss ----┘                    Lerp_LandNormal -> FlattenNormal -> N_Land --┐
N_Rock -------------------------┘                                             Lerp_FinalNormal -> Material Normal
SV5 N_RiverWave -------------------------------------------------------------┘
OutForestMask -> Lerp_GroundNormal Alpha
OutRockMask   -> Lerp_LandNormal Alpha
OutWaterMask  -> Lerp_FinalNormal Alpha
```

SV5 水波 Custom 的 `WorldNormal` 也改接 `OutProjectionNormalWS`，`FlowWorld/FlowCoord/WaterMask` 接本阶段主 Custom 输出。这样相同方向的重叠 Tile 得到同一组投影法线纹理和水波方向；网格真实法线仍由材质最终 TBN 与几何光照体现。

`OutEmissive` 接 `Emissive Color`。不要再把 PICD `0..3` 直接接入 Emissive，否则会叠加旧 HISM 整实例高亮，干扰本阶段的球面 SDF 边界验收。

## 8. 验收与风险判定

### 8.1 必须通过

1. `LegacyHISMDebug`、`ContinuousSurface`、`HISMSDFExperiment` 可来回切换，前两种模式的材质与交互不变。
2. 实验模式只显示旧 HISM 球，不生成或保存 sub=7 连续 Dynamic Mesh。
3. Plain/Forest/Mountain Static Mesh 原本的材质颜色被统一 SDF 地形覆盖；同一地形区域的颜色不由实例所属 HISM Component 决定。
4. 森林/山脉/平原混合边界、水体、高亮位置与同一 WorldGen Seed 下的连续模式方向一致。
5. 旋转镜头观察 Tile 遮挡边缘，BaseColor、纹理相位和 Roughness 不应在边缘突然跳变。
6. HISM 点击、hover、棋子移动、棋子高度和 UI 流程保持原行为。

### 8.2 本实验要主动观察

| 现象 | 说明与下一步判断 |
| --- | --- |
| 颜色连续但明暗有折线 | 几何边界法线/切线不连续；共享 SDF 材质不能自动焊接实例法线。 |
| 接缝处出现黑边或阴影线 | Tile 几何相交、深度竞争、AO/阴影问题，不是 SDF 采样问题。 |
| 某个 Tile 远端地形类别错误 | Mesh 超出 Owner 一环；需扩大实例上下文或限制资产覆盖范围。 |
| 河流沿山体侧面向上爬 | 径向投影的预期副作用；若保留路线，水体需增加可见表面高度/坡度约束。 |
| 山地仍像离散山头 | 几何资产库与摆放问题；材质统一不会改变轮廓。 |
| HISM 与连续模式纹理细节位置不同 | 连续模式使用真实 SV4 位移位置，SV6-B 使用固定规范半径；本实验只要求方向语义一致。 |
| GPU 时间明显上升 | 河段循环和多组三平面采样按屏幕像素执行；Nanite 降低几何成本，不降低复杂材质的像素成本。 |

只有在材质接缝、交互和性能均可接受后，才讨论用更多 HISM 山体变体替换连续宏观山体。本阶段不据材质验收结果直接废弃连续表面。

