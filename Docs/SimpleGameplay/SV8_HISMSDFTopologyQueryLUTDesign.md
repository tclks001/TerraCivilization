# SV8：HISM + SDF GPU 球面拓扑查询 LUT 设计稿

> 编码：UTF-8，简体中文。前置：SV6-B、SV7 已验收。后续主线见 [HISMSDFTerrainVisualPresentationDesign.md](HISMSDFTerrainVisualPresentationDesign.md)。

## 1. 目标与边界

SV8 移除 SV6-B 材质对 HISM Owner Cell 与六邻居 PICD 的依赖。材质以像素的球面方向直接遍历 GPU 上的球面三角树，精确复现 CPU `FSphereTopologyQuery::FindNearestCell` 的 root/child/leaf 选择过程，得到当前方向所在叶三角形的三个 CellId。

这使任意地形 HISM 实例都可跨越多个 Cell：Ridge、Cliff、Peak 或未来的平原/森林基础块不需要预写 Owner Cell、邻居集合或实例级 Cell 上下文，材质仍能在任意像素得到正确的地形类别、SDF 边界、河网和高亮。

本阶段不改变：

- SV7 CPU 侧 `ImpactPoint -> FindNearestCell` 点击与 hover 查询；
- Gameplay、棋子规则、UI、WorldGen 和 HISM Transform；
- SV6-B/SV6-A 的 BaseColor、Roughness、法线、水波和河网后续节点；
- 旧 `LegacyHISMDebug` 与 `ContinuousSurface` 的现有兼容路径。

## 2. CPU/GPU 同构查询

现有 CPU `FSphereTopologyQuery::FindNearestCell` 采用固定球面三角树：

```text
Dir
 -> 20 个 Root 的 Center·Dir 最大者
 -> 每层 4 个 Child 的 Center·Dir 最大者
 -> 叶节点 CellIds[0..2] 的 UnitCenter·Dir 最大者
 -> CellId
```

SV8 将同一棵 `FSphereTopology::TriTreeRoots` 序列化为两张一维 LUT：

| 参数/资源 | 内容 | 格式 |
| --- | --- | --- |
| `SurfaceTopologyNodeCenterLUT` | 所有 TriTree Node 的 `Center.xyz`，按层宽度优先排列 | `PF_A32B32G32R32F` |
| `SurfaceTopologyLeafCellLUT` | 每个叶节点的 `CellIds[0..2]`，以 RGB 精确 float 整数存放 | `PF_A32B32G32R32F` |
| `TopologySubdivisionLevel` | 逻辑 `CellTopology` 的细分层级，默认 `3` | Scalar |
| `TopologyRootCount` | 根节点数量，当前固定 `20` | Scalar |

节点顺序必须严格等于 CPU 树顺序：根节点沿 `TriTreeRoots[0..19]` 排列；下一层依次追加前一层每个节点的 `Children[0..3]`。对第 `L` 层：

```text
LevelOffset(L) = RootCount * (4^L - 1) / 3
ChildLocalIndex = ParentLocalIndex * 4 + ChildIndex
```

默认 `CellSubdivisionLevel=3` 时，查询成本为 `20 + 3*4 + 3` 次 dot 比较，且只有固定数量 LUT `Load`；禁止在像素 shader 中扫描 642 个 Cell。

## 3. C++ 实现

`UTerrainVisualSurfaceComponent::InitializeTopologyQueryResources` 在每次 CellTopology 重建时：

1. 以广度优先顺序走 `TriTreeRoots -> Children[0..3]`。
2. 写出 Node Center 与叶子三 CellId。
3. 创建最近采样、关闭 sRGB、无 mip 的两张 transient float LUT。
4. 用 256 个确定性方向重新执行“序列化数组版”查询，并与真实 `FSphereTopologyQuery::FindNearestCell` 比对；任一不一致即拒绝资源初始化。
5. 向连续表面 MID 与 HISM + SDF MID 注入两张 LUT、`TopologySubdivisionLevel`、`TopologyRootCount`。

SV8 后，`FPlanetHISMTileRenderer` 的 Per Instance Custom Data 从 11 个恢复为旧的 4 个高亮 float；不再写 Owner Cell 或邻居 Cell。HISM + SDF 主材质不得读取 PICD `4..10`。

`HISMSDFExperiment` 只有在 Highlight LUT 和 Topology Query LUT 均成功初始化时才激活，以避免未绑定拓扑资源时材质错误渲染。

