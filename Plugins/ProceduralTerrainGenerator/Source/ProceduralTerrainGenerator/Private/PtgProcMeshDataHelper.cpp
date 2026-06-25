// Copyright 2021 VICTOR HERNANDEZ MOLPECERES (Rockam). All rights reserved.

#include "PtgProcMeshDataHelper.h"
#include "PtgFastNoiseLiteWrapper.h"

const float MIN_RADIUS = 100.0f;
const float DEFAULT_RADIUS = 15000.0f;
const uint32 MIN_RESOLUTION = 1;
const uint32 DEFAULT_RESOLUTION = 150;
const uint8 NUM_CUBE_FACES = 6;
const uint8 NUM_TRIANGLE_POINTS_PER_TILE = 6;    // 3 points per triangle and 2 triangles per tile
const FLinearColor DEFAULT_COLOR = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

namespace CubeFaceDirections
{
    const FVector Front = FVector(1.0f, 0.0f, -1.0f);
    const FVector Back = FVector(-1.0f, 0.0f, -1.0f);
    const FVector Left = FVector(0.0f, 1.0f, -1.0f);
    const FVector Right = FVector(0.0f, -1.0f, -1.0f);
    const FVector Top = FVector(1.0f, 1.0f, 0.0f);
    const FVector Bottom = FVector(1.0f, -1.0f, 0.0f);
}

namespace CubeFaceNormals
{
    const FVector Front = FVector(0.0f, 1.0f, 0.0f);
    const FVector Back = FVector(0.0f, -1.0f, 0.0f);
    const FVector Left = FVector(-1.0f, 0.0f, 0.0f);
    const FVector Right = FVector(1.0f, 0.0f, 0.0f);
    const FVector Top = FVector(0.0f, 0.0f, 1.0f);
    const FVector Bottom = FVector(0.0f, 0.0f, -1.0f);
}

namespace CubeFaceTangents
{
    const FVector Front = FVector(1.0f, 0.0f, 0.0f);
    const FVector Back = FVector(-1.0f, 0.0f, 0.0f);
    const FVector Left = FVector(0.0f, 1.0f, 0.0f);
    const FVector Right = FVector(0.0f, -1.0f, 0.0f);
    const FVector Top = FVector(1.0f, 0.0f, 0.0f);
    const FVector Bottom = FVector(-1.0f, 0.0f, 0.0f);
}

namespace CubeVertexFaceAssetRotations
{
    const FQuat Front = FRotator(0.0f, 0.0f, 90.0f).Quaternion();
    const FQuat Back = FRotator(0.0f, 0.0f, -90.0f).Quaternion();
    const FQuat Left = FRotator(90.0f, 0.0f, 0.0f).Quaternion();
    const FQuat Right = FRotator(-90.0f, 0.0f, 0.0f).Quaternion();
    const FQuat Top = FRotator(0.0f, 0.0f, 0.0f).Quaternion();
    const FQuat Bottom = FRotator(0.0f, 0.0f, 180.0f).Quaternion();
}

FPtgProcMeshData UPtgProcMeshDataHelper::GenerateProcMeshData
(
    float& lowestGeneratedHeight,
    float& highestGeneratedHeight,
    const EPtgProcMeshShapes procMeshShape,
    const float radius, const int32 resolution,
    const bool bUseTerrainTiling,
    UPtgFastNoiseLiteWrapper* fastNoiseLiteWrapper,
    const float noiseInput,
    const float noiseOutput,
    const FVector& actorLocation
)
{
    const float checkedRadius = radius >= MIN_RADIUS ? radius : DEFAULT_RADIUS;
    const uint32 checkedResolution = resolution >= MIN_RESOLUTION ? resolution : DEFAULT_RESOLUTION;
    const FVector checkedActorLocationForTiling = bUseTerrainTiling ? actorLocation : FVector::ZeroVector;
    FPtgProcMeshData procMeshData;

    // Generate different data depending on the specified shape
    switch (procMeshShape)
    {
    case EPtgProcMeshShapes::Cube:

        GenerateCubeData(procMeshData, lowestGeneratedHeight, highestGeneratedHeight, checkedRadius, checkedResolution, fastNoiseLiteWrapper, noiseInput, noiseOutput);
        break;

    case EPtgProcMeshShapes::Sphere:

        GenerateSphereData(procMeshData, lowestGeneratedHeight, highestGeneratedHeight, checkedRadius, checkedResolution, fastNoiseLiteWrapper, noiseInput, noiseOutput);
        break;

    case EPtgProcMeshShapes::Plane:
    default:

        GeneratePlaneData(procMeshData, lowestGeneratedHeight, highestGeneratedHeight, checkedRadius, checkedResolution, fastNoiseLiteWrapper, noiseInput, noiseOutput, actorLocation);
    }

    return procMeshData;
}

// Defines two triangles in counter clockwise order
static FORCEINLINE void DefineTrianglesOnTile(TArray<int32>& triangles, uint32& trianglesCount, const uint32 tileUpLeftVertexIndex, const uint32 tileUpRightVertexIndex, const uint32 tileBottomLeftVertexIndex, const uint32 tileBottomRightVertexIndex)
{
    // Triangle 1
    triangles[trianglesCount] = tileUpLeftVertexIndex;
    triangles[trianglesCount + 1] = tileBottomLeftVertexIndex;
    triangles[trianglesCount + 2] = tileUpRightVertexIndex;

    // Triangle 2
    triangles[trianglesCount + 3] = tileBottomLeftVertexIndex;
    triangles[trianglesCount + 4] = tileBottomRightVertexIndex;
    triangles[trianglesCount + 5] = tileUpRightVertexIndex;

    trianglesCount += NUM_TRIANGLE_POINTS_PER_TILE;
}

static FORCEINLINE FVector CalculateNormalVectorOfPlane(const FVector& pointA, const FVector& pointB, const FVector& pointC)
{
    FVector normalVector = FVector::CrossProduct(pointC - pointA, pointB - pointA);
    normalVector.Normalize();
    return normalVector;
}

