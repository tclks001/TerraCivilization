// Copyright 2021 VICTOR HERNANDEZ MOLPECERES (Rockam). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PtgProcMeshDataHelper.h"
#include "PtgFastNoiseLiteWrapper.h"
#include "PtgManager.generated.h"

// Delegate declarations for generation lifecycle (Blueprint-assignable)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPTGGenerationStartedDelegate, FString, GenerationType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPTGGenerationCompletedDelegate, FString, GenerationType, int32, NumTriangles);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPTGGenerationFailedDelegate, FString, GenerationType, FString, Reason);
// Progress delegate (Blueprint-assignable)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPTGGenerationProgressDelegate, FString, Step, float, Percentage);

#define NUM_BIOMA_ROTATION_FUNCS 5
#define NUM_TRIANGLE_INSIDE_HEIGHT_RANGE_FUNCS 3

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class AStaticMeshActor;
class UShapeComponent;
class URuntimeMeshComponent;
class UPtgRuntimeMesh;
class APtgModifier;

const int32 DEFAULT_SEED = 1619;

/**
* Types of water height generation. Fixed Height is only available on plane terrain shape.
*/
UENUM(BlueprintType)
enum class EPtgWaterHeightGenerationType : uint8
{
	RandomPercentage	UMETA(DisplayName = "Random Percentage"),
	FixedPercentage		UMETA(DisplayName = "Fixed Percentage"),
	FixedHeight			UMETA(DisplayName = "Fixed Height")
};

/**
* Types of biomas.
*/
UENUM(BlueprintType)
enum class EPtgNatureBiomas : uint8
{
	Earth				UMETA(DisplayName = "Earth surface"),
	Underwater			UMETA(DisplayName = "Underwater"),
	Both				UMETA(DisplayName = "Both")
};

/**
* Types of nature and actors rotations.
*/
UENUM(BlueprintType)
enum class EPtgNatureRotationTypes : uint8
{
	Random				UMETA(DisplayName = "Random"),
	TerrainShapeNormal	UMETA(DisplayName = "Terrain Shape Normal"),
	MeshSurfaceNormal	UMETA(DisplayName = "Mesh Surface Normal")
};

/**
* Defines a bunch of properties that will generate a random number of nature meshes on a certain bioma.
*/
USTRUCT(BlueprintType)
struct FPtgBiomaNature
{
	GENERATED_BODY()

public:

	/** Bioma where the nature mesh will be randomly placed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	EPtgNatureBiomas CorrespondingBioma = EPtgNatureBiomas::Earth;

	/** Distance (in centimeters) from camera at which each generated instance fades out. A value of 0 means infinite. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties", meta = (ClampMin = 0, UIMin = 0))
	int32 CullDistance = 150000;	// 1,5 km.

	/**
	* Enable for detail meshes that don't really affect the game. Disable for anything important.
	* Typically, this will be enabled for small meshes without collision (e.g. grass) and disabled for large meshes with collision (e.g. trees)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bEnableDensityScaling = false;

	/** Whether this component should cast shadows or not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bCastShadow = true;

	/**
	* Whether this component can potentially influence navigation.
	* **WARNING**: Enabling this can cause a HUGE MASSIVE performance issue when generating nature.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bCanAffectNavigation = false;

	/** Whether this component should generate overlap events or not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bGenerateOverlapEvents = false;

	/** Component collision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	TEnumAsByte<ECollisionEnabled::Type> CollisionEnabled = ECollisionEnabled::QueryAndPhysics;

	/** Component collision object type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	TEnumAsByte<ECollisionChannel> CollisionObjectType = ECollisionChannel::ECC_WorldStatic;

	/** Array of nature meshes used by PTG to randomly pick each time it adds a nature mesh in the bioma. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	TArray<TObjectPtr<UStaticMesh>> Meshes;

	/** Minimum nature meshes that will be added on the corresponding bioma. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	int32 MinMeshesToSpawn = 100;

	/** Maximum nature meshes that will be added on the corresponding bioma. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	int32 MaxMeshesToSpawn = 1000;

	/** Min. and max. range to randomly scale each added nature mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	FVector2D MinMaxScale = FVector2D(0.5f, 3.0f);

	/**
	* Different types of rotation for the nature mesh:
	* - Random: random rotation, commonly used on stones, for example.
	* - Terrain Shape Normal: rotation that matches the terrain face normal, commonly used on trees, for example.
	* - Mesh Surface Normal: rotation that matches the normal of the nature mesh surface where it is placed, commonly used for grass, for example.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	EPtgNatureRotationTypes RotationType = EPtgNatureRotationTypes::TerrainShapeNormal;

	/** Seed used for all the random operations of this bioma nature. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	int32 Seed = DEFAULT_SEED;

	/** Array of shape modifiers to avoid when placing new nature meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	TArray<TObjectPtr<APtgModifier>> Modifiers;

	/** Disable this option to get the inverted result on modifiers (nature meshes will be located only inside of them). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bUseLocationsOutsideModifiers = true;

	/**
	* Height percentage range where nature meshes will be placed.
	* For example, with these values:
	* · Lowest Landscape Height = 0.0
	* · Highest Landscape Height = 1000.0
	* · Height Percentage Range to Locate Meshes = (25.0, 75.0)
	* Nature meshes will be only placed in heights between 250.0 and 750.0.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties", meta = (ClampMin = 0.0f, UIMin = 0.0f, ClampMax = 100.0f, UIMax = 100.0f))
	FVector2D HeightPercentageRangeToLocateNatureMeshes = FVector2D(0.0f, 100.0f);

	/** Enable this option to get the inverted result (nature meshes will be located only out of height range). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bUseLocationsOutsideHeightRange = false;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	~FPtgBiomaNature()
	{
		Meshes.Empty();
		Modifiers.Empty();
	}
};

/**
* Defines a bunch of properties that will spawn a random number of actors on a certain bioma.
*/
USTRUCT(BlueprintType)
struct FPtgBiomaActors
{
	GENERATED_BODY()

public:

