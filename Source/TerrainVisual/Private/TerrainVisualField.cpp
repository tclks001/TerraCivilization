#include "TerrainVisualField.h"

#include "CellGeoData.h"
#include "FSphereTopology.h"
#include "FSphereTopologyQuery.h"

bool FTerrainVisualField::Initialize(
    const FSphereTopology& InCellTopology,
    const TArray<FCellGeoData>& InGeoCells,
    const TArray<FIntPoint>& InMountainRidgeSegments,
    const FTerrainVisualConfig& InConfig,
    FString& OutError)
{
    Reset();
    OutError.Reset();

    const int32 CellCount = InCellTopology.Cells.Num();
    if (CellCount <= 0)
    {
        OutError = TEXT("TerrainVisual requires a built CellTopology with at least one Cell.");
        return false;
    }
    if (InGeoCells.Num() != CellCount)
    {
        OutError = FString::Printf(
            TEXT("TerrainVisual GeoCells count mismatch. GeoCells=%d CellTopology=%d."),
            InGeoCells.Num(),
            CellCount);
        return false;
    }
    if (InConfig.GlobeRadiusCM <= KINDA_SMALL_NUMBER)
    {
        OutError = FString::Printf(
            TEXT("TerrainVisual GlobeRadiusCM must be positive. Got=%.3f."),
            InConfig.GlobeRadiusCM);
        return false;
    }

    CellTopology = &InCellTopology;
    GeoCells = &InGeoCells;
    Config = InConfig;

    for (int32 CellId = 0; CellId < CellCount; ++CellId)
    {
        const FCellGeoData& GeoCell = InGeoCells[CellId];
        const FCell& Cell = InCellTopology.Cells[CellId];
        float NearestNeighborRadians = UE_BIG_NUMBER;
        for (const int32 NeighborId : Cell.NeighborCellIds)
        {
            if (InCellTopology.Cells.IsValidIndex(NeighborId))
            {
                const float Dot = FVector::DotProduct(Cell.UnitCenter, InCellTopology.Cells[NeighborId].UnitCenter);
                NearestNeighborRadians = FMath::Min(NearestNeighborRadians, FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
            }
        }

        if (NearestNeighborRadians == UE_BIG_NUMBER)
        {
            continue;
        }

        FLandformSource Source;
        Source.UnitCenter = Cell.UnitCenter.GetSafeNormal();
        Source.SupportRadians = NearestNeighborRadians * FMath::Max(InConfig.LandformSupportScale, 1.0f);
        if (GeoCell.SimpleTerrainType == ETerraSimpleTerrainType::Forest)
        {
            ForestSources.Add(Source);
        }
    }

    for (const FIntPoint& SourceSegment : InMountainRidgeSegments)
    {
        if (!InCellTopology.Cells.IsValidIndex(SourceSegment.X)
            || !InCellTopology.Cells.IsValidIndex(SourceSegment.Y)
            || SourceSegment.X == SourceSegment.Y)
        {
            continue;
        }

        const FVector StartUnit = InCellTopology.Cells[SourceSegment.X].UnitCenter.GetSafeNormal();
        const FVector EndUnit = InCellTopology.Cells[SourceSegment.Y].UnitCenter.GetSafeNormal();
        const float ArcRadians = FMath::Acos(FMath::Clamp(FVector::DotProduct(StartUnit, EndUnit), -1.0f, 1.0f));
        if (StartUnit.IsNearlyZero() || EndUnit.IsNearlyZero() || ArcRadians <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        FRidgeSegment& Ridge = MountainRidgeSegments.AddDefaulted_GetRef();
        Ridge.StartUnit = StartUnit;
        Ridge.EndUnit = EndUnit;
        Ridge.SupportRadians = ArcRadians * FMath::Max(InConfig.LandformSupportScale, 1.0f);
    }
    return true;
}

void FTerrainVisualField::Reset()
{
    CellTopology = nullptr;
    GeoCells = nullptr;
    Config = FTerrainVisualConfig();
    MountainRidgeSegments.Reset();
    ForestSources.Reset();
}

FTerrainSurfaceQueryResult FTerrainVisualField::QueryBaseSurface(const FVector& UnitDirection) const
{
    FTerrainSurfaceQueryResult Result;
    if (!IsInitialized())
    {
        return Result;
    }

    const FVector SafeDirection = UnitDirection.GetSafeNormal();
    if (SafeDirection.IsNearlyZero())
    {
        return Result;
    }

    Result.SurfaceRadiusCM = EvaluateSurfaceRadiusCM_(SafeDirection);
    Result.WorldPosition = Config.PlanetCenterWorld + SafeDirection * Result.SurfaceRadiusCM;
    Result.WorldNormal = EvaluateSurfaceNormal_(SafeDirection);
    Result.CellId = ResolveCellId(SafeDirection);
    Result.bIsValid = true;
    Result.bHasContinuousSurface = false;
    return Result;
}

float FTerrainVisualField::EvaluateMacroHeightCM_(const FVector& UnitDirection) const
{
    const float MountainHeight = EvaluateMountainRidgeHeightCM_(UnitDirection);
    const float ForestHeight = EvaluateForestHeightCM_(UnitDirection);
    return MountainHeight + ForestHeight;
}

float FTerrainVisualField::EvaluateMountainRidgeHeightCM_(const FVector& UnitDirection) const
{
    if (MountainRidgeSegments.IsEmpty() || Config.MountainHeightCM <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    float MinimumDistance = UE_BIG_NUMBER;
    float SupportRadians = 0.0f;
    for (const FRidgeSegment& Ridge : MountainRidgeSegments)
    {
        const float Distance = DistanceToGreatCircleArcRadians_(UnitDirection, Ridge);
        if (Distance < MinimumDistance)
        {
            MinimumDistance = Distance;
            SupportRadians = Ridge.SupportRadians;
        }
    }

    if (MinimumDistance == UE_BIG_NUMBER || SupportRadians <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    const float SdfInterior = FMath::Max(0.0f, 1.0f - MinimumDistance / SupportRadians);
    return Config.MountainHeightCM * FMath::Pow(SdfInterior, FMath::Max(Config.MountainFalloffExponent, 1.0f));
}

float FTerrainVisualField::EvaluateForestHeightCM_(const FVector& UnitDirection) const
{
    if (Config.ForestHeightCM <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    float CombinedWeight = 0.0f;
    for (const FLandformSource& Source : ForestSources)
    {
        if (Source.SupportRadians <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        const float AngularDistance = FMath::Acos(FMath::Clamp(FVector::DotProduct(UnitDirection, Source.UnitCenter), -1.0f, 1.0f));
        const float SdfInterior = FMath::Max(0.0f, 1.0f - AngularDistance / Source.SupportRadians);
        // The normalized sigmoid has zero at the forest boundary and one at the
        // center, avoiding the sharp edge produced by sqrt(SdfInterior).
        const float Weight = EvaluateNormalizedSigmoid_(SdfInterior, Config.ForestSigmoidSteepness);
        CombinedWeight = 1.0f - (1.0f - CombinedWeight) * (1.0f - Weight);
    }
    return Config.ForestHeightCM * CombinedWeight;
}

float FTerrainVisualField::DistanceToGreatCircleArcRadians_(const FVector& UnitDirection, const FRidgeSegment& Segment)
{
    const FVector GreatCircleNormal = FVector::CrossProduct(Segment.StartUnit, Segment.EndUnit).GetSafeNormal();
    if (GreatCircleNormal.IsNearlyZero())
    {
        return FMath::Min(
            FMath::Acos(FMath::Clamp(FVector::DotProduct(UnitDirection, Segment.StartUnit), -1.0f, 1.0f)),
            FMath::Acos(FMath::Clamp(FVector::DotProduct(UnitDirection, Segment.EndUnit), -1.0f, 1.0f)));
    }

    const FVector Projected = UnitDirection - GreatCircleNormal * FVector::DotProduct(UnitDirection, GreatCircleNormal);
    const FVector Foot = Projected.GetSafeNormal();
    const float ArcLength = FMath::Acos(FMath::Clamp(FVector::DotProduct(Segment.StartUnit, Segment.EndUnit), -1.0f, 1.0f));
    if (!Foot.IsNearlyZero())
    {
        const float StartToFoot = FMath::Acos(FMath::Clamp(FVector::DotProduct(Segment.StartUnit, Foot), -1.0f, 1.0f));
        const float FootToEnd = FMath::Acos(FMath::Clamp(FVector::DotProduct(Foot, Segment.EndUnit), -1.0f, 1.0f));
        if (StartToFoot + FootToEnd <= ArcLength + 1e-4f)
        {
            return FMath::Acos(FMath::Clamp(FVector::DotProduct(UnitDirection, Foot), -1.0f, 1.0f));
        }
    }

    return FMath::Min(
        FMath::Acos(FMath::Clamp(FVector::DotProduct(UnitDirection, Segment.StartUnit), -1.0f, 1.0f)),
        FMath::Acos(FMath::Clamp(FVector::DotProduct(UnitDirection, Segment.EndUnit), -1.0f, 1.0f)));
}

float FTerrainVisualField::EvaluateNormalizedSigmoid_(float Interior, float Steepness)
{
    const float ClampedInterior = FMath::Clamp(Interior, 0.0f, 1.0f);
    const float SafeSteepness = FMath::Max(Steepness, KINDA_SMALL_NUMBER);
    const auto Logistic = [SafeSteepness](float X)
    {
        return 1.0f / (1.0f + FMath::Exp(-SafeSteepness * (X - 0.5f)));
    };

    const float AtZero = Logistic(0.0f);
    const float AtOne = Logistic(1.0f);
    return FMath::Clamp((Logistic(ClampedInterior) - AtZero) / FMath::Max(AtOne - AtZero, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
}

float FTerrainVisualField::EvaluateSurfaceRadiusCM_(const FVector& UnitDirection) const
{
    return Config.GlobeRadiusCM + EvaluateMacroHeightCM_(UnitDirection);
}

FVector FTerrainVisualField::EvaluateSurfaceNormal_(const FVector& UnitDirection) const
{
    FVector TangentX = FVector::VectorPlaneProject(FVector::ForwardVector, UnitDirection).GetSafeNormal();
    if (TangentX.IsNearlyZero())
    {
        TangentX = FVector::VectorPlaneProject(FVector::RightVector, UnitDirection).GetSafeNormal();
    }
    const FVector TangentY = FVector::CrossProduct(UnitDirection, TangentX).GetSafeNormal();
    if (TangentX.IsNearlyZero() || TangentY.IsNearlyZero())
    {
        return UnitDirection;
    }

    constexpr float SampleRadians = 0.0015f;
    const FVector SampleX = (UnitDirection + TangentX * SampleRadians).GetSafeNormal();
    const FVector SampleY = (UnitDirection + TangentY * SampleRadians).GetSafeNormal();
    const FVector Position = UnitDirection * EvaluateSurfaceRadiusCM_(UnitDirection);
    const FVector PositionX = SampleX * EvaluateSurfaceRadiusCM_(SampleX);
    const FVector PositionY = SampleY * EvaluateSurfaceRadiusCM_(SampleY);
    FVector Normal = FVector::CrossProduct(PositionY - Position, PositionX - Position).GetSafeNormal();
    if (FVector::DotProduct(Normal, UnitDirection) < 0.0f)
    {
        Normal = -Normal;
    }
    return Normal.IsNearlyZero() ? UnitDirection : Normal;
}

int32 FTerrainVisualField::ResolveCellId(const FVector& UnitDirection) const
{
    const FVector SafeDirection = UnitDirection.GetSafeNormal();
    if (!CellTopology || SafeDirection.IsNearlyZero())
    {
        return INDEX_NONE;
    }

    const FSphereTopologyQuery Query(CellTopology);
    return Query.FindNearestCell(SafeDirection).CellId;
}