static FORCEINLINE FVector CalculateMiddleVector(const FVector& vectorA, const FVector& vectorB)
{
    FVector middleVector = vectorA + vectorB;
    middleVector.Normalize();
    return middleVector;
}

static FORCEINLINE void ApplyNoiseToPlaneVertex(FVector& vertex, const float tileSize, const float startLocation, UPtgFastNoiseLiteWrapper* fastNoiseLiteWrapper, const FVector2D& noiseOffset, const float noiseInput, const float noiseOutput, float& lowestGeneratedHeight, float& highestGeneratedHeight, const bool bIsFirstVertex)
{
    vertex.Z = fastNoiseLiteWrapper->GetNoise2D((noiseOffset.X + vertex.X) * noiseInput, (noiseOffset.Y + vertex.Y) * noiseInput) * noiseOutput;

    if (bIsFirstVertex)
    {
        lowestGeneratedHeight = highestGeneratedHeight = vertex.Z;
    }
    else
    {
        if (vertex.Z < lowestGeneratedHeight) lowestGeneratedHeight = vertex.Z;
        if (vertex.Z > highestGeneratedHeight) highestGeneratedHeight = vertex.Z;
    }
}

void AddVectorPlaneDataWithNormalAndTangent(FPtgProcMeshData& procMeshData, TMultiMap<FIntVector, uint32>& uniqueVertices, uint32& vertexCount, uint32& vertexIndex, const FVector& vertex, const FVector& vertexNormal, const FVector2D& UV0)
{
    // Create rounded vector to avoid decimal precision errors when checking for similar vertices
    const FIntVector roundedVector = FIntVector(FMath::FloorToInt(vertex.X), FMath::FloorToInt(vertex.Y), 0.0f);
    const uint32* vertexIndexPointer = uniqueVertices.Find(roundedVector);

    if (vertexIndexPointer == nullptr)
    {
        // Add new vertex data if doesn't exist yet
        uniqueVertices.Emplace(roundedVector, vertexCount);
        procMeshData.Vertices.Emplace(vertex);
        procMeshData.Normals.Emplace(vertexNormal);
        procMeshData.UV0.Emplace(UV0);
        procMeshData.Tangents.Emplace(FRuntimeMeshTangent(FVector3f(FVector::CrossProduct(procMeshData.Normals[vertexCount], CubeFaceTangents::Right)), false));
        vertexIndex = vertexCount++;
    }
    else
    {
        // If vertex already exists, don't add it, but update normal and tangent vectors
        vertexIndex = *vertexIndexPointer;
        procMeshData.Normals[vertexIndex] = CalculateMiddleVector(procMeshData.Normals[vertexIndex], vertexNormal);
        procMeshData.Tangents[vertexIndex] = FRuntimeMeshTangent(FVector3f(FVector::CrossProduct(procMeshData.Normals[vertexIndex], CubeFaceTangents::Right)), false);
    }
}