	/** Bioma where the actor will be randomly placed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	EPtgNatureBiomas CorrespondingBioma = EPtgNatureBiomas::Earth;

	/** Distance (in centimeters) from camera at which each generated actor fades out. A value of 0 means infinite. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties", meta = (ClampMin = 0, UIMin = 0))
	int32 CullDistance = 150000;	// 1,5 km.

	/** Class of the actor that will be randomly placed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	TSubclassOf<AActor> ActorClass = nullptr;

	/** Minimum actors that will be spawned on the corresponding bioma. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	int32 MinActorsToSpawn = 5;

	/** Maximum actors that will be spawned on the corresponding bioma. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	int32 MaxActorsToSpawn = 30;

	/** Min. and max. range to randomly scale each added actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	FVector2D MinMaxScale = FVector2D(1.0f, 1.0f);

	/**
	* Different types of rotation for the mesh:
	* - Random: random rotation, commonly used on stones, for example.
	* - Terrain Shape Normal: rotation that matches the terrain face normal, commonly used on trees, for example.
	* - Mesh Surface Normal: rotation that matches the normal of the mesh surface where it is placed, commonly used for grass, for example.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	EPtgNatureRotationTypes RotationType = EPtgNatureRotationTypes::TerrainShapeNormal;

	/** Seed used for all the random operations on the actor generation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	int32 Seed = DEFAULT_SEED;

	/** Array of shape modifiers to avoid when placing new actors.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	TArray<TObjectPtr<APtgModifier>> Modifiers;

	/** Disable this option to get the inverted result on modifiers (actors will be located only inside of them). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bUseLocationsOutsideModifiers = true;

	/**
	* Height percentage range where actors will be placed.
	* For example, with these values:
	* · Lowest Landscape Height = 0.0
	* · Highest Landscape Height = 1000.0
	* · Height Percentage Range to Locate Actors = (25.0, 75.0)
	* Actors will be only placed in heights between 250.0 and 750.0.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties", meta = (ClampMin = 0.0f, UIMin = 0.0f, ClampMax = 100.0f, UIMax = 100.0f))
	FVector2D HeightPercentageRangeToLocateActors = FVector2D(0.0f, 100.0f);

	/** Enable this option to get the inverted result (actors will be located only out of height range). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bUseLocationsOutsideHeightRange = false;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	~FPtgBiomaActors()
	{
		ActorClass = nullptr;
		Modifiers.Empty();
	}
};

/**
* Represents a terrain mesh triangle. It has some functions to help creating random locations to place nature and actors.
*/
USTRUCT()
struct FPtgTriangle
{
	GENERATED_BODY()

public:

	FVector VertexA;
	FVector VertexB;
	FVector VertexC;
	FQuat CubeVertexFaceNormal;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	FPtgTriangle
	(
		const FVector& vertexA = FVector::ZeroVector,
		const FVector& vertexB = FVector::ZeroVector,
		const FVector& vertexC = FVector::ZeroVector,
		const FQuat& cubeVertexFaceNormal = FRotator::ZeroRotator.Quaternion()
	)
	{
		VertexA = vertexA;
		VertexB = vertexB;
		VertexC = vertexC;
		CubeVertexFaceNormal = cubeVertexFaceNormal;
	}

	~FPtgTriangle() {}

	/**
	* Tells whether at least 2 vertices of this triangle are within the given height range for plane terrain shapes.
	*
	* @param lowerHeight -	The lower height value.
	* @param higherHeight -	The higher height value.
	* @return True if at least 2 vertices of this triangle are within the given height range.
	*/
	FORCEINLINE bool IsInsidePlaneHeigthRange(const float lowerHeight, const float higherHeight, const FTransform& terrainTransform) const
	{
		const bool bIsVertexA_InsideHeigthRange = VertexA.Z > lowerHeight && VertexA.Z < higherHeight;
		const bool bIsVertexB_InsideHeigthRange = VertexB.Z > lowerHeight && VertexB.Z < higherHeight;
		const bool bIsVertexC_InsideHeigthRange = VertexC.Z > lowerHeight && VertexC.Z < higherHeight;

		return (bIsVertexA_InsideHeigthRange && bIsVertexB_InsideHeigthRange)
			|| (bIsVertexA_InsideHeigthRange && bIsVertexC_InsideHeigthRange)
			|| (bIsVertexB_InsideHeigthRange && bIsVertexC_InsideHeigthRange);
	}

