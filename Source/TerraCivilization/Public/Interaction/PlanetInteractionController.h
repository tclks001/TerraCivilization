// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PlanetInteractionController.generated.h"

class APlanetBinder;
class APlanetTessellatedMesh;

/**
 * APlanetInteractionController
 *
 * Step 3 阶段最小输入层：每帧从鼠标位置发一根 ECC_Visibility 射线，
 * 命中 PTG Manager 时把 ImpactPoint 转交给 APlanetBinder::OnHoverWorldPoint。
 *
 * 设计原则（详见 Docs/HexHighlightInteractionPlan.md 5.1 / 11.3）：
 * - 不依赖 EnhancedInput，复用 GetHitResultUnderCursorByChannel 即可；
 * - bTraceComplex = true 是必须的，RuntimeMeshComponent 默认开复杂碰撞；
 * - 只有命中目标 == Binder->GetPtgManager() 时才上报，避免触发其他 Actor。
 */
UCLASS()
class TERRACIVILIZATION_API APlanetInteractionController : public APlayerController
{
    GENERATED_BODY()

public:
    APlanetInteractionController();

    //~ APlayerController
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaTime) override;
    //~ End APlayerController

    /** 鼠标命中通道。可在 BP 中改成自定义通道（例如 ECC_GameTraceChannel1）。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Interaction")
    TEnumAsByte<ECollisionChannel> HoverTraceChannel = ECC_Visibility;

private:
    /** G8：是否已经从当前视角同步过手动轨道相机状态。 */
    bool bG8OrbitCameraInitialized = false;

    /** G8：当前手动轨道相机经度（度）。 */
    float G8CameraLongitudeDeg = 0.0f;

    /** G8：当前手动轨道相机纬度（度）。 */
    float G8CameraLatitudeDeg = 0.0f;

    /** G8：当前手动轨道相机离球面高度（cm）。 */
    float G8CameraHeightOffsetCM = 8000.0f;

    /** 在关卡里自动查找的 Binder 缓存。 */
    UPROPERTY(Transient)
    TObjectPtr<APlanetBinder> CachedBinder;

    /** 上一帧是否处于 hover 状态，用于 enter/leave 边沿判定。 */
    bool bWasHovering = false;

    /** G8：从当前摄像机位置同步一次轨道相机状态。 */
    bool InitializeG8OrbitCameraState_(APlanetTessellatedMesh* Tess);

    /** G8：手动球面轨道相机控制。 */
    void UpdateG8ManualCameraControl_(float DeltaTime, APlanetTessellatedMesh* Tess);
};
