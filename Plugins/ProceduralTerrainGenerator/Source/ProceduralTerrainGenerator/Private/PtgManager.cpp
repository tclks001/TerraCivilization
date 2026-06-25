// Copyright 2021 VICTOR HERNANDEZ MOLPECERES (Rockam). All Rights Reserved.

#include "PtgManager.h"
#include "ProceduralTerrainGenerator.h"
#include "PtgModifier.h"
#include "PtgUtils.h"
#include "RuntimeMeshComponent.h"
#include "RuntimeMesh.h"
#include "Providers/RuntimeMeshProviderStatic.h"
#include "Components/PrimitiveComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/Material.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_EDITORONLY_DATA
#include "Components/BillboardComponent.h"
#endif

#if WITH_EDITOR
#include "Misc/ScopedSlowTask.h"
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma region FUNCTION POINTER INDEXES

#define BIOMA_RANDOM_ROTATION_FUNC_INDEX 0
#define BIOMA_PLANE_SHAPE_ROTATION_FUNC_INDEX 1
#define BIOMA_CUBE_SHAPE_ROTATION_FUNC_INDEX 2
#define BIOMA_SPHERE_SHAPE_ROTATION_FUNC_INDEX 3
#define BIOMA_MESH_SURFACE_ROTATION_FUNC_INDEX 4

#define TRIANGLE_IS_INSIDE_PLANE_HEIGHT_RANGE_FUNC_INDEX 0
#define TRIANGLE_IS_INSIDE_CUBE_HEIGHT_RANGE_FUNC_INDEX 1
#define TRIANGLE_IS_INSIDE_SPHERE_HEIGHT_RANGE_FUNC_INDEX 2

#pragma endregion

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma region PTG TRIANGLE DEFINITIONS

bool FPtgTriangle::IsInsideCubeHeigthRange(const float lowerHeight, const float higherHeight, const FTransform& terrainTransform) const
{
	const FPtgTriangle& worldSpaceTriangle = GetWorldSpaceTriangle(terrainTransform);

	// Check if triangle is inside the higher height box
	const FVector higherRangeBoxExtent = FVector(higherHeight, higherHeight, higherHeight);
	const bool bIsVertexA_InsideHighRangeBox = UKismetMathLibrary::IsPointInBoxWithTransform(worldSpaceTriangle.VertexA, terrainTransform, higherRangeBoxExtent);
	const bool bIsVertexB_InsideHighRangeBox = UKismetMathLibrary::IsPointInBoxWithTransform(worldSpaceTriangle.VertexB, terrainTransform, higherRangeBoxExtent);
	const bool bIsVertexC_InsideHighRangeBox = UKismetMathLibrary::IsPointInBoxWithTransform(worldSpaceTriangle.VertexC, terrainTransform, higherRangeBoxExtent);

	// Check if triangle is outside the lower height box
	const FVector lowerRangeBoxExtent = FVector(lowerHeight, lowerHeight, lowerHeight);
	const bool bIsVertexA_OutsideLowRangeBox = !UKismetMathLibrary::IsPointInBoxWithTransform(worldSpaceTriangle.VertexA, terrainTransform, lowerRangeBoxExtent);
	const bool bIsVertexB_OutsideLowRangeBox = !UKismetMathLibrary::IsPointInBoxWithTransform(worldSpaceTriangle.VertexB, terrainTransform, lowerRangeBoxExtent);
	const bool bIsVertexC_OutsideLowRangeBox = !UKismetMathLibrary::IsPointInBoxWithTransform(worldSpaceTriangle.VertexC, terrainTransform, lowerRangeBoxExtent);

	const bool bIsVertexA_InsideHeigthRange = bIsVertexA_InsideHighRangeBox && bIsVertexA_OutsideLowRangeBox;
	const bool bIsVertexB_InsideHeigthRange = bIsVertexB_InsideHighRangeBox && bIsVertexB_OutsideLowRangeBox;
	const bool bIsVertexC_InsideHeigthRange = bIsVertexC_InsideHighRangeBox && bIsVertexC_OutsideLowRangeBox;

	return (bIsVertexA_InsideHeigthRange && bIsVertexB_InsideHeigthRange)
		|| (bIsVertexA_InsideHeigthRange && bIsVertexC_InsideHeigthRange)
		|| (bIsVertexB_InsideHeigthRange && bIsVertexC_InsideHeigthRange);
}

bool FPtgTriangle::IsInsideShape(const UShapeComponent* shapeComponent) const
{
	if (const USphereComponent* sphereComponent = Cast<USphereComponent>(shapeComponent))
	{
		const FVector& shapeWorldPosition = shapeComponent->GetComponentLocation();
		const float sphereRadius = sphereComponent->GetScaledSphereRadius();
		const bool bIsVertexA_InsideShape = FVector::Distance(shapeWorldPosition, VertexA) < sphereRadius;
		const bool bIsVertexB_InsideShape = FVector::Distance(shapeWorldPosition, VertexB) < sphereRadius;
		const bool bIsVertexC_InsideShape = FVector::Distance(shapeWorldPosition, VertexC) < sphereRadius;

		// If two triangle points are inside the sphere then treat the whole triangle as inside the sphere
		return (bIsVertexA_InsideShape && bIsVertexB_InsideShape)
			|| (bIsVertexA_InsideShape && bIsVertexC_InsideShape)
			|| (bIsVertexB_InsideShape && bIsVertexC_InsideShape);
	}
	else if (const UBoxComponent* boxComponent = Cast<UBoxComponent>(shapeComponent))
	{
		const FTransform& shapeTransform = shapeComponent->GetComponentTransform();
		const FVector& boxExtent = boxComponent->GetUnscaledBoxExtent();
		const bool bIsVertexA_InsideShape = UKismetMathLibrary::IsPointInBoxWithTransform(FVector(VertexA), shapeTransform, boxExtent);
		const bool bIsVertexB_InsideShape = UKismetMathLibrary::IsPointInBoxWithTransform(FVector(VertexB), shapeTransform, boxExtent);
		const bool bIsVertexC_InsideShape = UKismetMathLibrary::IsPointInBoxWithTransform(FVector(VertexC), shapeTransform, boxExtent);

		// If two triangle points are inside the box then treat the whole triangle as inside the box
		return (bIsVertexA_InsideShape && bIsVertexB_InsideShape)
			|| (bIsVertexA_InsideShape && bIsVertexC_InsideShape)
			|| (bIsVertexB_InsideShape && bIsVertexC_InsideShape);
	}

	return false;
}

FPtgTriangle FPtgTriangle::GetWorldSpaceTriangle(const FTransform& transform) const
{
	return FPtgTriangle
	(
		UKismetMathLibrary::TransformLocation(transform, FVector(VertexA)),
		UKismetMathLibrary::TransformLocation(transform, FVector(VertexB)),
		UKismetMathLibrary::TransformLocation(transform, FVector(VertexC))
	);
}

#pragma endregion

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const int32 SEED_MIN = -1000000;
const int32 SEED_MAX = 1000000;
const float MIN_ANGLE = 0.0f;
const float MAX_ANGLE = 359.99f;
const float PI2 = PI * 2.0f;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma region SETUP

APtgManager::APtgManager()
{
	// Disable tick on start, cause is not needed
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Create Fast Noise wrapper
	fastNoiseWrapper = CreateDefaultSubobject<UPtgFastNoiseLiteWrapper>("FastNoiseLiteWrapper");

	// Create root component as a scene component
	RootComp = CreateDefaultSubobject<USceneComponent>("RootComponent");
	RootComp->SetMobility(EComponentMobility::Movable);
	SetRootComponent(RootComp);

	// Create procedural mesh component for terrain and attach it to the root component
	ProcMeshTerrainComp = CreateDefaultSubobject<URuntimeMeshComponent>("ProcMeshTerrainComp");
	ProcMeshTerrainComp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	ProcMeshTerrainComp->SetEnableGravity(false);
	ProcMeshTerrainComp->bApplyImpulseOnDamage = false;
	ProcMeshTerrainComp->SetGenerateOverlapEvents(false);
	ProcMeshTerrainComp->SetCastShadow(false);
	ProcMeshTerrainComp->SetMobility(EComponentMobility::Movable);
	ProcMeshTerrainComp->SetupAttachment(RootComp);

	// Create procedural mesh component for water and attach it to the root component
	ProcMeshWaterComp = CreateDefaultSubobject<URuntimeMeshComponent>("ProcMeshWaterComp");
	ProcMeshWaterComp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	ProcMeshWaterComp->SetEnableGravity(false);
	ProcMeshWaterComp->bApplyImpulseOnDamage = false;
	ProcMeshWaterComp->SetGenerateOverlapEvents(false);
	ProcMeshWaterComp->SetCastShadow(false);
	ProcMeshWaterComp->SetMobility(EComponentMobility::Movable);
	ProcMeshWaterComp->SetupAttachment(RootComp);

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>("PtgManagerBillboard");
	if (SpriteComponent)
	{
		SpriteComponent->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
		SpriteComponent->bHiddenInGame = true;
		SpriteComponent->bIsScreenSizeScaled = true;
		SpriteComponent->SetupAttachment(RootComp);
	}
#endif

	// Disable another actor unused stuff
	SetCanBeDamaged(false);
	bFindCameraComponentWhenViewTarget = 0;

	// Init function pointers
	InitFuncPtrs();
}

void APtgManager::SetupFastNoiseLite()
{
	// Create fast noise lite wrapper in the case that it lost the reference
	if (fastNoiseWrapper == nullptr)
	{
		fastNoiseWrapper = NewObject<UPtgFastNoiseLiteWrapper>(this, "FastNoiseLiteWrapper");

		if (fastNoiseWrapper == nullptr)
		{
			UPtgUtils::PrintDebugMessage(this, TEXT("Cannot create fastNoiseWrapper, please, respawn the PTG actor on the map."));
			return;
		}
	}

	fastNoiseWrapper->SetupFastNoiseLite(
		Seed, Frequency, NoiseType, RotationType3D,
		FractalType, FractalOctaves, FractalLacunarity, FractalGain, FractalWeightedStrength, FractalPingPongStrength,
		CellularDistanceFunction, CellularReturnType, CellularJitter,
		DomainWarpType, DomainWarpAmplitude);
}