	/**
	* Tells whether at least 2 vertices of this triangle are within the given height range for cube terrain shapes.
	*
	* @param lowerHeight -	The lower height value.
	* @param higherHeight -	The higher height value.
	* @return True if at least 2 vertices of this triangle are within the given height range.
	*/
	FORCEINLINE bool IsInsideCubeHeigthRange(const float lowerHeight, const float higherHeight, const FTransform& terrainTransform) const;

	/**
	* Tells whether at least 2 vertices of this triangle are within the given height range for sphere terrain shapes.
	*
	* @param lowerHeight -	The lower height value.
	* @param higherHeight -	The higher height value.
	* @return True if at least 2 vertices of this triangle are within the given height range.
	*/
	FORCEINLINE bool IsInsideSphereHeigthRange(const float lowerHeight, const float higherHeight, const FTransform& terrainTransform) const
	{
		const float distanceA = FMath::Sqrt((VertexA.X * VertexA.X) + (VertexA.Y * VertexA.Y) + (VertexA.Z * VertexA.Z));
		const float distanceB = FMath::Sqrt((VertexB.X * VertexB.X) + (VertexB.Y * VertexB.Y) + (VertexB.Z * VertexB.Z));
		const float distanceC = FMath::Sqrt((VertexC.X * VertexC.X) + (VertexC.Y * VertexC.Y) + (VertexC.Z * VertexC.Z));
		const bool bIsVertexA_InsideHeigthRange = distanceA > lowerHeight && distanceA < higherHeight;
		const bool bIsVertexB_InsideHeigthRange = distanceB > lowerHeight && distanceB < higherHeight;
		const bool bIsVertexC_InsideHeigthRange = distanceC > lowerHeight && distanceC < higherHeight;

		return (bIsVertexA_InsideHeigthRange && bIsVertexB_InsideHeigthRange)
			|| (bIsVertexA_InsideHeigthRange && bIsVertexC_InsideHeigthRange)
			|| (bIsVertexB_InsideHeigthRange && bIsVertexC_InsideHeigthRange);
	}

	/**
	* Gets a random point on the triangle surface.
	* References:
	* - https://adamswaab.wordpress.com/2009/12/11/random-point-in-a-triangle-barycentric-coordinates/
	* - https://math.stackexchange.com/questions/538458/triangle-point-picking-in-3d
	*
	* @param randomNumberGenerator -	Random generator stream.
	* @return A random point in the triangle.
	*/
	FORCEINLINE FVector GetRandomPointOnTriangle(FRandomStream& randomNumberGenerator) const
	{
		float randomA = randomNumberGenerator.GetFraction();
		float randomB = randomNumberGenerator.GetFraction();

		if (randomA + randomB >= 1.0f)
		{
			randomA = 1.0f - randomA;
			randomB = 1.0f - randomB;
		}

		return VertexA + randomA * (VertexB - VertexA) + randomB * (VertexC - VertexA);
	}

	/**
	* Gets the triangle surface normal quaternion.
	* References:
	* - https://math.stackexchange.com/questions/305642/how-to-find-surface-normal-of-a-triangle
	* - https://math.stackexchange.com/questions/538458/triangle-point-picking-in-3d
	*
	* @return The normal quaternion of the triangle surface
	*/
	FORCEINLINE FQuat GetSurfaceNormal() const
	{
		const FVector v = VertexB - VertexA;
		const FVector w = VertexC - VertexA;
		const FRotator rotatorCorrection = FRotator(90.0f, 0.0f, 0.0f);

		return (FVector((v.Y * w.Z) - (v.Z * w.Y), (v.Z * w.X) - (v.X * w.Z), (v.X * w.Y) - (v.Y * w.X)).Rotation() + rotatorCorrection).Quaternion();
	}

	/**
	* Tells whether at least 2 vertices of this triangle are inside the given shape
	*
	* @param shapeComponent - The shape component reference.
	* @return True if at least 2 vertices of this triangle are inside the given shape, false otherwise.
	*/
	FORCEINLINE bool IsInsideShape(const UShapeComponent* shapeComponent) const;

	/**
	* Gets this triangle in world space using the given transform
	*
	* @param transform - The transform used to calculate the world space triangle.
	* @return The transformed triangle.
	*/
	FORCEINLINE FPtgTriangle GetWorldSpaceTriangle(const FTransform& transform) const;
};

/**
* Main class for procedural terrain generation.
*/
UCLASS(Abstract)
class PROCEDURALTERRAINGENERATOR_API APtgManager : public AActor
{
	GENERATED_BODY()
	
public:

