// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class FSphereTopology;

/**
 * FMeshDisplacementBuilder
 *
 * T 阶段（TessellatedMesh）—— 顶点位移预计算器（详见 Docs/TessellatedMeshDesign.md §2.2 / §3）。
 *
 * 职责：
 *   1) 为 MeshTopology 的每个 mesh 顶点 v 找出"它所属的全部粗 CellTopology 三角形索引列表"
 *      （5 或 6 个；若顶点恰位于 Cell 内部时可能去重为 1 个）。
 *   2) 给定 cell elevation 数组，按 D3 三 Cell `dot` 权重线性插值 + D4 多重三角形算术平均，
 *      算出 mesh 各顶点的最终径向偏移（cm）。
 *
 * T1 验收范围：
 *   - 仅完成构造 / 析构 / 字段持有 / 空方法骨架。
 *   - BuildVertexToCoarseTris() / ComputeVertexElevationCM() 实际算法体留给 T2。
 *
 * 设计稿锚点：
 *   - D3：w_i = max(0, dot(Dir, UnitCenter_i)) 归一化（线性插值）
 *   - D4：mesh 顶点可属多个粗 Tri → 各算一次 → 算术平均
 *   - D5：径向位移不改变法线方向（仍朝外），Tangent 留空
 */
class TERRACIVILIZATION_API FMeshDisplacementBuilder
{
public:
    FMeshDisplacementBuilder(const FSphereTopology* InCellTopo,
                             const FSphereTopology* InMeshTopo);
    ~FMeshDisplacementBuilder();

    /**
     * 一次性预计算：为 MeshTopo 中每个 mesh 顶点 v，列出它所属的全部粗 CellTopo 叶子三角形索引。
     * 算法详见 Docs/TessellatedMeshDesign.md §3.3 / §3.4。
     *
     * T2 已实现：
     *   1、5 / 6 个 `Corner` × `PrimalTriTreeNodes` 拿叶子节点。
     *   2、向上爬升 (MeshSub - CellSub) 次。
     *   3、父节点 `CellIds[0..2]` 在 CellTopo->PrimalTris 中反查粗 Tri 索引。
     *   4、用 `AddUnique` 去重（同一顶点多个 Corner 可能爬升到同一个粗 Tri）。
     */
    void BuildVertexToCoarseTris();

    /**
     * 给定 CellElevation[NumCells]（[-1, 1] 量纲）+ ElevationScaleCM（cm），
     * 算出 OutVertexElevationCM[NumMeshVerts]。
     *
     * T2 已实现：
     *   - D3 三点 dot 权重线性插值（各粗 Tri 独立归一化）
     *   - D4 多重归属算术平均
     *   - D9 fbm 微细节接口 ComputeMicroDetail 预留但为空体
     */
    void ComputeVertexElevationCM(
        const TArray<float>& CellElevation,
        float ElevationScaleCM,
        TArray<float>& OutVertexElevationCM) const;

    /**
     * fbm 高频细节叠加（D9）。T 阶段主验收暂不接入；接口预留供后续子里程碑实现。
     */
    void ComputeMicroDetail(TArray<float>& InOutVertexElevationCM) const {}

    /**
     * T2 验收自检（设计稿 §5.1）：
     *   - 任机采样 100 个 mesh 顶点 × 其首个粗 Tri，检查 dot 权重归一化后的和 ∈ [0.99, 1.01]。
     *   - 检查所有 mesh 顶点的 VertexToCoarseTriIds[v].Num() ∈ [1, 6]。
     *   - 验收信息通过 Output Log 打印，供上层 actor 调用。
     * 返回验收是否全部通过。
     */
    bool RunSelfCheckT2(int32 RandomSampleCount = 100, int32 RandomSeed = 0xC1FFEE) const;

    /** 已构建后的查询：mesh 顶点 v 命中的粗 CellTopology 三角形索引列表（5/6 个，可能去重）。 */
    const TArray<TArray<int32>>& GetVertexToCoarseTriIds() const { return VertexToCoarseTriIds; }

    int32 GetNumMeshVerts() const;
    int32 GetNumCells() const;

private:
    /** sub=CellSubdivisionLevel 的逻辑拓扑（外部持有，本类只读）。 */
    const FSphereTopology* CellTopo = nullptr;

    /** sub=MeshSubdivisionLevel 的渲染拓扑（外部持有，本类只读）。 */
    const FSphereTopology* MeshTopo = nullptr;

    /** 每个 mesh 顶点 v 所属的"粗 CellTopology 叶子三角形"索引列表。 */
    TArray<TArray<int32>> VertexToCoarseTriIds;

    /**
     * 预建 LUT：排序三元组 (a, b, c) → CellTopology 中粗 PrimalTri 的索引。
     * 设计稿 §3.4 末尾推荐的“实现一次 O(NumPrimalTris) 查询 O(log)”。
     * 在 BuildVertexToCoarseTris 顶部一次性填充。
     */
    TMap<uint64, int32> CoarseTriIdByCellTriple;

    /** 将三个不同的 cell 索引打包为 uint64 key（先排序、跨 sub<=5 不会越 2^21）。 */
    static uint64 PackTripleKey_(int32 A, int32 B, int32 C);

    /** 在 CellTopology->PrimalTris 中查找以 (a, b, c) 为顶点的 Tri 索引；INDEX_NONE = 不存在。 */
    int32 FindCoarseTriIdByCells_(int32 A, int32 B, int32 C) const;

    /** 在 CellTopology->PrimalTris 遭一遍，填充 CoarseTriIdByCellTriple。 */
    void RebuildCoarseTriLUT_();
};
