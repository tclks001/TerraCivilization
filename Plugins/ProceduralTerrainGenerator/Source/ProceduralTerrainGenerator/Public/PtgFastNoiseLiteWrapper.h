// Copyright 2021 VICTOR HERNANDEZ MOLPECERES (Rockam). All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ThirdParty/FastNoiseLite/Public/FastNoiseLite.h"
#include "PtgFastNoiseLiteWrapper.generated.h"

// Fast Noise Lite enum wrappers
UENUM(BlueprintType)
enum class EPtgFastNoiseLiteWrapperNoiseType : uint8
{
	NoiseType_OpenSimplex2					UMETA(DisplayName = "Open Simplex 2"),
	NoiseType_OpenSimplex2S					UMETA(DisplayName = "Open Simplex 2S"),
	NoiseType_Cellular						UMETA(DisplayName = "Cellular"),
	NoiseType_Perlin						UMETA(DisplayName = "Perlin"),
	NoiseType_ValueCubic					UMETA(DisplayName = "Value Cubic"),
	NoiseType_Value							UMETA(DisplayName = "Value")
};

UENUM(BlueprintType)
enum class EPtgFastNoiseLiteWrapperRotationType3D : uint8
{
	RotationType3D_None						UMETA(DisplayName = "None"),
	RotationType3D_ImproveXYPlanes			UMETA(DisplayName = "Improve XY Planes"),
	RotationType3D_ImproveXZPlanes			UMETA(DisplayName = "Improve XZ Planes")
};

UENUM(BlueprintType)
enum class EPtgFastNoiseLiteWrapperFractalType : uint8
{
	FractalType_None						UMETA(DisplayName = "None"),
	FractalType_FBm							UMETA(DisplayName = "FBM"),
	FractalType_Ridged						UMETA(DisplayName = "Ridged"),
	FractalType_PingPong					UMETA(DisplayName = "Ping Pong"),
	FractalType_DomainWarpProgressive		UMETA(DisplayName = "Domain Warp Progressive"),
	FractalType_DomainWarpIndependent		UMETA(DisplayName = "Domain Warp Independent")
};

UENUM(BlueprintType)
enum class EPtgFastNoiseLiteWrapperCellularDistanceFunction : uint8
{
	CellularDistanceFunction_Euclidean		UMETA(DisplayName = "Euclidean"),
	CellularDistanceFunction_EuclideanSq	UMETA(DisplayName = "Euclidean Sq"),
	CellularDistanceFunction_Manhattan		UMETA(DisplayName = "Manhattan"),
	CellularDistanceFunction_Hybrid			UMETA(DisplayName = "Hybrid")
};

UENUM(BlueprintType)
enum class EPtgFastNoiseLiteWrapperCellularReturnType : uint8
{
	CellularReturnType_CellValue			UMETA(DisplayName = "Cell Value"),
	CellularReturnType_Distance				UMETA(DisplayName = "Distance"),
	CellularReturnType_Distance2			UMETA(DisplayName = "Distance 2"),
	CellularReturnType_Distance2Add			UMETA(DisplayName = "Distance 2 Add"),
	CellularReturnType_Distance2Sub			UMETA(DisplayName = "Distance 2 Sub"),
	CellularReturnType_Distance2Mul			UMETA(DisplayName = "Distance 2 Mul"),
	CellularReturnType_Distance2Div			UMETA(DisplayName = "Distance 2 Div")
};

UENUM(BlueprintType)
enum class EPtgFastNoiseLiteWrapperDomainWarpType : uint8
{
	DomainWarpType_None						UMETA(DisplayName = "None"),
	DomainWarpType_OpenSimplex2				UMETA(DisplayName = "Open Simplex 2"),
	DomainWarpType_OpenSimplex2Reduced		UMETA(DisplayName = "Open Simplex 2 Reduced"),
	DomainWarpType_BasicGrid				UMETA(DisplayName = "Basic Grid")
};

/**
 * Wrapper for Auburns's Fast Noise Lite library, also available for blueprints usage.
 */
