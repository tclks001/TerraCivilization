// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldGenerator.h"

#include "FCell.h"
#include "FSphereTopology.h"
#include "FSphereTopologyQuery.h"
#include "WorldGenLog.h"

FWorldGenerator::FWorldGenerator(FSphereTopology* InTopology, const FWorldGenSettings& InSettings)
    : Topology(InTopology)
    , Settings(InSettings)
{
}

void FWorldGenerator::Generate()
{
    check(Topology);

    Rng.Initialize(Settings.RandomSeed);

    InitSimpleTerrainAsPlain();
    CollectSimpleBaseCells();
    BuildSimpleProtectedCells(/*Radius=*/2);
    GenerateMountainStrips();
    GenerateForestPatches();
    WriteSimpleTerrainToCellData();
    LogSimpleTerrainSummary();
}

void FWorldGenerator::InitSimpleTerrainAsPlain()
{
    const int32 N = Topology ? Topology->Cells.Num() : 0;

    CellData.SetNum(N);
    SimpleTerrainField.Init(ETerraSimpleTerrainType::Plain, N);
    bProtectedCell.Init(false, N);
    BaseCellIds.Reset();

    LastProtectedCount = 0;
    LastMountainCount = 0;
    LastForestCount = 0;
    LastCompletedMountainStripCount = 0;
    LastCompletedForestPatchCount = 0;
    LastStoppedMountainStripCount = 0;
    LastStoppedForestPatchCount = 0;

    for (int32 CellId = 0; CellId < N; ++CellId)
    {
        FCellGeoData& CD = CellData[CellId];
        CD = FCellGeoData();
        CD.CellId = CellId;
        CD.bIsPentagon = Topology->Cells[CellId].bIsPentagon ? 1 : 0;
        CD.SimpleTerrainType = ETerraSimpleTerrainType::Plain;
    }
}

void FWorldGenerator::CollectSimpleBaseCells()
{
    BaseCellIds.Reset();

    if (!Topology)
    {
        return;
    }

    for (const FCell& Cell : Topology->Cells)
    {
        if (Cell.bIsPentagon)
        {
            BaseCellIds.Add(Cell.CellId);
        }
    }

    BaseCellIds.Sort();

    if (BaseCellIds.Num() != 12)
    {
        UE_LOG(LogWorldGen, Warning,
            TEXT("[WorldGen] SimpleGameplay expected 12 pentagon bases, got %d."),
            BaseCellIds.Num());
    }
}

void FWorldGenerator::BuildSimpleProtectedCells(int32 Radius)
{
    if (!Topology)
    {
        return;
    }

    FSphereTopologyQuery Query(Topology);
    TArray<int32> Disk;

    for (const int32 BaseCellId : BaseCellIds)
    {
        Disk.Reset();
        Query.CollectCellDisk(BaseCellId, Radius, Disk);
        for (const int32 CellId : Disk)
        {
            if (!IsValidCellId(CellId))
            {
                continue;
            }

            if (!bProtectedCell[CellId])
            {
                bProtectedCell[CellId] = true;
                ++LastProtectedCount;
            }

            SimpleTerrainField[CellId] = ETerraSimpleTerrainType::Plain;
        }
    }
}

void FWorldGenerator::GenerateMountainStrips()
{
    const int32 StripCount = FMath::Max(0, Settings.MountainStripCount);
    for (int32 StripIndex = 0; StripIndex < StripCount; ++StripIndex)
    {
        const int32 TargetCount = FMath::Max(2, SamplePositiveNormalCount(Settings.AverageMountainCellCount));
        const int32 BeforeCount = LastMountainCount;
        GrowMountainStrip(TargetCount);

        if (LastMountainCount > BeforeCount)
        {
            ++LastCompletedMountainStripCount;
            if (LastMountainCount - BeforeCount < TargetCount)
            {
                ++LastStoppedMountainStripCount;
            }
        }
        else
        {
            ++LastStoppedMountainStripCount;
        }
    }
}

