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
    /** C3：是否已经从当前视角同步过焦点式手动相机状态。 */
    bool bC3FocusCameraInitialized = false;

    /**
     * C3：球面视野中心方向的本地单位向量。这是焦点位置的**真值**。
     * 采用 3D 单位向量而非经纬度对，是为了让焦点可以平滑穿越南北极点，
     * 详见 Docs/SimpleGameplay/C3FocusCameraManualControlDesign.md §2。
     */
    FVector C3FocusUnitDir = FVector::ForwardVector;

    /** C3：球面视野中心经度（度）。仅供调试/显示，从 C3FocusUnitDir 派生。 */
    float C3FocusLongitudeDeg = 0.0f;

    /** C3：球面视野中心纬度（度）。仅供调试/显示，从 C3FocusUnitDir 派生。 */
    float C3FocusLatitudeDeg = 0.0f;

    /** C3：摄像机到球面视野中心的距离（cm）。 */
    float C3DistanceToFocusCM = 8000.0f;

    /** C3：视线中心方向与视野中心地面切平面的夹角（度）。 */
    float C3TiltDeg = 55.0f;

    /**
     * C3：绕视野中心外法线的观察方位（度）。
     * 与 ForwardTangent = cos(Yaw)·North + sin(Yaw)·East 保持同一约定。
     * 手动模式初始化后，此值只由 WSAD 输入的 Yaw 平行运输驱动，不再每帧从相机重解。
     */
    float C3YawAroundFocusDeg = 0.0f;

    /** 在关卡里自动查找的 Binder 缓存。 */
    UPROPERTY(Transient)
    TObjectPtr<APlanetBinder> CachedBinder;

    /** 上一帧是否处于 hover 状态，用于 enter/leave 边沿判定。 */
    bool bWasHovering = false;

    /** C3：从当前摄像机视图同步一次焦点式相机状态。 */
    bool InitializeC3FocusCameraState_(APlanetTessellatedMesh* Tess);

    /** C3：焦点式手动球面相机控制。 */
    void UpdateC3FocusCameraControl_(float DeltaTime, APlanetTessellatedMesh* Tess);
};