UCLASS(BlueprintType)
class PROCEDURALTERRAINGENERATOR_API UPtgFastNoiseLiteWrapper : public UObject
{
	GENERATED_BODY()

public:

	/**
	* Set all the properties needed to generate the noise.
	*
	* @param seed						- Seed used for all noise types. Using different seeds will cause the noise output to change. Default value: 1619
	* @param frequency					- Sets frequency for all noise types. Default value: 0.02
	* @param noiseType					- Sets noise algorithm used for GetNoise(...). Default value: OpenSimplex2
	* @param rotationType3D				- Sets domain rotation type for 3D Noise and 3D DomainWarp. Default value: None
	* @param fractalType				- Sets method for combining octaves in all fractal noise types. Default value: FBM
	* @param octaves					- Sets octave count for all fractal noise types. Default value: 5
	* @param lacunarity					- Sets octave lacunarity for all fractal noise types. Default value: 2.0
	* @param gain						- Sets octave gain for all fractal noise types. Default value: 0.5
	* @param weightedStrength			- Sets octave weighting for all non DomainWarp fractal types. Default value: 0.0. Note: Keep between 0...1 to maintain -1...1 output bounding.
	* @param pingPongStrength			- Sets strength of the fractal ping pong effect. Default value: 2.0
	* @param cellularDistanceFunction	- Sets distance function used in cellular noise calculations. Default value: EuclideanSq
	* @param cellularReturnType			- Sets return type from cellular noise calculations. Default value: Distance
	* @param cellularJitter				- Sets the maximum distance a cellular point can move from it's grid position. Default value: 1.0. Note: Setting this higher than 1 will cause artifacts
	* @param domainWarpType				- Sets the warp algorithm when using DomainWarp(...). Default value: None
	* @param domainWarpAmp				- Sets the maximum warp distance from original position when using DomainWarp(...). Default value: 30.0
	*/
	UFUNCTION(BlueprintCallable, Category = "PTG Fast Noise Lite Wrapper")
	void SetupFastNoiseLite
	(
		const int seed = 1619,
		const float frequency = 0.02f,
		const EPtgFastNoiseLiteWrapperNoiseType noiseType = EPtgFastNoiseLiteWrapperNoiseType::NoiseType_OpenSimplex2,
		const EPtgFastNoiseLiteWrapperRotationType3D rotationType3D = EPtgFastNoiseLiteWrapperRotationType3D::RotationType3D_None,
		const EPtgFastNoiseLiteWrapperFractalType fractalType = EPtgFastNoiseLiteWrapperFractalType::FractalType_FBm,
		const int octaves = 5,
		const float lacunarity = 2.0f,
		const float gain = 0.5f,
		const float weightedStrength = 0.0f,
		const float pingPongStrength = 2.0f,
		const EPtgFastNoiseLiteWrapperCellularDistanceFunction cellularDistanceFunction = EPtgFastNoiseLiteWrapperCellularDistanceFunction::CellularDistanceFunction_EuclideanSq,
		const EPtgFastNoiseLiteWrapperCellularReturnType cellularReturnType = EPtgFastNoiseLiteWrapperCellularReturnType::CellularReturnType_Distance,
		const float cellularJitter = 1.0f,
		const EPtgFastNoiseLiteWrapperDomainWarpType domainWarpType = EPtgFastNoiseLiteWrapperDomainWarpType::DomainWarpType_None,
		const float domainWarpAmp = 30.0f
	)
	{
		SetSeed(seed);
		SetFrequency(frequency);
		SetNoiseType(noiseType);
		SetRotationType3D(rotationType3D);
		SetFractalType(fractalType);
		SetFractalOctaves(octaves);
		SetFractalLacunarity(lacunarity);
		SetFractalGain(gain);
		SetFractalWeightedStrength(weightedStrength);
		SetFractalPingPongStrength(pingPongStrength);
		SetCellularDistanceFunction(cellularDistanceFunction);
		SetCellularReturnType(cellularReturnType);
		SetCellularJitter(cellularJitter);
		SetDomainWarpType(domainWarpType);
		SetDomainWarpAmp(domainWarpAmp);

		bInitialized = true;
	}