	APtgManager();

	/** Called when this actor is explicitly being destroyed during gameplay or in the editor, not called during level streaming or gameplay ending */
	virtual void Destroyed() override;

	/** Regenerates the terrain, water, nature and actors using the seeds configured in the manager. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "PTG 1 - General actions")
	void GenerateEverything();

	/** 
	* Use custom seeds to regenerate the terrain, water, nature and actors.
	* 
	* @param terrainSeed -	Custom seed for the terrain noise generation algorythm.
	* @param waterSeed -	Custom seed for the water heigh generation algorythm.
	* @param natureSeed -	Custom seed for the nature meshes generation algorythm.
	* @param actorsSeed -	Custom seed for the actor generation algorythm.
	*/
	UFUNCTION(BlueprintCallable, Category = "PTG 1 - General actions")
	void GenerateEverythingWithCustomSeed(const int32 terrainSeed, const int32 waterSeed, const int32 natureSeed, const int32 actorsSeed);

	/** Calculates random seeds and regenerates the terrain, water, nature and actors. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "PTG 1 - General actions")
	void GenerateEverythingWithRandomSeed();

	/** Removes all nature meshes and actors on the terrain. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "PTG 1 - General actions")
	void ClearNatureAndActors()
	{
		ClearNature();
		ClearActors();
	}

#pragma region TERRAIN GENERATION

	/** Regenerates the terrain mesh using the seed configured in the manager. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "PTG 2 - Terrain generation actions")
	void GenerateTerrainMesh();

	/**
	* Use custom seed for the terrain and regenerates the mesh.
	*
	* @param terrainSeed -	Custom seed for the terrain noise generation algorythm.
	*/
	UFUNCTION(BlueprintCallable, Category = "PTG 2 - Terrain generation actions")
	void GenerateTerrainMeshWithCustomSeed(const int32 terrainSeed);

	/** Calculates a random seed for the terrain mesh and regenerates the mesh. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "PTG 2 - Terrain generation actions")
	void GenerateTerrainMeshWithRandomSeed();

	/** Removes the terrain mesh. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "PTG 2 - Terrain generation actions")
	void ClearTerrainMesh();

#pragma endregion

#pragma region WATER GENERATION

	/** Regenerates the water mesh using the seed configured in the manager. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "PTG 3 - Water generation actions")
	void GenerateWaterMesh();

	/** 
	* Use custom seed for the water and regenerates the mesh ONLY if Generate Water is true AND Water Height Generation Type is Random Percentage. 
	* 
	* @param waterSeed -	Custom seed for the water heigh generation algorythm.
	*/
	UFUNCTION(BlueprintCallable, Category = "PTG 3 - Water generation actions")
	void GenerateWaterMeshWithCustomSeed(const int32 waterSeed);

	/** Calculates a random seed for the water and regenerates the mesh ONLY if Generate Water is true AND Water Height Generation Type is Random Percentage. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "PTG 3 - Water generation actions")
	void GenerateWaterMeshWithRandomSeed();

	/** Removes the water mesh. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "PTG 3 - Water generation actions")
	void ClearWaterMesh();

#pragma endregion

#pragma region NATURE GENERATION

	/** Regenerates the nature and the actor spawning using the seeds configured in the manager. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "PTG 4 - Nature generation actions")
	void GenerateNature();

	/** 
	* Regenerates the nature spawning using the seeds configured in the manager. 
	* 
	* @param natureSeed -	Custom seed for the nature meshes generation algorythm.
	*/
	UFUNCTION(BlueprintCallable, Category = "PTG 4 - Nature generation actions")
	void GenerateNatureWithCustomSeed(const int32 natureSeed);

	/** Calculates a random seed for the bioma nature and regenerates the nature ONLY if Generate Nature is true. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "PTG 4 - Nature generation actions")
	void GenerateNatureWithRandomSeed();

	/** Removes all nature meshes on the terrain. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "PTG 4 - Nature generation actions")
	void ClearNature();

#pragma endregion

#pragma region ACTORS GENERATION

	/** Regenerates the actor spawning using the seeds configured in the manager. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "PTG 5 - Actors generation actions")
	void GenerateActors();

	/**
	* Regenerates the actor spawning using the seeds configured in the manager.
	*
	* @param actorsSeed -	Custom seed for the actor generation algorythm.
	*/
	UFUNCTION(BlueprintCallable, Category = "PTG 5 - Actors generation actions")
	void GenerateActorsWithCustomSeed(const int32 actorsSeed);

