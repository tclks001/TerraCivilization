// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/CellHighlightComponent.h"

#include "Interaction/PlanetBinder.h"

#include "FCell.h"
#include "FCorner.h"
#include "FSphereTopology.h"

#include "PtgManager.h"

#include "Components/LineBatchComponent.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogCellHighlight, Log, All);

UCellHighlightComponent::UCellHighlightComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

FVector UCellHighlightComponent::SlerpUnit(const FVector& A, const FVector& B, float T)
{
    const float Dot = FMath::Clamp(FVector::DotProduct(A, B), -1.f, 1.f);
    const float Theta = FMath::Acos(Dot);
    if (Theta < KINDA_SMALL_NUMBER)
    {
        return A;
    }
    const float SinTheta = FMath::Sin(Theta);
    const float W1 = FMath::Sin((1.f - T) * Theta) / SinTheta;
    const float W2 = FMath::Sin(T        * Theta) / SinTheta;
    return (A * W1 + B * W2);
}

APlanetBinder* UCellHighlightComponent::GetBinder() const
{
    return Cast<APlanetBinder>(GetOwner());
}

void UCellHighlightComponent::SetHighlightedCell(int32 CellId)
{
    if (CellId == INDEX_NONE)
    {
        ClearHighlight();
        return;
    }
    CurrentCellId = CellId;
    RebuildLines();
}

void UCellHighlightComponent::ClearHighlight()
{
    CurrentCellId = INDEX_NONE;
    // LineBatcher 的瞬时线会自动随 LifeTime 过期；这里不主动 Flush，
    // 避免误清掉同帧其他系统画的调试线。
}

void UCellHighlightComponent::RebuildLines()
{
    APlanetBinder* Binder = GetBinder();
    if (!Binder) return;

    const FSphereTopology* Topology = Binder->GetTopology();
    APtgManager* PtgManager = Binder->GetPtgManager();
    if (!Topology || !PtgManager) return;

    if (CurrentCellId < 0 || CurrentCellId >= Topology->Cells.Num()) return;

    UWorld* World = GetWorld();
    if (!World) return;

    // UE 5.6+：World->LineBatcher 已废弃；改用 GetLineBatcher(ELineBatcherType::World)。
    // 这种 LineBatcher 适合"瞬时"线（每帧重画），生命周期由 LineLifeTime 控制。
    ULineBatchComponent* LB = World->GetLineBatcher(UWorld::ELineBatcherType::World);
    if (!LB) return;

    const FCell& Cell = Topology->Cells[CurrentCellId];
    const FVector Center = PtgManager->GetActorLocation();
    const float   Radius = Binder->GetRadius();
    const float   DrawR  = Radius + LiftOffset;
    const FLinearColor Color = Cell.bIsPentagon ? PentagonHighlightColor : HighlightColor;

    // 邻居数 = 边数 = 角数：5（pentagon）或 6（hexagon）。
    int32 N = 0;
    while (N < 6 && Cell.CornerIds[N] != INDEX_NONE) ++N;
    if (N < 3) return;

    // 拓扑已经过 CW 环对齐：CornerIds[i] 与 CornerIds[(i+1)%N] 即第 i 条边的两端。
    // 因此沿环走一圈、对每条边做 Slerp 采样、首尾相接 DrawLine 即可。
    const int32 Segs = FMath::Max(1, SegmentsPerEdge);
    TArray<FVector> Pts;
    Pts.Reserve(N * Segs);

    for (int32 I = 0; I < N; ++I)
    {
        const int32 CornerA = Cell.CornerIds[I];
        const int32 CornerB = Cell.CornerIds[(I + 1) % N];
        if (CornerA == INDEX_NONE || CornerB == INDEX_NONE) continue;

        const FVector UnitA = Topology->Corners[CornerA].UnitDir;
        const FVector UnitB = Topology->Corners[CornerB].UnitDir;

        // 每条边的 [0, 1) 段；最后一段会由下一条边的起点接上。
        for (int32 S = 0; S < Segs; ++S)
        {
            const float T = static_cast<float>(S) / static_cast<float>(Segs);
            const FVector Unit = SlerpUnit(UnitA, UnitB, T).GetSafeNormal();
            Pts.Add(Unit * DrawR + Center);
        }
    }

    if (Pts.Num() < 2) return;

    // 闭环：把每个相邻点对画成线段；最后一段连回起点。
    for (int32 I = 0; I < Pts.Num(); ++I)
    {
        const FVector& P0 = Pts[I];
        const FVector& P1 = Pts[(I + 1) % Pts.Num()];
        LB->DrawLine(P0, P1, Color, /*DepthPriority=*/SDPG_Foreground, Thickness, LineLifeTime);
    }
}