	/** Returns true if Fast Noise Lite properties are initialized, false otherwise. */
	UFUNCTION(BlueprintPure, Category = "PTG Fast Noise Lite Wrapper")
	bool IsInitialized() const { return bInitialized; }

	/**
	* Returns the noise calculation given x and y values.
	*
	* @param x	- The x axis value.
	* @param y	- The y axis value.
	*/
	UFUNCTION(BlueprintPure, Category = "PTG Fast Noise Lite Wrapper")
	float GetNoise2D(const float x, const float y) { return IsInitialized() ? fastNoiseLite.GetNoise(x, y) : 0.0f; }

	/**
	* Returns the noise calculation given x, y and z values.
	*
	* @param x	- The x axis value.
	* @param y	- The y axis value.
	* @param z	- The z axis value.
	*/
	UFUNCTION(BlueprintPure, Category = "PTG Fast Noise Lite Wrapper")
	float GetNoise3D(const float x, const float y, const float z = 0.0f) { return IsInitialized() ? fastNoiseLite.GetNoise(x, y, z) : 0.0f; }

	//***********************************************************
	//*********************     SETTERS     *********************
	//***********************************************************

	UFUNCTION(BlueprintCallable, Category = "PTG Fast Noise Lite Wrapper|General settings")
	void SetSeed(const int32 seed) { fastNoiseLite.SetSeed(seed); }

	UFUNCTION(BlueprintCallable, Category = "PTG Fast Noise Lite Wrapper|General settings")
	void SetFrequency(const float frequency) { fastNoiseLite.SetFrequency(frequency); }

	UFUNCTION(BlueprintCallable, Category = "PTG Fast Noise Lite Wrapper|General settings")
	void SetNoiseType(const EPtgFastNoiseLiteWrapperNoiseType noiseType)
	{
		switch (noiseType)
		{
		case EPtgFastNoiseLiteWrapperNoiseType::NoiseType_OpenSimplex2:
			fastNoiseLite.SetNoiseType(FastNoiseLite::NoiseType::NoiseType_OpenSimplex2);
			break;
		case EPtgFastNoiseLiteWrapperNoiseType::NoiseType_OpenSimplex2S:
			fastNoiseLite.SetNoiseType(FastNoiseLite::NoiseType::NoiseType_OpenSimplex2S);
			break;
		case EPtgFastNoiseLiteWrapperNoiseType::NoiseType_Cellular:
			fastNoiseLite.SetNoiseType(FastNoiseLite::NoiseType::NoiseType_Cellular);
			break;
		case EPtgFastNoiseLiteWrapperNoiseType::NoiseType_Perlin:
			fastNoiseLite.SetNoiseType(FastNoiseLite::NoiseType::NoiseType_Perlin);
			break;
		case EPtgFastNoiseLiteWrapperNoiseType::NoiseType_ValueCubic:
			fastNoiseLite.SetNoiseType(FastNoiseLite::NoiseType::NoiseType_ValueCubic);
			break;
		case EPtgFastNoiseLiteWrapperNoiseType::NoiseType_Value:
		default:
			fastNoiseLite.SetNoiseType(FastNoiseLite::NoiseType::NoiseType_Value);
		}
	}

	UFUNCTION(BlueprintCallable, Category = "PTG Fast Noise Lite Wrapper|General settings")
	void SetRotationType3D(const EPtgFastNoiseLiteWrapperRotationType3D rotationType3D)
	{
		switch (rotationType3D)
		{
		case EPtgFastNoiseLiteWrapperRotationType3D::RotationType3D_None:
			fastNoiseLite.SetRotationType3D(FastNoiseLite::RotationType3D::RotationType3D_None);
			break;
		case EPtgFastNoiseLiteWrapperRotationType3D::RotationType3D_ImproveXYPlanes:
			fastNoiseLite.SetRotationType3D(FastNoiseLite::RotationType3D::RotationType3D_ImproveXYPlanes);
			break;
		case EPtgFastNoiseLiteWrapperRotationType3D::RotationType3D_ImproveXZPlanes:
		default:
			fastNoiseLite.SetRotationType3D(FastNoiseLite::RotationType3D::RotationType3D_ImproveXZPlanes);
		}
	}