void APtgManager::Destroyed()
{
	procMeshData.ClearData();
	vertexHeightData.Empty();
	ClearTerrainMesh();
	ClearWaterMesh();
	ClearNatureAndActors();

	Super::Destroyed();
}

#pragma endregion

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma region FUNCTION POINTERS SETUP

void APtgManager::InitFuncPtrs()
{
	getBiomaRotationFuncs[BIOMA_RANDOM_ROTATION_FUNC_INDEX] = &APtgManager::GetBiomaRandomRotation;
	getBiomaRotationFuncs[BIOMA_PLANE_SHAPE_ROTATION_FUNC_INDEX] = &APtgManager::GetBiomaPlaneShapeRotation;
	getBiomaRotationFuncs[BIOMA_CUBE_SHAPE_ROTATION_FUNC_INDEX] = &APtgManager::GetBiomaCubeShapeRotation;
	getBiomaRotationFuncs[BIOMA_SPHERE_SHAPE_ROTATION_FUNC_INDEX] = &APtgManager::GetBiomaSphereShapeRotation;
	getBiomaRotationFuncs[BIOMA_MESH_SURFACE_ROTATION_FUNC_INDEX] = &APtgManager::GetBiomaMeshSurfaceRotation;

	isInsideHeigthRangeFunc[TRIANGLE_IS_INSIDE_PLANE_HEIGHT_RANGE_FUNC_INDEX] = &FPtgTriangle::IsInsidePlaneHeigthRange;
	isInsideHeigthRangeFunc[TRIANGLE_IS_INSIDE_CUBE_HEIGHT_RANGE_FUNC_INDEX] = &FPtgTriangle::IsInsideCubeHeigthRange;
	isInsideHeigthRangeFunc[TRIANGLE_IS_INSIDE_SPHERE_HEIGHT_RANGE_FUNC_INDEX] = &FPtgTriangle::IsInsideSphereHeigthRange;
}

FQuat APtgManager::GetRandomYawQuat(FRandomStream& randomNumberGenerator) const
{
	return FQuat(FVector::UpVector, randomNumberGenerator.FRandRange(0.0f, PI2));
}

FQuat APtgManager::GetBiomaRandomRotation(FRandomStream& randomNumberGenerator, const FPtgTriangle& triangle, const FVector& location) const
{
	// Get a random rotation
	return FRotator(randomNumberGenerator.FRandRange(MIN_ANGLE, MAX_ANGLE), randomNumberGenerator.FRandRange(MIN_ANGLE, MAX_ANGLE), randomNumberGenerator.FRandRange(MIN_ANGLE, MAX_ANGLE)).Quaternion();
}

FQuat APtgManager::GetBiomaPlaneShapeRotation(FRandomStream& randomNumberGenerator, const FPtgTriangle& triangle, const FVector& location) const
{
	// Get a random yaw rotation
	return GetRandomYawQuat(randomNumberGenerator);
}

FQuat APtgManager::GetBiomaCubeShapeRotation(FRandomStream& randomNumberGenerator, const FPtgTriangle& triangle, const FVector& location) const
{
	// Get cube vertex face normal and apply a random yaw rotation
	return triangle.CubeVertexFaceNormal * GetRandomYawQuat(randomNumberGenerator);
}

FQuat APtgManager::GetBiomaSphereShapeRotation(FRandomStream& randomNumberGenerator, const FPtgTriangle& triangle, const FVector& location) const
{
	// Apply pitch correction and randomize yaw
	return (UKismetMathLibrary::FindLookAtRotation(FVector::ZeroVector, location) - FRotator(90.0f, 0.0f, 0.0f)).Quaternion() * GetRandomYawQuat(randomNumberGenerator);
}

FQuat APtgManager::GetBiomaMeshSurfaceRotation(FRandomStream& randomNumberGenerator, const FPtgTriangle& triangle, const FVector& location) const
{
	// Get surface normal and apply a random yaw rotation
	return triangle.GetSurfaceNormal() * GetRandomYawQuat(randomNumberGenerator);
}

#pragma endregion

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma region GENERATION OF EVERYTHING

void APtgManager::GenerateEverything()
{
	const double startTime = FPlatformTime::Seconds();

	// Broadcast start of everything generation
	OnGenerationStartedDelegate.Broadcast(TEXT("Everything"));
	OnGenerationProgressDelegate.Broadcast(TEXT("Everything"), 0.0f);
	UPtgUtils::PrintDebugMessage(this, TEXT("GenerateEverything started."), EPtgDebugMessageTypes::Info, DebugMessagesTimeOnScreen);

#if WITH_EDITOR
	// Editor: show a modal progress dialog for the full generation flow
	const float totalSteps = 1.0f + (bGenerateWater ? 1.0f : 0.0f) + (bGenerateNature ? 1.0f : 0.0f) + (bGenerateActors ? 1.0f : 0.0f);
	FScopedSlowTask SlowTask(totalSteps, FText::FromString(TEXT("Generating Procedural Terrain...")));
	SlowTask.MakeDialog();
	bEditorSlowTaskActive = true;

	SlowTask.EnterProgressFrame(1.0f, FText::FromString(TEXT("Generating Terrain")));
	GenerateTerrainMesh();
	OnGenerationProgressDelegate.Broadcast(TEXT("Everything"), 0.25f);

	if (bGenerateWater)
	{
		SlowTask.EnterProgressFrame(1.0f, FText::FromString(TEXT("Generating Water")));
		GenerateWaterMesh();
		OnGenerationProgressDelegate.Broadcast(TEXT("Everything"), 0.5f);
	}

	if (bGenerateNature)
	{
		SlowTask.EnterProgressFrame(1.0f, FText::FromString(TEXT("Generating Nature")));
		GenerateNature();
		OnGenerationProgressDelegate.Broadcast(TEXT("Everything"), 0.75f);
	}

	if (bGenerateActors)
	{
		SlowTask.EnterProgressFrame(1.0f, FText::FromString(TEXT("Spawning Actors")));
		GenerateActors();
		OnGenerationProgressDelegate.Broadcast(TEXT("Everything"), 1.0f);
	}

	bEditorSlowTaskActive = false;
#else
	// Generate procedural meshes for terrain and water, generate the nature meshes and spawn actors
	GenerateTerrainMesh();
	OnGenerationProgressDelegate.Broadcast(TEXT("Everything"), 0.25f);
	if (bGenerateWater) { GenerateWaterMesh(); OnGenerationProgressDelegate.Broadcast(TEXT("Everything"), 0.5f); }
	if (bGenerateNature) { GenerateNature(); OnGenerationProgressDelegate.Broadcast(TEXT("Everything"), 0.75f); }
	if (bGenerateActors) { GenerateActors(); OnGenerationProgressDelegate.Broadcast(TEXT("Everything"), 1.0f); }
#endif

	// Debug
	if (bShowDebugMessages)
	{
		UPtgUtils::PrintDebugMessage(this, TEXT("Generate everything took ") + FString::SanitizeFloat(FPlatformTime::Seconds() - startTime) + TEXT(" seconds in total."), EPtgDebugMessageTypes::Info, DebugMessagesTimeOnScreen);
	}

	// Broadcast everything completed
	OnGenerationCompletedDelegate.Broadcast(TEXT("Everything"), 0);
}

void APtgManager::GenerateEverythingWithCustomSeed(const int32 terrainSeed, const int32 waterSeed, const int32 natureSeed, const int32 actorsSeed)
{
	// Create fast noise lite wrapper in the case that it lost the reference
	if (fastNoiseWrapper == nullptr)
	{
		fastNoiseWrapper = NewObject<UPtgFastNoiseLiteWrapper>(this, "FastNoiseLiteWrapper");

		if (fastNoiseWrapper == nullptr)
		{
			UPtgUtils::PrintDebugMessage(this, TEXT("Cannot create fastNoiseWrapper, please, respawn the PTG actor on the map."), EPtgDebugMessageTypes::Info, DebugMessagesTimeOnScreen);
			return;
		}
	}

	// Set random seeds for everything
	Seed = terrainSeed;
	fastNoiseWrapper->SetSeed(terrainSeed);

	if (bGenerateWater && WaterHeightGenerationType == EPtgWaterHeightGenerationType::RandomPercentage)
	{
		WaterSeed = waterSeed;
	}

	if (bGenerateNature)
	{
		for (FPtgBiomaNature& biomaNature : BiomaNature)
		{
			biomaNature.Seed = natureSeed;
		}
	}

	if (bGenerateActors)
	{
		for (FPtgBiomaActors& biomaActor : BiomaActors)
		{
			biomaActor.Seed = actorsSeed;
		}
	}

	// Then, generate the terrain
	GenerateEverything();
}

void APtgManager::GenerateEverythingWithRandomSeed()
{
	GenerateEverythingWithCustomSeed(FMath::RandRange(SEED_MIN, SEED_MAX), FMath::RandRange(SEED_MIN, SEED_MAX), FMath::RandRange(SEED_MIN, SEED_MAX), FMath::RandRange(SEED_MIN, SEED_MAX));
}

#pragma endregion

#pragma region TERRAIN GENERATION