void FWorldGenerator::GenerateForestPatches()
{
    const int32 PatchCount = FMath::Max(0, Settings.ForestPatchCount);
    for (int32 PatchIndex = 0; PatchIndex < PatchCount; ++PatchIndex)
    {
        const int32 TargetCount = SamplePositiveNormalCount(Settings.AverageForestCellCount);
        const int32 BeforeCount = LastForestCount;
        GrowForestPatch(TargetCount);

        if (LastForestCount > BeforeCount)
        {
            ++LastCompletedForestPatchCount;
            if (LastForestCount - BeforeCount < TargetCount)
            {
                ++LastStoppedForestPatchCount;
            }
        }
        else
        {
            ++LastStoppedForestPatchCount;
        }
    }
}

void FWorldGenerator::WriteSimpleTerrainToCellData()
{
    LastMountainCount = 0;
    LastForestCount = 0;

    for (int32 CellId = 0; CellId < CellData.Num(); ++CellId)
    {
        if (IsProtectedCell(CellId))
        {
            SimpleTerrainField[CellId] = ETerraSimpleTerrainType::Plain;
        }

        FCellGeoData& CD = CellData[CellId];
        CD.SimpleTerrainType = SimpleTerrainField[CellId];
        CD.BaseFactionId = INDEX_NONE;

        switch (SimpleTerrainField[CellId])
        {
            case ETerraSimpleTerrainType::Mountain:
                ++LastMountainCount;
                break;

            case ETerraSimpleTerrainType::Forest:
                ++LastForestCount;
                break;

            case ETerraSimpleTerrainType::Plain:
            default:
                break;
        }
    }

    for (int32 FactionId = 0; FactionId < BaseCellIds.Num(); ++FactionId)
    {
        const int32 BaseCellId = BaseCellIds[FactionId];
        if (CellData.IsValidIndex(BaseCellId))
        {
            CellData[BaseCellId].BaseFactionId = FactionId;
        }
    }
}

void FWorldGenerator::LogSimpleTerrainSummary() const
{
    const int32 N = CellData.Num();
    const int32 PlainCount = N - LastMountainCount - LastForestCount;

    UE_LOG(LogWorldGen, Log,
        TEXT("[WorldGen] SimpleGameplay OK, Cells=%d, Bases=%d, Protected=%d, Plain=%d, Mountain=%d, Forest=%d, ")
        TEXT("MountainStrips=%d/%d stopped=%d, ForestPatches=%d/%d stopped=%d, Seed=%d"),
        N,
        BaseCellIds.Num(),
        LastProtectedCount,
        PlainCount,
        LastMountainCount,
        LastForestCount,
        LastCompletedMountainStripCount,
        FMath::Max(0, Settings.MountainStripCount),
        LastStoppedMountainStripCount,
        LastCompletedForestPatchCount,
        FMath::Max(0, Settings.ForestPatchCount),
        LastStoppedForestPatchCount,
        Settings.RandomSeed);
}

int32 FWorldGenerator::SamplePositiveNormalCount(float AverageCount) const
{
    const float Mean = FMath::Max(1.0f, AverageCount);
    const float StdDev = FMath::Max(0.001f, Mean * 0.25f);

    const float U1 = FMath::Max(KINDA_SMALL_NUMBER, Rng.GetFraction());
    const float U2 = Rng.GetFraction();
    const float Z0 = FMath::Sqrt(-2.0f * FMath::Loge(U1)) * FMath::Cos(2.0f * PI * U2);
    const float Sample = Mean + Z0 * StdDev;

    return FMath::Max(1, FMath::RoundToInt(Sample));
}

bool FWorldGenerator::IsValidCellId(int32 CellId) const
{
    return Topology && Topology->Cells.IsValidIndex(CellId);
}

bool FWorldGenerator::IsProtectedCell(int32 CellId) const
{
    return bProtectedCell.IsValidIndex(CellId) && bProtectedCell[CellId];
}

bool FWorldGenerator::IsValidMountainCell(int32 CellId) const
{
    return IsValidCellId(CellId)
        && !IsProtectedCell(CellId)
        && !Topology->Cells[CellId].bIsPentagon
        && SimpleTerrainField.IsValidIndex(CellId)
        && SimpleTerrainField[CellId] != ETerraSimpleTerrainType::Mountain;
}

