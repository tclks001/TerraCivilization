// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CellHighlightComponent.generated.h"

class APlanetBinder;

/**
 * UCellHighlightComponent
 *
 * Step 4 阶段最小高亮渲染组件：
 * - 接收一个 CellId，提取该 Cell 在球面上的多边形边界（5 或 6 条边）；
 * - 每条边按"测地线 Slerp"采样若干段，得到细密的单位向量序列；
 * - 直接乘以 Radius + 球心偏移，然后用 ULineBatchComponent::DrawLine 画出。
 *
 * 设计要点（详见 Docs/HexHighlightInteractionPlan.md 5.3 ~ 5.6）：
 * - 假设 FSphereTopology 已经做过"环对齐"：FCell.CornerIds[i] / CornerIds[(i+1)%N]
 *   就是第 i 条边的两个端点，无须再做 OrderCellCorners 闭环排序。
 * - 不做高度对齐（Step 5 才做）。当前直接绘制在名义半径球面上，
 *   会与 PTG 的真实地形发生穿插，这是预期行为。
 * - 用 SDPG_Foreground，确保线在地形之外也能看见。
 *
 * 选择 UActorComponent 而非 USceneComponent：本组件不持有 Transform，
 * 所有几何信息都从 APlanetBinder->GetPtgManager() 反取，不需要参与场景层级。
 */
UCLASS(ClassGroup = (Planet), meta = (BlueprintSpawnableComponent))
class TERRACIVILIZATION_API UCellHighlightComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCellHighlightComponent();

    /** Hover 高亮的颜色。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Highlight")
    FLinearColor HighlightColor = FLinearColor(1.0f, 0.85f, 0.1f, 1.0f);

    /** 五边形 Cell 的高亮颜色（一般和六边形不同色，便于一眼识别 12 个特殊点）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Highlight")
    FLinearColor PentagonHighlightColor = FLinearColor(0.0f, 1.0f, 1.0f, 1.0f);

    /** 每条 Cell 边的 Slerp 采样段数（越大越光滑）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Highlight", meta = (ClampMin = "1", ClampMax = "64"))
    int32 SegmentsPerEdge = 10;

    /** 线条像素厚度（DepthPriorityGroup = SDPG_Foreground 下不是真的像素，仅作参考）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Highlight")
    float Thickness = 3.0f;

    /** 在名义球面之上额外抬起的偏移（cm），避免与 PTG 真实地形 Z-Fighting。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Highlight")
    float LiftOffset = 50.0f;

    /**
     * 让 LineBatcher 持续显示线条的时间（秒）。
     * Step 4 用 LineBatcher::DrawLine + 短 LifeTime 的方式，
     * 配合 hover 频次（每帧或仅 CellId 变化时）实现"覆盖式"刷新，
     * 不需要严格的 Flush 管理。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Highlight")
    float LineLifeTime = 0.15f;

    /** 设置高亮的 Cell；CellId == INDEX_NONE 等价于 ClearHighlight()。 */
    void SetHighlightedCell(int32 CellId);

    /** 清空高亮（把内部缓存的 CellId 也清掉，下一次 SetHighlightedCell 即使是同 ID 也会重画）。 */
    void ClearHighlight();

private:
    /** 沿 CurrentCellId 的 Cell 边界绘制一帧线条；不做"是否变化"判断。 */
    void RebuildLines();

    /**
     * 在以 N 为外法向的"球面大圆"上做 Slerp。
     * 要求 A、B 都是单位向量。返回值仍是单位向量。
     */
    static FVector SlerpUnit(const FVector& A, const FVector& B, float T);

    /** 找到本组件挂载到的 APlanetBinder，失败返回 nullptr。 */
    APlanetBinder* GetBinder() const;

    /** 上一次绘制的 CellId，用于"同 ID 不重画"的微优化。 */
    int32 CurrentCellId = INDEX_NONE;
};