void UPtgProcMeshDataHelper::GeneratePlaneData(FPtgProcMeshData& procMeshData, float& lowestGeneratedHeight, float& highestGeneratedHeight, const float radius, const uint32 resolution, UPtgFastNoiseLiteWrapper* fastNoiseLiteWrapper, const float noiseInput, const float noiseOutput, const FVector& actorLocation)
{
    const uint32 totalTiles = resolution * resolution;
    const uint32 totalTrianglePoints = totalTiles * NUM_TRIANGLE_POINTS_PER_TILE;
    const float tileSize = (radius * 2.0f) / resolution;
    const float noiseInputScaled = noiseInput / tileSize;
    const bool bWithNoise = fastNoiseLiteWrapper != nullptr;
    const FVector topLeftCorner = FVector(-radius, -radius, 0.0f);
    const FVector directionComponentX_Scaled = CubeFaceNormals::Right * tileSize;
    const FVector directionComponentY_Scaled = CubeFaceNormals::Front * tileSize;
    const FVector2D noiseOffset = FVector2D(actorLocation.X, actorLocation.Y);    // used for tiling
    uint32 vertexCount = 0;
    uint32 trianglesCount = 0;
    uint32 x = 0, y = 0;
    uint32 topLeftVertexIndex = 0;
    uint32 topRightVertexIndex = 0;
    uint32 bottomLeftVertexIndex = 0;
    uint32 bottomRightVertexIndex = 0;
    FVector topLeftVertex = FVector::ZeroVector;
    FVector topRightVertex = FVector::ZeroVector;
    FVector bottomLeftVertex = FVector::ZeroVector;
    FVector bottomRightVertex = FVector::ZeroVector;
    FVector topLeftVertexNormal = FVector::ZeroVector;
    FVector topRightVertexNormal = FVector::ZeroVector;
    FVector bottomLeftVertexNormal = FVector::ZeroVector;
    FVector bottomRightVertexNormal = FVector::ZeroVector;
    FVector firstTriangleNormal = FVector::ZeroVector;
    FVector secondTriangleNormal = FVector::ZeroVector;
    FVector commonVertexMiddleVector = FVector::ZeroVector;
    TMultiMap<FIntVector, uint32> uniqueVertices;

    // Specify the number of vertices could be not safe at this point, as many of them will be unified, so simply empty the arrays. Triangle number is well known.
    procMeshData.ResizeData(0, totalTrianglePoints);

    // Reserve estimated capacity to avoid repeated reallocations in the inner loop
    const uint32 estimatedVertices = (resolution + 1) * (resolution + 1);
    procMeshData.Vertices.Reserve(estimatedVertices);
    procMeshData.Normals.Reserve(estimatedVertices);
    procMeshData.UV0.Reserve(estimatedVertices);
    procMeshData.Tangents.Reserve(estimatedVertices);
    uniqueVertices.Reserve(estimatedVertices);

    for (uint32 currentTileIndex = 0; currentTileIndex < totalTiles; currentTileIndex++)
    {
        x = currentTileIndex % resolution;
        y = currentTileIndex / resolution;

        // Calculate vertex location of the 4 vertices that conforms the tile
        topLeftVertex = topLeftCorner + x * directionComponentX_Scaled + y * directionComponentY_Scaled;
        topRightVertex = topLeftVertex + directionComponentX_Scaled;
        bottomLeftVertex = topLeftVertex + directionComponentY_Scaled;
        bottomRightVertex = topLeftVertex + directionComponentX_Scaled + directionComponentY_Scaled;

        // Apply noise if specified
        if (bWithNoise)
        {
            ApplyNoiseToPlaneVertex(topLeftVertex, tileSize, radius, fastNoiseLiteWrapper, noiseOffset, noiseInputScaled, noiseOutput, lowestGeneratedHeight, highestGeneratedHeight, vertexCount == 0);
            ApplyNoiseToPlaneVertex(topRightVertex, tileSize, radius, fastNoiseLiteWrapper, noiseOffset, noiseInputScaled, noiseOutput, lowestGeneratedHeight, highestGeneratedHeight, false);
            ApplyNoiseToPlaneVertex(bottomLeftVertex, tileSize, radius, fastNoiseLiteWrapper, noiseOffset, noiseInputScaled, noiseOutput, lowestGeneratedHeight, highestGeneratedHeight, false);
            ApplyNoiseToPlaneVertex(bottomRightVertex, tileSize, radius, fastNoiseLiteWrapper, noiseOffset, noiseInputScaled, noiseOutput, lowestGeneratedHeight, highestGeneratedHeight, false);
        }

        // Calculate normals after noise is applied
        firstTriangleNormal = CalculateNormalVectorOfPlane(topLeftVertex, bottomLeftVertex, topRightVertex);
        secondTriangleNormal = CalculateNormalVectorOfPlane(bottomLeftVertex, bottomRightVertex, topRightVertex);
        commonVertexMiddleVector = CalculateMiddleVector(firstTriangleNormal, secondTriangleNormal);    // combined normal for vertices that are owned by two triangles at the same time

        topLeftVertexNormal = firstTriangleNormal;
        topRightVertexNormal = commonVertexMiddleVector;
        bottomLeftVertexNormal = commonVertexMiddleVector;
        bottomRightVertexNormal = secondTriangleNormal;

        // Add the rest of the vertex data unifying the ones that are in the same location
        AddVectorPlaneDataWithNormalAndTangent(procMeshData, uniqueVertices, vertexCount, topLeftVertexIndex, topLeftVertex, topLeftVertexNormal, FVector2D(x, y));
        AddVectorPlaneDataWithNormalAndTangent(procMeshData, uniqueVertices, vertexCount, topRightVertexIndex, topRightVertex, topRightVertexNormal, FVector2D(x + 1.0f, y));
        AddVectorPlaneDataWithNormalAndTangent(procMeshData, uniqueVertices, vertexCount, bottomLeftVertexIndex, bottomLeftVertex, bottomLeftVertexNormal, FVector2D(x, y + 1.0f));
        AddVectorPlaneDataWithNormalAndTangent(procMeshData, uniqueVertices, vertexCount, bottomRightVertexIndex, bottomRightVertex, bottomRightVertexNormal, FVector2D(x + 1.0f, y + 1.0f));

        // Finally, define the 2 triangles on the tile
        DefineTrianglesOnTile(procMeshData.Triangles, trianglesCount, topLeftVertexIndex, topRightVertexIndex, bottomLeftVertexIndex, bottomRightVertexIndex);
    }

    // Init vertex colors with the same value
    procMeshData.VertexColors.Init(DEFAULT_COLOR, procMeshData.GetNumVertices());
}

struct FCubeFaceData
{
public:

    uint32 Resolution = 0;
    uint32 TotalTiles = 0;
    float TileSize = 0.0f;
    float MinHeight = 0.0f;
    FVector TopLeftCorner = FVector::ZeroVector;
    FVector FaceAxisDirections = FVector::ZeroVector;
    FVector Normal = FVector::ZeroVector;
    FVector Tangent = FVector::ZeroVector;
};

void ApplyNoiseToCubeVertex(FVector& vertex, const FVector& vertexNormal, const FVector& faceNormal, UPtgFastNoiseLiteWrapper* fastNoiseLiteWrapper, const float noiseInput, const float noiseOutput, float& lowestGeneratedHeight, float& highestGeneratedHeight, const bool bIsFirstVertex)
{
    float noiseValue = fastNoiseLiteWrapper->GetNoise3D(vertex.X * noiseInput, vertex.Y * noiseInput, vertex.Z * noiseInput) * noiseOutput;

    // To avoid overlapping vertex below the radius, all noise generated under 0 is inverted
    if (noiseValue < 0.0f) noiseValue *= -1.0f;

    // Add noise to vertex using its normal
    vertex += vertexNormal * noiseValue;

    float height = 0.0f;

    // Height is calculated on the Z component of the cube face
    if (faceNormal.X != 0)
    {
        height = FMath::Abs(vertex.X);
    }
    else if (faceNormal.Y != 0)
    {
        height = FMath::Abs(vertex.Y);
    }
    else if (faceNormal.Z != 0)
    {
        height = FMath::Abs(vertex.Z);
    }

    if (bIsFirstVertex)
    {
        lowestGeneratedHeight = highestGeneratedHeight = height;
    }
    else
    {
        if (height < lowestGeneratedHeight) lowestGeneratedHeight = height;
        if (height > highestGeneratedHeight) highestGeneratedHeight = height;
    }
}

