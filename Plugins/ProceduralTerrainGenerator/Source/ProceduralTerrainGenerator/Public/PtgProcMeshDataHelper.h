// Copyright 2021 VICTOR HERNANDEZ MOLPECERES (Rockam). All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RuntimeMeshCore.h"
#include "PtgProcMeshDataHelper.generated.h"

// Cube unwrapped
//
//			  +---------+
//			  |         |
//			  |   top   |
//			  |		    |
//	+---------+---------+---------+---------+
//	|         |			|		  |			|
//	|  left   |  front  |  right  |	 back	|
//	|		  |			|		  |			|
//	+---------+---------+---------+---------+
//			  |		    |
//			  | bottom  |
//			  |		    |
//			  +---------+

class UPtgFastNoiseLiteWrapper;

/**
* Possible shapes for the procedural mesh.
*/
UENUM(BlueprintType)
enum class EPtgProcMeshShapes : uint8
{
	Plane	UMETA(DisplayName = "Plane"),
	Cube	UMETA(DisplayName = "Cube"),
	Sphere	UMETA(DisplayName = "Sphere")	// it's really a spherified cube
};

/**
* Properties that define a tile when a procedural mesh is used.
*/
USTRUCT(BlueprintType)
struct FPtgProcMeshData
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, Category = "Properties")
	int32 SectionIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Properties")
	TArray<FVector> Vertices;

	UPROPERTY(BlueprintReadWrite, Category = "Properties")
	TArray<int32> Triangles;

	UPROPERTY(BlueprintReadWrite, Category = "Properties")
	TArray<FVector> Normals;

	UPROPERTY(BlueprintReadWrite, Category = "Properties")
	TArray<FVector2D> UV0;

	UPROPERTY(BlueprintReadWrite, Category = "Properties")
	TArray<FLinearColor> VertexColors;

	UPROPERTY(BlueprintReadWrite, Category = "Properties")
	TArray<FRuntimeMeshTangent> Tangents;

	UPROPERTY(BlueprintReadWrite, Category = "Properties")
	bool bEnableCollision = false;

	UPROPERTY()
	TArray<FQuat> CubeVertexFaceAssetRotation;

	////////////////////////////////////////////////////////////////////////////////////////

	~FPtgProcMeshData()
	{
		ClearData();
	}

	void ResizeData(const int32 newNumVertices, const int32 newNumTriangles)
	{
		const int32 verticesDifference = newNumVertices - Vertices.Num();
		const int32 trianglesDifference = newNumTriangles - Triangles.Num();

		if (newNumVertices == 0)
		{
			Vertices.Empty();
			Normals.Empty();
			UV0.Empty();
			VertexColors.Empty();
			Tangents.Empty();
		}
		else if (verticesDifference > 0)
		{
			Vertices.AddUninitialized(verticesDifference);
			Normals.AddUninitialized(verticesDifference);
			UV0.AddUninitialized(verticesDifference);
			VertexColors.AddUninitialized(verticesDifference);
			Tangents.AddUninitialized(verticesDifference);
		}
		else if (verticesDifference < 0)
		{
			const uint32 index = newNumVertices - 1;
			const uint32 itemsToRemove = FMath::Abs(verticesDifference);

			Vertices.RemoveAtSwap(index, itemsToRemove);
			Normals.RemoveAtSwap(index, itemsToRemove);
			UV0.RemoveAtSwap(index, itemsToRemove);
			VertexColors.RemoveAtSwap(index, itemsToRemove);
			Tangents.RemoveAtSwap(index, itemsToRemove);
		}

		CubeVertexFaceAssetRotation.Empty();

		if (newNumTriangles == 0)
		{
			Triangles.Empty();
		}
		else if (trianglesDifference > 0)
		{
			Triangles.AddUninitialized(trianglesDifference);
		}
		else if (trianglesDifference < 0)
		{
			Triangles.RemoveAtSwap(newNumTriangles - 1, FMath::Abs(trianglesDifference));
		}
	}

	void ClearData()
	{
		Vertices.Empty();
		Triangles.Empty();
		Normals.Empty();
		UV0.Empty();
		VertexColors.Empty();
		Tangents.Empty();
		CubeVertexFaceAssetRotation.Empty();
	}

	FORCEINLINE uint32 GetNumVertices() const { return Vertices.Num(); }
	FORCEINLINE uint32 GetNumTriangles() const { return Triangles.Num(); }
	FORCEINLINE const TArray<FVector>& GetVertexData() const { return Vertices; }
};