void APtgManager::GenerateTerrainMesh()
{
	// Create fast noise wrapper in the case that it lost the reference
	if (fastNoiseWrapper == nullptr)
	{
		fastNoiseWrapper = NewObject<UPtgFastNoiseLiteWrapper>(this, "FastNoiseLiteWrapper");

		if (fastNoiseWrapper == nullptr)
		{
			UPtgUtils::PrintDebugMessage(this, TEXT("Cannot create fastNoiseWrapper, please, respawn the PTG actor on the map."), EPtgDebugMessageTypes::Error, DebugMessagesTimeOnScreen);
			OnGenerationFailedDelegate.Broadcast(TEXT("Terrain"), TEXT("Cannot create fastNoiseWrapper"));
			return;
		}
	}

	// If Fast Noise is not initialized, initialize it first
	if (!fastNoiseWrapper->IsInitialized())
	{
		SetupFastNoiseLite();
	}
	
	// Log start and broadcast
	UPtgUtils::PrintDebugMessage(this, FString::Printf(TEXT("GenerateTerrainMesh started: Shape=%d Resolution=%d NumberOfLODs=%d Seed=%d"), (int32)Shape, Resolution, NumberOfLODs, Seed), EPtgDebugMessageTypes::Info, DebugMessagesTimeOnScreen);
	OnGenerationStartedDelegate.Broadcast(TEXT("Terrain"));
	OnGenerationProgressDelegate.Broadcast(TEXT("Terrain"), 0.0f);

	const double startTime = FPlatformTime::Seconds();

	// Get or create provider
	URuntimeMeshProviderStatic* staticProvider = nullptr;

	if (URuntimeMeshProvider* provider = ProcMeshTerrainComp->GetProvider())
	{
		staticProvider = Cast<URuntimeMeshProviderStatic>(provider);
	}
	else
	{
		staticProvider = NewObject<URuntimeMeshProviderStatic>(this, "RuntimeMeshProvider-Static_Terrain");

		// The static provider should initialize before we use it
		ProcMeshTerrainComp->Initialize(staticProvider);
	}

	#if WITH_EDITOR
	TUniquePtr<FScopedSlowTask> TerrainSlowTask;
	bool bCreatedTerrainSlow = false;
	if (!bEditorSlowTaskActive)
	{
		TerrainSlowTask = MakeUnique<FScopedSlowTask>((float)NumberOfLODs, FText::FromString(TEXT("Generating Terrain mesh...")));
		TerrainSlowTask->MakeDialog();
		bEditorSlowTaskActive = true;
		bCreatedTerrainSlow = true;
	}
	#endif

	if (staticProvider != nullptr)
	{
		ClearTerrainMesh();

		// Generate LODs configurations
		TArray<FRuntimeMeshLODProperties> LODsProperties;
		for (int32 LODIndex = 0; LODIndex < NumberOfLODs; LODIndex++)
		{
			FRuntimeMeshLODProperties currentLOD;
			currentLOD.ScreenSize = (LODIndex == 0) ? 1.0f : FMath::Pow(LODScreenSizeMultiplier, LODIndex);
			LODsProperties.Emplace(currentLOD);
		}
		staticProvider->ConfigureLODs(LODsProperties);
		staticProvider->MarkAllLODsDirty();

		procMeshData.bEnableCollision = bEnableTerrainCollision;

		// Generate meshes for every LOD
		for (int32 LODIndex = 1; LODIndex <= NumberOfLODs; LODIndex++)
		{
			#if WITH_EDITOR
			if (TerrainSlowTask)
			{
				TerrainSlowTask->EnterProgressFrame(1.0f, FText::FromString(FString::Printf(TEXT("Generating Terrain LOD %d/%d"), LODIndex, NumberOfLODs)));
			}
			#endif

			const bool bIsFirstIteration = LODIndex == 1;

			// Only set the Lowest and Highest generated height values on the first iteration (LOD 0), to avoid the rest of the LODs messing everything up
			float dummyHeight = 0.0f;
			float& lowestGeneratedHeightPtr = bIsFirstIteration ? LowestGeneratedHeight : dummyHeight;
			float& highestGeneratedHeightPtr = bIsFirstIteration ? HighestGeneratedHeight : dummyHeight;

			// Generate tile grid depending on the shape using noise
			switch (Shape)
			{
			case EPtgProcMeshShapes::Cube:

				UPtgProcMeshDataHelper::GenerateCubeData(procMeshData, lowestGeneratedHeightPtr, highestGeneratedHeightPtr, Radius, Resolution / LODIndex, fastNoiseWrapper, NoiseInputScale * LODIndex, NoiseOutputScale);
				break;

			case EPtgProcMeshShapes::Sphere:

				UPtgProcMeshDataHelper::GenerateSphereData(procMeshData, lowestGeneratedHeightPtr, highestGeneratedHeightPtr, Radius, Resolution / LODIndex, fastNoiseWrapper, NoiseInputScale * LODIndex, NoiseOutputScale);
				break;

			case EPtgProcMeshShapes::Plane:
			default:

				UPtgProcMeshDataHelper::GeneratePlaneData(procMeshData, lowestGeneratedHeightPtr, highestGeneratedHeightPtr, Radius, Resolution / LODIndex, fastNoiseWrapper, NoiseInputScale * LODIndex, NoiseOutputScale, bUseTerrainTiling ? GetActorLocation() : FVector::ZeroVector);

				// Save vertex height data in the first iteration (LOD 0) to retrieve it later on heightmap creation request
				if (bIsFirstIteration)
				{
					vertexHeightData.Empty();
					for (const FVector& vertexData : procMeshData.Vertices) vertexHeightData.Emplace(vertexData.Z);
				}
			}

			// Regenerate the procedural terrain mesh
			// Convert and reuse FVector -> FVector3f/FVector2f arrays to avoid temporary allocations per call
			ConvertedVertices3f.SetNumUninitialized(procMeshData.Vertices.Num());
			for (int32 vIdx = 0; vIdx < procMeshData.Vertices.Num(); ++vIdx) ConvertedVertices3f[vIdx] = FVector3f(procMeshData.Vertices[vIdx]);
			ConvertedNormals3f.SetNumUninitialized(procMeshData.Normals.Num());
			for (int32 nIdx = 0; nIdx < procMeshData.Normals.Num(); ++nIdx) ConvertedNormals3f[nIdx] = FVector3f(procMeshData.Normals[nIdx]);
			ConvertedUV0_2f.SetNumUninitialized(procMeshData.UV0.Num());
			for (int32 uIdx = 0; uIdx < procMeshData.UV0.Num(); ++uIdx) ConvertedUV0_2f[uIdx] = FVector2f(procMeshData.UV0[uIdx]);

			staticProvider->CreateSectionFromComponents(LODIndex - 1, procMeshData.SectionIndex, 0, ConvertedVertices3f, procMeshData.Triangles, ConvertedNormals3f, ConvertedUV0_2f, procMeshData.VertexColors, procMeshData.Tangents, ERuntimeMeshUpdateFrequency::Infrequent, procMeshData.bEnableCollision);

			// Broadcast progress for terrain LOD
			OnGenerationProgressDelegate.Broadcast(FString::Printf(TEXT("Terrain LOD %d/%d"), LODIndex, NumberOfLODs), (float)LODIndex / (float)NumberOfLODs);

			// Save the terrain triangles in the first iteration (LOD 0)
			if (bIsFirstIteration)
			{
				const uint32 numTris = procMeshData.GetNumTriangles();

				// Get procedural mesh triangles information to retrieve them later on nature & actors generation
				if (Shape == EPtgProcMeshShapes::Cube)
				{
					for (uint32 triangleIndex = 0; triangleIndex < numTris; triangleIndex += 3)
					{
						const int32* currentTriangle = &(procMeshData.Triangles[triangleIndex]);
						terrainTriangles.Emplace(FPtgTriangle(
							procMeshData.Vertices[*currentTriangle],
							procMeshData.Vertices[*(currentTriangle + 1)],
							procMeshData.Vertices[*(currentTriangle + 2)],
							procMeshData.CubeVertexFaceAssetRotation[*currentTriangle]));
					}
				}
				else
				{
					for (uint32 triangleIndex = 0; triangleIndex < numTris; triangleIndex += 3)
					{
						const int32* currentTriangle = &(procMeshData.Triangles[triangleIndex]);
						terrainTriangles.Emplace(FPtgTriangle(
							procMeshData.Vertices[*currentTriangle],
							procMeshData.Vertices[*(currentTriangle + 1)],
							procMeshData.Vertices[*(currentTriangle + 2)]));
					}
				}

				// Precompute compact triangle data (centroid and surface quaternion) to avoid recomputing normals/rotations repeatedly
				// This reduces CPU work in nature/actor placement where triangle orientation is queried many times.
				TriangleCentroids.SetNumUninitialized(terrainTriangles.Num());
				TriangleSurfaceQuats.SetNumUninitialized(terrainTriangles.Num());
				for (int32 t = 0; t < terrainTriangles.Num(); ++t)
				{
					const FPtgTriangle& tri = terrainTriangles[t];
					TriangleCentroids[t] = (tri.VertexA + tri.VertexB + tri.VertexC) / 3.0f;
					// Compute surface normal quaternion (matches GetSurfaceNormal implementation)
					const FVector v = tri.VertexB - tri.VertexA;
					const FVector w = tri.VertexC - tri.VertexA;
					const FVector cross((v.Y * w.Z) - (v.Z * w.Y), (v.Z * w.X) - (v.X * w.Z), (v.X * w.Y) - (v.Y * w.X));
					TriangleSurfaceQuats[t] = (cross.Rotation() + FRotator(90.0f, 0.0f, 0.0f)).Quaternion();
				}
			}
		}

		staticProvider->SetupMaterialSlot(0, "TerrainMaterial", TerrainMaterial);

		// Broadcast completed and provide triangle count (triangles stored as 3 indices so divide by 3)
		int32 numTriangles = terrainTriangles.Num();
		if (numTriangles == 0 && procMeshData.GetNumTriangles() > 0)
		{
			numTriangles = procMeshData.GetNumTriangles() / 3;
		}
		OnGenerationCompletedDelegate.Broadcast(TEXT("Terrain"), numTriangles);
		OnGenerationProgressDelegate.Broadcast(TEXT("Terrain"), 1.0f);
	}

	// Debug
	if (bShowDebugMessages)
	{
		UPtgUtils::PrintDebugMessage(this, TEXT("Terrain mesh generation took ") + FString::SanitizeFloat(FPlatformTime::Seconds() - startTime) + TEXT(" seconds."), EPtgDebugMessageTypes::Info, DebugMessagesTimeOnScreen);
	}

#if WITH_EDITOR
	if (bCreatedTerrainSlow)
	{
		bEditorSlowTaskActive = false;
	}
#endif
}

