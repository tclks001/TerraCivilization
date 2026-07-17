#include "TerrainVisualSurfaceComponent.h"

#include "FSphereTopology.h"
#include "TerrainSurfaceQuery.h"
#include "TerrainVisualRiverSystem.h"
#include "CellGeoData.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

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
        HighlightMID->SetTextureParameterValue(TEXT("SurfaceTerrainLUT"), SurfaceTerrainLUT);
        HighlightMID->SetTextureParameterValue(TEXT("SurfaceRiverSegmentLUT"), SurfaceRiverSegmentLUT);
        HighlightMID->SetTextureParameterValue(TEXT("SurfaceRiverLakeLUT"), SurfaceRiverLakeLUT);
        HighlightMID->SetScalarParameterValue(TEXT("RiverSegmentCount"), RiverSegmentCount);
        HighlightMID->SetScalarParameterValue(TEXT("RiverLakeCount"), RiverLakeCount);
    }
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