bool FWorldGenerator::IsValidForestCell(int32 CellId) const
{
    return IsValidCellId(CellId)
        && !IsProtectedCell(CellId)
        && !Topology->Cells[CellId].bIsPentagon
        && SimpleTerrainField.IsValidIndex(CellId)
        && SimpleTerrainField[CellId] == ETerraSimpleTerrainType::Plain;
}

int32 FWorldGenerator::FindNeighborIndex(int32 CurCellId, int32 NeighborCellId) const
{
    if (!IsValidCellId(CurCellId))
    {
        return INDEX_NONE;
    }

    const FCell& CurCell = Topology->Cells[CurCellId];
    for (int32 Index = 0; Index < 6; ++Index)
    {
        if (CurCell.NeighborCellIds[Index] == NeighborCellId)
        {
            return Index;
        }
    }

    return INDEX_NONE;
}

bool FWorldGenerator::PickRandomValidMountainSeedPair(int32& OutA, int32& OutB)
{
    OutA = INDEX_NONE;
    OutB = INDEX_NONE;

    TArray<FIntPoint> CandidatePairs;
    for (int32 CellId = 0; CellId < SimpleTerrainField.Num(); ++CellId)
    {
        if (!IsValidMountainCell(CellId))
        {
            continue;
        }

        for (const int32 NeighborId : Topology->Cells[CellId].NeighborCellIds)
        {
            if (!IsValidMountainCell(NeighborId) || CellId >= NeighborId)
            {
                continue;
            }
            CandidatePairs.Emplace(CellId, NeighborId);
        }
    }

    if (CandidatePairs.IsEmpty())
    {
        return false;
    }

    const FIntPoint PickedPair = CandidatePairs[Rng.RandRange(0, CandidatePairs.Num() - 1)];
    OutA = PickedPair.X;
    OutB = PickedPair.Y;
    return true;
}

void FWorldGenerator::GrowMountainStrip(int32 TargetCount)
{
    int32 SeedA = INDEX_NONE;
    int32 SeedB = INDEX_NONE;
    if (!PickRandomValidMountainSeedPair(SeedA, SeedB))
    {
        return;
    }

    SimpleTerrainField[SeedA] = ETerraSimpleTerrainType::Mountain;
    SimpleTerrainField[SeedB] = ETerraSimpleTerrainType::Mountain;
    LastMountainCount += 2;

    FMountainGrowTip TipA;
    TipA.PrevCellId = SeedB;
    TipA.CurCellId = SeedA;
    TipA.bActive = true;

    FMountainGrowTip TipB;
    TipB.PrevCellId = SeedA;
    TipB.CurCellId = SeedB;
    TipB.bActive = true;

    int32 RemainingCount = FMath::Max(0, TargetCount - 2);
    while (RemainingCount > 0 && (TipA.bActive || TipB.bActive))
    {
        TryGrowMountainTip(TipA, RemainingCount);
        if (RemainingCount <= 0)
        {
            break;
        }
        TryGrowMountainTip(TipB, RemainingCount);
    }
}

bool FWorldGenerator::TryGrowMountainTip(FMountainGrowTip& Tip, int32& RemainingCount)
{
    if (!Tip.bActive || RemainingCount <= 0)
    {
        return false;
    }

    const int32 NextCellId = PickMountainNextCell(Tip.PrevCellId, Tip.CurCellId);
    if (!IsValidMountainCell(NextCellId))
    {
        Tip.bActive = false;
        return false;
    }

    SimpleTerrainField[NextCellId] = ETerraSimpleTerrainType::Mountain;
    ++LastMountainCount;
    --RemainingCount;

    Tip.PrevCellId = Tip.CurCellId;
    Tip.CurCellId = NextCellId;
    return true;
}

int32 FWorldGenerator::PickMountainNextCell(int32 PrevCellId, int32 CurCellId)
{
    if (!IsValidCellId(PrevCellId) || !IsValidCellId(CurCellId))
    {
        return INDEX_NONE;
    }

    const FCell& CurCell = Topology->Cells[CurCellId];
    if (CurCell.bIsPentagon)
    {
        return INDEX_NONE;
    }

    const int32 PrevIndex = FindNeighborIndex(CurCellId, PrevCellId);
    if (PrevIndex == INDEX_NONE)
    {
        return INDEX_NONE;
    }

    const float Roll = Rng.GetFraction();
    int32 Offset = 3;
    if (Roll >= 0.5f && Roll < 0.75f)
    {
        Offset = 2;
    }
    else if (Roll >= 0.75f)
    {
        Offset = 4;
    }

    const int32 NextIndex = (PrevIndex + Offset) % 6;
    return CurCell.NeighborCellIds[NextIndex];
}