void APtgManager::GenerateTerrainMeshWithCustomSeed(const int32 terrainSeed)
{
	// Set random seeds for everything
	Seed = terrainSeed;
	fastNoiseWrapper->SetSeed(terrainSeed);

	GenerateTerrainMesh();
}

void APtgManager::GenerateTerrainMeshWithRandomSeed()
{
	GenerateTerrainMeshWithCustomSeed(FMath::RandRange(SEED_MIN, SEED_MAX));
}

void APtgManager::ClearTerrainMesh()
{
	LowestGeneratedHeight = HighestGeneratedHeight = 0.0f;

	// Clear current sections
	if (URuntimeMeshProviderStatic* staticProvider = Cast<URuntimeMeshProviderStatic>(ProcMeshTerrainComp->GetProvider()))
	{
		staticProvider->ClearAllLODsForSection(0);
	}

	terrainTriangles.Empty();
}

#pragma endregion

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma region WATER GENERATION

void APtgManager::GenerateWaterMesh()
{
	ClearWaterMesh();

	if (bGenerateWater)
	{
		// Broadcast start
		OnGenerationStartedDelegate.Broadcast(TEXT("Water"));
		OnGenerationProgressDelegate.Broadcast(TEXT("Water"), 0.0f);
		UPtgUtils::PrintDebugMessage(this, TEXT("GenerateWaterMesh started."), EPtgDebugMessageTypes::Info, DebugMessagesTimeOnScreen);

		const double startTime = FPlatformTime::Seconds();

		CalculateAndApplyWaterMeshHeight();

		// Get or create provider
		URuntimeMeshProviderStatic* staticProvider = nullptr;

		if (URuntimeMeshProvider* provider = ProcMeshWaterComp->GetProvider())
		{
			staticProvider = Cast<URuntimeMeshProviderStatic>(provider);
		}
		else
		{
			staticProvider = NewObject<URuntimeMeshProviderStatic>(this, "RuntimeMeshProvider-Static_Water");

			// The static provider should initialize before we use it
			ProcMeshWaterComp->Initialize(staticProvider);
		}

		if (staticProvider != nullptr)
		{
			// Generate LODs configurations
			TArray<FRuntimeMeshLODProperties> LODsProperties;
			for (int32 LODIndex = 0; LODIndex < NumberOfLODs; LODIndex++)
			{
				FRuntimeMeshLODProperties currentLOD;
				currentLOD.ScreenSize = (LODIndex == 0) ? 1.0f : FMath::Pow(LODScreenSizeMultiplier, LODIndex);
				LODsProperties.Emplace(currentLOD);
			}
			staticProvider->ConfigureLODs(LODsProperties);
			staticProvider->MarkAllLODsDirty();

			procMeshData.bEnableCollision = bEnableWaterCollision;

			// Editor slow task for water
			#if WITH_EDITOR
			TUniquePtr<FScopedSlowTask> WaterSlowTask;
			bool bCreatedWaterSlow = false;
			if (!bEditorSlowTaskActive)
			{
				WaterSlowTask = MakeUnique<FScopedSlowTask>((float)NumberOfLODs, FText::FromString(TEXT("Generating Water mesh...")));
				WaterSlowTask->MakeDialog();
				bEditorSlowTaskActive = true;
				bCreatedWaterSlow = true;
			}
			#endif

			// Generate meshes for every LOD
			for (int32 LODIndex = 1; LODIndex <= NumberOfLODs; LODIndex++)
			{
				#if WITH_EDITOR
				if (WaterSlowTask)
				{
					WaterSlowTask->EnterProgressFrame(1.0f, FText::FromString(FString::Printf(TEXT("Generating Water LOD %d/%d"), LODIndex, NumberOfLODs)));
				}
				#endif

				switch (Shape)
				{
				case EPtgProcMeshShapes::Cube:

					UPtgProcMeshDataHelper::GenerateCubeData(procMeshData, LowestGeneratedHeight, HighestGeneratedHeight, WaterHeight, Resolution / LODIndex);
					break;

				case EPtgProcMeshShapes::Sphere:

					UPtgProcMeshDataHelper::GenerateSphereData(procMeshData, LowestGeneratedHeight, HighestGeneratedHeight, WaterHeight, Resolution / LODIndex);
					break;

				case EPtgProcMeshShapes::Plane:
				default:

					UPtgProcMeshDataHelper::GeneratePlaneData(procMeshData, LowestGeneratedHeight, HighestGeneratedHeight, Radius, Resolution / LODIndex);
				}

				// Regenerate the procedural water mesh
				// Convert and reuse arrays for water as well to avoid allocations
				ConvertedVertices3f.SetNumUninitialized(procMeshData.Vertices.Num());
				for (int32 vIdx = 0; vIdx < procMeshData.Vertices.Num(); ++vIdx) ConvertedVertices3f[vIdx] = FVector3f(procMeshData.Vertices[vIdx]);
				ConvertedNormals3f.SetNumUninitialized(procMeshData.Normals.Num());
				for (int32 nIdx = 0; nIdx < procMeshData.Normals.Num(); ++nIdx) ConvertedNormals3f[nIdx] = FVector3f(procMeshData.Normals[nIdx]);
				ConvertedUV0_2f.SetNumUninitialized(procMeshData.UV0.Num());
				for (int32 uIdx = 0; uIdx < procMeshData.UV0.Num(); ++uIdx) ConvertedUV0_2f[uIdx] = FVector2f(procMeshData.UV0[uIdx]);

				staticProvider->CreateSectionFromComponents(LODIndex - 1, procMeshData.SectionIndex, 0, ConvertedVertices3f, procMeshData.Triangles, ConvertedNormals3f, ConvertedUV0_2f, procMeshData.VertexColors, procMeshData.Tangents, ERuntimeMeshUpdateFrequency::Infrequent, procMeshData.bEnableCollision);

				// Broadcast progress for water LOD
				OnGenerationProgressDelegate.Broadcast(FString::Printf(TEXT("Water LOD %d/%d"), LODIndex, NumberOfLODs), (float)LODIndex / (float)NumberOfLODs);
			}

			#if WITH_EDITOR
			if (bCreatedWaterSlow)
			{
				bEditorSlowTaskActive = false;
			}
			#endif

			staticProvider->SetupMaterialSlot(0, "WaterMaterial", WaterMaterial);

			// Broadcast completed (no triangles data saved for water, send 0)
			OnGenerationCompletedDelegate.Broadcast(TEXT("Water"), 0);
			OnGenerationProgressDelegate.Broadcast(TEXT("Water"), 1.0f);
		}

		// Debug
		if (bShowDebugMessages)
		{
			UPtgUtils::PrintDebugMessage(this, TEXT("Water mesh generation took ") + FString::SanitizeFloat(FPlatformTime::Seconds() - startTime) + TEXT(" seconds."), EPtgDebugMessageTypes::Info, DebugMessagesTimeOnScreen);
		}
	}
	else
	{
		UPtgUtils::PrintDebugMessage(this, TEXT("To use this action enable Generate Water!"), EPtgDebugMessageTypes::Warning, DebugMessagesTimeOnScreen);
		OnGenerationFailedDelegate.Broadcast(TEXT("Water"), TEXT("GenerateWater disabled"));
	}
}

void APtgManager::GenerateWaterMeshWithCustomSeed(const int32 waterSeed)
{
	if (bGenerateWater && WaterHeightGenerationType == EPtgWaterHeightGenerationType::RandomPercentage)
	{
		// Set seed for water height
		WaterSeed = waterSeed;

		// Then generate water mesh
		GenerateWaterMesh();
	}
	else
	{
		UPtgUtils::PrintDebugMessage(this, TEXT("To use this action enable Generate Water and set Water Height Generation Type to Random Percentage!"), EPtgDebugMessageTypes::Warning, DebugMessagesTimeOnScreen);
	}
}

void APtgManager::GenerateWaterMeshWithRandomSeed()
{
	GenerateWaterMeshWithCustomSeed(FMath::RandRange(SEED_MIN, SEED_MAX));
}

void APtgManager::ClearWaterMesh()
{
	// If there will be no water, equal its height to the lowest of the terrain, for nature generation purposes
	WaterHeight = LowestGeneratedHeight;

	// Clear current sections
	if (URuntimeMeshProviderStatic* staticProvider = Cast<URuntimeMeshProviderStatic>(ProcMeshWaterComp->GetProvider()))
	{
		staticProvider->ClearAllLODsForSection(0);
	}
}