/**
 * This class creates the needed procedural mesh data to create a plane, cube or sphere with noise, if specified.
 * It is also exposed to blueprints.
 */
UCLASS()
class PROCEDURALTERRAINGENERATOR_API UPtgProcMeshDataHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	* Generates data for procedural mesh component with the specified shape. Noise could be applied if specified.
	*
	* @param procMeshShape					- Shape of the procedural mesh that will be created.
	* @param radius							- Radius of the generated mesh.
	* @param resolution						- Number of subdivisions the algorithm will generate on x and y axis.
	* @param useTerrainTiling				- If true, it will add an offset to the noise based on actor location, so the terrain will tile with other generated terrains. Only available on plane terrain shape.
	* @param fastNoiseLiteWrapper			- Fast Noise Lite wrapper that should be specified if noise will be applied.
	* @param noiseInput						- Making this smaller will "stretch" the noise terrain, if Fast Noise Lite wrapper is specified.
	* @param noiseOutput					- Making this bigger will scale the terrain's height, if Fast Noise Lite wrapper is specified.
	* @param actorLocation					- Terrain actor location, used only for terrain tiling on plane terrains if use terrain tiling is true.
	*
	* @param [out] lowestGeneratedHeight	- Lowest generated vertex height.
	* @param [out] highestGeneratedHeight	- Highest generated vertex height.
	* @return Procedural mesh data structure (vertices, triangles, normals, etc.).
	*/
	UFUNCTION(BlueprintCallable, Category = "PTG Procedural Mesh Data Helper")
	static FPtgProcMeshData GenerateProcMeshData
	(
		float& lowestGeneratedHeight,
		float& highestGeneratedHeight,
		const EPtgProcMeshShapes procMeshShape = EPtgProcMeshShapes::Plane,
		const float radius = 15000.0f,
		const int32 resolution = 150,
		const bool bUseTerrainTiling = false,
		UPtgFastNoiseLiteWrapper* fastNoiseLiteWrapper = nullptr,
		const float noiseInput = 1.0f,
		const float noiseOutput = 1000.0f,
		const FVector& actorLocation = FVector::ZeroVector
	);

	static void GeneratePlaneData(FPtgProcMeshData& procMeshData, float& lowestGeneratedHeight, float& highestGeneratedHeight, const float radius, const uint32 resolution, UPtgFastNoiseLiteWrapper* fastNoiseLiteWrapper = nullptr, const float noiseInput = 1.0f, const float noiseOutput = 1000.0f, const FVector& actorLocation = FVector::ZeroVector);
	static void GenerateCubeData(FPtgProcMeshData& procMeshData, float& lowestGeneratedHeight, float& highestGeneratedHeight, const float radius, const uint32 resolution, UPtgFastNoiseLiteWrapper* fastNoiseLiteWrapper = nullptr, const float noiseInput = 1.0f, const float noiseOutput = 1000.0f);
	static void GenerateSphereData(FPtgProcMeshData& procMeshData, float& lowestGeneratedHeight, float& highestGeneratedHeight, const float radius, const uint32 resolution, UPtgFastNoiseLiteWrapper* fastNoiseLiteWrapper = nullptr, const float noiseInput = 1.0f, const float noiseOutput = 1000.0f);
};