bool FWorldGenerator::PickRandomValidForestSeed(int32& OutSeed)
{
    OutSeed = INDEX_NONE;

    TArray<int32> Candidates;
    for (int32 CellId = 0; CellId < SimpleTerrainField.Num(); ++CellId)
    {
        if (IsValidForestCell(CellId))
        {
            Candidates.Add(CellId);
        }
    }

    if (Candidates.IsEmpty())
    {
        return false;
    }

    OutSeed = Candidates[Rng.RandRange(0, Candidates.Num() - 1)];
    return true;
}

void FWorldGenerator::GrowForestPatch(int32 TargetCount)
{
    int32 SeedCellId = INDEX_NONE;
    if (!PickRandomValidForestSeed(SeedCellId))
    {
        return;
    }

    TArray<int32> ForestCells;
    ForestCells.Reserve(TargetCount);

    SimpleTerrainField[SeedCellId] = ETerraSimpleTerrainType::Forest;
    ForestCells.Add(SeedCellId);
    ++LastForestCount;

    while (ForestCells.Num() < TargetCount)
    {
        const FVector CenterDir = ComputeForestCenter(ForestCells);

        TSet<int32> Frontier;
        CollectForestFrontier(ForestCells, Frontier);
        if (Frontier.IsEmpty())
        {
            break;
        }

        const int32 NextCellId = PickBestForestFrontierCell(Frontier, CenterDir);
        if (!IsValidForestCell(NextCellId))
        {
            break;
        }

        SimpleTerrainField[NextCellId] = ETerraSimpleTerrainType::Forest;
        ForestCells.Add(NextCellId);
        ++LastForestCount;
    }
}

FVector FWorldGenerator::ComputeForestCenter(const TArray<int32>& ForestCells) const
{
    FVector Sum = FVector::ZeroVector;
    for (const int32 CellId : ForestCells)
    {
        if (IsValidCellId(CellId))
        {
            Sum += Topology->Cells[CellId].UnitCenter;
        }
    }

    if (Sum.SizeSquared() > KINDA_SMALL_NUMBER)
    {
        return Sum.GetSafeNormal();
    }

    return ForestCells.Num() > 0 && IsValidCellId(ForestCells[0])
        ? Topology->Cells[ForestCells[0]].UnitCenter
        : FVector::ForwardVector;
}

void FWorldGenerator::CollectForestFrontier(const TArray<int32>& ForestCells, TSet<int32>& OutFrontier) const
{
    OutFrontier.Reset();

    for (const int32 ForestCellId : ForestCells)
    {
        if (!IsValidCellId(ForestCellId))
        {
            continue;
        }

        for (const int32 NeighborId : Topology->Cells[ForestCellId].NeighborCellIds)
        {
            if (IsValidForestCell(NeighborId))
            {
                OutFrontier.Add(NeighborId);
            }
        }
    }
}

int32 FWorldGenerator::PickBestForestFrontierCell(const TSet<int32>& Frontier, const FVector& CenterDir)
{
    constexpr float TieTolerance = 1e-5f;

    float BestDot = -2.0f;
    TArray<int32> BestCandidates;

    for (const int32 CandidateId : Frontier)
    {
        if (!IsValidForestCell(CandidateId))
        {
            continue;
        }

        const float Dot = FVector::DotProduct(Topology->Cells[CandidateId].UnitCenter, CenterDir);
        if (Dot > BestDot + TieTolerance)
        {
            BestDot = Dot;
            BestCandidates.Reset();
            BestCandidates.Add(CandidateId);
        }
        else if (FMath::Abs(Dot - BestDot) <= TieTolerance)
        {
            BestCandidates.Add(CandidateId);
        }
    }

    if (BestCandidates.IsEmpty())
    {
        return INDEX_NONE;
    }

    return BestCandidates[Rng.RandRange(0, BestCandidates.Num() - 1)];
}
