// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/PlanetInteractionController.h"

#include "Interaction/PlanetBinder.h"
#include "Render/PlanetTessellatedMesh.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DefaultPawn.h"
#include "Camera/PlayerCameraManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlanetInteraction, Log, All);

APlanetInteractionController::APlanetInteractionController()
{
    bShowMouseCursor       = true;
    bEnableClickEvents     = true;
    bEnableMouseOverEvents = true;
    DefaultMouseCursor     = EMouseCursor::Crosshairs;
}

void APlanetInteractionController::BeginPlay()
{
    Super::BeginPlay();

    // G8：关闭引擎默认 Pawn 移动输入。
    // 否则在纬度已被 clamp 到极区后，继续按 W/S 时虽然 G8 轨道参数不再变化，
    // 但默认 Pawn 仍会沿前向移动，表现成“高度被偷偷改了”。
    SetIgnoreMoveInput(true);

    // 关卡里只期望存在唯一一个 PlanetBinder。这里做缓存查找，
    // 失败时打日志并保持 CachedBinder 为 null（PlayerTick 会安全跳过）。
    CachedBinder = Cast<APlanetBinder>(
        UGameplayStatics::GetActorOfClass(this, APlanetBinder::StaticClass()));

    if (!CachedBinder)
    {
        UE_LOG(LogPlanetInteraction, Warning,
            TEXT("[PlanetInteractionController] No APlanetBinder found in level. Hover/Click will be inactive."));
    }
    else
    {
        UE_LOG(LogPlanetInteraction, Log,
            TEXT("[PlanetInteractionController] Bound to %s"), *GetNameSafe(CachedBinder));
    }
}

void APlanetInteractionController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

    if (!CachedBinder)
    {
        return;
    }

    // R11：几何源优先 Tess actor、次选 PTG manager。GetHostActor 内部已动态选择。
    AActor* HostActor = CachedBinder->GetHostActor();
    if (!HostActor)
    {
        return;
    }

    // 鼠标射线 -> ECC 通道命中。bTraceComplex = true 必须：
    //   - PTG 路径：RMC 默认开复杂碰撞
    //   - Tess 路径：ProceduralMeshComponent 仅提供复杂碰撞 + bUseComplexAsSimpleCollision=true
    FHitResult Hit;
    const bool bHit = GetHitResultUnderCursorByChannel(
        UEngineTypes::ConvertToTraceType(HoverTraceChannel),
        /*bTraceComplex=*/true,
        Hit);

    const bool bHitHost = bHit && Hit.GetActor() == HostActor;

    APlanetTessellatedMesh* Tess = CachedBinder->GetTessellatedMesh();
    if (Tess)
    {
        UpdateG8ManualCameraControl_(DeltaTime, Tess);
    }

    const bool bUseHISMHighlightPath = Tess && Tess->bEnableHISMInstanceHighlight;
    const bool bHISMHoverHandled = bHit && Tess && Tess->HandleHISMHoverHit(Hit);

    if (bHISMHoverHandled)
    {
        bWasHovering = true;

        if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
        {
            Tess->HandleHISMClickHit(Hit);
        }
    }
    else if (bUseHISMHighlightPath)
    {
        Tess->ClearHISMHover();
        bWasHovering = false;
    }
    else if (bHitHost)
    {
        CachedBinder->OnHoverWorldPoint(Hit.ImpactPoint);
        bWasHovering = true;

        // R11：左键点击以 toggle select（仅在鼠标在球上时才生效）。
        // 不走 EnhancedInput——APlayerController::WasInputKeyJustPressed 在颜色状态机
        // 上足够使用。如后续需 BP 接线可在 IMC_Planet 中加 IA_ClickPlanet。
        if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
        {
            CachedBinder->OnClickWorldPoint(Hit.ImpactPoint);
        }
    }
    else if (bWasHovering)
    {
        // 边沿：从 hover 状态退出，通知一次 Leave，避免每帧重复调用。
        CachedBinder->OnLeavePlanet();
        bWasHovering = false;
    }
}