void APtgManager::CalculateAndApplyWaterMeshHeight()
{
	const float terrainHeightDifference = HighestGeneratedHeight - LowestGeneratedHeight;

	switch (WaterHeightGenerationType)
	{
	case EPtgWaterHeightGenerationType::RandomPercentage:
	{
		FRandomStream randomNumberGenerator;
		randomNumberGenerator.Initialize(WaterSeed);

		WaterHeight = LowestGeneratedHeight + (randomNumberGenerator.RandRange(
			terrainHeightDifference * (WaterRandomHeightRangePercentages.X / 100.0f),
			terrainHeightDifference * (WaterRandomHeightRangePercentages.Y / 100.0f))
			);
		break;
	}

	case EPtgWaterHeightGenerationType::FixedPercentage:

		WaterHeight = LowestGeneratedHeight + (terrainHeightDifference * (WaterFixedHeightPercentage / 100.0f));
		break;

	case EPtgWaterHeightGenerationType::FixedHeight:
	default:
		WaterHeight = WaterFixedHeightValue;
	}

	switch (Shape)
	{
	case EPtgProcMeshShapes::Cube:
	case EPtgProcMeshShapes::Sphere:

		ProcMeshWaterComp->SetRelativeLocation(FVector::ZeroVector);
		
		break;

	case EPtgProcMeshShapes::Plane:
	default:
	{
		const FVector currentWaterRelativeLocation = ProcMeshWaterComp->GetRelativeTransform().GetLocation();
		const FVector newWaterRelativeLocation = FVector(0.0f, 0.0f, WaterHeight);

		// As changing transform could be heavy for huge meshes (as the ones we could have here) set relative location only if needed
		if (currentWaterRelativeLocation != newWaterRelativeLocation)
		{
			ProcMeshWaterComp->SetRelativeLocation(newWaterRelativeLocation);
		}
	}
	}
}

#pragma endregion

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma region NATURE GENERATION

void APtgManager::GenerateNature()
{
	if (bGenerateNature)
	{
		// Clear the current nature meshes and fill the biomas locations arrays to generate the nature meshes on its corresponding locations
		ClearNature();

		if (terrainTriangles.Num() == 0)
		{
			UPtgUtils::PrintDebugMessage(this, TEXT("No vertex data found! please, generate terrain before generating nature."), EPtgDebugMessageTypes::Error, DebugMessagesTimeOnScreen);
			OnGenerationFailedDelegate.Broadcast(TEXT("Nature"), TEXT("No terrain data"));
			return;
		}

		// Broadcast start
		OnGenerationStartedDelegate.Broadcast(TEXT("Nature"));
		UPtgUtils::PrintDebugMessage(this, TEXT("GenerateNature started."), EPtgDebugMessageTypes::Info, DebugMessagesTimeOnScreen);

		// Generate new nature
		GenerateNatureInternal();
	}
	else
	{
		UPtgUtils::PrintDebugMessage(this, TEXT("To use this action enable Generate Nature!"), EPtgDebugMessageTypes::Error, DebugMessagesTimeOnScreen);
		OnGenerationFailedDelegate.Broadcast(TEXT("Nature"), TEXT("GenerateNature disabled"));
	}
}

void APtgManager::GenerateNatureWithCustomSeed(const int32 natureSeed)
{
	// Set random seed for every bioma nature
	for (FPtgBiomaNature& biomaNature : BiomaNature)
	{
		biomaNature.Seed = natureSeed;
	}

	// Then, generate nature
	GenerateNature();
}

void APtgManager::GenerateNatureWithRandomSeed()
{
	GenerateNatureWithCustomSeed(FMath::RandRange(SEED_MIN, SEED_MAX));
}

void APtgManager::GenerateNatureInternal()
{
	const double startTime = FPlatformTime::Seconds();
	uint32 biomaNaturesCounter = 0;
	FRandomStream randomNumberGenerator;
	IsInsideHeigthRangeFuncPtr isInsideHeigthRangeFunction = nullptr;

	// Editor slow task for nature
	#if WITH_EDITOR
	TUniquePtr<FScopedSlowTask> NatureSlowTask;
	bool bCreatedNatureSlow = false;
	if (!bEditorSlowTaskActive && BiomaNature.Num() > 0)
	{
		NatureSlowTask = MakeUnique<FScopedSlowTask>((float)BiomaNature.Num(), FText::FromString(TEXT("Generating Nature...")));
		NatureSlowTask->MakeDialog();
		bEditorSlowTaskActive = true;
		bCreatedNatureSlow = true;
	}
	#endif

	// Choose function to calculate if a triangle is inside heigh range depending on the terrain shape
	switch (Shape)
	{
	case EPtgProcMeshShapes::Cube:

		isInsideHeigthRangeFunction = isInsideHeigthRangeFunc[TRIANGLE_IS_INSIDE_CUBE_HEIGHT_RANGE_FUNC_INDEX];
		break;

	case EPtgProcMeshShapes::Sphere:

		isInsideHeigthRangeFunction = isInsideHeigthRangeFunc[TRIANGLE_IS_INSIDE_SPHERE_HEIGHT_RANGE_FUNC_INDEX];
		break;

	case EPtgProcMeshShapes::Plane:
	default:

		isInsideHeigthRangeFunction = isInsideHeigthRangeFunc[TRIANGLE_IS_INSIDE_PLANE_HEIGHT_RANGE_FUNC_INDEX];
	}

	// Iterate through every bioma nature locating every random-picked mesh on its corresponding bioma and generating a random bunch of them, with other random stuff...
	for (int32 biomaIndex = 0; biomaIndex < BiomaNature.Num(); ++biomaIndex)
	{
		const FPtgBiomaNature& biomaNature = BiomaNature[biomaIndex];

		#if WITH_EDITOR
		if (NatureSlowTask)
		{
			NatureSlowTask->EnterProgressFrame(1.0f, FText::FromString(FString::Printf(TEXT("Generating Nature Bioma %d/%d"), biomaIndex + 1, BiomaNature.Num())));
		}
		#endif

		// Broadcast progress for nature (coarse per-bioma)
		OnGenerationProgressDelegate.Broadcast(FString::Printf(TEXT("Nature Bioma %d/%d"), biomaIndex + 1, BiomaNature.Num()), (float)(biomaIndex + 1) / (float)BiomaNature.Num());

		// If there are no meshes on this bioma go to the next one
		if (biomaNature.Meshes.Num() == 0) continue;

		// Initialize seed for random generation
		randomNumberGenerator.Initialize(biomaNature.Seed);

		// Generate a random number of meshes for this bioma nature between a defined min. and max.
		const uint32 meshesToSpawn = randomNumberGenerator.RandRange(biomaNature.MinMeshesToSpawn, biomaNature.MaxMeshesToSpawn);

		// Get the candidate triangles of the terrain taking into account modifiers and height percentage
		TArray<int32> triangleCandidateIndexes;
		triangleCandidateIndexes.Reserve(meshesToSpawn);

		GetTriangleCandidates
		(
			biomaNature.Modifiers,
			biomaNature.bUseLocationsOutsideModifiers,
			biomaNature.HeightPercentageRangeToLocateNatureMeshes,
			biomaNature.bUseLocationsOutsideHeightRange,
			meshesToSpawn,
			isInsideHeigthRangeFunction,
			biomaNature.CorrespondingBioma,
			randomNumberGenerator,
			triangleCandidateIndexes,
			terrainTriangles
		);

		const uint32 numTriangleCandidates = triangleCandidateIndexes.Num();

		// If there are no triangle candidates on this iteration go to the next one
		if (numTriangleCandidates == 0) continue;

		GetBiomaRotationFuncPtr getBiomaElementRotationFunction = nullptr;

		switch (biomaNature.RotationType)
		{
		case EPtgNatureRotationTypes::Random:

			getBiomaElementRotationFunction = getBiomaRotationFuncs[BIOMA_RANDOM_ROTATION_FUNC_INDEX];
			break;

		case EPtgNatureRotationTypes::TerrainShapeNormal:

			switch (Shape)
			{
			case EPtgProcMeshShapes::Cube:

				getBiomaElementRotationFunction = getBiomaRotationFuncs[BIOMA_CUBE_SHAPE_ROTATION_FUNC_INDEX];
				break;

			case EPtgProcMeshShapes::Sphere:

				getBiomaElementRotationFunction = getBiomaRotationFuncs[BIOMA_SPHERE_SHAPE_ROTATION_FUNC_INDEX];
				break;

			case EPtgProcMeshShapes::Plane:
			default:

				getBiomaElementRotationFunction = getBiomaRotationFuncs[BIOMA_PLANE_SHAPE_ROTATION_FUNC_INDEX];
			}

			break;

		case EPtgNatureRotationTypes::MeshSurfaceNormal:
		default:
			getBiomaElementRotationFunction = getBiomaRotationFuncs[BIOMA_MESH_SURFACE_ROTATION_FUNC_INDEX];
		}

		// Cache other stuff to avoid inecessary access and calculations on loop time
		const uint32 numBiomaMeshesLessOne = biomaNature.Meshes.Num() - 1;
		const uint32 numTriangleCandidatesLessOne = numTriangleCandidates - 1;
		const float minScale = biomaNature.MinMaxScale.X;
		const float maxScale = biomaNature.MinMaxScale.Y;
		const TArray<TObjectPtr<UStaticMesh>>& meshes = biomaNature.Meshes;
		const int32 cullDistance = biomaNature.CullDistance;
		const bool bEnableDensityScaling = biomaNature.bEnableDensityScaling;
		const bool bCastShadow = biomaNature.bCastShadow;
		const bool bCanAffectNavigation = biomaNature.bCanAffectNavigation;
		const bool bGenerateOverlapEvents = biomaNature.bGenerateOverlapEvents;
		const ECollisionEnabled::Type CollisionEnabled = biomaNature.CollisionEnabled;
		const ECollisionChannel CollisionObjectType = biomaNature.CollisionObjectType;
		FTransform meshTransform;

		// Iterate through every mesh to generate, giving him a new random location, scale and rotation if needed
		for (uint32 newMeshIndex = 0; newMeshIndex < meshesToSpawn; newMeshIndex++)
		{
			// If all triangle candidates has been already used, take a random one
			const int32 triangleCandidateIndex = newMeshIndex <= numTriangleCandidatesLessOne
				? newMeshIndex
				: randomNumberGenerator.RandRange(0, numTriangleCandidatesLessOne);

			const int32 triIndex = triangleCandidateIndexes[triangleCandidateIndex];
			const FPtgTriangle& randomTriangle = terrainTriangles[triIndex];

			// Pick a random available location on triangle
			meshTransform.SetLocation(randomTriangle.GetRandomPointOnTriangle(randomNumberGenerator));

			// Set rotation: if using mesh-surface-normal rotation, use precomputed quaternion to avoid recomputing the normal
			if (getBiomaElementRotationFunction == getBiomaRotationFuncs[BIOMA_MESH_SURFACE_ROTATION_FUNC_INDEX] && TriangleSurfaceQuats.IsValidIndex(triIndex))
			{
				meshTransform.SetRotation(TriangleSurfaceQuats[triIndex] * GetRandomYawQuat(randomNumberGenerator));
			}
			else
			{
				meshTransform.SetRotation(((this->*getBiomaElementRotationFunction)(randomNumberGenerator, randomTriangle, meshTransform.GetLocation())));
			}

			// Generate a random mesh scale between a defined min. and max.
			const float randomScale = randomNumberGenerator.FRandRange(minScale, maxScale);
			meshTransform.SetScale3D(FVector(randomScale, randomScale, randomScale));

			// Pick a random mesh from the array
			const int32 meshToPickIndex = randomNumberGenerator.RandRange(0, numBiomaMeshesLessOne);
			
			if (UStaticMesh* meshToPick = meshes[meshToPickIndex])
			{
				// If HISM was found, add new instance
				if (auto HISM_ref = NatureStaticMeshHISM_Correspondence.Find(meshToPick))
				{
					UHierarchicalInstancedStaticMeshComponent* ExistingHISM = *HISM_ref;
					if (ExistingHISM)
					{
						ExistingHISM->AddInstance(meshTransform);
						biomaNaturesCounter++;
					}
				}
				// If there is no corresponding HISM component, create one and store it
				else if (UHierarchicalInstancedStaticMeshComponent* HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this))
				{
					HISM->SetStaticMesh(meshToPick);
					HISM->SetEnableGravity(false);
					HISM->bApplyImpulseOnDamage = false;
					HISM->SetGenerateOverlapEvents(bGenerateOverlapEvents);
					HISM->SetCullDistances(0, cullDistance);
					HISM->SetMobility(EComponentMobility::Movable);
					HISM->CastShadow = bCastShadow;
					HISM->bNavigationRelevant = bCanAffectNavigation;
					HISM->SetCanEverAffectNavigation(bCanAffectNavigation);
					HISM->SetCollisionEnabled(CollisionEnabled);
					HISM->SetCollisionObjectType(CollisionObjectType);
					HISM->bEnableDensityScaling = bEnableDensityScaling;
					HISM->AttachToComponent(RootComp, FAttachmentTransformRules::KeepRelativeTransform);
					HISM->RegisterComponent();

					// Add the new HISM to the map
					NatureStaticMeshHISM_Correspondence.Emplace(meshToPick, HISM);

					// Finally, add instance
					HISM->AddInstance(meshTransform);
					biomaNaturesCounter++;
				}
			}
		}
	}

	// Debug
	if (bShowDebugMessages && biomaNaturesCounter > 0)
	{
		UPtgUtils::PrintDebugMessage(this, FString::FromInt(biomaNaturesCounter) + TEXT(" bioma nature meshes generated in ") + FString::SanitizeFloat(FPlatformTime::Seconds() - startTime) + TEXT(" seconds."), EPtgDebugMessageTypes::Info, DebugMessagesTimeOnScreen);
	}

	// Broadcast completion with count
	if (biomaNaturesCounter > 0)
	{
		OnGenerationCompletedDelegate.Broadcast(TEXT("Nature"), static_cast<int32>(biomaNaturesCounter));
		OnGenerationProgressDelegate.Broadcast(TEXT("Nature"), 1.0f);
	}
}

