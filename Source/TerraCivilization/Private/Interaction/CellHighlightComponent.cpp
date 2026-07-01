// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/CellHighlightComponent.h"

#include "Interaction/PlanetBinder.h"
#include "Render/PlanetTessellatedMesh.h"

#include "FCell.h"
#include "FSphereTopology.h"

#include "Engine/Texture2D.h"
#include "PixelFormat.h"
#include "RHITypes.h"   // FUpdateTextureRegion2D（被 Engine/Texture2D.h 间接包含，但显式 include 防御未来重构）
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogCellHighlight, Log, All);

UCellHighlightComponent::UCellHighlightComponent()
{
    // R11：必须 tick 来驱动 HoverFadeTimer 倒计时（详见 §7 evt 5）。
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

APlanetBinder* UCellHighlightComponent::GetBinder() const
{
    return Cast<APlanetBinder>(GetOwner());
}

void UCellHighlightComponent::BeginPlay()
{
    Super::BeginPlay();

    // BeginPlay 时尝试创建 LUT。Tess actor 此刻可能还没 OnConstruction 完成 MID，
    // 这种情况下 SetHighlightLUT 不会立即看到效果——但 LUT 字段会被保存到
    // APlanetTessellatedMesh::HighlightLUT，下次 ApplyTerrainMaterial_ 末尾会反向填回。
    int32 NumCells = 0;
    if (EnsureLUTCreated_(NumCells))
    {
        UE_LOG(LogCellHighlight, Log,
            TEXT("[CellHighlight] LUT created: %d cells × R8G8 = %d bytes"),
            NumCells, NumCells * 2);
    }
    else
    {
        UE_LOG(LogCellHighlight, Warning,
            TEXT("[CellHighlight] BeginPlay: dependencies not ready, LUT deferred."));
    }
}

void UCellHighlightComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 状态完全清空：避免 PIE 第二次启动时 LUTCpuMirror 残留旧像素值。
    LUTCpuMirror.Reset();
    CurrentHoverCell = INDEX_NONE;
    PendingHoverCell = INDEX_NONE;
    HoverFadeTimer   = 0.0f;
    SelectedCells.Empty();
    HighlightLUT     = nullptr;

    Super::EndPlay(EndPlayReason);
}

bool UCellHighlightComponent::EnsureLUTCreated_(int32& OutNumCells)
{
    OutNumCells = 0;

    // 已建好 → 走最快路径（避免每帧 Cast + 读 Tess）。LUT 行数与 sub level 强绑定，
    // 但 R11 假设运行期不改 CellSubdivisionLevel；如未来需热切换，请显式调 ClearAll
    // 后重新触发 BeginPlay 或新增 RebuildLUT() API。
    if (HighlightLUT && LUTCpuMirror.Num() > 0)
    {
        OutNumCells = LUTCpuMirror.Num();
        return true;
    }

    APlanetBinder* Binder = GetBinder();
    if (!Binder)
    {
        return false;
    }
    APlanetTessellatedMesh* Tess = Binder->GetTessellatedMesh();
    if (!Tess)
    {
        return false;
    }

    // NumCells 必须从 Tess 的 CellTopology 读取（玩法层 sub）。
    // CellTopology 是 TUniquePtr 私有字段——没有 getter；我们通过 §15.4 / §15.4
    // 的 cell 数公式直接算（10 * 4^N + 2）。这避免了暴露 CellTopology 的访问器。
    const int32 CellSub = FMath::Clamp(Tess->CellSubdivisionLevel, 1, 5);
    const int32 NumCells = 10 * (1 << (2 * CellSub)) + 2;   // 10 * 4^N + 2
    if (NumCells <= 0)
    {
        return false;
    }

    OutNumCells = NumCells;

    UTexture2D* NewLUT = UTexture2D::CreateTransient(NumCells, 1, PF_R8G8,
        TEXT("CellHighlightLUT_Transient"));
    if (!NewLUT)
    {
        UE_LOG(LogCellHighlight, Error,
            TEXT("[CellHighlight] CreateTransient(R8G8 %dx1) failed."), NumCells);
        return false;
    }

    NewLUT->Filter        = TF_Nearest;
    NewLUT->SRGB          = false;
    NewLUT->AddressX      = TA_Clamp;
    NewLUT->AddressY      = TA_Clamp;
    NewLUT->NeverStream   = true;
    NewLUT->LODGroup      = TEXTUREGROUP_ColorLookupTable;
    NewLUT->MipGenSettings = TMGS_NoMipmaps;
    NewLUT->CompressionSettings = TC_VectorDisplacementmap;

    // 全 0 初始：所有 cell 都未 hover、未 select。直接通过 mip 0 BulkData 写入。
    FTexturePlatformData* Plat = NewLUT->GetPlatformData();
    if (Plat && Plat->Mips.Num() > 0)
    {
        FByteBulkData& Bulk = Plat->Mips[0].BulkData;
        if (uint8* Dst = static_cast<uint8*>(Bulk.Lock(LOCK_READ_WRITE)))
        {
            FMemory::Memzero(Dst, NumCells * 2);
            Bulk.Unlock();
        }
    }
    NewLUT->UpdateResource();

    HighlightLUT = NewLUT;
    LUTCpuMirror.Init(0, NumCells);

    // 把 LUT 注入 Tess 的 TerrainMID。即使 MID 还没建好，Tess 也会保存 LUT 指针，
    // 下次 ApplyTerrainMaterial_ 末尾自动填回（详见 PlanetTessellatedMesh::SetHighlightLUT）。
    Tess->SetHighlightLUT(HighlightLUT);

    return true;
}