### 3.1 山脊测地线中点验证 HISM

为直接验证“无需实例 Cell 上下文的跨资产材质查询”，Actor 新增独立的 `SV8VerificationRidgeMidpointHISMComp`：

- 对 WorldGen/Gameplay 权威 `MountainRidgeSegments` 的每一段，读取两端 Cell 中心单位向量 `A/B`，以 `normalize(A + B)` 求球面大圆短弧的中点方向。
- 在每条段的测地线中点新增一个 Mountain Static Mesh 实例；局部 `+Z` 使用 `FQuat::FindBetweenNormals(Up, MidpointUnit)` 对齐径向方向。
- 变换沿用主 Tile HISM 的 `TargetRadius = GlobeRadiusCM + HISMTileRadiusOffsetCM`、`SourceRadius` 与统一缩放，因此资产基础高度仍处于球面半径，不增加径向偏移。
- 组件只在 `HISMSDFExperiment` 可见，强制 `NoCollision`，不参与 SV7 点击、P2.5 棋子高度或旧 InstanceId 映射。
- 它绑定同一份 SV8 HISM + SDF MID，因此无需 Owner/Neighbor PICD 即会根据每个像素的世界方向自动和周围地形融合。

Actor Details 参数：

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `bEnableSV8RidgeMidpointVerificationMeshes` | `true` | 显示/生成山脊测地线中点验证实例。 |
| `SV8VerificationMountainStaticMesh` | 空 | 验证资产；为空时回退到 `MountainTileStaticMesh`。 |

验证资产现在位于两个 Cell 之间，不会与单个原始 Tile 完全共面。为更直观检验融合，可指定一个遵守同一球面 Tile 坐标契约、但轮廓不同的山体变体 Static Mesh。

## 4. 从 SV6-B 材质迁移到 SV8

以当前已验收 `M_TerrainVisual_HISMSDFExperiment` 为输入，不新建其它 PBR 节点。建议先 Duplicate 为 `M_TerrainVisual_HISMSDF_SV8`，完成验收后再决定是否替换旧资产。

### 4.1 删除旧 PICD 上下文

在主材质图中找到 SV6-B 主 Custom 左侧的 7 组：

```text
PerInstanceCustomData(Data Index 4..10)
    -> VertexInterpolator
    -> OwnerCell / Neighbor0..Neighbor5
```

1. 删除这 7 个 `PerInstanceCustomData` 节点。
2. 删除相连的 7 个 `VertexInterpolator`。
3. 在主 Custom Details 的 Inputs 中删除以下 7 行：

```text
OwnerCell
Neighbor0
Neighbor1
Neighbor2
Neighbor3
Neighbor4
Neighbor5
```

不要删除旧 PICD `0..3` 的节点以外的任何已有节点；SV6-B 材质本身不应读取 `0..3`，而它们仍保留给 Legacy HISM Debug 的旧实例材质。

### 4.2 新增拓扑 LUT 参数

创建两个 `Texture Object Parameter`：

| 参数名 | Sampler Type | 默认纹理 |
| --- | --- | --- |
| `SurfaceTopologyNodeCenterLUT` | `Linear Color` | 可留空；运行时 C++ 注入 transient LUT。 |
| `SurfaceTopologyLeafCellLUT` | `Linear Color` | 可留空；运行时 C++ 注入 transient LUT。 |

再创建两个 `Scalar Parameter`：

| 参数名 | 默认值 | 说明 |
| --- | ---: | --- |
| `TopologySubdivisionLevel` | `3` | C++ 依据 CellTopology 覆盖。 |
| `TopologyRootCount` | `20` | C++ 依据 TriTreeRoots 覆盖。 |

把两张 Texture Object 和两个 Scalar 接入主 Custom。保留 SV6-B 已有的 `SurfaceCellDirectionLUT`、`SurfaceTerrainLUT`、`SurfaceHighlightLUT`、River LUT、`SurfaceCellCount` 和其他输入不变。

### 4.3 主 Custom 的输入清单

SV8 主 Custom 不再有任何 `PerInstanceCustomData` 输入。最终输入为：