void APtgManager::ClearNature()
{
	for (auto& elem : NatureStaticMeshHISM_Correspondence)
	{
		if (UHierarchicalInstancedStaticMeshComponent* HISM = elem.Value)
		{
			HISM->ClearInstances();
			HISM->DestroyComponent();
			HISM = nullptr;
		}

		elem.Key = nullptr;
	}

	NatureStaticMeshHISM_Correspondence.Empty();
}

#pragma endregion

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma region ACTORS GENERATION

void APtgManager::GenerateActors()
{
	if (bGenerateActors)
	{
		// Clear the current spawned actors and fill the biomas locations to generate the actors on its corresponding locations
		ClearActors();

		if (terrainTriangles.Num() == 0)
		{
			UPtgUtils::PrintDebugMessage(this, TEXT("No vertex data found! please, generate terrain before generating actors."), EPtgDebugMessageTypes::Error, DebugMessagesTimeOnScreen);
			OnGenerationFailedDelegate.Broadcast(TEXT("Actors"), TEXT("No terrain data"));
			return;
		}

		// Broadcast start
		OnGenerationStartedDelegate.Broadcast(TEXT("Actors"));
		UPtgUtils::PrintDebugMessage(this, TEXT("GenerateActors started."), EPtgDebugMessageTypes::Info, DebugMessagesTimeOnScreen);

		// Generate new actors
		GenerateActorsInternal();
	}
	else
	{
		UPtgUtils::PrintDebugMessage(this, TEXT("To use this action enable Generate Actors!"), EPtgDebugMessageTypes::Error, DebugMessagesTimeOnScreen);
		OnGenerationFailedDelegate.Broadcast(TEXT("Actors"), TEXT("GenerateActors disabled"));
	}
}

void APtgManager::GenerateActorsWithCustomSeed(const int32 actorsSeed)
{
	// Set random seed for every bioma actor
	for (FPtgBiomaActors& biomaActors : BiomaActors)
	{
		biomaActors.Seed = actorsSeed;
	}

	// Then, generate actors
	GenerateActors();
}

void APtgManager::GenerateActorsWithRandomSeed()
{
	GenerateActorsWithCustomSeed(FMath::RandRange(SEED_MIN, SEED_MAX));
}