void UCellHighlightComponent::WriteLUTPixel_(int32 CellId)
{
    if (!HighlightLUT || !LUTCpuMirror.IsValidIndex(CellId))
    {
        return;
    }

    // 重新算这一行的 (R, G) byte。R 仅由 CurrentHoverCell 决定（hover 是单值
    // 状态——任意时刻最多 1 个 cell 的 R = 255），G 由 SelectedCells 决定。
    const uint8 NewR = (CurrentHoverCell == CellId) ? 255 : 0;
    const uint8 NewG = SelectedCells.Contains(CellId) ? 255 : 0;
    const uint16 Packed = static_cast<uint16>(NewR) | (static_cast<uint16>(NewG) << 8);

    if (LUTCpuMirror[CellId] == Packed)
    {
        // 完全相等 → 没必要发 RHI 任务（防御反复 WriteLUTPixel_ 同 cell 同值）。
        return;
    }
    LUTCpuMirror[CellId] = Packed;

    // 投递 RHI 上传：1×1 region 在 (CellId, 0)，src stride = 2 byte。
    //
    // FUpdateTextureRegion2D 必须在 lambda 捕获后由 RHI 线程释放——按 UE5
    // UTexture2D::UpdateTextureRegions 约定：回调里 delete Region。
    FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(
        /*DestX*/ static_cast<uint32>(CellId), /*DestY*/ 0,
        /*SrcX*/  0, /*SrcY*/ 0,
        /*Width*/ 1, /*Height*/ 1);

    // 单像素本地拷贝（R8G8 = 2 byte）。data 的生命周期由 lambda 拷贝接管。
    uint8* PixBuf = new uint8[2];
    PixBuf[0] = NewR;
    PixBuf[1] = NewG;

    HighlightLUT->UpdateTextureRegions(
        /*MipIndex*/ 0,
        /*NumRegions*/ 1,
        Region,
        /*SrcPitch*/ 2,            // 单行只有 1 像素 = 2 byte
        /*SrcBpp*/   2,
        PixBuf,
        [](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
        {
            delete[] SrcData;
            // Regions 在调用方 new 出来的，约定回调里释放；
            // UE5.8 签名为 const FUpdateTextureRegion2D*，需要 const_cast 才能 delete。
            delete const_cast<FUpdateTextureRegion2D*>(Regions);
        });
}

void UCellHighlightComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // §7 evt 5：状态 C 倒计时
    if (HoverFadeTimer > 0.0f)
    {
        HoverFadeTimer -= DeltaTime;
        if (HoverFadeTimer <= 0.0f)
        {
            HoverFadeTimer = 0.0f;
            const int32 OldHover = CurrentHoverCell;
            CurrentHoverCell = INDEX_NONE;
            // PendingHoverCell 此时也是 INDEX_NONE（事件 2 设置的）
            if (OldHover != INDEX_NONE)
            {
                WriteLUTPixel_(OldHover);  // R 通道清 0
            }
        }
    }
}