void AddVectorCubeDataWithNormalAndTangent(FPtgProcMeshData& procMeshData, TMultiMap<FIntVector, uint32>& uniqueVertices, uint32& vertexCount, uint32& vertexIndex, const FVector& vertex, const FVector& vertexNormal, const FVector& directionComponentY, const FVector2D& UV0, const bool bIsCornerIndex)
{
    // Create rounded vector to avoid decimal precision errors when checking for similar vertices
    const FIntVector roundedVector = FIntVector(FMath::RoundToInt(vertex.X), FMath::RoundToInt(vertex.Y), FMath::RoundToInt(vertex.Z));
    const uint32* vertexIndexPointer = uniqueVertices.Find(roundedVector);

    if (vertexIndexPointer == nullptr)
    {
        // Add new vertex data if doesn't exist yet
        procMeshData.Vertices.Emplace(vertex);
        procMeshData.Normals.Emplace(vertexNormal);
        uniqueVertices.Emplace(roundedVector, vertexCount);
        procMeshData.UV0.Emplace(UV0);
        procMeshData.Tangents.Emplace(FRuntimeMeshTangent(FVector3f(FVector::CrossProduct(procMeshData.Normals[vertexCount], -directionComponentY)), false));
        vertexIndex = vertexCount++;
    }
    else if (bIsCornerIndex)
    {
        // On face limits we need to add the vertex, even if it's in the same location of another one, because it needs to have different UVs
        procMeshData.Vertices.Emplace(vertex);

        const FVector normal = CalculateMiddleVector(procMeshData.Normals[*vertexIndexPointer], vertexNormal);
        TArray<uint32> result;

        // Calculate the new normal and make it the same for the rest of coincident vertices
        uniqueVertices.MultiFind(roundedVector, result);
        for (const uint32 currentVertexIndex : result) procMeshData.Normals[currentVertexIndex] = normal;

        procMeshData.Normals.Emplace(normal);

        uniqueVertices.Emplace(roundedVector, vertexCount);
        procMeshData.UV0.Emplace(UV0);
        procMeshData.Tangents.Emplace(FRuntimeMeshTangent(FVector3f(FVector::CrossProduct(normal, -directionComponentY)), false));
        vertexIndex = vertexCount++;
    }
    else
    {
        // If vertex already exists, don't add it, but update normal and tangent vectors
        vertexIndex = *vertexIndexPointer;
        procMeshData.Normals[vertexIndex] = CalculateMiddleVector(procMeshData.Normals[vertexIndex], vertexNormal);
        procMeshData.Tangents[vertexIndex] = FRuntimeMeshTangent(FVector3f(FVector::CrossProduct(procMeshData.Normals[vertexIndex], -directionComponentY)), false);
    }
}

void FillCubeVertexFaceAssetRotations(TArray<FQuat>& vertexFaceAssetRotation, const uint32& numVertices, const FQuat& rotation)
{
    const uint32 numVertexToAdd = numVertices - vertexFaceAssetRotation.Num();

    for (uint32 i = 0; i < numVertexToAdd; i++) vertexFaceAssetRotation.Emplace(rotation);
}

void BuildCubeFace(FPtgProcMeshData& procMeshData, const FCubeFaceData& cubeFaceData, TMultiMap<FIntVector, uint32>& uniqueVertices, uint32& vertexCount, uint32& trianglesCount, float& lowestGeneratedHeight, float& highestGeneratedHeight, UPtgFastNoiseLiteWrapper* fastNoiseLiteWrapper, const float noiseInput, const float noiseOutput)
{
    const bool bWithNoise = fastNoiseLiteWrapper != nullptr;
    const uint32 resolution = cubeFaceData.Resolution;
    const uint32 lastAxisIndex = resolution - 1;
    const uint32 totalTiles = cubeFaceData.TotalTiles;
    const FVector directionComponentX = (cubeFaceData.FaceAxisDirections.X == 0.0f) ? FVector(0.0f, cubeFaceData.FaceAxisDirections.Y, 0.0f) : FVector(cubeFaceData.FaceAxisDirections.X, 0.0f, 0.0f);
    const FVector directionComponentY = (cubeFaceData.FaceAxisDirections.Z == 0.0f) ? FVector(0.0f, cubeFaceData.FaceAxisDirections.Y, 0.0f) : FVector(0.0f, 0.0f, cubeFaceData.FaceAxisDirections.Z);
    const FVector directionComponentX_Scaled = directionComponentX * cubeFaceData.TileSize;
    const FVector directionComponentY_Scaled = directionComponentY * cubeFaceData.TileSize;
    const FVector topLeftCorner = cubeFaceData.TopLeftCorner;
    const FVector normal = cubeFaceData.Normal;
    uint32 x = 0, y = 0;
    uint32 topLeftVertexIndex = 0;
    uint32 topRightVertexIndex = 0;
    uint32 bottomLeftVertexIndex = 0;
    uint32 bottomRightVertexIndex = 0;
    FVector topLeftVertex = FVector::ZeroVector;
    FVector topRightVertex = FVector::ZeroVector;
    FVector bottomLeftVertex = FVector::ZeroVector;
    FVector bottomRightVertex = FVector::ZeroVector;
    FVector topLeftVertexNormal = cubeFaceData.Normal;
    FVector topRightVertexNormal = cubeFaceData.Normal;
    FVector bottomLeftVertexNormal = cubeFaceData.Normal;
    FVector bottomRightVertexNormal = cubeFaceData.Normal;
    FVector firstTriangleNormal = FVector::ZeroVector;
    FVector secondTriangleNormal = FVector::ZeroVector;
    FVector commonVertexMiddlevector = FVector::ZeroVector;

    for (uint32 currentTileIndex = 0; currentTileIndex < totalTiles; currentTileIndex++)
    {
        x = currentTileIndex % resolution;
        y = currentTileIndex / resolution;

        // Calculate vertex location of the 4 vertices that conforms the tile
        topLeftVertex = topLeftCorner + x * directionComponentX_Scaled + y * directionComponentY_Scaled;
        topRightVertex = topLeftVertex + directionComponentX_Scaled;
        bottomLeftVertex = topLeftVertex + directionComponentY_Scaled;
        bottomRightVertex = topLeftVertex + directionComponentX_Scaled + directionComponentY_Scaled;

        // Apply noise if specified
        if (bWithNoise)
        {
            topLeftVertexNormal = topLeftVertex;
            topRightVertexNormal = topRightVertex;
            bottomLeftVertexNormal = bottomLeftVertex;
            bottomRightVertexNormal = bottomRightVertex;

            topLeftVertexNormal.Normalize();
            topRightVertexNormal.Normalize();
            bottomLeftVertexNormal.Normalize();
            bottomRightVertexNormal.Normalize();

            // Apply the noise with normalized vertex normals, to make each face limits coincide with its adjacent ones
            ApplyNoiseToCubeVertex(topLeftVertex, topLeftVertexNormal, normal, fastNoiseLiteWrapper, noiseInput, noiseOutput, lowestGeneratedHeight, highestGeneratedHeight, vertexCount == 0);
            ApplyNoiseToCubeVertex(topRightVertex, topRightVertexNormal, normal, fastNoiseLiteWrapper, noiseInput, noiseOutput, lowestGeneratedHeight, highestGeneratedHeight, false);
            ApplyNoiseToCubeVertex(bottomLeftVertex, bottomLeftVertexNormal, normal, fastNoiseLiteWrapper, noiseInput, noiseOutput, lowestGeneratedHeight, highestGeneratedHeight, false);
            ApplyNoiseToCubeVertex(bottomRightVertex, bottomRightVertexNormal, normal, fastNoiseLiteWrapper, noiseInput, noiseOutput, lowestGeneratedHeight, highestGeneratedHeight, false);

            // Calculate normals after noise is applied
            firstTriangleNormal = CalculateNormalVectorOfPlane(topLeftVertex, bottomLeftVertex, topRightVertex);
            secondTriangleNormal = CalculateNormalVectorOfPlane(bottomLeftVertex, bottomRightVertex, topRightVertex);
            commonVertexMiddlevector = CalculateMiddleVector(firstTriangleNormal, secondTriangleNormal);    // combined normal for vertices that are owned by two triangles at the same time

            topLeftVertexNormal = firstTriangleNormal;
            topRightVertexNormal = commonVertexMiddlevector;
            bottomLeftVertexNormal = commonVertexMiddlevector;
            bottomRightVertexNormal = secondTriangleNormal;
        }

        // Add the rest of the vertex data unifying the ones that are in the same location
        AddVectorCubeDataWithNormalAndTangent(procMeshData, uniqueVertices, vertexCount, topLeftVertexIndex, topLeftVertex, topLeftVertexNormal, directionComponentY, FVector2D(x, y), x == 0 || y == 0);
        AddVectorCubeDataWithNormalAndTangent(procMeshData, uniqueVertices, vertexCount, topRightVertexIndex, topRightVertex, topRightVertexNormal, directionComponentY, FVector2D(x + 1.0f, y), x == lastAxisIndex || y == 0);
        AddVectorCubeDataWithNormalAndTangent(procMeshData, uniqueVertices, vertexCount, bottomLeftVertexIndex, bottomLeftVertex, bottomLeftVertexNormal, directionComponentY, FVector2D(x, y + 1.0f), x == 0 || y == lastAxisIndex);
        AddVectorCubeDataWithNormalAndTangent(procMeshData, uniqueVertices, vertexCount, bottomRightVertexIndex, bottomRightVertex, bottomRightVertexNormal, directionComponentY, FVector2D(x + 1.0f, y + 1.0f), x == lastAxisIndex || y == lastAxisIndex);

        // Finally, define the 2 triangles on the tile
        DefineTrianglesOnTile(procMeshData.Triangles, trianglesCount, topLeftVertexIndex, topRightVertexIndex, bottomLeftVertexIndex, bottomRightVertexIndex);
    }
}