void APtgManager::GenerateActorsInternal()
{
	UWorld* world = GetWorld();
	if (world == nullptr)
	{
		UPtgUtils::PrintDebugMessage(this, TEXT("world is null!!!"), EPtgDebugMessageTypes::Error, DebugMessagesTimeOnScreen);
		OnGenerationFailedDelegate.Broadcast(TEXT("Actors"), TEXT("World is null"));
		return;
	}

	const double startTime = FPlatformTime::Seconds();
	uint32 biomaActorsCounter = 0;
	FRandomStream randomNumberGenerator;
	FActorSpawnParameters spawnParams;
	spawnParams.Owner = this;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	spawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	IsInsideHeigthRangeFuncPtr isInsideHeigthRangeFunction = nullptr;

	// Editor slow task for actors
	#if WITH_EDITOR
	TUniquePtr<FScopedSlowTask> ActorsSlowTask;
	bool bCreatedActorsSlow = false;
	if (!bEditorSlowTaskActive && BiomaActors.Num() > 0)
	{
		ActorsSlowTask = MakeUnique<FScopedSlowTask>((float)BiomaActors.Num(), FText::FromString(TEXT("Spawning Actors...")));
		ActorsSlowTask->MakeDialog();
		bEditorSlowTaskActive = true;
		bCreatedActorsSlow = true;
	}
	#endif

	// Choose function to calculate if a triangle is inside heigh range depending on the terrain shape
	switch (Shape)
	{
	case EPtgProcMeshShapes::Cube:

		isInsideHeigthRangeFunction = isInsideHeigthRangeFunc[TRIANGLE_IS_INSIDE_CUBE_HEIGHT_RANGE_FUNC_INDEX];
		break;

	case EPtgProcMeshShapes::Sphere:

		isInsideHeigthRangeFunction = isInsideHeigthRangeFunc[TRIANGLE_IS_INSIDE_SPHERE_HEIGHT_RANGE_FUNC_INDEX];
		break;

	case EPtgProcMeshShapes::Plane:
	default:

		isInsideHeigthRangeFunction = isInsideHeigthRangeFunc[TRIANGLE_IS_INSIDE_PLANE_HEIGHT_RANGE_FUNC_INDEX];
	}

	// Iterate through every nature actor setting locating every random-picked mesh on its corresponding height and generating a random bunch of them, with other random stuff...
	for (int32 biomaIndex = 0; biomaIndex < BiomaActors.Num(); ++biomaIndex)
	{
		FPtgBiomaActors& biomaActor = BiomaActors[biomaIndex];

		#if WITH_EDITOR
		if (ActorsSlowTask)
		{
			ActorsSlowTask->EnterProgressFrame(1.0f, FText::FromString(FString::Printf(TEXT("Spawning Actors Bioma %d/%d"), biomaIndex + 1, BiomaActors.Num())));
		}
		#endif

		// Broadcast progress for actors (coarse per-bioma)
		OnGenerationProgressDelegate.Broadcast(FString::Printf(TEXT("Actors Bioma %d/%d"), biomaIndex + 1, BiomaActors.Num()), (float)(biomaIndex + 1) / (float)BiomaActors.Num());

		// If the actor is not especified go to the next one
		if (biomaActor.ActorClass == nullptr) continue;

		// Initialize seed for random generation
		randomNumberGenerator.Initialize(biomaActor.Seed);

		// Generate a random number of actors to spawn between a defined min. and max.
		const int32 actorsToSpawn = randomNumberGenerator.RandRange(biomaActor.MinActorsToSpawn, biomaActor.MaxActorsToSpawn);

		// Get the candidate triangles of the terrain taking into account modifiers and height percentage
		TArray<int32> triangleCandidateIndexes;
		triangleCandidateIndexes.Reserve(actorsToSpawn);

		GetTriangleCandidates(
			biomaActor.Modifiers,
			biomaActor.bUseLocationsOutsideModifiers,
			biomaActor.HeightPercentageRangeToLocateActors,
			biomaActor.bUseLocationsOutsideHeightRange,
			actorsToSpawn,
			isInsideHeigthRangeFunction,
			biomaActor.CorrespondingBioma,
			randomNumberGenerator,
			triangleCandidateIndexes,
			terrainTriangles
		);

		const int32 numTriangleCandidates = triangleCandidateIndexes.Num();

		// If there are no triangle candidates on this iteration go to the next one
		if (numTriangleCandidates == 0) continue;

		GetBiomaRotationFuncPtr getBiomaElementRotationFunction = nullptr;

		switch (biomaActor.RotationType)
		{
		case EPtgNatureRotationTypes::Random:

			getBiomaElementRotationFunction = getBiomaRotationFuncs[BIOMA_RANDOM_ROTATION_FUNC_INDEX];
			break;

		case EPtgNatureRotationTypes::TerrainShapeNormal:

			switch (Shape)
			{
			case EPtgProcMeshShapes::Cube:

				getBiomaElementRotationFunction = getBiomaRotationFuncs[BIOMA_CUBE_SHAPE_ROTATION_FUNC_INDEX];
				break;

			case EPtgProcMeshShapes::Sphere:

				getBiomaElementRotationFunction = getBiomaRotationFuncs[BIOMA_SPHERE_SHAPE_ROTATION_FUNC_INDEX];
				break;

			case EPtgProcMeshShapes::Plane:
			default:

				getBiomaElementRotationFunction = getBiomaRotationFuncs[BIOMA_PLANE_SHAPE_ROTATION_FUNC_INDEX];
			}

			break;

		case EPtgNatureRotationTypes::MeshSurfaceNormal:
		default:
			getBiomaElementRotationFunction = getBiomaRotationFuncs[BIOMA_MESH_SURFACE_ROTATION_FUNC_INDEX];
		}

		// Cache other stuff to avoid inecessary access on loop time
		const int32 numActorsToSpawnLessOne = actorsToSpawn - 1;
		const int32 numTriangleCandidatesLessOne = numTriangleCandidates - 1;
		const TSubclassOf<AActor> actorClass = biomaActor.ActorClass;
		const float minScale = biomaActor.MinMaxScale.X;
		const float maxScale = biomaActor.MinMaxScale.Y;
		const int32 cullDistance = biomaActor.CullDistance;
		FTransform actorTransform;

		// Iterate through every actor to generate, giving him a new random location, scale and rotation if needed
		for (int32 newActorIndex = 0; newActorIndex < actorsToSpawn; newActorIndex++)
		{
			// If all triangle candidates has been already used, take a random one
			const int32 triangleCandidateIndex = newActorIndex <= numTriangleCandidatesLessOne
				? newActorIndex
				: randomNumberGenerator.RandRange(0, numTriangleCandidatesLessOne);

			const int32 triIndex = triangleCandidateIndexes[triangleCandidateIndex];
			const FPtgTriangle& randomTriangle = terrainTriangles[triIndex];

			// Pick a random available location on triangle
			actorTransform.SetLocation(randomTriangle.GetRandomPointOnTriangle(randomNumberGenerator));

			// Set rotation: if using mesh-surface-normal rotation, use precomputed quaternion to avoid recomputing the normal
			if (getBiomaElementRotationFunction == getBiomaRotationFuncs[BIOMA_MESH_SURFACE_ROTATION_FUNC_INDEX] && TriangleSurfaceQuats.IsValidIndex(triIndex))
			{
				actorTransform.SetRotation(TriangleSurfaceQuats[triIndex] * GetRandomYawQuat(randomNumberGenerator));
			}
			else
			{
				actorTransform.SetRotation((this->*getBiomaElementRotationFunction)(randomNumberGenerator, randomTriangle, actorTransform.GetLocation()));
			}

			// Generate a random mesh scale between a defined min. and max.
			const float randomScale = randomNumberGenerator.FRandRange(minScale, maxScale);
			actorTransform.SetScale3D(FVector(randomScale, randomScale, randomScale));

			// Spawn actor
			if (AActor* actor = world->SpawnActor<AActor>(actorClass, actorTransform, spawnParams))
			{
				// Attach actor to root component to have it with the correct relative transform
				actor->AttachToComponent(RootComp, FAttachmentTransformRules::KeepRelativeTransform);

				// For some reason, scale has to be manually set, despite of the specified actor transform on the spawn
				actor->SetActorScale3D(actorTransform.GetScale3D());

				// Get all actor's primitive components
				TArray<UPrimitiveComponent*> primitiveComponents;
				actor->GetComponents<UPrimitiveComponent>(primitiveComponents, true);

				// Set cull distance for actor
				for (UPrimitiveComponent* primitiveComponent : primitiveComponents)
				{
					if (primitiveComponent != nullptr)
					{
						primitiveComponent->LDMaxDrawDistance = cullDistance;
					}
				}

				// Save a reference to keep it under control
				biomaActorsSpawned.Emplace(actor);
				biomaActorsCounter++;
			}
		}
	}

	// Debug
	if (bShowDebugMessages && biomaActorsCounter > 0)
	{
		UPtgUtils::PrintDebugMessage(this, FString::FromInt(biomaActorsCounter) + TEXT(" bioma actors generated in ") + FString::SanitizeFloat(FPlatformTime::Seconds() - startTime) + TEXT(" seconds."), EPtgDebugMessageTypes::Info, DebugMessagesTimeOnScreen);
	}

	// Broadcast completion with count
	if (biomaActorsCounter > 0)
	{
		OnGenerationCompletedDelegate.Broadcast(TEXT("Actors"), static_cast<int32>(biomaActorsCounter));
		OnGenerationProgressDelegate.Broadcast(TEXT("Actors"), 1.0f);
	}
}

void APtgManager::ClearActors()
{
	// Clean up nature meshes
	for (AActor* actor : biomaActorsSpawned)
	{
		if (actor != nullptr)
		{
			actor->Destroy();
			actor = nullptr;
		}
	}

	biomaActorsSpawned.Empty();
}

#pragma endregion

void APtgManager::GetTriangleCandidates
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
	const TArray<FPtgTriangle>& InTerrainTriangles
)
{
	// Get the height range depending on the bioma current bioma 
	FVector2D heightRange;
	switch (bioma)
	{
	case EPtgNatureBiomas::Earth:

		heightRange = FVector2D(WaterHeight, HighestGeneratedHeight);
		break;

	case EPtgNatureBiomas::Underwater:

		heightRange = FVector2D(LowestGeneratedHeight, WaterHeight);
		break;

	case EPtgNatureBiomas::Both:
	default:
	heightRange = FVector2D(LowestGeneratedHeight, HighestGeneratedHeight);
	}

	// Get height range
	const FVector2D percentageRange = FVector2D(0.0f, 100.0f);
	const float lowestAllowedHeight = FMath::GetMappedRangeValueClamped(percentageRange, heightRange, heightPercentageRangeToLocateElements.X);
	const float highestAllowedHeight = FMath::GetMappedRangeValueClamped(percentageRange, heightRange, heightPercentageRangeToLocateElements.Y);

	// Cache other stuff
	const int32 numTriangles = InTerrainTriangles.Num();
	if (numTriangles == 0) return;
	const int32 lastIndex = numTriangles - 1;
	const int32 numModifiers = modifiers.Num();
	const FTransform& terrainTransform = GetTransform();

	// Height check is needed only if bioma is Earth or Underwater or percentages are different than 0-100%
	const bool bHeightCheckNeeded = bioma != EPtgNatureBiomas::Both || heightPercentageRangeToLocateElements.X != 0.0f || heightPercentageRangeToLocateElements.Y != 100.0f;

	// Reserve candidate array
	triangleCandidateIndexes.Reserve(FMath::Min<int32>(maxCandidatesToGet, numTriangles));

	// Create an index list to shuffle rather than copying triangle data
	TArray<int32> indices;
	indices.SetNumUninitialized(numTriangles);
	for (int32 i = 0; i < numTriangles; ++i) indices[i] = i;

	// Partial Fisher–Yates shuffle using indices
	for (int32 i = 0; i < numTriangles; ++i)
	{
		// Shuffle current element to randomize order
		const int32 swapIndex = randomNumberGenerator.RandRange(i, lastIndex);
		if (i != swapIndex) Swap(indices[i], indices[swapIndex]);

		const int32 triIdx = indices[i];
		const FPtgTriangle& triangle = InTerrainTriangles[triIdx];

		// If height check is needed...
		if (bHeightCheckNeeded)
		{
			const bool bIsTriangleInsideHeigthRange = (&triangle->*isInsideHeigthRangeFunction)(lowestAllowedHeight, highestAllowedHeight, terrainTransform);

			// Refuse candidate if:
			// - We only have to use locations outside height range AND triangle is inside the range OR Insie earth or water heigh ranges (needed to avoid locating things in the other bioma)
			// OR
			// - We only have to use locations inside height range AND triangle is outside the range
			if ((bUseLocationsOutsideHeightRange && 
				(
					bIsTriangleInsideHeigthRange || 
					(bioma == EPtgNatureBiomas::Earth && (&triangle->*isInsideHeigthRangeFunction)(LowestGeneratedHeight, WaterHeight, terrainTransform)) ||
					(bioma == EPtgNatureBiomas::Underwater && (&triangle->*isInsideHeigthRangeFunction)(WaterHeight, HighestGeneratedHeight, terrainTransform))
				)) ||
				(!bUseLocationsOutsideHeightRange && !bIsTriangleInsideHeigthRange))
			{
				continue;
			}
		}

		bool bRefuseCandidate = false;

		// Check modifiers
		if (numModifiers > 0)
		{
			bRefuseCandidate = !bUseLocationsOutsideModifiers;

			const FPtgTriangle worldSpaceTriangle = triangle.GetWorldSpaceTriangle(terrainTransform);

			for (const APtgModifier* modifier : modifiers)
			{
				// Refuse candidate if:
				// - Triangle is inside modifier AND we only have to use locations outside modifiers
				// OR
				// - Triangle is outside every modifier AND we only have to use locations inside some modifier
				if (modifier != nullptr && worldSpaceTriangle.IsInsideShape(modifier->GetShapeComponent()))
				{
					bRefuseCandidate = bUseLocationsOutsideModifiers;
					break;
				}
			}
		}

		if (!bRefuseCandidate)
		{
			// Add triangle index as a valid candidate
			triangleCandidateIndexes.Add(triIdx);

			// If we have enough candidates, exit
			if (triangleCandidateIndexes.Num() == maxCandidatesToGet)
			{
				break;
			}
		}
	}
}

