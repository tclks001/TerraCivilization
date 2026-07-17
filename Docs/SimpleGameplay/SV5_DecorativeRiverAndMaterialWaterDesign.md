# SV5：装饰性河网与材质水体设计稿

> 编码：UTF-8，简体中文。前置：SV4/SV4.5 连续高度场。

## 1. 范围

SV5 只生成无碰撞、无 Gameplay 语义的河网和小型终点湖泊。C++ 先选少量彼此分离的低地终点盆地，再对 Cell 图做 Priority-Flood 式填洼，生成所有 Cell 指向终点盆地的父链；只有 Mountain Cell 可作为河源，高地源头沿父链汇流，因此不会在 SV4.5 视觉噪声造成的局部洼地停止。每条路径以球面 Catmull-Rom 投影平滑并离散为 5 段短测地线弧。只有实际抵达指定终点盆地、且累计流量达到阈值的河流才生成半径不超过相邻 Cell 中心间距 `25%` 的小湖；路径长度上限截断的候选路径不会生成湖。

河网不参与点击、CellId、棋子高度、相机或移动规则。

河网参数在 Actor Details 的 `PlanetTopology|Terrain Visual|SV5 Decorative Rivers` 中公开。调节河流数量时优先修改 `TerrainVisualRiverSourceCount`；路径整体长度由 `TerrainVisualRiverMinPathCells` 和 `TerrainVisualRiverMaxPathCells` 控制；湖泊数量由终点盆地数和 `TerrainVisualRiverTerminalLakeMinDischarge` 控制。

河宽是 DAG 节点的唯一属性，不是边的独立属性：节点宽度由累计流量与最长上游路径长度计算，同一节点相连的所有曲线边共享该宽度。边内只在两个节点宽度之间插值，禁止把沿程增宽仅写到单边终点，否则会在节点处产生圆形鼓包和断续感。

## 2. LUT 协议

`SurfaceRiverSegmentLUT` 为 `PF_A32B32G32R32F`，每河段两个像素：

| 像素 | RGB | A |
| --- | --- | --- |
| `2*i` | 起点单位方向 | 起点半宽，弧度 |
| `2*i+1` | 终点单位方向 | 终点半宽，弧度 |

`SurfaceRiverLakeLUT` 同格式，每湖泊两个像素：

| 像素 | RGB | A |
| --- | --- | --- |
| `2*i` | 湖中心单位方向 | 沿河方向半径，弧度 |
| `2*i+1` | 流向切线 | 横向半径，弧度 |

两张纹理由 C++ 以 transient 资源注入 MID。`RiverSegmentCount` 与 `RiverLakeCount` 为对应有效数量；C++ 对共享下游逻辑边去重，每逻辑边仅离散为 2 段；材质默认最多读取 256 河段和 16 湖泊。

## 3. 材质编辑

在现有 `M_TerrainVisual_SurfaceTerrain` 的同一个 Custom 节点上新增输入：

| 输入 | 类型 | 节点 |
| --- | --- | --- |
| `SurfaceRiverSegmentLUT` | Texture2D | Texture Object Parameter，同名 |
| `SurfaceRiverLakeLUT` | Texture2D | Texture Object Parameter，同名 |
| `RiverSegmentCount` | Float1 | Scalar Parameter，默认 0 |
| `RiverLakeCount` | Float1 | Scalar Parameter，默认 0 |
| `RiverColor` | Float3 | Vector Parameter，`(0.025,0.16,0.22)` |
| `RiverWetColor` | Float3 | Vector Parameter，`(0.07,0.20,0.12)` |
| `RiverWetWidth` | Float1 | Scalar Parameter，默认 `1.8` |
| `RiverHighlightStrength` | Float1 | Scalar Parameter，默认 `0.18` |

原 SV3 的 14 个输入、三个输出保持不变。新增 Additional Output：`OutMetallic`、`OutWaterMask`、`OutFlowWorld`、`OutFlowCoord`，类型依次为 `Float1`、`Float1`、`Float3`、`Float1`。`OutMetallic` 连接材质 `Metallic`；后三项供下方的少量材质节点构造切线空间水波法线。

保持材质属性 **Tangent Space Normal 开启**。不要连接世界空间 `Normal` 输出，也不要关闭该属性；非水区域必须继续保留连续网格原有的顶点法线与受光。