	/** Calculates a random seed for the bioma actors and regenerates actors ONLY if Generate Actors is true. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "PTG 5 - Actors generation actions")
	void GenerateActorsWithRandomSeed();

	/** Removes all actors on the terrain. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "PTG 5 - Actors generation actions")
	void ClearActors();

#pragma endregion

#pragma region GETTERS

	UFUNCTION(BlueprintPure, Category = "PTG Manager")
	EPtgProcMeshShapes GetShape() const { return Shape; }

	UFUNCTION(BlueprintPure, Category = "PTG Manager")
	URuntimeMeshComponent* GetProcMeshTerrainComp() const { return ProcMeshTerrainComp; }

	UFUNCTION(BlueprintPure, Category = "PTG Manager")
	URuntimeMeshComponent* GetProcMeshWaterComp() const { return ProcMeshWaterComp; }

	UFUNCTION(BlueprintPure, Category = "PTG Manager")
	const TArray<float>& GetVertexHeightData() const;

	UFUNCTION(BlueprintPure, Category = "PTG Manager")
	float GetLowestGeneratedHeight() const { return LowestGeneratedHeight; }

	UFUNCTION(BlueprintPure, Category = "PTG Manager")
	float GetHighestGeneratedHeight() const { return HighestGeneratedHeight; }

#pragma endregion

#pragma region DELEGATES

    UPROPERTY(BlueprintAssignable, Transient, Category = "Property|Delegates")
    FOnPTGGenerationStartedDelegate OnGenerationStartedDelegate;

    UPROPERTY(BlueprintAssignable, Transient, Category = "Property|Delegates")
    FOnPTGGenerationCompletedDelegate OnGenerationCompletedDelegate;

    UPROPERTY(BlueprintAssignable, Transient, Category = "Property|Delegates")
    FOnPTGGenerationFailedDelegate OnGenerationFailedDelegate;

    UPROPERTY(BlueprintAssignable, Transient, Category = "Property|Delegates")
    FOnPTGGenerationProgressDelegate OnGenerationProgressDelegate;

#pragma endregion

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:

	/**
	* Extension radius of the terrain, in centimeters.
	* Default value: 15000.0 (150 m.)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties", meta = (ClampMin = 100.0f, UIMin = 100.0f))
	float Radius = 15000.0f;	// 150 m.

	/**
	* Number of tiles that will be generated on x and y.
	* Default value: 150
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties", meta = (ClampMin = 1, UIMin = 1))
	int32 Resolution = 150;

	/**
	* Number of LODs that will be generated for the terrain and water meshes. A Value of 1 means no LODs (only LOD 0 will be generated).
	* Default value: 8
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties", meta = (ClampMin = 1, UIMin = 1, ClampMax = 8, UIMax = 8))
	int32 NumberOfLODs = 8;

	/**
	* The screen size multiplier applied to each LOD (0.0 - 0.99). The screen size is calculated using this: Pow(LODScreenSizeMultiplier, LODIndex)
	* So, for example, for a value of 0.5 and 4 LODs, the screen sizes for each LOD would be:
	* - LOD 0: 1.0
	* - LOD 1: Pow(0.5, 1) = 0.5
	* - LOD 2: Pow(0.5, 2) = 0.25
	* - LOD 3: Pow(0.5, 3) = 0.125
	* Default value: 0.5
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties", DisplayName = "LOD Screen Size Multiplier",
		meta = (ClampMin = 0.0f, UIMin = 0.0f, ClampMax = 0.99f, UIMax = 0.99f, EditCondition = "NumberOfLODs > 1", EditConditionHides))
	float LODScreenSizeMultiplier = 0.5f;

	/** If true, regenerates the corresponding stuff automatically when some property changes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties")
	bool bGenerateEverythingOnPropertyChange = false;

#pragma region TERRAIN SETTINGS

	/** Describes the shape of the terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Terrain generation")
	EPtgProcMeshShapes Shape = EPtgProcMeshShapes::Plane;

	/** If true, it will add an offset to the noise based on actor location, so the terrain will tile with other generated terrains. Only available on plane terrain shape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Terrain generation", meta = (EditCondition = "Shape == EPtgProcMeshShapes::Plane", EditConditionHides))
	bool bUseTerrainTiling = false;

	/**
	* Making this smaller will "stretch" the noise on the terrain.
	* Default value: 1.0
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Terrain generation")
	float NoiseInputScale = 1.0f;

	/**
	* Making this bigger will scale the terrain's height.
	* Default value: 1000.0
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Terrain generation")
	float NoiseOutputScale = 1000.0f;

	/** Material used for the terrain mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Terrain generation")
	TObjectPtr<UMaterialInterface> TerrainMaterial = nullptr;

	/** Whether the generated terrain mesh should have collision or not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Terrain generation")
	bool bEnableTerrainCollision = true;

	/** Lowest height value of the generated terrain. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PTG 0 - Properties|Terrain generation")
	float LowestGeneratedHeight = 0.0f;

	/** Highest height value of the generated terrain. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PTG 0 - Properties|Terrain generation")
	float HighestGeneratedHeight = 0.0f;

#pragma endregion

#pragma region WATER SETTINGS

	/** Whether to generate water mesh or not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Water generation")
	bool bGenerateWater = true;

	/** Type of water height generation. Fixed Height is only available on plane terrain shape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Water generation", meta = (EditCondition = "bGenerateWater", EditConditionHides))
	EPtgWaterHeightGenerationType WaterHeightGenerationType = EPtgWaterHeightGenerationType::RandomPercentage;

	/**
	* Seed used to generate the water mesh height when Water Height Generation Type is Random Percentage using Water Random Height Range Percentages.
	* Default value: 1619
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Water generation",
		meta = (EditCondition = "bGenerateWater && WaterHeightGenerationType == EPtgWaterHeightGenerationType::RandomPercentage", EditConditionHides))
	int32 WaterSeed = DEFAULT_SEED;

	/**
	* Range of height percentages where the water will be randomly placed between Lowest Generated Height and Highest Generated Height ONLY when Water Height Generation Type is Random Percentage.
	* For example: Lowest Generated Height = -100, Highest Generated Height = 100 and Water Random Height Range Percentages = (25,75), the water will be placed at a random height between -50 and 50.
	* Default value: (10%,50%)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Water generation",
		meta = (EditCondition = "bGenerateWater && WaterHeightGenerationType == EPtgWaterHeightGenerationType::RandomPercentage", EditConditionHides))
	FVector2D WaterRandomHeightRangePercentages = FVector2D(10.0f, 50.0f);

	/**
	* Percentage of height where the water will be placed between Lowest Generated Height and Highest Generated Height ONLY when Water Height Generation Type is Fixed Percentage.
	* For example: Lowest Generated Height = -100, Highest Generated Height = 100 and Water Fixed Height Percentage = 75, the water will be placed at a height of 50.
	* Default value: 50%
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Water generation",
		meta = (ClampMin = 0.0f, UIMin = 0.0f, ClampMax = 100.0f, UIMax = 100.0f, EditCondition = "bGenerateWater && WaterHeightGenerationType == EPtgWaterHeightGenerationType::FixedPercentage", EditConditionHides))
	float WaterFixedHeightPercentage = 50.0f;

	/**
	* Relative fixed height (Z) value for the water generation ONLY when Water Height Generation Type is Fixed Height.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Water generation", 
		meta = (EditCondition = "bGenerateWater && WaterHeightGenerationType == EPtgWaterHeightGenerationType::FixedHeight", EditConditionHides))
	float WaterFixedHeightValue = 0.0f;

	/** Material used for the water mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Water generation", meta = (EditCondition = "bGenerateWater", EditConditionHides))
	TObjectPtr<UMaterialInterface> WaterMaterial = nullptr;

	/** Whether the generated water mesh should have collision or not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Water generation", meta = (EditCondition = "bGenerateWater", EditConditionHides))
	bool bEnableWaterCollision = false;

	/** Relative height of the water mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PTG 0 - Properties|Water generation", meta = (EditCondition = "bGenerateWater", EditConditionHides))
	float WaterHeight = 0.0f;

#pragma endregion

#pragma region NATURE SETTINGS

	/** Whether to generate nature meshes or not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Nature generation")
	bool bGenerateNature = true;

	/** Array of different meshes per bioma. For example, you can set 2 registers for Underwater, one with bush meshes, and other for rocks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Nature generation", meta = (EditCondition = "bGenerateNature", EditConditionHides))
	TArray<FPtgBiomaNature> BiomaNature;

#pragma endregion

#pragma region ACTORS SETTINGS

	/** Whether to spawn actors or not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Actors generation")
	bool bGenerateActors = true;

	/** Array of different actors per bioma. For example, you can set 2 registers for Underwater, one with a character, and other with an actor blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Actors generation", meta = (EditCondition = "bGenerateActors", EditConditionHides))
	TArray<FPtgBiomaActors> BiomaActors;

#pragma endregion

#pragma region FAST NOISE LITE SETTINGS

	/**
	* Noise algorithm used to generate the terrain height.
	* Default value: Open Simplex 2
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Fast Noise Lite|General settings")
	EPtgFastNoiseLiteWrapperNoiseType NoiseType = EPtgFastNoiseLiteWrapperNoiseType::NoiseType_OpenSimplex2;

	/**
	* Domain rotation type for 3D Noise (Cube or Sphere terrain shapes) and 3D DomainWarp.
	* Default value: None
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Fast Noise Lite|General settings",
		meta = (EditCondition = "Shape != EPtgProcMeshShapes::Plane"))
	EPtgFastNoiseLiteWrapperRotationType3D RotationType3D = EPtgFastNoiseLiteWrapperRotationType3D::RotationType3D_None;

	/**
	* Seed used for all noise types. Using different seeds will cause the noise output to change.
	* Default value: 1619
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Fast Noise Lite|General settings")
	int32 Seed = DEFAULT_SEED;

	/**
	* Frequency for all noise types. Affects how coarse the noise output is.
	* Default value: 0.02
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Fast Noise Lite|General settings")
	float Frequency = 0.02f;

	/**
	* Method for combining octaves in all fractal noise types.
	* Default value: FBM
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Fast Noise Lite|Fractal settings")
	EPtgFastNoiseLiteWrapperFractalType FractalType = EPtgFastNoiseLiteWrapperFractalType::FractalType_FBm;

	/**
	* Octave count for all fractal noise types. The amount of noise layers used to create the fractal.
	* Default value: 5
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Fast Noise Lite|Fractal settings",
		meta = (EditCondition = "FractalType != EPtgFastNoiseLiteWrapperFractalType::FractalType_None"))
	int32 FractalOctaves = 5;

	/**
	* Octave lacunarity for all fractal noise types. The frequency multiplier between each octave.
	* Default value: 2.0
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Fast Noise Lite|Fractal settings",
		meta = (EditCondition = "FractalType != EPtgFastNoiseLiteWrapperFractalType::FractalType_None"))
	float FractalLacunarity = 2.0f;

	/**
	* Octave gain for all fractal noise types. The relative strength of noise from each layer when compared to the last.
	* Default value: 0.5
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Fast Noise Lite|Fractal settings",
		meta = (EditCondition = "FractalType != EPtgFastNoiseLiteWrapperFractalType::FractalType_None"))
	float FractalGain = 0.5f;

	/**
	* Octave weighting for all non DomainWarp fractal types. Default value: 0.0. Note: Keep between 0...1 to maintain -1...1 output bounding.
	* Default value: 0.0
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Fast Noise Lite|Fractal settings",
		meta = (EditCondition = "FractalType == EPtgFastNoiseLiteWrapperFractalType::FractalType_FBm || FractalType == EPtgFastNoiseLiteWrapperFractalType::FractalType_Ridged || FractalType == EPtgFastNoiseLiteWrapperFractalType::FractalType_PingPong"))
	float FractalWeightedStrength = 0.0f;

	/**
	* Strength of the fractal ping pong effect.
	* Default value: 2.0
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Fast Noise Lite|Fractal settings",
		meta = (EditCondition = "FractalType == EPtgFastNoiseLiteWrapperFractalType::FractalType_PingPong"))
	float FractalPingPongStrength = 2.0f;

	/**
	* Distance function used in cellular noise calculations. The distance function used to calculate the cell for a given point.
	* Default value: Euclidean Sq
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Fast Noise Lite|Cellular settings",
		meta = (EditCondition = "NoiseType == EPtgFastNoiseLiteWrapperNoiseType::NoiseType_Cellular"))
	EPtgFastNoiseLiteWrapperCellularDistanceFunction CellularDistanceFunction = EPtgFastNoiseLiteWrapperCellularDistanceFunction::CellularDistanceFunction_EuclideanSq;

	/**
	* Return type from cellular noise calculations.
	* Default value: Distance
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Fast Noise Lite|Cellular settings",
		meta = (EditCondition = "NoiseType == EPtgFastNoiseLiteWrapperNoiseType::NoiseType_Cellular"))
	EPtgFastNoiseLiteWrapperCellularReturnType CellularReturnType = EPtgFastNoiseLiteWrapperCellularReturnType::CellularReturnType_Distance;

	/**
	* Maximum distance a cellular point can move from its grid position. Setting this higher than 1 will cause artifacts.
	* Default value: 1.0
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Fast Noise Lite|Cellular settings",
		meta = (EditCondition = "NoiseType == EPtgFastNoiseLiteWrapperNoiseType::NoiseType_Cellular"))
	float CellularJitter = 1.0f;

	/**
	* Warp algorithm.
	* Default value: None
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Fast Noise Lite|Domain Warp settings")
	EPtgFastNoiseLiteWrapperDomainWarpType DomainWarpType = EPtgFastNoiseLiteWrapperDomainWarpType::DomainWarpType_None;

	/**
	* Maximum warp distance from original position when using DomainWarp.
	* Default value: 30.0
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Fast Noise Lite|Domain Warp settings",
		meta = (EditCondition = "DomainWarpType != EPtgFastNoiseLiteWrapperDomainWarpType::DomainWarpType_None"))
	float DomainWarpAmplitude = 30.0f;

#pragma endregion

#pragma region DEBUG SETTINGS

	/** If enabled, it will show debug info messages on screen and on the output log. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Debug")
	bool bShowDebugMessages = true;

	/** Specifies the time debug messages will be on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PTG 0 - Properties|Debug", meta = (ClampMin = 0.1f, UIMin = 0.1f, EditCondition = "bShowDebugMessages"))
	float DebugMessagesTimeOnScreen = 8.0f;

#pragma endregion

#pragma region COMPONENTS SETTINGS

	/** Root component where the procedural terrain will be attached. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PTG 0 - Properties|Components")
	TObjectPtr<USceneComponent> RootComp = nullptr;

	/** Procedural mesh where the noise functions will apply to generate the terrain. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PTG 0 - Properties|Components")
	TObjectPtr<URuntimeMeshComponent> ProcMeshTerrainComp = nullptr;

	/** Procedural mesh for the water. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PTG 0 - Properties|Components")
	TObjectPtr<URuntimeMeshComponent> ProcMeshWaterComp = nullptr;

#if WITH_EDITORONLY_DATA

	/** Billboard used to see the PTG Manager actor in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PTG 0 - Properties|Components")
	TObjectPtr<UBillboardComponent> SpriteComponent = nullptr;

#endif

#pragma endregion

private:

	//UPROPERTY()    //    making this UPROPERTY will slow the in-editor setting change
	FPtgProcMeshData procMeshData;
	//UPROPERTY()    //    making this UPROPERTY will slow the in-editor setting change
	TArray<float> vertexHeightData;

	//UPROPERTY()    //    making this UPROPERTY will slow the in-editor setting change
	TArray<FPtgTriangle> terrainTriangles;

	// Cached converted arrays to avoid repeated allocations when creating runtime mesh sections (reuse across LODs)
	TArray<FVector3f> ConvertedVertices3f;
	TArray<FVector3f> ConvertedNormals3f;
	TArray<FVector2f> ConvertedUV0_2f;

	// Precomputed per-triangle compact data to avoid recalculating normals/rotations repeatedly
	TArray<FVector> TriangleCentroids;
	TArray<FQuat> TriangleSurfaceQuats;

	/** Map of HISM components and its corresponding static mesh used to generate nature dinamically while being efficient. */
	UPROPERTY()
	TMap<TObjectPtr<UStaticMesh>, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> NatureStaticMeshHISM_Correspondence;