const TArray<float>& APtgManager::GetVertexHeightData() const
{
	if (procMeshData.GetNumVertices() == 0)
	{
		UPtgUtils::PrintDebugMessage(this, TEXT("No vertex data found! please, generate terrain before creating heightmap."), EPtgDebugMessageTypes::Error, DebugMessagesTimeOnScreen);
	}

	return vertexHeightData;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma region EDITOR

#if WITH_EDITOR
void APtgManager::PreEditChange(FProperty* PropertyThatWillChange)
{
	// Sometimes, when modifying actor properties, the RMC unregisters losing its runtime mesh. Here we save both to restore them later, in PostEditChangeProperty 
	terrainRuntimeMesh = (IsValid(ProcMeshTerrainComp)) ? ProcMeshTerrainComp->GetRuntimeMesh() : nullptr;
	waterRuntimeMesh = (IsValid(ProcMeshWaterComp)) ? ProcMeshWaterComp->GetRuntimeMesh() : nullptr;

	Super::PreEditChange(PropertyThatWillChange);
}

void APtgManager::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Restore runtime mesh for terrain RMC
	if (IsValid(ProcMeshTerrainComp) && IsValid(terrainRuntimeMesh))
	{
		ProcMeshTerrainComp->SetRuntimeMesh(terrainRuntimeMesh);
		terrainRuntimeMesh = nullptr;
	}

	// Restore runtime mesh for water RMC
	if (IsValid(ProcMeshWaterComp) && IsValid(waterRuntimeMesh))
	{
		ProcMeshWaterComp->SetRuntimeMesh(waterRuntimeMesh);
		waterRuntimeMesh = nullptr;
	}

	const FName changedPropertyName = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	if (changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, Radius) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, Resolution) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, NumberOfLODs) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, LODScreenSizeMultiplier) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, NoiseInputScale) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, NoiseOutputScale))
	{
		if (bGenerateEverythingOnPropertyChange)
		{
			GenerateEverything();
		}
	}
	else if (changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, Shape))
	{
		// Force ProceduralMesh on Sphere and Cube shapes
		switch (Shape)
		{
		case EPtgProcMeshShapes::Cube:
		case EPtgProcMeshShapes::Sphere:

			bUseTerrainTiling = false;

			if (WaterHeightGenerationType == EPtgWaterHeightGenerationType::FixedHeight)
			{
				WaterHeightGenerationType = EPtgWaterHeightGenerationType::RandomPercentage;

				UPtgUtils::PrintDebugMessage(this, TEXT("Water Height Generation Type changed automatically to Random Percentage."), EPtgDebugMessageTypes::Info, DebugMessagesTimeOnScreen);
			}

			break;

		case EPtgProcMeshShapes::Plane:
		default:
			break;
		}

		if (bGenerateEverythingOnPropertyChange)
		{
			GenerateEverything();
		}
	}
	// On terrain material changed
	else if (changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, TerrainMaterial))
	{
		URuntimeMeshProvider* provider = ProcMeshTerrainComp->GetProvider();

		if (provider != nullptr)
		{
			if (TerrainMaterial == nullptr)
			{
				provider->SetupMaterialSlot(0, "TerrainMaterial", nullptr);
			}
			else
			{
				UMaterial* material = TerrainMaterial->GetMaterial();
				if (material != nullptr)
				{
					provider->SetupMaterialSlot(0, "TerrainMaterial", TerrainMaterial);
				}
			}
		}
	}
	else if (changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, bGenerateWater))
	{
		if (bGenerateEverythingOnPropertyChange)
		{
			GenerateEverything();
		}
		else if (!bGenerateWater)
		{
			ClearWaterMesh();
		}
	}
	// On any water height property changed
	else if (changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, WaterHeightGenerationType) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, WaterSeed) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, WaterRandomHeightRangePercentages) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, WaterFixedHeightPercentage) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, WaterFixedHeightValue))
	{
		if (WaterHeightGenerationType == EPtgWaterHeightGenerationType::FixedHeight && Shape != EPtgProcMeshShapes::Plane)
		{
			WaterHeightGenerationType = EPtgWaterHeightGenerationType::RandomPercentage;
			UPtgUtils::PrintDebugMessage(this, TEXT("Fixed Height is only available for plane terrains!"), EPtgDebugMessageTypes::Warning, DebugMessagesTimeOnScreen);
		}

		// Clamp percentages between 0 and 100
		WaterRandomHeightRangePercentages = WaterRandomHeightRangePercentages.ClampAxes(0.0f, 100.0f);

		if (bGenerateEverythingOnPropertyChange)
		{
			// Regenerate terrain with new noise settings
			GenerateEverything();
		}
	}
	// On water material changed
	else if ((changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, WaterMaterial)))
	{
		URuntimeMeshProvider* provider = ProcMeshWaterComp->GetProvider();

		if (provider != nullptr)
		{
			if (WaterMaterial == nullptr)
			{
				provider->SetupMaterialSlot(0, "WaterMaterial", nullptr);
			}
			else
			{
				UMaterial* material = WaterMaterial->GetMaterial();
				if (material != nullptr)
				{
					provider->SetupMaterialSlot(0, "WaterMaterial", WaterMaterial);
				}
			}
		}
	}
	else if (changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, bGenerateNature))
	{
		if (bGenerateNature && bGenerateEverythingOnPropertyChange)
		{
			GenerateNature();
		}
		else if (!bGenerateNature)
		{
			ClearNature();
		}
	}
	else if (changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, bGenerateActors))
	{
		if (bGenerateActors && bGenerateEverythingOnPropertyChange)
		{
			GenerateActors();
		}
		else if (!bGenerateActors)
		{
			ClearActors();
		}
	}
	else if (changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, BiomaNature))
	{
		for (FPtgBiomaNature& biomaNature : BiomaNature)
		{
			// Clamp some values to something logical
			if (biomaNature.MinMeshesToSpawn < 1) biomaNature.MinMeshesToSpawn = 1;
			if (biomaNature.MinMeshesToSpawn > biomaNature.MaxMeshesToSpawn) biomaNature.MaxMeshesToSpawn = biomaNature.MinMeshesToSpawn;
			if (biomaNature.MinMaxScale.X > biomaNature.MinMaxScale.Y) biomaNature.MinMaxScale.Y = biomaNature.MinMaxScale.X;

			const int32 cullDistance = biomaNature.CullDistance;

			// Find every static mesh on map to see if we should change cull distance on HISM
			for (const UStaticMesh* mesh : biomaNature.Meshes)
			{
				if (mesh != nullptr)
				{
					if (auto HISM_ref = NatureStaticMeshHISM_Correspondence.Find(mesh))
					{
						UHierarchicalInstancedStaticMeshComponent* HISM = *HISM_ref;

						// Change cull distance only if its different than the current one
						if (HISM != nullptr && HISM->InstanceEndCullDistance != cullDistance)
						{
							HISM->SetCullDistances(0, cullDistance);
						}
					}
				}
			}
		}
	}
	else if (changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, BiomaActors))
	{
		for (FPtgBiomaActors& biomaActor : BiomaActors)
		{
			// Clamp some values to something logical
			if (biomaActor.MinActorsToSpawn < 1) biomaActor.MinActorsToSpawn = 1;
			if (biomaActor.MinActorsToSpawn > biomaActor.MaxActorsToSpawn) biomaActor.MaxActorsToSpawn = biomaActor.MinActorsToSpawn;

			const int32 cullDistance = biomaActor.CullDistance;
			const TSubclassOf<AActor> actorClass = biomaActor.ActorClass;

			// Set cull distance for bioma actors
			for (const AActor* actor : biomaActorsSpawned)
			{
				if (actor != nullptr && actor->GetClass() == actorClass)
				{
					// Get all actor's primitive components
					TArray<UPrimitiveComponent*> primitiveComponents;
					actor->GetComponents<UPrimitiveComponent>(primitiveComponents, true);

					// Set new distance for every primitive component in actor
					for (UPrimitiveComponent* primitiveComponent : primitiveComponents)
					{
						if (primitiveComponent != nullptr)
						{
							primitiveComponent->SetCullDistance(cullDistance);
						}
					}
				}
			}
		}
	}
	// On any Fast Noise Lite property changed
	else if ((changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, NoiseType)) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, RotationType3D) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, Seed) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, Frequency) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, FractalType) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, FractalOctaves) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, FractalLacunarity) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, FractalGain) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, FractalWeightedStrength) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, FractalWeightedStrength) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, CellularJitter) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, CellularDistanceFunction) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, CellularReturnType) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, DomainWarpType) ||
		changedPropertyName == GET_MEMBER_NAME_CHECKED(APtgManager, DomainWarpAmplitude))
	{
		SetupFastNoiseLite();

		if (bGenerateEverythingOnPropertyChange)
		{
			// Regenerate terrain with new noise settings
			GenerateEverything();
		}
	}
}
#endif

#pragma endregion