bool APlanetInteractionController::InitializeG8OrbitCameraState_(APlanetTessellatedMesh* Tess)
{
    if (!Tess || !Tess->bEnableG8ManualCameraControl)
    {
        return false;
    }

    FVector CameraWorldPosition = FVector::ZeroVector;
    if (AActor* ViewTarget = GetViewTarget())
    {
        CameraWorldPosition = ViewTarget->GetActorLocation();
    }
    else if (PlayerCameraManager)
    {
        CameraWorldPosition = PlayerCameraManager->GetCameraLocation();
    }
    else
    {
        return false;
    }

    if (!Tess->SyncOrbitCameraStateFromWorldPosition(CameraWorldPosition, G8CameraLongitudeDeg, G8CameraLatitudeDeg, G8CameraHeightOffsetCM))
    {
        return false;
    }

    bG8OrbitCameraInitialized = true;
    return true;
}

void APlanetInteractionController::UpdateG8ManualCameraControl_(float DeltaTime, APlanetTessellatedMesh* Tess)
{
    if (!Tess || !Tess->bEnableG8ManualCameraControl)
    {
        return;
    }

    if (!bG8OrbitCameraInitialized && !InitializeG8OrbitCameraState_(Tess))
    {
        return;
    }

    FVector CameraWorldPosition = FVector::ZeroVector;
    if (AActor* ViewTarget = GetViewTarget())
    {
        CameraWorldPosition = ViewTarget->GetActorLocation();
    }
    else if (PlayerCameraManager)
    {
        CameraWorldPosition = PlayerCameraManager->GetCameraLocation();
    }

    float SyncedLongitudeDeg = G8CameraLongitudeDeg;
    float SyncedLatitudeDeg = G8CameraLatitudeDeg;
    float SyncedHeightOffsetCM = G8CameraHeightOffsetCM;
    if (Tess->SyncOrbitCameraStateFromWorldPosition(CameraWorldPosition, SyncedLongitudeDeg, SyncedLatitudeDeg, SyncedHeightOffsetCM))
    {
        G8CameraLongitudeDeg = SyncedLongitudeDeg;
        G8CameraLatitudeDeg = SyncedLatitudeDeg;
        G8CameraHeightOffsetCM = SyncedHeightOffsetCM;
    }

    const float OrbitDeltaDeg = Tess->G8CameraOrbitDegreesPerSecond * DeltaTime;
    bool bCameraChanged = false;

    if (IsInputKeyDown(EKeys::W))
    {
        G8CameraLatitudeDeg += OrbitDeltaDeg;
        bCameraChanged = true;
    }
    if (IsInputKeyDown(EKeys::S))
    {
        G8CameraLatitudeDeg -= OrbitDeltaDeg;
        bCameraChanged = true;
    }
    if (IsInputKeyDown(EKeys::A))
    {
        G8CameraLongitudeDeg += OrbitDeltaDeg;
        bCameraChanged = true;
    }
    if (IsInputKeyDown(EKeys::D))
    {
        G8CameraLongitudeDeg -= OrbitDeltaDeg;
        bCameraChanged = true;
    }
    if (WasInputKeyJustPressed(EKeys::MouseScrollUp))
    {
        G8CameraHeightOffsetCM -= Tess->G8CameraZoomStepCM;
        bCameraChanged = true;
    }
    if (WasInputKeyJustPressed(EKeys::MouseScrollDown))
    {
        G8CameraHeightOffsetCM += Tess->G8CameraZoomStepCM;
        bCameraChanged = true;
    }

    if (!bCameraChanged)
    {
        return;
    }

    G8CameraLatitudeDeg = FMath::Clamp(G8CameraLatitudeDeg, -89.0f, 89.0f);
    G8CameraHeightOffsetCM = FMath::Clamp(
        G8CameraHeightOffsetCM,
        Tess->G8CameraMinHeightOffsetCM,
        FMath::Max(Tess->G8CameraMinHeightOffsetCM, Tess->G8CameraMaxHeightOffsetCM));
    G8CameraLongitudeDeg = FRotator::NormalizeAxis(G8CameraLongitudeDeg);

    Tess->ApplyOrbitCameraState(G8CameraLongitudeDeg, G8CameraLatitudeDeg, G8CameraHeightOffsetCM);
}