	/** Set of generated bioma actors. */
	UPROPERTY()
	TSet<TObjectPtr<AActor>> biomaActorsSpawned;

	// Visible is neccesary for the pointer to not nullify after constructor.
	// https://forums.unrealengine.com/development-discussion/c-gameplay-programming/28537-uproperty-member-vars-reset-to-null-by-objectinitializer
	UPROPERTY(VisibleDefaultsOnly, Category = "PTG 0 - Properties|Fast Noise Lite")
	TObjectPtr<UPtgFastNoiseLiteWrapper> fastNoiseWrapper = nullptr;

	// Used to gather RMCs runtime meshes to restore them after editing something on this actor.
	UPROPERTY()
	TObjectPtr<UPtgRuntimeMesh> terrainRuntimeMesh = nullptr;
	UPROPERTY()
	TObjectPtr<UPtgRuntimeMesh> waterRuntimeMesh = nullptr;

	// Internal flag used to avoid nested editor slow task dialogs when calling generation functions from other generation functions
	bool bEditorSlowTaskActive = false;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// Function pointers declaration
	typedef FQuat(APtgManager::* GetBiomaRotationFuncPtr)(FRandomStream&, const FPtgTriangle&, const FVector&) const;
	typedef bool (FPtgTriangle::* IsInsideHeigthRangeFuncPtr)(const float lowerHeight, const float higherHeight, const FTransform& terrainTransform) const;