	UFUNCTION(BlueprintCallable, Category = "PTG Fast Noise Lite Wrapper|Fractal settings")
	void SetFractalType(const EPtgFastNoiseLiteWrapperFractalType fractalType)
	{
		switch (fractalType)
		{
		case EPtgFastNoiseLiteWrapperFractalType::FractalType_None:
			fastNoiseLite.SetFractalType(FastNoiseLite::FractalType::FractalType_None);
			break;
		case EPtgFastNoiseLiteWrapperFractalType::FractalType_FBm:
			fastNoiseLite.SetFractalType(FastNoiseLite::FractalType::FractalType_FBm);
			break;
		case EPtgFastNoiseLiteWrapperFractalType::FractalType_Ridged:
			fastNoiseLite.SetFractalType(FastNoiseLite::FractalType::FractalType_Ridged);
			break;
		case EPtgFastNoiseLiteWrapperFractalType::FractalType_PingPong:
			fastNoiseLite.SetFractalType(FastNoiseLite::FractalType::FractalType_PingPong);
			break;
		case EPtgFastNoiseLiteWrapperFractalType::FractalType_DomainWarpProgressive:
			fastNoiseLite.SetFractalType(FastNoiseLite::FractalType::FractalType_DomainWarpProgressive);
			break;
		case EPtgFastNoiseLiteWrapperFractalType::FractalType_DomainWarpIndependent:
		default:
			fastNoiseLite.SetFractalType(FastNoiseLite::FractalType::FractalType_DomainWarpIndependent);
		}
	}

	UFUNCTION(BlueprintCallable, Category = "PTG Fast Noise Lite Wrapper|Fractal settings")
	void SetFractalOctaves(const int32 octaves) { fastNoiseLite.SetFractalOctaves(octaves); }

	UFUNCTION(BlueprintCallable, Category = "PTG Fast Noise Lite Wrapper|Fractal settings")
	void SetFractalLacunarity(const float lacunarity) { fastNoiseLite.SetFractalLacunarity(lacunarity); }

	UFUNCTION(BlueprintCallable, Category = "PTG Fast Noise Lite Wrapper|Fractal settings")
	void SetFractalGain(const float gain) { fastNoiseLite.SetFractalGain(gain); }

	UFUNCTION(BlueprintCallable, Category = "PTG Fast Noise Lite Wrapper|Fractal settings")
	void SetFractalWeightedStrength(const float weightedStrength) { fastNoiseLite.SetFractalWeightedStrength(weightedStrength); }

	UFUNCTION(BlueprintCallable, Category = "PTG Fast Noise Lite Wrapper|Fractal settings")
	void SetFractalPingPongStrength(const float pingPongStrength) { fastNoiseLite.SetFractalPingPongStrength(pingPongStrength); }