void UCellHighlightComponent::UpdateHover(int32 NewCellId)
{
    // 缓存 LUT 行数用于越界保护
    int32 NumCells = 0;
    if (!EnsureLUTCreated_(NumCells))
    {
        return;
    }
    if (NewCellId != INDEX_NONE && (NewCellId < 0 || NewCellId >= NumCells))
    {
        UE_LOG(LogCellHighlight, Warning,
            TEXT("[CellHighlight] UpdateHover: CellId %d out of range [0, %d)."),
            NewCellId, NumCells);
        return;
    }

    // ----- §7 状态机分支 -----
    //
    // 当前隐式状态由 (CurrentHoverCell, PendingHoverCell, HoverFadeTimer) 三元组决定。
    // 我们直接按 NewCellId 与三元组的关系分流，不显式建枚举，更易读。

    if (NewCellId == INDEX_NONE)
    {
        // 鼠标不在球上
        if (CurrentHoverCell == INDEX_NONE)
        {
            return;  // 已是状态 A，无操作
        }
        if (PendingHoverCell != INDEX_NONE)
        {
            // 状态 B → C：进入 fade-out 缓冲期，LUT 不动
            PendingHoverCell = INDEX_NONE;
            HoverFadeTimer   = HoverFadeDuration;
        }
        // 已经在 C 状态（PendingHoverCell == INDEX_NONE）的话，让 Tick 自然倒计时
        return;
    }

    // NewCellId 是有效 cell
    if (CurrentHoverCell == INDEX_NONE)
    {
        // 状态 A → B：evt 1，从无 hover 进入新 cell
        CurrentHoverCell = NewCellId;
        PendingHoverCell = NewCellId;
        HoverFadeTimer   = 0.0f;
        WriteLUTPixel_(NewCellId);
        return;
    }

    if (NewCellId == CurrentHoverCell)
    {
        // 命中同一个 cell——可能是状态 B 稳定，也可能是状态 C 重入快路径（evt 3）
        // 两种情况都不写 LUT；如果之前在 fade-out，此时取消之
        PendingHoverCell = NewCellId;
        HoverFadeTimer   = 0.0f;
        return;
    }

    // NewCellId 与 Current 不同——evt 4，无论当前是 B 还是 C，立即切换
    const int32 OldHover = CurrentHoverCell;
    CurrentHoverCell = NewCellId;
    PendingHoverCell = NewCellId;
    HoverFadeTimer   = 0.0f;
    WriteLUTPixel_(OldHover);     // R = 0
    WriteLUTPixel_(NewCellId);    // R = 255
}

void UCellHighlightComponent::ClearHover()
{
    UpdateHover(INDEX_NONE);
}

void UCellHighlightComponent::SetSelected(int32 CellId, bool bSelected)
{
    int32 NumCells = 0;
    if (!EnsureLUTCreated_(NumCells))
    {
        return;
    }
    if (CellId < 0 || CellId >= NumCells)
    {
        return;
    }

    const bool bWas = SelectedCells.Contains(CellId);
    if (bSelected == bWas)
    {
        return;  // 无变化
    }

    if (bSelected)
    {
        SelectedCells.Add(CellId);
    }
    else
    {
        SelectedCells.Remove(CellId);
    }

    WriteLUTPixel_(CellId);   // 重写 G 通道（R 由 hover 决定，仍写但不变）
}

void UCellHighlightComponent::ToggleSelected(int32 CellId)
{
    SetSelected(CellId, !SelectedCells.Contains(CellId));
}

void UCellHighlightComponent::ClearAll()
{
    // 把所有当前为非零的行清回 0。先复制 SelectedCells 一份（因为 SetSelected 会改它）。
    const TArray<int32> ToClear = SelectedCells.Array();
    for (int32 CellId : ToClear)
    {
        SetSelected(CellId, false);
    }

    if (CurrentHoverCell != INDEX_NONE)
    {
        const int32 Old = CurrentHoverCell;
        CurrentHoverCell = INDEX_NONE;
        PendingHoverCell = INDEX_NONE;
        HoverFadeTimer   = 0.0f;
        WriteLUTPixel_(Old);
    }
}