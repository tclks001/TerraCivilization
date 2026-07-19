#include "TerrainVisualSurfaceComponent.h"

#include "FSphereTopology.h"
#include "FSphereTopologyQuery.h"
#include "TerrainSurfaceQuery.h"
#include "TerrainVisualRiverSystem.h"
#include "CellGeoData.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerrainVisualSurface, Log, All);

UTerrainVisualSurfaceComponent::UTerrainVisualSurfaceComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetMobility(EComponentMobility::Movable);
    SetCanEverAffectNavigation(false);
    bUseAsyncCooking = false;
    bUseComplexAsSimpleCollision = true;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetCollisionObjectType(ECC_WorldStatic);
    SetCollisionResponseToAllChannels(ECR_Ignore);
    SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

bool UTerrainVisualSurfaceComponent::RebuildBaseSphere(
    const FSphereTopology& SurfaceTopology,
    const FSphereTopology& CellTopology,
    const ITerrainSurfaceQuery& SurfaceQuery,
    float RadiusCM)
{
    ClearSurface();

    if (RadiusCM <= KINDA_SMALL_NUMBER
        || SurfaceTopology.PrimalVertsUnit.Num() == 0
        || SurfaceTopology.PrimalTris.Num() == 0)
    {
        return false;
    }

    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FLinearColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;
    TArray<int32> Triangles;

    const int32 VertexCount = SurfaceTopology.PrimalVertsUnit.Num();
    Vertices.Reserve(VertexCount);
    Normals.Reserve(VertexCount);
    UV0.Reserve(VertexCount);
    VertexColors.Reserve(VertexCount);

    for (const FVector& UnitVertex : SurfaceTopology.PrimalVertsUnit)
    {
        const FVector UnitDirection = UnitVertex.GetSafeNormal();
        if (UnitDirection.IsNearlyZero())
        {
            ClearSurface();
            return false;
        }

        const FTerrainSurfaceQueryResult Surface = SurfaceQuery.QueryBaseSurface(UnitDirection);
        if (!Surface.bIsValid || Surface.SurfaceRadiusCM <= KINDA_SMALL_NUMBER)
        {
            ClearSurface();
            return false;
        }

        Vertices.Add(UnitDirection * Surface.SurfaceRadiusCM);
        Normals.Add(Surface.WorldNormal.GetSafeNormal());
        UV0.Add(FVector2D::ZeroVector);
        VertexColors.Add(FLinearColor::White);
    }

    Triangles.Reserve(SurfaceTopology.PrimalTris.Num() * 3);
    for (const FIntVector& Triangle : SurfaceTopology.PrimalTris)
    {
        if (!Vertices.IsValidIndex(Triangle.X)
            || !Vertices.IsValidIndex(Triangle.Y)
            || !Vertices.IsValidIndex(Triangle.Z))
        {
            ClearSurface();
            return false;
        }

        Triangles.Add(Triangle.X);
        Triangles.Add(Triangle.Y);
        Triangles.Add(Triangle.Z);
    }

    CreateMeshSection_LinearColor(
        0,
        Vertices,
        Triangles,
        Normals,
        UV0,
        VertexColors,
        Tangents,
        true);

    TArray<FVector> HighlightVertices;
    TArray<FVector> HighlightNormals;
    TArray<FVector2D> HighlightUV0;
    TArray<FColor> HighlightColors;
    TArray<int32> HighlightTriangles;
    const int32 SurfaceTriangleCount = SurfaceTopology.PrimalTris.Num();
    HighlightVertices.Reserve(SurfaceTriangleCount * 3);
    HighlightNormals.Reserve(SurfaceTriangleCount * 3);
    HighlightUV0.Reserve(SurfaceTriangleCount * 3);
    HighlightColors.Reserve(SurfaceTriangleCount * 3);
    HighlightTriangles.Reserve(SurfaceTriangleCount * 3);

    for (int32 TriangleId = 0; TriangleId < SurfaceTriangleCount; ++TriangleId)
    {
        const FIntVector& Triangle = SurfaceTopology.PrimalTris[TriangleId];
        int32 Cell0 = INDEX_NONE;
        int32 Cell1 = INDEX_NONE;
        int32 Cell2 = INDEX_NONE;
        if (!FindLogicalCellsForSurfaceTriangle_(SurfaceTopology, CellTopology, TriangleId, Cell0, Cell1, Cell2))
        {
            ClearSurface();
            return false;
        }

        const uint8 Cell2High = static_cast<uint8>((Cell2 >> 8) & 0xFF);
        const uint8 Cell2Low = static_cast<uint8>(Cell2 & 0xFF);
        // Cell context is binary metadata, not display color. Store raw bytes so the
        // material's round(channel * 255) decode is exact and does not pass through sRGB.
        const FColor ContextColor(Cell2High, Cell2Low, 0, 255);
        const FVector2D ContextUV(static_cast<float>(Cell0), static_cast<float>(Cell1));
        const int32 SourceIndices[] = { Triangle.X, Triangle.Y, Triangle.Z };
        for (const int32 SourceIndex : SourceIndices)
        {
            HighlightVertices.Add(Vertices[SourceIndex]);
            HighlightNormals.Add(Normals[SourceIndex]);
            HighlightUV0.Add(ContextUV);
            HighlightColors.Add(ContextColor);
            HighlightTriangles.Add(HighlightVertices.Num() - 1);
        }
    }

    CreateMeshSection(
        1,
        HighlightVertices,
        HighlightTriangles,
        HighlightNormals,
        HighlightUV0,
        HighlightColors,
        Tangents,
        false);
    SetMeshSectionVisible(1, false);

    bHasBuiltSurface = true;
    BuiltSubdivisionLevel = SurfaceTopology.SubdivisionLevel;
    return true;
}