void UPtgProcMeshDataHelper::GenerateCubeData(FPtgProcMeshData& procMeshData, float& lowestGeneratedHeight, float& highestGeneratedHeight, const float radius, const uint32 resolution, UPtgFastNoiseLiteWrapper* fastNoiseLiteWrapper, const float noiseInput, const float noiseOutput)
{
    const uint32 totalTilesPerFace = resolution * resolution;
    const uint32 totalTiles = totalTilesPerFace * NUM_CUBE_FACES;
    const uint32 totalTrianglePoints = totalTiles * NUM_TRIANGLE_POINTS_PER_TILE;
    const float tileSize = (radius * 2.0f) / resolution;
    const float noiseInputScaled = noiseInput / tileSize;
    uint32 vertexCount = 0;
    uint32 trianglesCount = 0;
    FVector frontTopLeftCorner = FVector(-radius, radius, radius);        // front top left corner
    FVector frontTopRightCorner = FVector(radius, radius, radius);        // front top right corner
    FVector frontBottomLeftCorner = FVector(-radius, radius, -radius);    // front bottom left corner
    FVector backTopLeftCorner = FVector(-radius, -radius, radius);        // back top left corner
    FVector backTopRightCorner = FVector(radius, -radius, radius);        // back top right corner
    TMultiMap<FIntVector, uint32> uniqueVertices;
    FCubeFaceData cubeFaceData;

    // Specify the number of vertices could be not safe at this point, as many of them will be unified, so simply empty the arrays. Triangle number is well known.
    procMeshData.ResizeData(0, totalTrianglePoints);

    // Reserve an estimate for vertices to reduce reallocations across faces
    const uint32 estimatedVertices = (resolution + 1) * (resolution + 1) * NUM_CUBE_FACES;
    procMeshData.Vertices.Reserve(estimatedVertices);
    procMeshData.Normals.Reserve(estimatedVertices);
    procMeshData.UV0.Reserve(estimatedVertices);
    procMeshData.Tangents.Reserve(estimatedVertices);
    uniqueVertices.Reserve(estimatedVertices / 4);

    // Define common vars.
    cubeFaceData.Resolution = resolution;
    cubeFaceData.TotalTiles = totalTilesPerFace;
    cubeFaceData.TileSize = tileSize;
    cubeFaceData.MinHeight = radius;

    // Front face
    cubeFaceData.TopLeftCorner = frontTopLeftCorner;
    cubeFaceData.FaceAxisDirections = CubeFaceDirections::Front;
    cubeFaceData.Normal = CubeFaceNormals::Front;
    cubeFaceData.Tangent = CubeFaceTangents::Front;
    BuildCubeFace(procMeshData, cubeFaceData, uniqueVertices, vertexCount, trianglesCount, lowestGeneratedHeight, highestGeneratedHeight, fastNoiseLiteWrapper, noiseInputScaled, noiseOutput);
    FillCubeVertexFaceAssetRotations(procMeshData.CubeVertexFaceAssetRotation, procMeshData.GetNumVertices(), CubeVertexFaceAssetRotations::Front);

    // Back face
    cubeFaceData.TopLeftCorner = backTopRightCorner;
    cubeFaceData.FaceAxisDirections = CubeFaceDirections::Back;
    cubeFaceData.Normal = CubeFaceNormals::Back;
    cubeFaceData.Tangent = CubeFaceTangents::Back;
    BuildCubeFace(procMeshData, cubeFaceData, uniqueVertices, vertexCount, trianglesCount, lowestGeneratedHeight, highestGeneratedHeight, fastNoiseLiteWrapper, noiseInputScaled, noiseOutput);
    FillCubeVertexFaceAssetRotations(procMeshData.CubeVertexFaceAssetRotation, procMeshData.GetNumVertices(), CubeVertexFaceAssetRotations::Back);

    // Left face
    cubeFaceData.TopLeftCorner = backTopLeftCorner;
    cubeFaceData.FaceAxisDirections = CubeFaceDirections::Left;
    cubeFaceData.Normal = CubeFaceNormals::Left;
    cubeFaceData.Tangent = CubeFaceTangents::Left;
    BuildCubeFace(procMeshData, cubeFaceData, uniqueVertices, vertexCount, trianglesCount, lowestGeneratedHeight, highestGeneratedHeight, fastNoiseLiteWrapper, noiseInputScaled, noiseOutput);
    FillCubeVertexFaceAssetRotations(procMeshData.CubeVertexFaceAssetRotation, procMeshData.GetNumVertices(), CubeVertexFaceAssetRotations::Left);

    // Right face
    cubeFaceData.TopLeftCorner = frontTopRightCorner;
    cubeFaceData.FaceAxisDirections = CubeFaceDirections::Right;
    cubeFaceData.Normal = CubeFaceNormals::Right;
    cubeFaceData.Tangent = CubeFaceTangents::Right;
    BuildCubeFace(procMeshData, cubeFaceData, uniqueVertices, vertexCount, trianglesCount, lowestGeneratedHeight, highestGeneratedHeight, fastNoiseLiteWrapper, noiseInputScaled, noiseOutput);
    FillCubeVertexFaceAssetRotations(procMeshData.CubeVertexFaceAssetRotation, procMeshData.GetNumVertices(), CubeVertexFaceAssetRotations::Right);

    // Top face
    cubeFaceData.TopLeftCorner = backTopLeftCorner;
    cubeFaceData.FaceAxisDirections = CubeFaceDirections::Top;
    cubeFaceData.Normal = CubeFaceNormals::Top;
    cubeFaceData.Tangent = CubeFaceTangents::Top;
    BuildCubeFace(procMeshData, cubeFaceData, uniqueVertices, vertexCount, trianglesCount, lowestGeneratedHeight, highestGeneratedHeight, fastNoiseLiteWrapper, noiseInputScaled, noiseOutput);
    FillCubeVertexFaceAssetRotations(procMeshData.CubeVertexFaceAssetRotation, procMeshData.GetNumVertices(), CubeVertexFaceAssetRotations::Top);

    // Bottom face
    cubeFaceData.TopLeftCorner = frontBottomLeftCorner;
    cubeFaceData.FaceAxisDirections = CubeFaceDirections::Bottom;
    cubeFaceData.Normal = CubeFaceNormals::Bottom;
    cubeFaceData.Tangent = CubeFaceTangents::Bottom;
    BuildCubeFace(procMeshData, cubeFaceData, uniqueVertices, vertexCount, trianglesCount, lowestGeneratedHeight, highestGeneratedHeight, fastNoiseLiteWrapper, noiseInputScaled, noiseOutput);
    FillCubeVertexFaceAssetRotations(procMeshData.CubeVertexFaceAssetRotation, procMeshData.GetNumVertices(), CubeVertexFaceAssetRotations::Bottom);

    // Init vertex colors with the same value
    procMeshData.VertexColors.Init(DEFAULT_COLOR, procMeshData.GetNumVertices());
}

