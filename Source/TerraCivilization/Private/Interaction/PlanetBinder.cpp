// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/PlanetBinder.h"

#include "Interaction/CellHighlightComponent.h"
#include "Render/PlanetTessellatedMesh.h"

#include "FCell.h"
#include "FSphereTopology.h"
#include "FSphereTopologyQuery.h"
#include "FSurfaceQueryResult.h"

#include "PtgManager.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlanetBinder, Log, All);

APlanetBinder::APlanetBinder()
{
    PrimaryActorTick.bCanEverTick = false;

    // 高亮组件：作为本 Actor 的默认子对象创建。
    // SceneRoot 不是必需的：ULineBatchComponent 是 World 级别，不依赖本 Actor Transform。
    HighlightComp = CreateDefaultSubobject<UCellHighlightComponent>(TEXT("HighlightComp"));
}

// 注意：这两个特殊成员函数必须放在 .cpp（且本文件已 #include FSphereTopology / FSphereTopologyQuery 完整定义），
// 这样 TUniquePtr<FSphereTopology> / TUniquePtr<FSphereTopologyQuery> 的析构展开点
// 才能看到完整类型，避免 C4150「删除指向不完整类型的指针」。
// FVTableHelper 构造函数不能 = default（UE 编码规范，与基类 AActor 的实现保持一致）。
APlanetBinder::~APlanetBinder() = default;
APlanetBinder::APlanetBinder(FVTableHelper& Helper) : Super(Helper) {}

void APlanetBinder::BeginPlay()
{
    Super::BeginPlay();

    // 构建拓扑层。SubdivisionLevel 已由 UPROPERTY meta 限制在 [1, 6]，
    // 同时 FSphereTopology 构造里也有 Clamp 兜底。
    Topology = MakeUnique<FSphereTopology>(SubdivisionLevel);
    Query    = MakeUnique<FSphereTopologyQuery>(Topology.Get());

    const int32 CellCount   = Topology->Cells.Num();
    const int32 EdgeCount   = Topology->Edges.Num();
    const int32 CornerCount = Topology->Corners.Num();
    const int32 TriCount    = Topology->Tris.Num();

    UE_LOG(LogPlanetBinder, Log,
        TEXT("[PlanetBinder] Topology built. SubdivisionLevel=%d  Cells=%d  Edges=%d  Corners=%d  Tris=%d  PtgManagerRef=%s"),
        SubdivisionLevel,
        CellCount, EdgeCount, CornerCount, TriCount,
        *GetNameSafe(PtgManagerRef));

    if (SubdivisionLevel == 3 && CellCount != 642)
    {
        UE_LOG(LogPlanetBinder, Warning,
            TEXT("[PlanetBinder] Expect 642 cells when SubdivisionLevel=3, but got %d."), CellCount);
    }

    if (GEngine)
    {
        const FString Msg = FString::Printf(
            TEXT("PlanetBinder ready: Sub=%d, Cells=%d"), SubdivisionLevel, CellCount);
        GEngine->AddOnScreenDebugMessage(/*Key=*/-1, /*TimeToDisplay=*/5.0f, FColor::Green, Msg);
    }

    // PIE 启动时强制 Shape=Sphere，并自动生成 PTG mesh。
    // 原因：
    //  - APtgManager::Shape 默认是 Plane，自带 BP_PTG_Manager 也没改这个默认值；
    //  - APtgManager 没有重写 BeginPlay，也没有"运行时自动生成"开关；
    //  - RuntimeMeshComponent 的几何数据不会序列化到关卡 .umap。
    // 因此在 PIE 启动时主动覆盖 Shape 并触发一次生成，是看到球的最稳妥做法。
    if (PtgManagerRef)
    {
        if (bForceSphereShape)
        {
            EnsureSphereShape();
        }
        if (bAutoGenerateOnBeginPlay)
        {
            RegeneratePtg(TEXT("BeginPlay"));
        }
    }
}

void APlanetBinder::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

#if WITH_EDITOR
    // 仅在编辑器中（且未在 PIE 运行）触发：把 PTG Shape 强制为 Sphere 并自动生成一次，
    // 让用户把 BP_PlanetBinder 拖入关卡 / 修改 PtgManagerRef 的瞬间就能在编辑器视口看到球。
    // 在 PIE 中重复触发会浪费几百毫秒，所以仅限 IsEditorWorld()；GameWorld 由 BeginPlay 负责。
    UWorld* World = GetWorld();
    const bool bIsEditorOnly = World && World->WorldType == EWorldType::Editor;
    if (bIsEditorOnly && bAutoGenerateInEditor && PtgManagerRef)
    {
        bool bShapeChanged = false;
        if (bForceSphereShape)
        {
            bShapeChanged = EnsureSphereShape();
        }
        // 如果 Shape 没改过，并且 RMC 已经是构建好的状态，就避免每次属性面板编辑都重生成。
        // 这里用一个简单策略：仅在 Shape 实际发生变化、或 RMC 看起来还没生成时才触发。
        // 大部分情况下，第一次拖入 / 第一次绑定 PtgManagerRef 时 Shape 必然要从 Plane 改成 Sphere，
        // 触发一次；之后 OnConstruction 反复调用也只会跑很轻的 EnsureSphereShape 检查、不再生成。
        if (bShapeChanged)
        {
            RegeneratePtg(TEXT("OnConstruction (editor, shape changed)"));
        }
    }