	UFUNCTION(BlueprintCallable, Category = "PTG Fast Noise Lite Wrapper|Cellular settings")
	void SetCellularDistanceFunction(const EPtgFastNoiseLiteWrapperCellularDistanceFunction cellularDistanceFunction)
	{
		switch (cellularDistanceFunction)
		{
		case EPtgFastNoiseLiteWrapperCellularDistanceFunction::CellularDistanceFunction_Euclidean:
			fastNoiseLite.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction::CellularDistanceFunction_Euclidean);
			break;
		case EPtgFastNoiseLiteWrapperCellularDistanceFunction::CellularDistanceFunction_EuclideanSq:
			fastNoiseLite.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction::CellularDistanceFunction_EuclideanSq);
			break;
		case EPtgFastNoiseLiteWrapperCellularDistanceFunction::CellularDistanceFunction_Manhattan:
			fastNoiseLite.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction::CellularDistanceFunction_Manhattan);
			break;
		case EPtgFastNoiseLiteWrapperCellularDistanceFunction::CellularDistanceFunction_Hybrid:
		default:
			fastNoiseLite.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction::CellularDistanceFunction_Hybrid);
		}
	}

	UFUNCTION(BlueprintCallable, Category = "PTG Fast Noise Lite Wrapper|Cellular settings")
	void SetCellularReturnType(const EPtgFastNoiseLiteWrapperCellularReturnType cellularReturnType)
	{
		switch (cellularReturnType)
		{
		case EPtgFastNoiseLiteWrapperCellularReturnType::CellularReturnType_CellValue:
			fastNoiseLite.SetCellularReturnType(FastNoiseLite::CellularReturnType::CellularReturnType_CellValue);
			break;
		case EPtgFastNoiseLiteWrapperCellularReturnType::CellularReturnType_Distance:
			fastNoiseLite.SetCellularReturnType(FastNoiseLite::CellularReturnType::CellularReturnType_Distance);
			break;
		case EPtgFastNoiseLiteWrapperCellularReturnType::CellularReturnType_Distance2:
			fastNoiseLite.SetCellularReturnType(FastNoiseLite::CellularReturnType::CellularReturnType_Distance2);
			break;
		case EPtgFastNoiseLiteWrapperCellularReturnType::CellularReturnType_Distance2Add:
			fastNoiseLite.SetCellularReturnType(FastNoiseLite::CellularReturnType::CellularReturnType_Distance2Add);
			break;
		case EPtgFastNoiseLiteWrapperCellularReturnType::CellularReturnType_Distance2Sub:
			fastNoiseLite.SetCellularReturnType(FastNoiseLite::CellularReturnType::CellularReturnType_Distance2Sub);
			break;
		case EPtgFastNoiseLiteWrapperCellularReturnType::CellularReturnType_Distance2Mul:
			fastNoiseLite.SetCellularReturnType(FastNoiseLite::CellularReturnType::CellularReturnType_Distance2Mul);
			break;
		case EPtgFastNoiseLiteWrapperCellularReturnType::CellularReturnType_Distance2Div:
		default:
			fastNoiseLite.SetCellularReturnType(FastNoiseLite::CellularReturnType::CellularReturnType_Distance2Div);
		}
	}

	UFUNCTION(BlueprintCallable, Category = "PTG Fast Noise Lite Wrapper|Cellular settings")
	void SetCellularJitter(const float cellularJitter) { fastNoiseLite.SetCellularJitter(cellularJitter); }

	UFUNCTION(BlueprintCallable, Category = "PTG Fast Noise Lite Wrapper|Domain Warp settings")
	void SetDomainWarpType(const EPtgFastNoiseLiteWrapperDomainWarpType domainWarpType)
	{
		switch (domainWarpType)
		{
		case EPtgFastNoiseLiteWrapperDomainWarpType::DomainWarpType_OpenSimplex2:
			fastNoiseLite.SetDomainWarpType(FastNoiseLite::DomainWarpType::DomainWarpType_OpenSimplex2);
			break;
		case EPtgFastNoiseLiteWrapperDomainWarpType::DomainWarpType_OpenSimplex2Reduced:
			fastNoiseLite.SetDomainWarpType(FastNoiseLite::DomainWarpType::DomainWarpType_OpenSimplex2Reduced);
			break;
		case EPtgFastNoiseLiteWrapperDomainWarpType::DomainWarpType_BasicGrid:
			fastNoiseLite.SetDomainWarpType(FastNoiseLite::DomainWarpType::DomainWarpType_BasicGrid);
			break;
		case EPtgFastNoiseLiteWrapperDomainWarpType::DomainWarpType_None:
		default:
			fastNoiseLite.SetDomainWarpType(FastNoiseLite::DomainWarpType::DomainWarpType_None);
		}
	}

	UFUNCTION(BlueprintCallable, Category = "PTG Fast Noise Lite Wrapper|Domain Warp settings")
	void SetDomainWarpAmp(const float domainWarpAmp) { fastNoiseLite.SetDomainWarpAmp(domainWarpAmp); }

private:

	FastNoiseLite fastNoiseLite = FastNoiseLite();
	bool bInitialized = false;
};