将下方完整 HLSL 替换 Custom 节点 `Code`；在原 SV3 Custom Inputs 后按表中顺序追加 8 个河流输入。

```hlsl
int c0=(int)round(CellContextUV.x), c1=(int)round(CellContextUV.y);
int c2=(int)round(CellContextColor.r*255)*256+(int)round(CellContextColor.g*255);
float3 dir=normalize(WorldPos-PlanetCenter);
float3 v0=normalize(SurfaceCellDirectionLUT.Load(int3(c0,0,0)).rgb);
float3 v1=normalize(SurfaceCellDirectionLUT.Load(int3(c1,0,0)).rgb);
float3 v2=normalize(SurfaceCellDirectionLUT.Load(int3(c2,0,0)).rgb);
float a0=acos(clamp(dot(dir,v0),-1,1)),a1=acos(clamp(dot(dir,v1),-1,1)),a2=acos(clamp(dot(dir,v2),-1,1));
float d0=a0-min(a1,a2),d1=a1-min(a0,a2),d2=a2-min(a0,a1);
float bw=max(TerrainCellBlendRad,1e-5);
float w0=smoothstep(-bw,bw,-d0),w1=smoothstep(-bw,bw,-d1),w2=smoothstep(-bw,bw,-d2); float ws=max(w0+w1+w2,1e-5); w0/=ws;w1/=ws;w2/=ws;
float4 t0=SurfaceTerrainLUT.Load(int3(c0,0,0)),t1=SurfaceTerrainLUT.Load(int3(c1,0,0)),t2=SurfaceTerrainLUT.Load(int3(c2,0,0)); float4 terrain=t0*w0+t1*w1+t2*w2;
float mountain=smoothstep(.74,.98,terrain.r),forest=smoothstep(.24,.51,terrain.r)*(1-mountain),plain=saturate(1-forest-mountain);
float3 p=WorldPos*max(TerrainDetailFrequency,1e-6); float n=frac(sin(dot(p.xy+p.z,float2(12.9898,78.233)))*43758.5453); float g=frac(sin(dot(p.yz+p.x*1.73,float2(39.346,11.135)))*24634.6345);
float3 base=(PlainColor*plain+ForestColor*forest+MountainColor*mountain)*(1+(terrain.g*2-1)*.10+(n-.5)*.12); base=lerp(base,base*float3(.72,.70,.66),mountain*smoothstep(.42,.78,g+n*.45)*.65);
float riverSdf=1e6, flowCoord=0; float3 flowDir=float3(0,0,0);
[loop] for(int i=0;i<256;i++){ if(i>=min((int)RiverSegmentCount,256)) break; float4 sa=SurfaceRiverSegmentLUT.Load(int3(i*2,0,0)),sb=SurfaceRiverSegmentLUT.Load(int3(i*2+1,0,0)); float3 A=normalize(sa.rgb),B=normalize(sb.rgb),N=normalize(cross(A,B)); float3 q=normalize(dir-N*dot(dir,N)); float arc=acos(clamp(dot(A,B),-1,1)); float aq=acos(clamp(dot(A,q),-1,1)),qb=acos(clamp(dot(q,B),-1,1)); float onArc=step(aq+qb,arc+1e-4); float da=acos(clamp(dot(dir,A),-1,1)),db=acos(clamp(dot(dir,B),-1,1)); float dist=lerp(min(da,db),acos(clamp(dot(dir,q),-1,1)),onArc); float t=saturate(aq/max(arc,1e-5)); float segmentSdf=dist-lerp(sa.a,sb.a,t); if(segmentSdf<riverSdf){ riverSdf=segmentSdf; flowCoord=aq; flowDir=normalize(B-dir*dot(B,dir)); } }
[loop] for(int i=0;i<16;i++){ if(i>=min((int)RiverLakeCount,16)) break; float4 lc=SurfaceRiverLakeLUT.Load(int3(i*2,0,0)),la=SurfaceRiverLakeLUT.Load(int3(i*2+1,0,0)); float3 C=normalize(lc.rgb),X=normalize(la.rgb),Y=normalize(cross(C,X)); float centerAngle=acos(clamp(dot(dir,C),-1,1)); float localGate=step(centerAngle,max(lc.a,la.a)*2.5); float2 uv=float2(asin(clamp(dot(dir,X),-1,1))/max(lc.a,1e-5),asin(clamp(dot(dir,Y),-1,1))/max(la.a,1e-5)); float lakeSdf=lerp(1e6,(length(uv)-1)*min(lc.a,la.a),localGate); if(lakeSdf<riverSdf){ riverSdf=lakeSdf; flowDir=float3(0,0,0); } }
float water=saturate(1-smoothstep(-.0005,.0008,riverSdf)); float wet=saturate(1-smoothstep(0,max(.0001,RiverWetWidth*.002),riverSdf)); base=lerp(base,RiverWetColor,wet*.45); base=lerp(base,RiverColor,water);
float4 h0=SurfaceHighlightLUT.Load(int3(c0,0,0)),h1=SurfaceHighlightLUT.Load(int3(c1,0,0)),h2=SurfaceHighlightLUT.Load(int3(c2,0,0)); float pad=max(HighlightPaddingRad,1e-5); float e0=(1-smoothstep(0,pad,-d0))*step(0,-d0),e1=(1-smoothstep(0,pad,-d1))*step(0,-d1),e2=(1-smoothstep(0,pad,-d2))*step(0,-d2); float3 hi=e0*h0.rgb*h0.a+e1*h1.rgb*h1.a+e2*h2.rgb*h2.a;
OutEmissive=hi*HighlightStrength+water*RiverColor*RiverHighlightStrength; OutRoughness=lerp(saturate(lerp(.82,terrain.a,.80)+mountain*.08),.18,water); OutMetallic=0; OutWaterMask=water; OutFlowWorld=flowDir; OutFlowCoord=flowCoord; return saturate(base);
```

