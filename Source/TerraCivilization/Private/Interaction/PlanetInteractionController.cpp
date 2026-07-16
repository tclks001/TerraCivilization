// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/PlanetInteractionController.h"

#include "Interaction/PlanetBinder.h"
#include "Render/PlanetCameraComponent.h"
#include "Render/PlanetHISMInteractionComponent.h"
#include "Render/PlanetTessellatedMesh.h"
#include "UI/TerraUISettings.h"
#include "UI/TerraUISubsystem.h"

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

    if (const UTerraUISettings* UISettings = GetDefault<UTerraUISettings>(); UISettings->bShowFrontEndOnStartup)
    {
        if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
        {
            LocalPlayer->GetSubsystem<UTerraUISubsystem>()->ShowMainMenu();
        }
    }

    // G8：关闭引擎默认 Pawn 移动输入。
    // 否则在纬度已被 clamp 到极区后，继续按 W/S 时虽然 G8 轨道参数不再变化，
    // 但默认 Pawn 仍会沿前向移动，表现成“高度被偷偷改了”。
    SetIgnoreMoveInput(true);

    // The planet is controlled by cursor hit tests plus the explicit WSAD/QE camera path.
    // Never let GameOnly input recapture turn raw mouse movement into default controller look.
    SetIgnoreLookInput(true);

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

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UTerraUISubsystem* UI = LocalPlayer->GetSubsystem<UTerraUISubsystem>())
        {
            if (WasInputKeyJustPressed(EKeys::Escape))
            {
                UI->TogglePauseMenu();
            }
            if (UI->IsBlockingGameInput())
            {
                return;
            }
        }
    }

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
        UpdateC3FocusCameraControl_(DeltaTime, Tess);

        if (WasInputKeyJustPressed(EKeys::Tab))
        {
            const bool bReverseTab = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
            Tess->HandleC5NavigateCurrentFactionPiece(bReverseTab);
        }

        if (WasInputKeyJustPressed(EKeys::RightMouseButton))
        {
            Tess->HandleHISMUndo();
        }
    }

    const bool bUseContinuousSurfacePath = Tess && Tess->IsContinuousTerrainVisualActive();
    if (bUseContinuousSurfacePath)
    {
        const bool bContinuousHoverHandled = bHit && Tess->HandleContinuousSurfaceHoverHit(Hit);
        if (bContinuousHoverHandled)
        {
            bWasHovering = true;
            if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
            {
                Tess->HandleContinuousSurfaceClickHit(Hit);
            }
        }
        else
        {
            Tess->ClearHISMHover();
            bWasHovering = false;
        }
        return;
    }

    const bool bUseHISMHighlightPath = Tess
        && Tess->GetPlanetHISMInteractionComponent()
        && Tess->GetPlanetHISMInteractionComponent()->bEnableHISMInstanceHighlight;
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

bool APlanetInteractionController::InitializeC3FocusCameraState_(APlanetTessellatedMesh* Tess)
{
    UPlanetCameraComponent* CameraComponent = Tess ? Tess->GetPlanetCameraComponent() : nullptr;
    if (!Tess)
    {
        return false;
    }
    if (!CameraComponent)
    {
        return false;
    }
    if (!CameraComponent->bEnableG8ManualCameraControl)
    {
        return false;
    }

    FVector CameraWorldPosition = FVector::ZeroVector;
    // PlayerCameraManager is the final rendered view. ViewTarget can be a DefaultPawn whose
    // transform lags or differs from the camera manager during controller/camera transitions.
    if (PlayerCameraManager)
    {
        CameraWorldPosition = PlayerCameraManager->GetCameraLocation();
    }
    else if (AActor* ViewTarget = GetViewTarget())
    {
        CameraWorldPosition = ViewTarget->GetActorLocation();
    }
    else
    {
        return false;
    }

    FRotator CameraWorldRotation = FRotator::ZeroRotator;
    if (PlayerCameraManager)
    {
        CameraWorldRotation = PlayerCameraManager->GetCameraRotation();
    }
    else if (AActor* ViewTarget = GetViewTarget())
    {
        CameraWorldRotation = ViewTarget->GetActorRotation();
    }
    else
    {
        return false;
    }

    float DummyTiltDeg;
    if (!Tess->SyncFocusCameraStateFromView(
        CameraWorldPosition,
        CameraWorldRotation,
        C3FocusUnitDir,
        C3DistanceToFocusCM,
        DummyTiltDeg,
        C3YawAroundFocusDeg))
    {
        return false;
    }

    // 派生调试用经纬度。
    C3FocusUnitDir = C3FocusUnitDir.GetSafeNormal();
    C3FocusLatitudeDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(static_cast<float>(C3FocusUnitDir.Z), -1.0f, 1.0f)));
    C3FocusLongitudeDeg = FRotator::NormalizeAxis(FMath::RadiansToDegrees(
        FMath::Atan2(static_cast<float>(C3FocusUnitDir.Y), static_cast<float>(C3FocusUnitDir.X))));

    bC3FocusCameraInitialized = true;
    return true;
}

