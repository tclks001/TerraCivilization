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
    InitializeNoise_();

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
    const float MountainWeight = EvaluateMountainRidgeWeight_(UnitDirection);
    const float MountainHeight = Config.MountainHeightCM
        * FMath::Pow(MountainWeight, FMath::Max(Config.MountainFalloffExponent, 1.0f));
    const float ForestHeight = EvaluateForestHeightCM_(UnitDirection);
    return MountainHeight + ForestHeight + EvaluateMediumFrequencyHeightCM_(UnitDirection, MountainWeight);
}

float FTerrainVisualField::EvaluateMediumFrequencyHeightCM_(const FVector& UnitDirection, float MountainWeight) const
{
    if (!Config.bEnableMediumFrequencyErosion)
    {
        return 0.0f;
    }

    const FVector NoisePosition = GetNoisePosition_(UnitDirection);
    const float CrestVariation = CrestNoise.GetNoise(NoisePosition.X, NoisePosition.Y, NoisePosition.Z)
        * FMath::Max(Config.CrestNoiseAmplitudeCM, 0.0f)
        * MountainWeight;

    float WarpedX = NoisePosition.X;
    float WarpedY = NoisePosition.Y;
    float WarpedZ = NoisePosition.Z;
    ErosionWarpNoise.DomainWarp(WarpedX, WarpedY, WarpedZ);
    const float RidgedNoise = ErosionNoise.GetNoise(WarpedX, WarpedY, WarpedZ);
    const float ValleyMask = FMath::Pow(
        1.0f - FMath::Clamp((RidgedNoise + 1.0f) * 0.5f, 0.0f, 1.0f),
        FMath::Max(Config.ErosionValleySharpness, 0.01f));
    const float SlopeMask = 4.0f * MountainWeight * (1.0f - MountainWeight);
    const float MountainErosion = FMath::Max(Config.ErosionAmplitudeCM, 0.0f) * SlopeMask * ValleyMask;

    const float LowlandMask = 1.0f - MountainWeight;
    const float LowlandUndulation = LowlandNoise.GetNoise(NoisePosition.X, NoisePosition.Y, NoisePosition.Z)
        * FMath::Max(Config.LowlandNoiseAmplitudeCM, 0.0f)
        * LowlandMask;
    return CrestVariation - MountainErosion + LowlandUndulation;
}

float FTerrainVisualField::EvaluateMountainRidgeWeight_(const FVector& UnitDirection) const
{
    if (MountainRidgeSegments.IsEmpty())
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

    return FMath::Max(0.0f, 1.0f - MinimumDistance / SupportRadians);
}

float FTerrainVisualField::EvaluateMountainRidgeHeightCM_(const FVector& UnitDirection) const
{
    return Config.MountainHeightCM
        * FMath::Pow(EvaluateMountainRidgeWeight_(UnitDirection), FMath::Max(Config.MountainFalloffExponent, 1.0f));
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

FVector FTerrainVisualField::GetNoisePosition_(const FVector& UnitDirection) const
{
    return NoiseRotation.RotateVector(UnitDirection.GetSafeNormal());
}

void FTerrainVisualField::InitializeNoise_()
{
    FRandomStream Random(Config.GlobalVisualSeed ^ 0x4D455252);
    FVector RotationAxis(
        Random.FRandRange(-1.0f, 1.0f),
        Random.FRandRange(-1.0f, 1.0f),
        Random.FRandRange(-1.0f, 1.0f));
    if (RotationAxis.IsNearlyZero())
    {
        RotationAxis = FVector::UpVector;
    }
    NoiseRotation = FQuat(RotationAxis.GetSafeNormal(), FMath::DegreesToRadians(Random.FRandRange(0.0f, 360.0f)));

    const float Frequency = FMath::Max(Config.MediumFrequencyNoiseFrequency, 0.001f);
    CrestNoise = FastNoiseLite(Config.GlobalVisualSeed + 101);
    CrestNoise.SetNoiseType(FastNoiseLite::NoiseType::NoiseType_OpenSimplex2S);
    CrestNoise.SetFractalType(FastNoiseLite::FractalType::FractalType_FBm);
    CrestNoise.SetFractalOctaves(4);
    CrestNoise.SetFractalGain(0.5f);
    CrestNoise.SetFrequency(Frequency);

    ErosionWarpNoise = FastNoiseLite(Config.GlobalVisualSeed + 211);
    ErosionWarpNoise.SetNoiseType(FastNoiseLite::NoiseType::NoiseType_OpenSimplex2S);
    ErosionWarpNoise.SetFractalType(FastNoiseLite::FractalType::FractalType_DomainWarpProgressive);
    ErosionWarpNoise.SetFractalOctaves(3);
    ErosionWarpNoise.SetFrequency(Frequency * 0.8f);
    ErosionWarpNoise.SetDomainWarpType(FastNoiseLite::DomainWarpType::DomainWarpType_OpenSimplex2Reduced);
    ErosionWarpNoise.SetDomainWarpAmp(FMath::Max(Config.ErosionDomainWarpAmplitude, 0.0f));

    ErosionNoise = FastNoiseLite(Config.GlobalVisualSeed + 307);
    ErosionNoise.SetNoiseType(FastNoiseLite::NoiseType::NoiseType_OpenSimplex2S);
    ErosionNoise.SetFractalType(FastNoiseLite::FractalType::FractalType_Ridged);
    ErosionNoise.SetFractalOctaves(3);
    ErosionNoise.SetFractalGain(0.55f);
    ErosionNoise.SetFrequency(Frequency * 1.35f);

    LowlandNoise = FastNoiseLite(Config.GlobalVisualSeed + 401);
    LowlandNoise.SetNoiseType(FastNoiseLite::NoiseType::NoiseType_OpenSimplex2S);
    LowlandNoise.SetFractalType(FastNoiseLite::FractalType::FractalType_FBm);
    LowlandNoise.SetFractalOctaves(2);
    LowlandNoise.SetFractalGain(0.5f);
    LowlandNoise.SetFrequency(Frequency * 0.55f);
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