| 输入 | 类型/来源 |
| --- | --- |
| `WorldPos` | Float3，Absolute World Position（Absolute）。 |
| `PlanetCenter` | Float3，Vector Parameter。 |
| `BaseNormalWS` | Float3，VertexNormalWS。 |
| `SurfaceCellCount` | Float1，C++ 注入。 |
| `HISMSDFProjectionRadiusCM` | Float1，C++ 注入。 |
| `TopologySubdivisionLevel` / `TopologyRootCount` | Float1，新增 Scalar Parameter，C++ 注入。 |
| `SurfaceTopologyNodeCenterLUT` / `SurfaceTopologyLeafCellLUT` | Texture2D，新增 Texture Object Parameter，C++ 注入。 |
| `SurfaceCellDirectionLUT` / `SurfaceTerrainLUT` / `SurfaceHighlightLUT` | Texture2D，沿用 SV6-B。 |
| `SurfaceRiverSegmentLUT` / `SurfaceRiverLakeLUT` | Texture2D，沿用 SV6-B。 |
| `TerrainCellBlendRad` / `HighlightPaddingRad` / `HighlightStrength` | Float1，沿用 SV6-B。 |
| `RiverSegmentCount` / `RiverLakeCount` / `RiverWetWidth` | Float1，沿用 SV6-B。 |
| `RiverColor` | Float3，沿用 SV6-B。 |
| `RiverHighlightStrength` | Float1，沿用 SV6-B。 |

Additional Outputs 与 SV6-B 完全相同：

```text
OutPlainMask, OutForestMask, OutRockMask,
OutWetMask, OutWaterMask,
OutFlowWorld, OutFlowCoord, OutEmissive,
OutProjectionWorldPos, OutProjectionNormalWS
```

### 4.4 替换完整主 Custom HLSL

将 SV6-B 主 Custom 的 Code 全量替换为下面代码。它的变化仅在开头：从 Topology LUT 找到叶子三 CellId，再按与 SV6-B 相同的距离排序、one-hot 分类、河网和高亮流程继续计算。