	GetBiomaRotationFuncPtr getBiomaRotationFuncs[NUM_BIOMA_ROTATION_FUNCS];
	IsInsideHeigthRangeFuncPtr isInsideHeigthRangeFunc[NUM_TRIANGLE_INSIDE_HEIGHT_RANGE_FUNCS];

	void InitFuncPtrs();
	FORCEINLINE FQuat GetRandomYawQuat(FRandomStream& randomNumberGenerator) const;
	FORCEINLINE FQuat GetBiomaRandomRotation(FRandomStream& randomNumberGenerator, const FPtgTriangle& triangle, const FVector& location) const;
	FORCEINLINE FQuat GetBiomaPlaneShapeRotation(FRandomStream& randomNumberGenerator, const FPtgTriangle& triangle, const FVector& location) const;
	FORCEINLINE FQuat GetBiomaCubeShapeRotation(FRandomStream& randomNumberGenerator, const FPtgTriangle& triangle, const FVector& location) const;
	FORCEINLINE FQuat GetBiomaSphereShapeRotation(FRandomStream& randomNumberGenerator, const FPtgTriangle& triangle, const FVector& location) const;
	FORCEINLINE FQuat GetBiomaMeshSurfaceRotation(FRandomStream& randomNumberGenerator, const FPtgTriangle& triangle, const FVector& location) const;

	void SetupFastNoiseLite();
	void CalculateAndApplyWaterMeshHeight();
	void GenerateNatureInternal();
	void GenerateActorsInternal();
	void GetTriangleCandidates
	(
		const TArray<APtgModifier*>& modifiers,
		const bool bUseLocationsOutsideModifiers,
		const FVector2D& heightPercentageRangeToLocateElements,
		const bool bUseLocationsOutsideHeightRange,
		const int32 maxCandidatesToGet,
		const IsInsideHeigthRangeFuncPtr isInsideHeigthRangeFunction,
		const EPtgNatureBiomas bioma,
		FRandomStream& randomNumberGenerator,
		TArray<int32>& triangleCandidateIndexes,
		const TArray<FPtgTriangle>& terrainTriangles
	);
};