void ApplyNoiseToSphereVertex(FVector& vertex, const FVector& vertexNormal, UPtgFastNoiseLiteWrapper* fastNoiseLiteWrapper, const float noiseInput, const float noiseOutput, float& lowestGeneratedHeight, float& highestGeneratedHeight, const bool bIsFirstVertex)
{
    float noiseValue = fastNoiseLiteWrapper->GetNoise3D(vertex.X * noiseInput, vertex.Y * noiseInput, vertex.Z * noiseInput) * noiseOutput;

    // To avoid overlapping vertex below the radius, all noise generated under 0 is inverted
    if (noiseValue < 0.0f) noiseValue *= -1.0f;

    // Add noise to vertex using its normal
    vertex += vertexNormal * noiseValue;

    // Height is the distance from the center of the sphere to the vertex
    const float height = FVector::Distance(FVector::ZeroVector, vertex);

    if (bIsFirstVertex)
    {
        lowestGeneratedHeight = highestGeneratedHeight = height;
    }
    else
    {
        if (height < lowestGeneratedHeight) lowestGeneratedHeight = height;
        if (height > highestGeneratedHeight) highestGeneratedHeight = height;
    }
}

void BuildSphereFace(FPtgProcMeshData& procMeshData, const FCubeFaceData& cubeFaceData, TMultiMap<FIntVector, uint32>& uniqueVertices, uint32& vertexCount, uint32& trianglesCount, float& lowestGeneratedHeight, float& highestGeneratedHeight, UPtgFastNoiseLiteWrapper* fastNoiseLiteWrapper, const float noiseInput, const float noiseOutput)
{
    const bool bWithNoise = fastNoiseLiteWrapper != nullptr;
    const uint32 resolution = cubeFaceData.Resolution;
    const uint32 lastAxisIndex = resolution - 1;
    const uint32 totalTiles = cubeFaceData.TotalTiles;
    const FVector directionComponentX = (cubeFaceData.FaceAxisDirections.X == 0.0f) ? FVector(0.0f, cubeFaceData.FaceAxisDirections.Y, 0.0f) : FVector(cubeFaceData.FaceAxisDirections.X, 0.0f, 0.0f);
    const FVector directionComponentY = (cubeFaceData.FaceAxisDirections.Z == 0.0f) ? FVector(0.0f, cubeFaceData.FaceAxisDirections.Y, 0.0f) : FVector(0.0f, 0.0f, cubeFaceData.FaceAxisDirections.Z);
    const FVector directionComponentX_Scaled = directionComponentX * cubeFaceData.TileSize;
    const FVector directionComponentY_Scaled = directionComponentY * cubeFaceData.TileSize;
    const FVector topLeftCorner = cubeFaceData.TopLeftCorner;
    const float minHeight = cubeFaceData.MinHeight;
    uint32 x = 0, y = 0;
    uint32 topLeftVertexIndex = 0;
    uint32 topRightVertexIndex = 0;
    uint32 bottomLeftVertexIndex = 0;
    uint32 bottomRightVertexIndex = 0;
    FVector topLeftVertex = FVector::ZeroVector;
    FVector topRightVertex = FVector::ZeroVector;
    FVector bottomLeftVertex = FVector::ZeroVector;
    FVector bottomRightVertex = FVector::ZeroVector;
    FVector topLeftVertexNormal = FVector::ZeroVector;
    FVector topRightVertexNormal = FVector::ZeroVector;
    FVector bottomLeftVertexNormal = FVector::ZeroVector;
    FVector bottomRightVertexNormal = FVector::ZeroVector;
    FVector firstTriangleNormal = FVector::ZeroVector;
    FVector secondTriangleNormal = FVector::ZeroVector;
    FVector commonVertexMiddlevector = FVector::ZeroVector;

    for (uint32 currentTileIndex = 0; currentTileIndex < totalTiles; currentTileIndex++)
    {
        x = currentTileIndex % resolution;
        y = currentTileIndex / resolution;

        // Calculate vertex location of the 4 vertices that conforms the tile
        topLeftVertex = topLeftCorner + x * directionComponentX_Scaled + y * directionComponentY_Scaled;
        topRightVertex = topLeftVertex + directionComponentX_Scaled;
        bottomLeftVertex = topLeftVertex + directionComponentY_Scaled;
        bottomRightVertex = topLeftVertex + directionComponentX_Scaled + directionComponentY_Scaled;

        // Normalize the vertices to spherify them
        topLeftVertex.Normalize();
        topRightVertex.Normalize();
        bottomLeftVertex.Normalize();
        bottomRightVertex.Normalize();

        topLeftVertexNormal = topLeftVertex;
        topRightVertexNormal = topRightVertex;
        bottomLeftVertexNormal = bottomLeftVertex;
        bottomRightVertexNormal = bottomRightVertex;

        // Make vertex height according to the radius
        topLeftVertex *= minHeight;
        topRightVertex *= minHeight;
        bottomLeftVertex *= minHeight;
        bottomRightVertex *= minHeight;

        // Apply noise if specified
        if (bWithNoise)
        {
            ApplyNoiseToSphereVertex(topLeftVertex, topLeftVertexNormal, fastNoiseLiteWrapper, noiseInput, noiseOutput, lowestGeneratedHeight, highestGeneratedHeight, vertexCount == 0);
            ApplyNoiseToSphereVertex(topRightVertex, topRightVertexNormal, fastNoiseLiteWrapper, noiseInput, noiseOutput, lowestGeneratedHeight, highestGeneratedHeight, false);
            ApplyNoiseToSphereVertex(bottomLeftVertex, bottomLeftVertexNormal, fastNoiseLiteWrapper, noiseInput, noiseOutput, lowestGeneratedHeight, highestGeneratedHeight, false);
            ApplyNoiseToSphereVertex(bottomRightVertex, bottomRightVertexNormal, fastNoiseLiteWrapper, noiseInput, noiseOutput, lowestGeneratedHeight, highestGeneratedHeight, false);

            // Calculate normals after noise is applied
            firstTriangleNormal = CalculateNormalVectorOfPlane(topLeftVertex, bottomLeftVertex, topRightVertex);
            secondTriangleNormal = CalculateNormalVectorOfPlane(bottomLeftVertex, bottomRightVertex, topRightVertex);
            commonVertexMiddlevector = CalculateMiddleVector(firstTriangleNormal, secondTriangleNormal);    // combined normal for vertices that are owned by two triangles at the same time

            topLeftVertexNormal = firstTriangleNormal;
            topRightVertexNormal = commonVertexMiddlevector;
            bottomLeftVertexNormal = commonVertexMiddlevector;
            bottomRightVertexNormal = secondTriangleNormal;
        }

        // Add the rest of the vertex data unifying the ones that are in the same location
        AddVectorCubeDataWithNormalAndTangent(procMeshData, uniqueVertices, vertexCount, topLeftVertexIndex, topLeftVertex, topLeftVertexNormal, directionComponentY, FVector2D(x, y), x == 0 || y == 0);
        AddVectorCubeDataWithNormalAndTangent(procMeshData, uniqueVertices, vertexCount, topRightVertexIndex, topRightVertex, topRightVertexNormal, directionComponentY, FVector2D(x + 1.0f, y), x == lastAxisIndex || y == 0);
        AddVectorCubeDataWithNormalAndTangent(procMeshData, uniqueVertices, vertexCount, bottomLeftVertexIndex, bottomLeftVertex, bottomLeftVertexNormal, directionComponentY, FVector2D(x, y + 1.0f), x == 0 || y == lastAxisIndex);
        AddVectorCubeDataWithNormalAndTangent(procMeshData, uniqueVertices, vertexCount, bottomRightVertexIndex, bottomRightVertex, bottomRightVertexNormal, directionComponentY, FVector2D(x + 1.0f, y + 1.0f), x == lastAxisIndex || y == lastAxisIndex);

        // Finally, define the 2 triangles on the tile
        DefineTrianglesOnTile(procMeshData.Triangles, trianglesCount, topLeftVertexIndex, topRightVertexIndex, bottomLeftVertexIndex, bottomRightVertexIndex);
    }
}