#endif
}

void APlanetBinder::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 显式释放：Topology 析构里会递归释放 TriTreeRoots 4 叉树指针。
    Query.Reset();
    Topology.Reset();
    LastHoveredCellId = INDEX_NONE;

    Super::EndPlay(EndPlayReason);
}

void APlanetBinder::OnHoverWorldPoint(const FVector& WorldHit)
{
    AActor* Host = GetHostActor();
    if (!Host || !Query.IsValid())
    {
        return;
    }

    // 1) 命中点 -> 球心局部 -> 单位方向
    const FVector Center  = Host->GetActorLocation();
    const FVector Local   = WorldHit - Center;
    const FVector UnitDir = Local.GetSafeNormal();
    if (UnitDir.IsNearlyZero())
    {
        return;
    }

    // 2) 球面拓扑查询。FindNearestCell 内部会再次 normalize，传入 UnitDir 即可。
    const FSurfaceQueryResult R = Query->FindNearestCell(UnitDir);
    if (R.CellId == INDEX_NONE)
    {
        OnLeavePlanet();
        return;
    }

    // 3) R11：每帧都调 UpdateHover——状态机内部决定是否写 LUT，以及是否启动防抖。
    if (HighlightComp)
    {
        HighlightComp->UpdateHover(R.CellId);
    }

    // 4) 抖动抑制：CellId 没变就跳过下面"屏幕日志 + DebugSphere"的更新。
    if (R.CellId == LastHoveredCellId)
    {
        return;
    }
    LastHoveredCellId = R.CellId;

    // 5) 屏幕调试信息：CellId / 是否五边形
    const FCell& Cell = Topology->Cells[R.CellId];
    const float  Radius = GetRadius();
    if (GEngine)
    {
        const FString Msg = FString::Printf(
            TEXT("Hover Cell #%d  (%s)  Radius=%.0fcm  UnitCenter=(%.3f, %.3f, %.3f)"),
            R.CellId,
            Cell.bIsPentagon ? TEXT("PENTAGON") : TEXT("hexagon"),
            Radius,
            Cell.UnitCenter.X, Cell.UnitCenter.Y, Cell.UnitCenter.Z);

        // 用固定 Key=1 覆盖上一行，避免屏幕日志刷屏
        GEngine->AddOnScreenDebugMessage(
            /*Key=*/1, /*TimeToDisplay=*/2.0f,
            Cell.bIsPentagon ? FColor::Cyan : DebugHoverColor,
            Msg);
    }

    // 6) 在 Cell 中心位置画 DebugSphere 用作肉眼校准。
    //    位置 = Cell.UnitCenter * Radius + Center （球心是 Host actor 的 Location）。
    if (UWorld* World = GetWorld())
    {
        const FVector CellWorldCenter = Cell.UnitCenter * Radius + Center;
        DrawDebugSphere(
            World,
            CellWorldCenter,
            DebugSphereRadius,
            /*Segments=*/16,
            Cell.bIsPentagon ? FColor::Cyan : DebugHoverColor,
            /*bPersistentLines=*/false,
            /*LifeTime=*/2.0f,
            /*DepthPriority=*/SDPG_Foreground,
            /*Thickness=*/2.0f);
    }

    UE_LOG(LogPlanetBinder, Verbose,
        TEXT("[PlanetBinder] Hover -> Cell %d (%s)"),
        R.CellId, Cell.bIsPentagon ? TEXT("pent") : TEXT("hex"));
}

void APlanetBinder::OnClickWorldPoint(const FVector& WorldHit)
{
    // R11：点击 → 在鼠标命中的 Cell 上 toggle select 状态。
    // CellHighlightComponent 内部会写 LUT.G 通道，马上看到 SelectColor 描边。
    AActor* Host = GetHostActor();
    if (!Host || !Query.IsValid() || !HighlightComp)
    {
        return;
    }
    const FVector Center  = Host->GetActorLocation();
    const FVector Local   = WorldHit - Center;
    const FVector UnitDir = Local.GetSafeNormal();
    if (UnitDir.IsNearlyZero())
    {
        return;
    }
    const FSurfaceQueryResult R = Query->FindNearestCell(UnitDir);
    if (R.CellId == INDEX_NONE)
    {
        return;
    }
    HighlightComp->ToggleSelected(R.CellId);

    UE_LOG(LogPlanetBinder, Log,
        TEXT("[PlanetBinder] Click -> ToggleSelected(Cell %d)"), R.CellId);
}