bool APlanetInteractionController::RequestC4FocusOnUnitDir(const FVector& TargetFocusUnitDir, float BlendSeconds)
{
    const FVector NormalizedTarget = TargetFocusUnitDir.GetSafeNormal();
    if (NormalizedTarget.IsNearlyZero())
    {
        return false;
    }

    if (!bC3FocusCameraInitialized)
    {
        if (APlanetTessellatedMesh* Tess = CachedBinder ? CachedBinder->GetTessellatedMesh() : nullptr)
        {
            InitializeC3FocusCameraState_(Tess);
        }
    }

    C3FocusUnitDir = C3FocusUnitDir.GetSafeNormal();
    if (C3FocusUnitDir.IsNearlyZero())
    {
        C3FocusUnitDir = FVector::ForwardVector;
    }

    const float Duration = FMath::Max(0.0f, BlendSeconds);
    if (Duration <= KINDA_SMALL_NUMBER)
    {
        C3FocusUnitDir = NormalizedTarget;
        bC4SelectionFocusBlendActive = false;
        bC3FocusCameraInitialized = true;
        return true;
    }

    C4SelectionFocusStartUnitDir = C3FocusUnitDir;
    C4SelectionFocusTargetUnitDir = NormalizedTarget;
    C4SelectionFocusElapsedSeconds = 0.0f;
    C4SelectionFocusDurationSeconds = Duration;
    bC4SelectionFocusBlendActive = true;
    bC3FocusCameraInitialized = true;
    return true;
}

bool APlanetInteractionController::RequestC2_5FocusOnUnitDir(const FVector& TargetFocusUnitDir, float BlendSeconds)
{
    return RequestC4FocusOnUnitDir(TargetFocusUnitDir, BlendSeconds);
}

bool APlanetInteractionController::RequestC6FocusOnUnitDir(const FVector& TargetFocusUnitDir, float BlendSeconds)
{
    return RequestC4FocusOnUnitDir(TargetFocusUnitDir, BlendSeconds);
}

bool APlanetInteractionController::SetC3FocusCameraState(const FVector& FocusUnitDir, float DistanceToFocusCM, float YawAroundFocusDeg)
{
    const FVector NormalizedFocus = FocusUnitDir.GetSafeNormal();
    if (NormalizedFocus.IsNearlyZero())
    {
        return false;
    }

    C3FocusUnitDir = NormalizedFocus;
    C3DistanceToFocusCM = FMath::Max(1.0f, DistanceToFocusCM);
    C3YawAroundFocusDeg = FRotator::NormalizeAxis(YawAroundFocusDeg);
    C3FocusLatitudeDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(static_cast<float>(C3FocusUnitDir.Z), -1.0f, 1.0f)));
    C3FocusLongitudeDeg = FRotator::NormalizeAxis(FMath::RadiansToDegrees(
        FMath::Atan2(static_cast<float>(C3FocusUnitDir.Y), static_cast<float>(C3FocusUnitDir.X))));
    bC3FocusCameraInitialized = true;
    bC4SelectionFocusBlendActive = false;
    return true;
}

bool APlanetInteractionController::CaptureCurrentViewForPause()
{
    APlanetTessellatedMesh* Tess = CachedBinder ? CachedBinder->GetTessellatedMesh() : nullptr;
    if (!Tess || !InitializeC3FocusCameraState_(Tess))
    {
        return false;
    }

    // A paused camera must resume from the rendered pose, not from a stale C4/C6 blend target.
    bC4SelectionFocusBlendActive = false;
    UpdateC3FocusCameraControl_(0.0f, Tess);
    return true;
}

