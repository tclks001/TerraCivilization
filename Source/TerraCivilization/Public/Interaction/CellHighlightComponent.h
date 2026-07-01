// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CellHighlightComponent.generated.h"

class APlanetBinder;
class APlanetTessellatedMesh;
class UTexture2D;

/**
 * UCellHighlightComponent
 *
 * R11 阶段 hex/pent 高亮组件 —— **hover 与 select 共享同一套 §15 描边带管线**。
 *
 * 设计要点（详见 Docs/HexHighlightInteractionPlan.md）：
 * - 不再使用 ULineBatchComponent 画测地线折线（旧方案已弃用，附录 A 存档）；
 * - 持有 1×NumCells 的 R8G8 `CellHighlightLUT`：
 *     · R 通道 = HoverIntensity（0 或 255，二值切换）
 *     · G 通道 = SelectIntensity（0 或 255，toggle）
 * - hover 走 4 状态 / 6 事件状态机（§7），LUT 写入仅发生在状态切换瞬间；
 * - select 不进状态机，左键点击直接 ToggleSelected；
 * - 颜色 / 描边宽度 / 全局强度通过 MID 全局参数注入，在 APlanetTessellatedMesh
 *   端集中管理；本组件只负责"哪 cell 该亮"。
 *
 * Tick 必须开（驱动 HoverFadeTimer 倒计时）；CPU 端单帧成本 < 1 µs（不切换时）。
 *
 * 选择 UActorComponent 而非 USceneComponent：本组件不持有 Transform，
 * 所有几何 / 拓扑信息都从 APlanetBinder->GetTessellatedMesh() 反取，不需要参与场景层级。
 */
UCLASS(ClassGroup = (Planet), meta = (BlueprintSpawnableComponent))
class TERRACIVILIZATION_API UCellHighlightComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCellHighlightComponent();

    /**
     * 防抖时长（秒）：鼠标移出当前 hover cell（且没有立即进入新 cell）后，
     * 描边继续保留多久才彻底淡掉。鼠标在 cells 之间快速划动时**不**走防抖
     * （即时切换路径）；防抖仅在"鼠标离开整个球"时生效。
     *
     * 同 0.5s 内若鼠标重新指回原 cell，描边视觉完全无变化（LUT 不写、状态机
     * 走 evt 3 快路径）。详见 §7 状态机。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Highlight",
              meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float HoverFadeDuration = 0.5f;

    //~ UActorComponent
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;
    //~ End UActorComponent

    /**
     * 由 APlanetInteractionController::PlayerTick 每帧调用（即使鼠标没动）。
     *
     * @param NewCellId 鼠标射线命中并球面 Voronoi 解出的 cell id；
     *                  传 INDEX_NONE 表示鼠标当前不在球上。
     */
    void UpdateHover(int32 NewCellId);

    /** 等价于 UpdateHover(INDEX_NONE)；语义上更明确，由 OnLeavePlanet 调用。 */
    void ClearHover();

    /** 显式设 / 清除某 cell 的 select 状态，立即写 LUT。 */
    void SetSelected(int32 CellId, bool bSelected);

    /** 翻转 select 状态（已选则取消，未选则选中）。 */
    void ToggleSelected(int32 CellId);

    /** 强制清空 hover + 全部 select；EndPlay 也调一次。 */
    void ClearAll();

    /** 由 APlanetTessellatedMesh::Rebuild() 后从 ApplyTerrainMaterial_ 反向查询
     *  LUT 指针重新塞进新 MID 时使用。 */
    UTexture2D* GetHighlightLUT() const { return HighlightLUT; }

private:
    /** 找到挂在同 actor 的 APlanetBinder。失败返回 nullptr。 */
    APlanetBinder* GetBinder() const;

    /**
     * 创建 1×NumCells / PF_R8G8 的 transient texture，并把 LUT 指针塞给
     * APlanetTessellatedMesh::SetHighlightLUT。重复调用安全（已建好则直接返回）。
     *
     * @param OutNumCells 返回 LUT 行数（= CellTopology->Cells.Num()）。
     * @return true 表示 LUT 已就绪；false 表示依赖未齐（Binder/Tess 未挂）。
     */
    bool EnsureLUTCreated_(int32& OutNumCells);

    /**
     * 重新计算 CellId 那一行的 (R, G) byte，写入 CPU 镜像，再调
     * UpdateTextureRegions(1×1) 投递 RHI 上传。
     *
     * R = (CurrentHoverCell == CellId) ? 255 : 0
     * G = SelectedCells.Contains(CellId) ? 255 : 0
     */
    void WriteLUTPixel_(int32 CellId);

    /** 持有 LUT 资源；EndPlay 由 GC 自动回收。 */
    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> HighlightLUT;

    /**
     * CPU 端镜像：每 cell 一个 uint16
     *   bit 0..7   = HoverIntensity (0/255)
     *   bit 8..15  = SelectIntensity (0/255)
     *
     * GPU 端 LUT 是 PF_R8G8，每像素 2 byte，与本镜像 byte 序一致。
     */
    TArray<uint16> LUTCpuMirror;

    //----------------------------------------------------------
    // §7 Hover 状态机字段
    //----------------------------------------------------------

    /** LUT 上当前 R 通道为 255 的 cell id（INDEX_NONE 表示无 hover 描边显示中）。 */
    int32 CurrentHoverCell = INDEX_NONE;

    /** 鼠标真实位置目前指向的 cell（与 Current 不同时表示在 fade-out 缓冲期）。 */
    int32 PendingHoverCell = INDEX_NONE;

    /** > 0 表示 Current cell 处于 fade-out 倒计时；为 0 时 Current = Pending。 */
    float HoverFadeTimer = 0.0f;

    /** 多选集合（R11 仅做单选交互，但接口上保留 Set，未来扩展按 Shift 多选用）。 */
    TSet<int32> SelectedCells;
};