void UTerrainVisualSurfaceComponent::ClearSurface()
{
    ClearAllMeshSections();
    bHasBuiltSurface = false;
    BuiltSubdivisionLevel = INDEX_NONE;
}

bool UTerrainVisualSurfaceComponent::InitializeHighlightResources(const FSphereTopology& CellTopology)
{
    const int32 CellCount = CellTopology.Cells.Num();
    if (CellCount <= 0)
    {
        return false;
    }

    HighlightCellCount = CellCount;
    HighlightPixels.Init(FColor::Transparent, CellCount);

    SurfaceCellDirectionLUT = UTexture2D::CreateTransient(CellCount, 1, PF_A32B32G32R32F, TEXT("SurfaceCellDirectionLUT_Transient"));
    SurfaceHighlightLUT = UTexture2D::CreateTransient(CellCount, 1, PF_B8G8R8A8, TEXT("SurfaceHighlightLUT_Transient"));
    if (!SurfaceCellDirectionLUT || !SurfaceHighlightLUT)
    {
        HighlightCellCount = 0;
        HighlightPixels.Reset();
        return false;
    }

    auto ConfigureLUT = [](UTexture2D* Texture, TextureCompressionSettings CompressionSettings)
    {
        Texture->Filter = TF_Nearest;
        Texture->SRGB = false;
        Texture->NeverStream = true;
        Texture->MipGenSettings = TMGS_NoMipmaps;
        Texture->CompressionSettings = CompressionSettings;
    };
    ConfigureLUT(SurfaceCellDirectionLUT, TC_HDR);
    ConfigureLUT(SurfaceHighlightLUT, TC_VectorDisplacementmap);

    // CreateTransient only allocates platform data. The RHI resource must exist before
    // UpdateTextureRegions can receive the initial zero image or later single-cell updates.
    SurfaceHighlightLUT->UpdateResource();

    FTexturePlatformData* DirectionPlatformData = SurfaceCellDirectionLUT->GetPlatformData();
    if (!DirectionPlatformData || DirectionPlatformData->Mips.Num() == 0)
    {
        return false;
    }
    float* DirectionData = static_cast<float*>(DirectionPlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
    if (!DirectionData)
    {
        return false;
    }
    for (int32 CellId = 0; CellId < CellCount; ++CellId)
    {
        const FVector Direction = CellTopology.Cells[CellId].UnitCenter.GetSafeNormal();
        const int32 Offset = CellId * 4;
        DirectionData[Offset] = Direction.X;
        DirectionData[Offset + 1] = Direction.Y;
        DirectionData[Offset + 2] = Direction.Z;
        DirectionData[Offset + 3] = 1.0f;
    }
    DirectionPlatformData->Mips[0].BulkData.Unlock();
    SurfaceCellDirectionLUT->UpdateResource();
    UploadHighlightTexture_();
    return true;
}

bool UTerrainVisualSurfaceComponent::InitializeTopologyQueryResources(const FSphereTopology& CellTopology)
{
    SurfaceTopologyNodeCenterLUT = nullptr;
    SurfaceTopologyLeafCellLUT = nullptr;
    TopologySubdivisionLevel = 0;
    TopologyRootCount = 0;

    const int32 SubdivisionLevel = CellTopology.SubdivisionLevel;
    const int32 RootCount = CellTopology.TriTreeRoots.Num();
    if (SubdivisionLevel < 0 || RootCount <= 0 || CellTopology.Cells.IsEmpty())
    {
        return false;
    }

    TArray<const FTriTreeNode*> CurrentLevel;
    CurrentLevel.Reserve(RootCount);
    for (const FTriTreeNode* Root : CellTopology.TriTreeRoots)
    {
        if (!Root)
        {
            return false;
        }
        CurrentLevel.Add(Root);
    }

    TArray<FLinearColor> NodePixels;
    TArray<FLinearColor> LeafPixels;
    int32 ExpectedLevelNodeCount = RootCount;
    for (int32 Level = 0; Level <= SubdivisionLevel; ++Level)
    {
        if (CurrentLevel.Num() != ExpectedLevelNodeCount)
        {
            UE_LOG(LogTerrainVisualSurface, Error,
                TEXT("[TerrainVisual][SV8] Topology tree level size mismatch. Level=%d Actual=%d Expected=%d"),
                Level,
                CurrentLevel.Num(),
                ExpectedLevelNodeCount);
            return false;
        }

        TArray<const FTriTreeNode*> NextLevel;
        if (Level < SubdivisionLevel)
        {
            NextLevel.Reserve(CurrentLevel.Num() * 4);
        }

        for (const FTriTreeNode* Node : CurrentLevel)
        {
            const FVector Center = Node->Center.GetSafeNormal();
            if (Center.IsNearlyZero())
            {
                return false;
            }
            NodePixels.Emplace(Center.X, Center.Y, Center.Z, 1.0f);

            if (Level == SubdivisionLevel)
            {
                const int32 Cell0 = Node->CellIds[0];
                const int32 Cell1 = Node->CellIds[1];
                const int32 Cell2 = Node->CellIds[2];
                if (!CellTopology.Cells.IsValidIndex(Cell0)
                    || !CellTopology.Cells.IsValidIndex(Cell1)
                    || !CellTopology.Cells.IsValidIndex(Cell2))
                {
                    return false;
                }
                LeafPixels.Emplace(static_cast<float>(Cell0), static_cast<float>(Cell1), static_cast<float>(Cell2), 1.0f);
                continue;
            }

            for (int32 ChildIndex = 0; ChildIndex < 4; ++ChildIndex)
            {
                const FTriTreeNode* Child = Node->Children[ChildIndex];
                if (!Child)
                {
                    return false;
                }
                NextLevel.Add(Child);
            }
        }

        CurrentLevel = MoveTemp(NextLevel);
        ExpectedLevelNodeCount *= 4;
    }

    if (NodePixels.IsEmpty() || LeafPixels.IsEmpty())
    {
        return false;
    }

    const auto CreateFloatLUT = [](const TArray<FLinearColor>& Pixels, const TCHAR* Name) -> UTexture2D*
    {
        UTexture2D* Texture = UTexture2D::CreateTransient(Pixels.Num(), 1, PF_A32B32G32R32F, Name);
        if (!Texture)
        {
            return nullptr;
        }
        Texture->Filter = TF_Nearest;
        Texture->SRGB = false;
        Texture->NeverStream = true;
        Texture->MipGenSettings = TMGS_NoMipmaps;
        Texture->CompressionSettings = TC_VectorDisplacementmap;

        FTexturePlatformData* Data = Texture->GetPlatformData();
        float* Dest = Data && Data->Mips.Num() > 0
            ? static_cast<float*>(Data->Mips[0].BulkData.Lock(LOCK_READ_WRITE))
            : nullptr;
        if (!Dest)
        {
            return nullptr;
        }
        for (int32 Index = 0; Index < Pixels.Num(); ++Index)
        {
            Dest[Index * 4] = Pixels[Index].R;
            Dest[Index * 4 + 1] = Pixels[Index].G;
            Dest[Index * 4 + 2] = Pixels[Index].B;
            Dest[Index * 4 + 3] = Pixels[Index].A;
        }
        Data->Mips[0].BulkData.Unlock();
        Texture->UpdateResource();
        return Texture;
    };

    const auto ResolveSerializedCell = [&CellTopology, &NodePixels, &LeafPixels, RootCount, SubdivisionLevel](const FVector& UnitDirection)
    {
        int32 LocalNodeIndex = INDEX_NONE;
        float BestDot = -FLT_MAX;
        for (int32 RootIndex = 0; RootIndex < RootCount; ++RootIndex)
        {
            const FLinearColor& PackedCenter = NodePixels[RootIndex];
            const float Dot = FVector::DotProduct(FVector(PackedCenter.R, PackedCenter.G, PackedCenter.B), UnitDirection);
            if (Dot > BestDot)
            {
                BestDot = Dot;
                LocalNodeIndex = RootIndex;
            }
        }

        int32 LevelOffset = 0;
        int32 LevelNodeCount = RootCount;
        for (int32 Level = 0; Level < SubdivisionLevel; ++Level)
        {
            const int32 NextLevelOffset = LevelOffset + LevelNodeCount;
            const int32 ChildBaseIndex = LocalNodeIndex * 4;
            int32 BestChildIndex = INDEX_NONE;
            BestDot = -FLT_MAX;
            for (int32 ChildIndex = 0; ChildIndex < 4; ++ChildIndex)
            {
                const FLinearColor& PackedCenter = NodePixels[NextLevelOffset + ChildBaseIndex + ChildIndex];
                const float Dot = FVector::DotProduct(FVector(PackedCenter.R, PackedCenter.G, PackedCenter.B), UnitDirection);
                if (Dot > BestDot)
                {
                    BestDot = Dot;
                    BestChildIndex = ChildIndex;
                }
            }
            LocalNodeIndex = ChildBaseIndex + BestChildIndex;
            LevelOffset = NextLevelOffset;
            LevelNodeCount *= 4;
        }

        const FLinearColor& PackedLeaf = LeafPixels[LocalNodeIndex];
        const int32 CandidateCellIds[3] = {
            FMath::RoundToInt(PackedLeaf.R),
            FMath::RoundToInt(PackedLeaf.G),
            FMath::RoundToInt(PackedLeaf.B)};
        int32 BestCellId = INDEX_NONE;
        BestDot = -FLT_MAX;
        for (const int32 CellId : CandidateCellIds)
        {
            const float Dot = FVector::DotProduct(CellTopology.Cells[CellId].UnitCenter, UnitDirection);
            if (Dot > BestDot)
            {
                BestDot = Dot;
                BestCellId = CellId;
            }
        }
        return BestCellId;
    };

    const FSphereTopologyQuery CpuQuery(&CellTopology);
    FRandomStream VerificationRandom(875713);
    constexpr int32 VerificationSampleCount = 256;
    for (int32 SampleIndex = 0; SampleIndex < VerificationSampleCount; ++SampleIndex)
    {
        const FVector UnitDirection = SampleIndex < CellTopology.Cells.Num()
            ? CellTopology.Cells[SampleIndex].UnitCenter
            : VerificationRandom.VRand();
        const int32 ExpectedCellId = CpuQuery.FindNearestCell(UnitDirection).CellId;
        const int32 ActualCellId = ResolveSerializedCell(UnitDirection);
        if (ExpectedCellId != ActualCellId)
        {
            UE_LOG(LogTerrainVisualSurface, Error,
                TEXT("[TerrainVisual][SV8] Topology LUT verification failed. Sample=%d Expected=%d Actual=%d"),
                SampleIndex,
                ExpectedCellId,
                ActualCellId);
            return false;
        }
    }

    SurfaceTopologyNodeCenterLUT = CreateFloatLUT(NodePixels, TEXT("SurfaceTopologyNodeCenterLUT_Transient"));
    SurfaceTopologyLeafCellLUT = CreateFloatLUT(LeafPixels, TEXT("SurfaceTopologyLeafCellLUT_Transient"));
    if (!SurfaceTopologyNodeCenterLUT || !SurfaceTopologyLeafCellLUT)
    {
        SurfaceTopologyNodeCenterLUT = nullptr;
        SurfaceTopologyLeafCellLUT = nullptr;
        return false;
    }

    TopologySubdivisionLevel = SubdivisionLevel;
    TopologyRootCount = RootCount;
    UE_LOG(LogTerrainVisualSurface, Log,
        TEXT("[TerrainVisual][SV8] Topology Query LUT ready. Sub=%d Roots=%d Nodes=%d Leafs=%d VerifySamples=%d"),
        TopologySubdivisionLevel,
        TopologyRootCount,
        NodePixels.Num(),
        LeafPixels.Num(),
        VerificationSampleCount);
    return true;
}

bool UTerrainVisualSurfaceComponent::InitializeTerrainResources(const TArray<FCellGeoData>& GeoCells, int32 VisualSeed)
{
    if (GeoCells.IsEmpty())
    {
        SurfaceTerrainLUT = nullptr;
        return false;
    }

    TArray<FColor> TerrainPixels;
    TerrainPixels.Reserve(GeoCells.Num());
    FRandomStream Random(VisualSeed);
    for (int32 CellId = 0; CellId < GeoCells.Num(); ++CellId)
    {
        const FCellGeoData& Geo = GeoCells[CellId];
        if (Geo.CellId != CellId)
        {
            SurfaceTerrainLUT = nullptr;
            return false;
        }

        uint8 TerrainClass = 0;
        switch (Geo.SimpleTerrainType)
        {
        case ETerraSimpleTerrainType::Forest:
            TerrainClass = 127;
            break;
        case ETerraSimpleTerrainType::Mountain:
            TerrainClass = 255;
            break;
        case ETerraSimpleTerrainType::Plain:
        default:
            break;
        }

        const uint8 Variation = static_cast<uint8>(Random.RandRange(24, 231));
        const uint8 DetailScale = static_cast<uint8>(Random.RandRange(96, 191));
        const uint8 Roughness = Geo.SimpleTerrainType == ETerraSimpleTerrainType::Mountain ? 230 : 190;
        TerrainPixels.Add(FColor(TerrainClass, Variation, DetailScale, Roughness));
    }

    SurfaceTerrainLUT = UTexture2D::CreateTransient(
        TerrainPixels.Num(),
        1,
        PF_B8G8R8A8,
        TEXT("SurfaceTerrainLUT_Transient"),
        MakeArrayView(reinterpret_cast<const uint8*>(TerrainPixels.GetData()), sizeof(FColor) * TerrainPixels.Num()));
    if (!SurfaceTerrainLUT)
    {
        return false;
    }

    SurfaceTerrainLUT->Filter = TF_Nearest;
    SurfaceTerrainLUT->SRGB = false;
    SurfaceTerrainLUT->NeverStream = true;
    SurfaceTerrainLUT->MipGenSettings = TMGS_NoMipmaps;
    SurfaceTerrainLUT->CompressionSettings = TC_VectorDisplacementmap;
    SurfaceTerrainLUT->UpdateResource();

    if (HighlightMID)
    {
        HighlightMID->SetTextureParameterValue(TEXT("SurfaceTerrainLUT"), SurfaceTerrainLUT);
    }
    return true;
}

bool UTerrainVisualSurfaceComponent::InitializeRiverResources(const FTerrainVisualRiverSystem& RiverSystem)
{
    auto CreateFloatLUT = [](const TArray<FLinearColor>& Pixels, const TCHAR* Name) -> UTexture2D*
    {
        UTexture2D* Texture = UTexture2D::CreateTransient(Pixels.Num(), 1, PF_A32B32G32R32F, Name);
        if (!Texture) return nullptr;
        Texture->Filter = TF_Nearest; Texture->SRGB = false; Texture->NeverStream = true;
        Texture->MipGenSettings = TMGS_NoMipmaps; Texture->CompressionSettings = TC_VectorDisplacementmap;
        FTexturePlatformData* Data = Texture->GetPlatformData();
        float* Dest = Data ? static_cast<float*>(Data->Mips[0].BulkData.Lock(LOCK_READ_WRITE)) : nullptr;
        if (!Dest) return nullptr;
        for (int32 Index = 0; Index < Pixels.Num(); ++Index)
        {
            Dest[Index * 4] = Pixels[Index].R; Dest[Index * 4 + 1] = Pixels[Index].G;
            Dest[Index * 4 + 2] = Pixels[Index].B; Dest[Index * 4 + 3] = Pixels[Index].A;
        }
        Data->Mips[0].BulkData.Unlock(); Texture->UpdateResource(); return Texture;
    };
    TArray<FLinearColor> SegmentPixels;
    for (const FTerrainVisualRiverSegment& Segment : RiverSystem.GetSegments())
    {
        SegmentPixels.Emplace(Segment.StartUnit.X, Segment.StartUnit.Y, Segment.StartUnit.Z, Segment.StartWidthRad);
        SegmentPixels.Emplace(Segment.EndUnit.X, Segment.EndUnit.Y, Segment.EndUnit.Z, Segment.EndWidthRad);
    }
    TArray<FLinearColor> LakePixels;
    for (const FTerrainVisualRiverLake& Lake : RiverSystem.GetTerminalLakes())
    {
        LakePixels.Emplace(Lake.CenterUnit.X, Lake.CenterUnit.Y, Lake.CenterUnit.Z, Lake.RadiusAlongRad);
        LakePixels.Emplace(Lake.FlowAxisUnit.X, Lake.FlowAxisUnit.Y, Lake.FlowAxisUnit.Z, Lake.RadiusAcrossRad);
    }
    SurfaceRiverSegmentLUT = CreateFloatLUT(SegmentPixels.IsEmpty() ? TArray<FLinearColor>{FLinearColor::Transparent, FLinearColor::Transparent} : SegmentPixels, TEXT("SurfaceRiverSegmentLUT_Transient"));
    SurfaceRiverLakeLUT = CreateFloatLUT(LakePixels.IsEmpty() ? TArray<FLinearColor>{FLinearColor::Transparent, FLinearColor::Transparent} : LakePixels, TEXT("SurfaceRiverLakeLUT_Transient"));
    if (!SurfaceRiverSegmentLUT || !SurfaceRiverLakeLUT) return false;
    RiverSegmentCount = RiverSystem.GetSegments().Num();
    RiverLakeCount = RiverSystem.GetTerminalLakes().Num();
    if (HighlightMID)
    {
        HighlightMID->SetTextureParameterValue(TEXT("SurfaceRiverSegmentLUT"), SurfaceRiverSegmentLUT);
        HighlightMID->SetTextureParameterValue(TEXT("SurfaceRiverLakeLUT"), SurfaceRiverLakeLUT);
        HighlightMID->SetScalarParameterValue(TEXT("RiverSegmentCount"), RiverSegmentCount);
        HighlightMID->SetScalarParameterValue(TEXT("RiverLakeCount"), RiverLakeCount);
    }
    return true;
}

void UTerrainVisualSurfaceComponent::SetHighlightMaterial(UMaterialInterface* InMaterial)
{
    HighlightMID = InMaterial ? CreateDynamicMaterialInstance(1, InMaterial) : nullptr;
    SetMeshSectionVisible(0, HighlightMID == nullptr);
    SetMeshSectionVisible(1, HighlightMID != nullptr);
    if (HighlightMID)
    {
        HighlightMID->SetTextureParameterValue(TEXT("SurfaceCellDirectionLUT"), SurfaceCellDirectionLUT);
        HighlightMID->SetTextureParameterValue(TEXT("SurfaceHighlightLUT"), SurfaceHighlightLUT);
        HighlightMID->SetTextureParameterValue(TEXT("SurfaceTopologyNodeCenterLUT"), SurfaceTopologyNodeCenterLUT);
        HighlightMID->SetTextureParameterValue(TEXT("SurfaceTopologyLeafCellLUT"), SurfaceTopologyLeafCellLUT);
        HighlightMID->SetTextureParameterValue(TEXT("SurfaceTerrainLUT"), SurfaceTerrainLUT);
        HighlightMID->SetTextureParameterValue(TEXT("SurfaceRiverSegmentLUT"), SurfaceRiverSegmentLUT);
        HighlightMID->SetTextureParameterValue(TEXT("SurfaceRiverLakeLUT"), SurfaceRiverLakeLUT);
        HighlightMID->SetScalarParameterValue(TEXT("RiverSegmentCount"), RiverSegmentCount);
        HighlightMID->SetScalarParameterValue(TEXT("RiverLakeCount"), RiverLakeCount);
        HighlightMID->SetScalarParameterValue(TEXT("TopologySubdivisionLevel"), TopologySubdivisionLevel);
        HighlightMID->SetScalarParameterValue(TEXT("TopologyRootCount"), TopologyRootCount);
        ApplySurfaceEnhancementParameters_();
    }
}

void UTerrainVisualSurfaceComponent::SetSurfaceEnhancementParameters(
    UTexture2D* InGravelColor, UTexture2D* InGravelNormal, UTexture2D* InGravelRoughness,
    UTexture2D* InMossColor, UTexture2D* InMossNormal, UTexture2D* InMossRoughness,
    UTexture2D* InRockColor, UTexture2D* InRockNormal, UTexture2D* InRockRoughness,
    const FLinearColor& InPlainTint, const FLinearColor& InForestTint, const FLinearColor& InMountainTint,
    float InTileScaleCM, float InTriplanarSharpness, float InNormalStrength)
{
    GravelColorTexture = InGravelColor; GravelNormalTexture = InGravelNormal; GravelRoughnessTexture = InGravelRoughness;
    MossColorTexture = InMossColor; MossNormalTexture = InMossNormal; MossRoughnessTexture = InMossRoughness;
    RockColorTexture = InRockColor; RockNormalTexture = InRockNormal; RockRoughnessTexture = InRockRoughness;
    SurfacePlainTint = InPlainTint; SurfaceForestTint = InForestTint; SurfaceMountainTint = InMountainTint;
    SurfaceTileScaleCM = FMath::Max(InTileScaleCM, 1.0f);
    SurfaceTriplanarSharpness = FMath::Max(InTriplanarSharpness, 1.0f);
    SurfaceNormalStrength = FMath::Clamp(InNormalStrength, 0.0f, 2.0f);
    ApplySurfaceEnhancementParameters_();
}

void UTerrainVisualSurfaceComponent::ApplySurfaceEnhancementParameters_()
{
    if (!HighlightMID) return;
    HighlightMID->SetTextureParameterValue(TEXT("GravelColor"), GravelColorTexture);
    HighlightMID->SetTextureParameterValue(TEXT("GravelNormal"), GravelNormalTexture);
    HighlightMID->SetTextureParameterValue(TEXT("GravelRoughness"), GravelRoughnessTexture);
    HighlightMID->SetTextureParameterValue(TEXT("MossColor"), MossColorTexture);
    HighlightMID->SetTextureParameterValue(TEXT("MossNormal"), MossNormalTexture);
    HighlightMID->SetTextureParameterValue(TEXT("MossRoughness"), MossRoughnessTexture);
    HighlightMID->SetTextureParameterValue(TEXT("RockColor"), RockColorTexture);
    HighlightMID->SetTextureParameterValue(TEXT("RockNormal"), RockNormalTexture);
    HighlightMID->SetTextureParameterValue(TEXT("RockRoughness"), RockRoughnessTexture);
    HighlightMID->SetVectorParameterValue(TEXT("PlainTint"), SurfacePlainTint);
    HighlightMID->SetVectorParameterValue(TEXT("ForestTint"), SurfaceForestTint);
    HighlightMID->SetVectorParameterValue(TEXT("MountainTint"), SurfaceMountainTint);
    HighlightMID->SetScalarParameterValue(TEXT("TerrainTileScaleCM"), SurfaceTileScaleCM);
    HighlightMID->SetScalarParameterValue(TEXT("TerrainTriplanarSharpness"), SurfaceTriplanarSharpness);
    HighlightMID->SetScalarParameterValue(TEXT("TerrainNormalStrength"), SurfaceNormalStrength);
}

void UTerrainVisualSurfaceComponent::SetHighlightParameters(
    const FVector& PlanetCenter,
    const FLinearColor& BaseGroundColor,
    float HighlightPaddingRad,
    float HighlightStrength)
{
    if (!HighlightMID)
    {
        return;
    }

    HighlightMID->SetVectorParameterValue(TEXT("PlanetCenter"), FLinearColor(PlanetCenter.X, PlanetCenter.Y, PlanetCenter.Z, 0.0f));
    HighlightMID->SetVectorParameterValue(TEXT("BaseGroundColor"), BaseGroundColor);
    HighlightMID->SetScalarParameterValue(TEXT("HighlightPaddingRad"), FMath::Max(HighlightPaddingRad, 0.0001f));
    HighlightMID->SetScalarParameterValue(TEXT("HighlightStrength"), FMath::Max(HighlightStrength, 0.0f));
}

void UTerrainVisualSurfaceComponent::ApplySharedMaterialParameters(
    UMaterialInstanceDynamic* MaterialInstance,
    const FVector& PlanetCenter,
    const FLinearColor& BaseGroundColor,
    float HighlightPaddingRad,
    float HighlightStrength) const
{
    if (!MaterialInstance)
    {
        return;
    }

    MaterialInstance->SetTextureParameterValue(TEXT("SurfaceCellDirectionLUT"), SurfaceCellDirectionLUT);
    MaterialInstance->SetTextureParameterValue(TEXT("SurfaceHighlightLUT"), SurfaceHighlightLUT);
    MaterialInstance->SetTextureParameterValue(TEXT("SurfaceTopologyNodeCenterLUT"), SurfaceTopologyNodeCenterLUT);
    MaterialInstance->SetTextureParameterValue(TEXT("SurfaceTopologyLeafCellLUT"), SurfaceTopologyLeafCellLUT);
    MaterialInstance->SetTextureParameterValue(TEXT("SurfaceTerrainLUT"), SurfaceTerrainLUT);
    MaterialInstance->SetTextureParameterValue(TEXT("SurfaceRiverSegmentLUT"), SurfaceRiverSegmentLUT);
    MaterialInstance->SetTextureParameterValue(TEXT("SurfaceRiverLakeLUT"), SurfaceRiverLakeLUT);
    MaterialInstance->SetScalarParameterValue(TEXT("RiverSegmentCount"), RiverSegmentCount);
    MaterialInstance->SetScalarParameterValue(TEXT("RiverLakeCount"), RiverLakeCount);
    MaterialInstance->SetScalarParameterValue(TEXT("TopologySubdivisionLevel"), TopologySubdivisionLevel);
    MaterialInstance->SetScalarParameterValue(TEXT("TopologyRootCount"), TopologyRootCount);

    MaterialInstance->SetTextureParameterValue(TEXT("GravelColor"), GravelColorTexture);
    MaterialInstance->SetTextureParameterValue(TEXT("GravelNormal"), GravelNormalTexture);
    MaterialInstance->SetTextureParameterValue(TEXT("GravelRoughness"), GravelRoughnessTexture);
    MaterialInstance->SetTextureParameterValue(TEXT("MossColor"), MossColorTexture);
    MaterialInstance->SetTextureParameterValue(TEXT("MossNormal"), MossNormalTexture);
    MaterialInstance->SetTextureParameterValue(TEXT("MossRoughness"), MossRoughnessTexture);
    MaterialInstance->SetTextureParameterValue(TEXT("RockColor"), RockColorTexture);
    MaterialInstance->SetTextureParameterValue(TEXT("RockNormal"), RockNormalTexture);
    MaterialInstance->SetTextureParameterValue(TEXT("RockRoughness"), RockRoughnessTexture);
    MaterialInstance->SetVectorParameterValue(TEXT("PlainTint"), SurfacePlainTint);
    MaterialInstance->SetVectorParameterValue(TEXT("ForestTint"), SurfaceForestTint);
    MaterialInstance->SetVectorParameterValue(TEXT("MountainTint"), SurfaceMountainTint);
    MaterialInstance->SetScalarParameterValue(TEXT("TerrainTileScaleCM"), SurfaceTileScaleCM);
    MaterialInstance->SetScalarParameterValue(TEXT("TerrainTriplanarSharpness"), SurfaceTriplanarSharpness);
    MaterialInstance->SetScalarParameterValue(TEXT("TerrainNormalStrength"), SurfaceNormalStrength);

    MaterialInstance->SetVectorParameterValue(
        TEXT("PlanetCenter"),
        FLinearColor(PlanetCenter.X, PlanetCenter.Y, PlanetCenter.Z, 0.0f));
    MaterialInstance->SetVectorParameterValue(TEXT("BaseGroundColor"), BaseGroundColor);
    MaterialInstance->SetScalarParameterValue(TEXT("HighlightPaddingRad"), FMath::Max(HighlightPaddingRad, 0.0001f));
    MaterialInstance->SetScalarParameterValue(TEXT("HighlightStrength"), FMath::Max(HighlightStrength, 0.0f));
}

bool UTerrainVisualSurfaceComponent::WriteHighlightCell(int32 CellId, const FLinearColor& Color, float Intensity)
{
    if (!HighlightPixels.IsValidIndex(CellId))
    {
        return false;
    }

    const FColor NewPixel = FLinearColor(
        FMath::Clamp(Color.R, 0.0f, 1.0f),
        FMath::Clamp(Color.G, 0.0f, 1.0f),
        FMath::Clamp(Color.B, 0.0f, 1.0f),
        FMath::Clamp(Intensity, 0.0f, 1.0f)).ToFColor(false);
    if (HighlightPixels[CellId] == NewPixel)
    {
        return true;
    }

    HighlightPixels[CellId] = NewPixel;
    UploadHighlightTexture_(CellId);
    return true;
}

void UTerrainVisualSurfaceComponent::ClearHighlightCells()
{
    if (HighlightPixels.IsEmpty())
    {
        return;
    }

    HighlightPixels.Init(FColor::Transparent, HighlightPixels.Num());
    UploadHighlightTexture_();
}

void UTerrainVisualSurfaceComponent::UploadHighlightTexture_(int32 CellId)
{
    if (!SurfaceHighlightLUT || HighlightPixels.IsEmpty())
    {
        return;
    }

    const bool bSingleCellUpload = HighlightPixels.IsValidIndex(CellId);
    const int32 SourceCellId = bSingleCellUpload ? CellId : 0;
    const int32 UploadWidth = bSingleCellUpload ? 1 : HighlightPixels.Num();
    // UploadData is a compact buffer. For a single-cell update its source pixel is always at X=0;
    // only the destination texture coordinate uses CellId.
    FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(SourceCellId, 0, 0, 0, UploadWidth, 1);
    uint8* UploadData = new uint8[sizeof(FColor) * UploadWidth];
    FMemory::Memcpy(UploadData, HighlightPixels.GetData() + SourceCellId, sizeof(FColor) * UploadWidth);
    SurfaceHighlightLUT->UpdateTextureRegions(
        0,
        1,
        Region,
        sizeof(FColor) * UploadWidth,
        sizeof(FColor),
        UploadData,
        [](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
        {
            delete[] SrcData;
            delete const_cast<FUpdateTextureRegion2D*>(Regions);
        });
}

bool UTerrainVisualSurfaceComponent::FindLogicalCellsForSurfaceTriangle_(
    const FSphereTopology& SurfaceTopology,
    const FSphereTopology& CellTopology,
    int32 SurfaceTriangleId,
    int32& OutCell0,
    int32& OutCell1,
    int32& OutCell2) const
{
    OutCell0 = INDEX_NONE;
    OutCell1 = INDEX_NONE;
    OutCell2 = INDEX_NONE;
    if (!SurfaceTopology.PrimalTriTreeNodes.IsValidIndex(SurfaceTriangleId)
        || SurfaceTopology.SubdivisionLevel < CellTopology.SubdivisionLevel)
    {
        return false;
    }

    FTriTreeNode* Node = SurfaceTopology.PrimalTriTreeNodes[SurfaceTriangleId];
    const int32 ClimbSteps = SurfaceTopology.SubdivisionLevel - CellTopology.SubdivisionLevel;
    for (int32 Step = 0; Step < ClimbSteps && Node && Node->Father; ++Step)
    {
        Node = Node->Father;
    }
    if (!Node)
    {
        return false;
    }

    OutCell0 = Node->CellIds[0];
    OutCell1 = Node->CellIds[1];
    OutCell2 = Node->CellIds[2];
    return CellTopology.Cells.IsValidIndex(OutCell0)
        && CellTopology.Cells.IsValidIndex(OutCell1)
        && CellTopology.Cells.IsValidIndex(OutCell2);
}