bool APlanetInteractionController::HasC3ManualCameraInput_() const
{
    return IsInputKeyDown(EKeys::W)
        || IsInputKeyDown(EKeys::S)
        || IsInputKeyDown(EKeys::A)
        || IsInputKeyDown(EKeys::D)
        || IsInputKeyDown(EKeys::Q)
        || IsInputKeyDown(EKeys::E)
        || WasInputKeyJustPressed(EKeys::MouseScrollUp)
        || WasInputKeyJustPressed(EKeys::MouseScrollDown);
}

bool APlanetInteractionController::HasAutoCameraInterruptInput_() const
{
    return HasC3ManualCameraInput_()
        || WasInputKeyJustPressed(EKeys::LeftMouseButton)
        || WasInputKeyJustPressed(EKeys::RightMouseButton)
        || WasInputKeyJustPressed(EKeys::Tab);
}

void APlanetInteractionController::UpdateC3FocusCameraControl_(float DeltaTime, APlanetTessellatedMesh* Tess)
{
    UPlanetCameraComponent* CameraComponent = Tess ? Tess->GetPlanetCameraComponent() : nullptr;
    if (!Tess)
    {
        return;
    }
    if (!CameraComponent)
    {
        return;
    }
    if (!CameraComponent->bEnableG8ManualCameraControl)
    {
        return;
    }

    if (!bC3FocusCameraInitialized && !InitializeC3FocusCameraState_(Tess))
    {
        return;
    }

    // 注意：这里刻意不再每帧从相机反解 (FocusUnitDir, Yaw, Tilt, Distance)。
    // 这条同步在旧实现里会形成"每帧 Yaw≈0 → 沿新焦点当地东切向重启"的自反闭环，
    // 使 A/D 积分成纬线而非大圆，并在极点让 W/S 卡住原地打转。
    // 详见 Docs/SimpleGameplay/C3FocusCameraManualControlDesign.md §4.2。

    const bool bAutoCameraInterruptInput = HasAutoCameraInterruptInput_();
    if (bAutoCameraInterruptInput)
    {
        bC4SelectionFocusBlendActive = false;
    }
    else if (bC4SelectionFocusBlendActive)
    {
        C4SelectionFocusElapsedSeconds += DeltaTime;
        const float BlendAlpha = C4SelectionFocusDurationSeconds > KINDA_SMALL_NUMBER
            ? FMath::Clamp(C4SelectionFocusElapsedSeconds / C4SelectionFocusDurationSeconds, 0.0f, 1.0f)
            : 1.0f;
        const float SmoothAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, BlendAlpha, 2.0f);
        const FQuat FocusDeltaQuat = FQuat::FindBetweenNormals(C4SelectionFocusStartUnitDir, C4SelectionFocusTargetUnitDir);
        C3FocusUnitDir = FQuat::Slerp(FQuat::Identity, FocusDeltaQuat, SmoothAlpha)
            .RotateVector(C4SelectionFocusStartUnitDir)
            .GetSafeNormal();
        if (BlendAlpha >= 1.0f)
        {
            C3FocusUnitDir = C4SelectionFocusTargetUnitDir.GetSafeNormal();
            bC4SelectionFocusBlendActive = false;
        }
    }

    const float OrbitDeltaDeg = CameraComponent->G8CameraOrbitDegreesPerSecond * DeltaTime;
    float FocusRightDeltaDeg = 0.0f;
    float FocusForwardDeltaDeg = 0.0f;

    // C3.6：先处理 Q/E 绕焦点法线旋转 Yaw，再让 W/S/A/D 用更新后的 Yaw
    // 派生本帧的切向前进方向。这样"边转视角边前进"是连续的球面运动，
    // 不会出现 QE 结果延迟一帧才影响 WSAD 参考系的现象。
    // 详见 Docs/SimpleGameplay/C3_6QERollAroundFocusDesign.md §3。
    if (IsInputKeyDown(EKeys::E))
    {
        C3YawAroundFocusDeg += OrbitDeltaDeg;
    }
    if (IsInputKeyDown(EKeys::Q))
    {
        C3YawAroundFocusDeg -= OrbitDeltaDeg;
    }

    if (IsInputKeyDown(EKeys::W))
    {
        FocusForwardDeltaDeg += OrbitDeltaDeg;
    }
    if (IsInputKeyDown(EKeys::S))
    {
        FocusForwardDeltaDeg -= OrbitDeltaDeg;
    }
    if (IsInputKeyDown(EKeys::A))
    {
        FocusRightDeltaDeg -= OrbitDeltaDeg;
    }
    if (IsInputKeyDown(EKeys::D))
    {
        FocusRightDeltaDeg += OrbitDeltaDeg;
    }
    if (WasInputKeyJustPressed(EKeys::MouseScrollUp))
    {
        C3DistanceToFocusCM -= CameraComponent->G8CameraZoomStepCM;
    }
    if (WasInputKeyJustPressed(EKeys::MouseScrollDown))
    {
        C3DistanceToFocusCM += CameraComponent->G8CameraZoomStepCM;
    }

    if (!FMath::IsNearlyZero(FocusRightDeltaDeg) || !FMath::IsNearlyZero(FocusForwardDeltaDeg))
    {
        // 沿测地线推进焦点，同时把 Yaw 平行运输到新焦点。
        Tess->OffsetFocusCameraStateOnTangent(
            FocusRightDeltaDeg,
            FocusForwardDeltaDeg,
            C3FocusUnitDir,
            C3YawAroundFocusDeg);
    }

    // C3.5：每帧无条件 Clamp / 派生 / Apply。即便本帧无输入也走 Apply，
    // 目的是把相机严格锚定到当前 (FocusUnitDir, Yaw, Tilt, Distance)，
    // 避免 UE 默认 Pawn / PlayerController / CameraManager 在无输入帧偷偷改动相机姿态。
    // 详见 Docs/SimpleGameplay/C3_5IdleFrameCameraStabilityDesign.md。
    // 注意：这里只每帧"写"（Apply），不每帧"读回覆盖"（Sync），因此不会重新引入 C3 §4.2 消灭的
    // "沿纬线走 / 极点卡死"闭环。
    C3DistanceToFocusCM = FMath::Clamp(
        C3DistanceToFocusCM,
        CameraComponent->G8CameraMinHeightOffsetCM,
        FMath::Max(CameraComponent->G8CameraMinHeightOffsetCM, CameraComponent->G8CameraMaxHeightOffsetCM));

    // C3.7：倾角不再独立维护，改为每帧从距离线性插值自动派生。
    // 详见 Docs/SimpleGameplay/C3_7AutoTiltFromDistanceDesign.md。
    const float TiltT = (CameraComponent->C3AutoTiltMaxDistanceCM > CameraComponent->C3AutoTiltMinDistanceCM)
        ? (C3DistanceToFocusCM - CameraComponent->C3AutoTiltMinDistanceCM) / (CameraComponent->C3AutoTiltMaxDistanceCM - CameraComponent->C3AutoTiltMinDistanceCM)
        : 0.0f;
    const float C3TiltDeg = FMath::Lerp(
        CameraComponent->C3AutoTiltAtMinDistanceDeg,
        CameraComponent->C3AutoTiltAtMaxDistanceDeg,
        FMath::Clamp(TiltT, 0.0f, 1.0f));
    C3YawAroundFocusDeg = FRotator::NormalizeAxis(C3YawAroundFocusDeg);
    C3FocusUnitDir = C3FocusUnitDir.GetSafeNormal();

    // 派生调试用经纬度（不再对纬度做 [-89, 89] clamp——真值是 3D 单位向量，允许穿越极点）。
    C3FocusLatitudeDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(static_cast<float>(C3FocusUnitDir.Z), -1.0f, 1.0f)));
    C3FocusLongitudeDeg = FRotator::NormalizeAxis(FMath::RadiansToDegrees(
        FMath::Atan2(static_cast<float>(C3FocusUnitDir.Y), static_cast<float>(C3FocusUnitDir.X))));

    Tess->ApplyFocusCameraState(
        C3FocusUnitDir,
        C3DistanceToFocusCM,
        C3TiltDeg,
        C3YawAroundFocusDeg);
}