```hlsl
float3 dir=normalize(WorldPos-PlanetCenter);
int cellCount=max((int)round(SurfaceCellCount),1),rootCount=max((int)round(TopologyRootCount),1),topologySub=clamp((int)round(TopologySubdivisionLevel),0,6);
int localNode=0;float bestDot=-2;
[loop]for(int root=0;root<20;root++)
{
    if(root>=rootCount)break;
    float3 center=normalize(SurfaceTopologyNodeCenterLUT.Load(int3(root,0,0)).rgb);
    float d=dot(center,dir);
    if(d>bestDot){bestDot=d;localNode=root;}
}
int levelOffset=0,levelNodeCount=rootCount;
[loop]for(int level=0;level<6;level++)
{
    if(level>=topologySub)break;
    int nextLevelOffset=levelOffset+levelNodeCount,childBase=localNode*4,bestChild=0;bestDot=-2;
    [unroll]for(int child=0;child<4;child++)
    {
        float3 center=normalize(SurfaceTopologyNodeCenterLUT.Load(int3(nextLevelOffset+childBase+child,0,0)).rgb);
        float d=dot(center,dir);
        if(d>bestDot){bestDot=d;bestChild=child;}
    }
    localNode=childBase+bestChild;levelOffset=nextLevelOffset;levelNodeCount*=4;
}
float3 packedLeaf=SurfaceTopologyLeafCellLUT.Load(int3(localNode,0,0)).rgb;
int c0=clamp((int)round(packedLeaf.r),0,cellCount-1),c1=clamp((int)round(packedLeaf.g),0,cellCount-1),c2=clamp((int)round(packedLeaf.b),0,cellCount-1);
float3 v0=normalize(SurfaceCellDirectionLUT.Load(int3(c0,0,0)).rgb),v1=normalize(SurfaceCellDirectionLUT.Load(int3(c1,0,0)).rgb),v2=normalize(SurfaceCellDirectionLUT.Load(int3(c2,0,0)).rgb);
float a0=acos(clamp(dot(dir,v0),-1,1)),a1=acos(clamp(dot(dir,v1),-1,1)),a2=acos(clamp(dot(dir,v2),-1,1));
if(a1<a0){float ta=a0;a0=a1;a1=ta;int tc=c0;c0=c1;c1=tc;float3 tv=v0;v0=v1;v1=tv;}if(a2<a1){float ta=a1;a1=a2;a2=ta;int tc=c1;c1=c2;c2=tc;float3 tv=v1;v1=v2;v2=tv;}if(a1<a0){float ta=a0;a0=a1;a1=ta;int tc=c0;c0=c1;c1=tc;float3 tv=v0;v0=v1;v1=tv;}
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

### 4.5 保持不变的后续节点

以下内容不修改，仅确认它们继续接主 Custom 的同名输出：

1. Base Color Custom：`WorldPos=OutProjectionWorldPos`，`NormalWS=OutProjectionNormalWS`。
2. Roughness Custom：同样使用 `OutProjectionWorldPos/OutProjectionNormalWS`，沿用 SV6-A.1 的 Min/Max 与 Specular 参数。
3. 三组 `WorldAlignedNormal`：`Normal` 继续接 `OutProjectionNormalWS`。
4. SV5 水波：`WorldNormal` 继续接 `OutProjectionNormalWS`，其 Flow/Water 输入继续接主 Custom 输出。
5. `OutEmissive` 继续接材质 `Emissive Color`；不要接旧 PICD `0..3`。

## 5. 参数注入与日志

SV8 不需要在 Actor Details 新增可调参数。C++ 自动向 HISM MID 写入：

```text
SurfaceTopologyNodeCenterLUT
SurfaceTopologyLeafCellLUT
TopologySubdivisionLevel
TopologyRootCount
```

在 Output Log 搜索：

```text
[TerrainVisual][SV8] Topology Query LUT ready.
```

开启测地线中点验证后还应看到：

```text
[TerrainVisual][SV8] Rebuilt ridge midpoint verification HISM.
```

默认 CellSub=3 期望看到：`Roots=20`、`Nodes=1700`、`Leafs=1280`、`VerifySamples=256`。日志出现 verification failed 时，HISM + SDF 模式会拒绝激活，必须先修复 CPU/GPU 树顺序不一致。

## 6. 验收

1. 编译后 Output Log 出现 SV8 LUT ready，且没有 verification failed。
2. 材质图不再含 Data Index `4..10` 的 PICD 节点或 `OwnerCell/Neighbor*` Custom 输入。
3. 切到 `HISMSDFExperiment` 后，Plain/Forest/Mountain 颜色、河网、粗糙度、高亮与 SV6-B 视觉基线一致。
4. 关闭、重建或更换 HISM 实例顺序后，同一世界方向的材质类别不发生变化。
5. 预备跨 Cell HISM 资产后，资产表面任意位置仍能取得正确的 SDF 地形与高亮；不要求为资产补充 Cell PICD。
6. SV7 空间投影 hover/click、Legacy HISM Debug、ContinuousSurface 不回归。
7. 开启 `bEnableSV8RidgeMidpointVerificationMeshes` 后，每条 MountainRidgeSegment 的两个 Cell 中心之间出现一个额外、无碰撞的 Mountain HISM；其颜色、粗糙度、河网和高亮与两侧山体无实例边界色差。

## 7. 排错

| 现象 | 根因 | 修复/检查 |
| --- | --- | --- |
| 材质全黑或类别错乱 | 两张 Topology LUT 未注入，或主 Custom 输入名拼写不一致 | 检查 Actor 材质槽、SV8 ready 日志及四个新增参数名。 |
| 材质编译报 `OwnerCell` 未定义 | 删除了节点但未替换旧 SV6-B Code | 使用第 4.4 节完整 HLSL。 |
| 所有区域变成同类地形 | Leaf LUT 未使用或 `TopologySubdivisionLevel` 为 0 | 检查两个 Scalar 默认值、MID 注入和主 Custom 输入连线。 |
| 画面与 SV6-B 相比有边界偏移 | Node/Leaf LUT 资源来自不同 CellTopology | 重新触发 Actor Rebuild；日志必须通过 256 方向验证。 |
| HISM + SDF 模式无法激活 | 拓扑 LUT 创建或验证失败 | 检查 `[TerrainVisual][SV8]` Error；模式会有意回退为旧材质而非带错误 LUT 渲染。 |
| Legacy 高亮或棋子高度异常 | 修改了 `TryResolveHISMHitToCellId` 的实例反查语义 | SV8 不应修改该接口；它仍供 Legacy/P2.5 使用。 |
| 验证 HISM 抢走点击或棋子高度 | 验证组件启用了碰撞 | `SV8VerificationRidgeMidpointHISMComp` 必须保持 `NoCollision`；不要改为地形外壳。 |
| 看不到额外资产 | 当前 WorldGen 没有 MountainRidgeSegment，或验证 Mesh 被周围地形遮挡 | 检查 SV8 midpoint 日志中的 Segments/Instances；必要时为 `SV8VerificationMountainStaticMesh` 指定轮廓更明显的同契约山体变体。 |