### 3.1 切线空间流水法线节点

在同一材质图中增加以下节点与参数：

| 节点 | 配置/连接 |
| --- | --- |
| `TransformVector` | Source=`World`，Destination=`Tangent`；输入 `OutFlowWorld`。 |
| `ComponentMask` | 取 TransformVector 输出的 `R,G`。 |
| `Normalize` | 输入为上述 `R,G`，得到 `FlowXY`。 |
| `Time` | 勾选 `Ignore Pause`，接入小 Custom 的 `TimeSeconds`。 |
| Scalar Parameter | `WaterWaveAmplitude=0.12`。 |
| Scalar Parameter | `WaterWaveFrequency=180.0`。 |
| Scalar Parameter | `WaterWaveSpeed=1.6`。 |
| Scalar Parameter | `WaterCrossWaveRatio=0.45`。 |
| Custom | 输出 `Float3`，下表输入，输出直连材质 `Normal`。 |

小 Custom 节点输入：`FlowXY`(Float2)、`WaterMask`(Float1)、`FlowCoord`(Float1)、`TimeSeconds`(Float1)、`Amplitude`(Float1)、`Frequency`(Float1)、`Speed`(Float1)、`CrossRatio`(Float1)。分别连接上文的 `Normalize`、`OutWaterMask`、`OutFlowCoord`、`Time` 与四个参数。

```hlsl
float wave = sin(FlowCoord * Frequency - TimeSeconds * Speed);
float2 side = float2(-FlowXY.y, FlowXY.x);
float2 xy = (FlowXY * wave + side * cos(FlowCoord * Frequency * 1.37 - TimeSeconds * Speed * 0.73) * CrossRatio)
    * Amplitude
    * WaterMask;
float z = sqrt(saturate(1.0 - dot(xy, xy)));
return float3(xy, z);
```

`WaterMask=0` 时该节点严格返回 `(0,0,1)`，因此非水区域仍使用网格原有的切线空间顶点法线。湖泊的 `OutFlowWorld` 为零，会保持静水而不获得单向流水波纹。

## 4. 验收

1. 河网在高地发源、持续向更低 Cell 前进并自然汇流，不影响点击或移动。
2. 曲线河道无明显直线分叉；下游和汇流处宽度增加。
3. 每个终点都有受尺寸上限保护的小型湖泊，河道与湖泊视觉连续。
4. hover/选中高亮、SV3 地表材质、SV4.5 高度、棋子和相机行为均不回归。