void UPtgProcMeshDataHelper::GenerateSphereData(FPtgProcMeshData& procMeshData, float& lowestGeneratedHeight, float& highestGeneratedHeight, const float radius, const uint32 resolution, UPtgFastNoiseLiteWrapper* fastNoiseLiteWrapper, const float noiseInput, const float noiseOutput)
{
    const uint32 totalTilesPerFace = resolution * resolution;
    const uint32 totalTiles = totalTilesPerFace * NUM_CUBE_FACES;
    const uint32 totalTrianglePoints = totalTiles * NUM_TRIANGLE_POINTS_PER_TILE;
    const float tileSize = (radius * 2.0f) / resolution;
    const float noiseInputScaled = noiseInput / tileSize;
    uint32 vertexCount = 0;
    uint32 trianglesCount = 0;
    FVector frontTopLeftCorner = FVector(-radius, radius, radius);        // front top left corner
    FVector frontTopRightCorner = FVector(radius, radius, radius);        // front top right corner
    FVector frontBottomLeftCorner = FVector(-radius, radius, -radius);    // front bottom left corner
    FVector backTopLeftCorner = FVector(-radius, -radius, radius);        // back top left corner
    FVector backTopRightCorner = FVector(radius, -radius, radius);        // back top right corner
    TMultiMap<FIntVector, uint32> uniqueVertices;
    FCubeFaceData cubeFaceData;

    // Specify the number of vertices could be not safe at this point, as many of them will be unified, so simply empty the arrays. Triangle number is well known.
    procMeshData.ResizeData(0, totalTrianglePoints);

    // Reserve an estimate for vertices to reduce reallocations across faces
    const uint32 estimatedVertices = (resolution + 1) * (resolution + 1) * NUM_CUBE_FACES;
    procMeshData.Vertices.Reserve(estimatedVertices);
    procMeshData.Normals.Reserve(estimatedVertices);
    procMeshData.UV0.Reserve(estimatedVertices);
    procMeshData.Tangents.Reserve(estimatedVertices);
    uniqueVertices.Reserve(estimatedVertices / 4);

    // Define common vars.
    cubeFaceData.Resolution = resolution;
    cubeFaceData.TotalTiles = totalTilesPerFace;
    cubeFaceData.TileSize = tileSize;
    cubeFaceData.MinHeight = radius;

    // Front face
    cubeFaceData.TopLeftCorner = frontTopLeftCorner;
    cubeFaceData.FaceAxisDirections = CubeFaceDirections::Front;
    cubeFaceData.Normal = CubeFaceNormals::Front;
    cubeFaceData.Tangent = CubeFaceTangents::Front;
    BuildSphereFace(procMeshData, cubeFaceData, uniqueVertices, vertexCount, trianglesCount, lowestGeneratedHeight, highestGeneratedHeight, fastNoiseLiteWrapper, noiseInputScaled, noiseOutput);

    // Back face
    cubeFaceData.TopLeftCorner = backTopRightCorner;
    cubeFaceData.FaceAxisDirections = CubeFaceDirections::Back;
    cubeFaceData.Normal = CubeFaceNormals::Back;
    cubeFaceData.Tangent = CubeFaceTangents::Back;
    BuildSphereFace(procMeshData, cubeFaceData, uniqueVertices, vertexCount, trianglesCount, lowestGeneratedHeight, highestGeneratedHeight, fastNoiseLiteWrapper, noiseInputScaled, noiseOutput);

    // Left face
    cubeFaceData.TopLeftCorner = backTopLeftCorner;
    cubeFaceData.FaceAxisDirections = CubeFaceDirections::Left;
    cubeFaceData.Normal = CubeFaceNormals::Left;
    cubeFaceData.Tangent = CubeFaceTangents::Left;
    BuildSphereFace(procMeshData, cubeFaceData, uniqueVertices, vertexCount, trianglesCount, lowestGeneratedHeight, highestGeneratedHeight, fastNoiseLiteWrapper, noiseInputScaled, noiseOutput);

    // Right face
    cubeFaceData.TopLeftCorner = frontTopRightCorner;
    cubeFaceData.FaceAxisDirections = CubeFaceDirections::Right;
    cubeFaceData.Normal = CubeFaceNormals::Right;
    cubeFaceData.Tangent = CubeFaceTangents::Right;
    BuildSphereFace(procMeshData, cubeFaceData, uniqueVertices, vertexCount, trianglesCount, lowestGeneratedHeight, highestGeneratedHeight, fastNoiseLiteWrapper, noiseInputScaled, noiseOutput);

    // Top face
    cubeFaceData.TopLeftCorner = backTopLeftCorner;
    cubeFaceData.FaceAxisDirections = CubeFaceDirections::Top;
    cubeFaceData.Normal = CubeFaceNormals::Top;
    cubeFaceData.Tangent = CubeFaceTangents::Top;
    BuildSphereFace(procMeshData, cubeFaceData, uniqueVertices, vertexCount, trianglesCount, lowestGeneratedHeight, highestGeneratedHeight, fastNoiseLiteWrapper, noiseInputScaled, noiseOutput);

    // Bottom face
    cubeFaceData.TopLeftCorner = frontBottomLeftCorner;
    cubeFaceData.FaceAxisDirections = CubeFaceDirections::Bottom;
    cubeFaceData.Normal = CubeFaceNormals::Bottom;
    cubeFaceData.Tangent = CubeFaceTangents::Bottom;
    BuildSphereFace(procMeshData, cubeFaceData, uniqueVertices, vertexCount, trianglesCount, lowestGeneratedHeight, highestGeneratedHeight, fastNoiseLiteWrapper, noiseInputScaled, noiseOutput);

    // Init vertex colors with the same value
    procMeshData.VertexColors.Init(DEFAULT_COLOR, procMeshData.GetNumVertices());
}
