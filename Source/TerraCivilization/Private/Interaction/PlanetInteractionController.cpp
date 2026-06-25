// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/PlanetInteractionController.h"

#include "Interaction/PlanetBinder.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "PtgManager.h"

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

    APtgManager* PtgManager = CachedBinder->GetPtgManager();
    if (!PtgManager)
    {
        return;
    }

    // 鼠标射线 -> ECC 通道命中。bTraceComplex = true 必须，RMC 默认开复杂碰撞。
    FHitResult Hit;
    const bool bHit = GetHitResultUnderCursorByChannel(
        UEngineTypes::ConvertToTraceType(HoverTraceChannel),
        /*bTraceComplex=*/true,
        Hit);

    const bool bHitPtg = bHit && Hit.GetActor() == PtgManager;

    if (bHitPtg)
    {
        CachedBinder->OnHoverWorldPoint(Hit.ImpactPoint);
        bWasHovering = true;
    }
    else if (bWasHovering)
    {
        // 边沿：从 hover 状态退出，通知一次 Leave，避免每帧重复调用。
        CachedBinder->OnLeavePlanet();
        bWasHovering = false;
    }
}