void APlanetBinder::OnLeavePlanet()
{
    // 当鼠标移出球体时，清掉抖动抑制缓存、屏幕消息、调 ClearHover。
    // 注意：这里不调 ClearAll，避免把 select 纯牌也清掉；ClearHover 内部
    // 走 UpdateHover(INDEX_NONE)，仅启动 0.5s 防抖、不动 select。
    LastHoveredCellId = INDEX_NONE;
    if (GEngine)
    {
        GEngine->RemoveOnScreenDebugMessage(/*Key=*/1);
    }
    if (HighlightComp)
    {
        HighlightComp->ClearHover();
    }
}

AActor* APlanetBinder::GetHostActor() const
{
    if (TessellatedMeshRef)
    {
        return TessellatedMeshRef.Get();
    }
    return PtgManagerRef.Get();
}

float APlanetBinder::GetRadius() const
{
    constexpr float DefaultRadius = 15000.0f;

    // R11 优先：从 TessellatedMeshRef->GlobeRadiusCM 取（公开字段，无需反射）。
    if (TessellatedMeshRef)
    {
        return TessellatedMeshRef->GlobeRadiusCM;
    }

    // Fallback：旧 PTG 反射路径。APtgManager::Radius 是 protected UPROPERTY，
    // 这里用 UE 反射在不修改插件源码的前提下获取。
    if (PtgManagerRef)
    {
        if (const FFloatProperty* Prop = FindFProperty<FFloatProperty>(
                PtgManagerRef->GetClass(), TEXT("Radius")))
        {
            return Prop->GetPropertyValue_InContainer(PtgManagerRef);
        }
    }

    return DefaultRadius;
}

bool APlanetBinder::EnsureSphereShape()
{
    if (!PtgManagerRef)
    {
        return false;
    }

    // EPtgProcMeshShapes 是 uint8 enum：Plane=0, Cube=1, Sphere=2
    constexpr uint8 SphereValue = 2;

    // APtgManager::Shape 是 protected UPROPERTY，同样以反射访问。
    // 枚举型 UPROPERTY 在反射层面上是 FByteProperty（因为底层是 uint8）。
    if (FByteProperty* ByteProp = FindFProperty<FByteProperty>(
            PtgManagerRef->GetClass(), TEXT("Shape")))
    {
        const uint8 Current = ByteProp->GetPropertyValue_InContainer(PtgManagerRef);
        if (Current != SphereValue)
        {
            ByteProp->SetPropertyValue_InContainer(PtgManagerRef, SphereValue);
            UE_LOG(LogPlanetBinder, Log,
                TEXT("[PlanetBinder] Forced PtgManagerRef->Shape: %d -> Sphere(2)"), (int32)Current);
            return true;
        }
        return false;
    }

    // 备选路径：老版 UE 中 UENUM uint8 可能被反射为 FEnumProperty。
    if (FEnumProperty* EnumProp = FindFProperty<FEnumProperty>(
            PtgManagerRef->GetClass(), TEXT("Shape")))
    {
        FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
        void* ValuePtr = EnumProp->ContainerPtrToValuePtr<void>(PtgManagerRef);
        const int64 Current = Underlying->GetSignedIntPropertyValue(ValuePtr);
        if (Current != SphereValue)
        {
            Underlying->SetIntPropertyValue(ValuePtr, (int64)SphereValue);
            UE_LOG(LogPlanetBinder, Log,
                TEXT("[PlanetBinder] Forced PtgManagerRef->Shape (Enum): %lld -> Sphere(2)"), Current);
            return true;
        }
        return false;
    }

    UE_LOG(LogPlanetBinder, Warning,
        TEXT("[PlanetBinder] EnsureSphereShape: cannot reflect 'Shape' on %s."),
        *GetNameSafe(PtgManagerRef->GetClass()));
    return false;
}

void APlanetBinder::RegeneratePtg(const TCHAR* Reason)
{
    if (!PtgManagerRef)
    {
        return;
    }
    UE_LOG(LogPlanetBinder, Log,
        TEXT("[PlanetBinder] PtgManager->GenerateEverything() | reason=%s | shape=%d radius=%.0f"),
        Reason ? Reason : TEXT("(none)"),
        (int32)PtgManagerRef->GetShape(),
        GetRadius());
    PtgManagerRef->GenerateEverything();
}
