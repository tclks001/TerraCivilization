#include "TerraTerrainDecorGeneratorLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "TextureResource.h"
#include "UDynamicMesh.h"

namespace TerraTerrainDecorGenerator
{
	using namespace UE::Geometry;

	struct FHeightSampler
	{
		TArray64<uint8> MipData;
		int32 SizeX = 0;
		int32 SizeY = 0;
		ETextureSourceFormat Format = TSF_Invalid;
		bool bValid = false;

		bool Initialize(UTexture2D* Texture, FString& OutMessage)
		{
			if (Texture == nullptr)
			{
				return true;
			}

			if (!Texture->Source.IsValid())
			{
				OutMessage = FString::Printf(TEXT("Height texture '%s' has no readable source data."), *Texture->GetName());
				return false;
			}

			SizeX = Texture->Source.GetSizeX();
			SizeY = Texture->Source.GetSizeY();
			Format = Texture->Source.GetFormat();
			if (SizeX <= 0 || SizeY <= 0 || !Texture->Source.GetMipData(MipData, 0))
			{
				OutMessage = FString::Printf(TEXT("Unable to read top mip from height texture '%s'."), *Texture->GetName());
				return false;
			}

			bValid = true;
			return true;
		}

		float Sample(float U, float V) const
		{
			if (!bValid)
			{
				return 0.5f;
			}

			const float X = FMath::Clamp(U, 0.0f, 1.0f) * static_cast<float>(SizeX - 1);
			const float Y = FMath::Clamp(V, 0.0f, 1.0f) * static_cast<float>(SizeY - 1);
			const int32 X0 = FMath::FloorToInt(X);
			const int32 Y0 = FMath::FloorToInt(Y);
			const int32 X1 = FMath::Min(X0 + 1, SizeX - 1);
			const int32 Y1 = FMath::Min(Y0 + 1, SizeY - 1);
			const float TX = X - X0;
			const float TY = Y - Y0;
			return FMath::Lerp(
				FMath::Lerp(SampleNearest(X0, Y0), SampleNearest(X1, Y0), TX),
				FMath::Lerp(SampleNearest(X0, Y1), SampleNearest(X1, Y1), TX),
				TY);
		}

		float SampleNearest(int32 X, int32 Y) const
		{
			const int64 PixelIndex = static_cast<int64>(Y) * SizeX + X;
			switch (Format)
			{
			case TSF_G8:
				return static_cast<float>(MipData[PixelIndex]) / 255.0f;
			case TSF_G16:
				return static_cast<float>(reinterpret_cast<const uint16*>(MipData.GetData())[PixelIndex]) / 65535.0f;
			case TSF_BGRA8:
			{
				const FColor C = reinterpret_cast<const FColor*>(MipData.GetData())[PixelIndex];
				return (0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B) / 255.0f;
			}
			case TSF_RGBA16:
			{
				const uint16* Data = reinterpret_cast<const uint16*>(MipData.GetData()) + PixelIndex * 4;
				return (0.2126f * Data[0] + 0.7152f * Data[1] + 0.0722f * Data[2]) / 65535.0f;
			}
			case TSF_RGBA16F:
			{
				const FLinearColor C = reinterpret_cast<const FFloat16Color*>(MipData.GetData())[PixelIndex].GetFloats();
				return FMath::Clamp(0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B, 0.0f, 1.0f);
			}
			default:
				return 0.5f;
			}
		}
	};

	uint32 Hash(uint32 Value)
	{
		Value ^= Value >> 16;
		Value *= 0x7feb352du;
		Value ^= Value >> 15;
		Value *= 0x846ca68bu;
		return Value ^ (Value >> 16);
	}

	float Hash01(int32 X, int32 Y, int32 Seed)
	{
		const uint32 H = Hash(static_cast<uint32>(X) * 0x9e3779b9u ^ static_cast<uint32>(Y) * 0x85ebca6bu ^ static_cast<uint32>(Seed));
		return static_cast<float>(H & 0x00ffffffu) / static_cast<float>(0x00ffffffu);
	}

	float ValueNoise(float X, float Y, int32 Seed)
	{
		const int32 X0 = FMath::FloorToInt(X);
		const int32 Y0 = FMath::FloorToInt(Y);
		const float TX = FMath::SmoothStep(0.0f, 1.0f, X - X0);
		const float TY = FMath::SmoothStep(0.0f, 1.0f, Y - Y0);
		return FMath::Lerp(
			FMath::Lerp(Hash01(X0, Y0, Seed), Hash01(X0 + 1, Y0, Seed), TX),
			FMath::Lerp(Hash01(X0, Y0 + 1, Seed), Hash01(X0 + 1, Y0 + 1, Seed), TX),
			TY) * 2.0f - 1.0f;
	}

	float FractalNoise(float X, float Y, int32 Seed, int32 Octaves)
	{
		float Sum = 0.0f;
		float Amplitude = 0.5f;
		float Normalization = 0.0f;
		for (int32 Octave = 0; Octave < FMath::Clamp(Octaves, 1, 8); ++Octave)
		{
			Sum += ValueNoise(X, Y, Seed + Octave * 1013) * Amplitude;
			Normalization += Amplitude;
			X *= 2.03f;
			Y *= 2.03f;
			Amplitude *= 0.5f;
		}
		return Sum / FMath::Max(Normalization, UE_SMALL_NUMBER);
	}

	bool ValidateObjectPath(const FString& Path, const TCHAR* Label, FString& OutMessage)
	{
		if (!FPackageName::IsValidObjectPath(Path))
		{
			OutMessage = FString::Printf(TEXT("%s is not a valid object path: %s"), Label, *Path);
			return false;
		}
		return true;
	}

	void SetTriangleUVs(FDynamicMeshUVOverlay* UVOverlay, int32 TriangleId, const FVector2f& A, const FVector2f& B, const FVector2f& C)
	{
		if (UVOverlay == nullptr || TriangleId < 0)
		{
			return;
		}
		UVOverlay->SetTriangle(TriangleId, FIndex3i(
			UVOverlay->AppendElement(A),
			UVOverlay->AppendElement(B),
			UVOverlay->AppendElement(C)));
	}

	class ITerrainDecorMeshGenerator
	{
	public:
		virtual ~ITerrainDecorMeshGenerator() = default;
		virtual void BuildMesh(FDynamicMesh3& Mesh) const = 0;
	};

	class FRidgeMeshGenerator final : public ITerrainDecorMeshGenerator
	{
	public:
		FRidgeMeshGenerator(const FTTDGRidgeShapeSettings& InSettings, const FHeightSampler& InHeightSampler, int32 InSeed)
			: Settings(InSettings), HeightSampler(InHeightSampler), Seed(InSeed)
		{
		}

		virtual void BuildMesh(FDynamicMesh3& Mesh) const override
		{
			Mesh.Clear();
			Mesh.EnableAttributes();
			Mesh.Attributes()->SetNumUVLayers(1);
			FDynamicMeshUVOverlay* UVOverlay = Mesh.Attributes()->PrimaryUV();

			const int32 NX = FMath::Clamp(Settings.LengthSegments, 8, 512);
			const int32 NY = FMath::Clamp(Settings.WidthSegments, 4, 256);
			const int32 RowSize = NY + 1;
			TArray<int32> TopIds;
			TArray<int32> BottomIds;
			TopIds.SetNum((NX + 1) * RowSize);
			BottomIds.SetNum((NX + 1) * RowSize);

			for (int32 IX = 0; IX <= NX; ++IX)
			{
				const float U = static_cast<float>(IX) / NX;
				const float X = (U - 0.5f) * Settings.LengthCM;
				const float Along = U * Settings.ErosionFrequency;
				const float BroadNoise = FractalNoise(Along * 0.45f, 0.17f, Seed + 11, 3);
				const float FineNoise = FractalNoise(Along, 1.71f, Seed + 29, Settings.ErosionOctaves);
				const float Wavelength = FMath::Max(Settings.CenterlineWavelengthCM, 1.0f);
				const float Phase = static_cast<float>(Hash(Seed) & 0xffffu) / 65535.0f * UE_TWO_PI;
				const float CenterY = Settings.CenterlineWavinessCM *
					(0.65f * FMath::Sin(X / Wavelength * UE_TWO_PI + Phase) + 0.35f * BroadNoise);
				const float HalfWidth = Settings.WidthCM * 0.5f * FMath::Max(0.25f, 1.0f + Settings.WidthVariation * BroadNoise);
				const float HeightScale = FMath::Max(0.15f, 1.0f + Settings.HeightVariation * FineNoise);
				const float CrestOffset = FMath::Clamp(
					Settings.CrestOffsetNormalized + Settings.CrestOffsetVariation * BroadNoise,
					-0.75f,
					0.75f);
				const float EndDistance = FMath::Min(U, 1.0f - U);
				const float EndTaper = FMath::SmoothStep(0.0f, FMath::Max(Settings.EndTaperFraction, 0.001f), EndDistance);

				for (int32 IY = 0; IY <= NY; ++IY)
				{
					const float V = static_cast<float>(IY) / NY;
					const float Across = V * 2.0f - 1.0f;
					const float Y = CenterY + Across * HalfWidth;
					const float SideDenominator = Across <= CrestOffset ? CrestOffset + 1.0f : 1.0f - CrestOffset;
					const float CrestDistance = FMath::Clamp(FMath::Abs(Across - CrestOffset) / FMath::Max(SideDenominator, 0.01f), 0.0f, 1.0f);
					const float Profile = FMath::Pow(1.0f - CrestDistance, FMath::Max(Settings.CrossSectionSharpness, 0.25f));
					const float Erosion = FractalNoise(Along, V * Settings.ErosionFrequency, Seed + 53, Settings.ErosionOctaves);
					const float TextureDisplacement = (HeightSampler.Sample(U, V) - 0.5f) * 2.0f * Settings.HeightTextureDisplacementCM;
					const float Z = FMath::Max(0.0f,
						Settings.HeightCM * HeightScale * EndTaper * Profile +
						Settings.ErosionStrengthCM * Erosion * Profile * EndTaper +
						TextureDisplacement * Profile * EndTaper);

					const int32 Index = IX * RowSize + IY;
					TopIds[Index] = Mesh.AppendVertex(FVector3d(X, Y, Z));
					BottomIds[Index] = Mesh.AppendVertex(FVector3d(X, Y, -Settings.SkirtDepthCM));
				}
			}

			for (int32 IX = 0; IX < NX; ++IX)
			{
				for (int32 IY = 0; IY < NY; ++IY)
				{
					const int32 A = IX * RowSize + IY;
					const int32 B = (IX + 1) * RowSize + IY;
					const int32 C = IX * RowSize + IY + 1;
					const int32 D = (IX + 1) * RowSize + IY + 1;
					const FVector2f UVA(static_cast<float>(IX) / NX * Settings.TextureTiling, static_cast<float>(IY) / NY);
					const FVector2f UVB(static_cast<float>(IX + 1) / NX * Settings.TextureTiling, static_cast<float>(IY) / NY);
					const FVector2f UVC(static_cast<float>(IX) / NX * Settings.TextureTiling, static_cast<float>(IY + 1) / NY);
					const FVector2f UVD(static_cast<float>(IX + 1) / NX * Settings.TextureTiling, static_cast<float>(IY + 1) / NY);
					SetTriangleUVs(UVOverlay, Mesh.AppendTriangle(TopIds[A], TopIds[C], TopIds[B]), UVA, UVC, UVB);
					SetTriangleUVs(UVOverlay, Mesh.AppendTriangle(TopIds[B], TopIds[C], TopIds[D]), UVB, UVC, UVD);
					SetTriangleUVs(UVOverlay, Mesh.AppendTriangle(BottomIds[A], BottomIds[B], BottomIds[C]), UVA, UVB, UVC);
					SetTriangleUVs(UVOverlay, Mesh.AppendTriangle(BottomIds[B], BottomIds[D], BottomIds[C]), UVB, UVD, UVC);
				}
			}

			AppendSide(Mesh, UVOverlay, TopIds, BottomIds, NX, NY, RowSize, true, 0);
			AppendSide(Mesh, UVOverlay, TopIds, BottomIds, NX, NY, RowSize, true, NY);
			AppendSide(Mesh, UVOverlay, TopIds, BottomIds, NY, NX, RowSize, false, 0);
			AppendSide(Mesh, UVOverlay, TopIds, BottomIds, NY, NX, RowSize, false, NX);
		}

	private:
		void AppendSide(
			FDynamicMesh3& Mesh,
			FDynamicMeshUVOverlay* UVOverlay,
			const TArray<int32>& TopIds,
			const TArray<int32>& BottomIds,
			int32 SegmentCount,
			int32 OtherSegmentCount,
			int32 RowSize,
			bool bAlongX,
			int32 FixedIndex) const
		{
			for (int32 I = 0; I < SegmentCount; ++I)
			{
				const int32 A = bAlongX ? I * RowSize + FixedIndex : FixedIndex * RowSize + I;
				const int32 B = bAlongX ? (I + 1) * RowSize + FixedIndex : FixedIndex * RowSize + I + 1;
				const float U0 = static_cast<float>(I) / SegmentCount;
				const float U1 = static_cast<float>(I + 1) / SegmentCount;
				const bool bReverse = (bAlongX && FixedIndex == OtherSegmentCount) || (!bAlongX && FixedIndex == 0);
				if (bReverse)
				{
					SetTriangleUVs(UVOverlay, Mesh.AppendTriangle(TopIds[A], BottomIds[A], BottomIds[B]), FVector2f(U0, 0), FVector2f(U0, 1), FVector2f(U1, 1));
					SetTriangleUVs(UVOverlay, Mesh.AppendTriangle(TopIds[A], BottomIds[B], TopIds[B]), FVector2f(U0, 0), FVector2f(U1, 1), FVector2f(U1, 0));
				}
				else
				{
					SetTriangleUVs(UVOverlay, Mesh.AppendTriangle(TopIds[A], BottomIds[B], BottomIds[A]), FVector2f(U0, 0), FVector2f(U1, 1), FVector2f(U0, 1));
					SetTriangleUVs(UVOverlay, Mesh.AppendTriangle(TopIds[A], TopIds[B], BottomIds[B]), FVector2f(U0, 0), FVector2f(U1, 0), FVector2f(U1, 1));
				}
			}
		}

		const FTTDGRidgeShapeSettings& Settings;
		const FHeightSampler& HeightSampler;
		int32 Seed = 0;
	};

	UMaterialInstanceConstant* CreateOrUpdateMaterialInstance(const FTTDGMaterialSettings& Settings, FString& OutMessage)
	{
		if (!Settings.bCreateMaterialInstance || Settings.ParentMaterial == nullptr)
		{
			return nullptr;
		}
		if (!ValidateObjectPath(Settings.MaterialInstanceAssetPathAndName, TEXT("Material instance path"), OutMessage))
		{
			return nullptr;
		}

		const FString PackageName = FPackageName::ObjectPathToPackageName(Settings.MaterialInstanceAssetPathAndName);
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		UPackage* Package = CreatePackage(*PackageName);
		Package->FullyLoad();
		UMaterialInstanceConstant* Instance = FindObject<UMaterialInstanceConstant>(Package, *AssetName);
		if (Instance == nullptr)
		{
			if (FindObject<UObject>(Package, *AssetName) != nullptr)
			{
				OutMessage = FString::Printf(TEXT("Material path is occupied by another asset type: %s"), *Settings.MaterialInstanceAssetPathAndName);
				return nullptr;
			}
			Instance = NewObject<UMaterialInstanceConstant>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Instance);
		}

		Instance->SetParentEditorOnly(Settings.ParentMaterial);
		if (Settings.BaseColorTexture != nullptr)
		{
			Instance->SetTextureParameterValueEditorOnly(Settings.TextureParameterNames.BaseColor, Settings.BaseColorTexture);
		}
		if (Settings.NormalTexture != nullptr)
		{
			Instance->SetTextureParameterValueEditorOnly(Settings.TextureParameterNames.Normal, Settings.NormalTexture);
		}
		if (Settings.RoughnessTexture != nullptr)
		{
			Instance->SetTextureParameterValueEditorOnly(Settings.TextureParameterNames.Roughness, Settings.RoughnessTexture);
		}
		if (Settings.HeightTexture != nullptr)
		{
			Instance->SetTextureParameterValueEditorOnly(Settings.TextureParameterNames.Height, Settings.HeightTexture);
		}
		Instance->PostEditChange();
		Instance->MarkPackageDirty();
		return Instance;
	}

	FString BuildMeshPath(const FTTDGAssetOutputSettings& Output, int32 VariantIndex)
	{
		FString Directory = Output.OutputDirectory;
		Directory.RemoveFromEnd(TEXT("/"));
		return FString::Printf(TEXT("%s/%s_%02d"), *Directory, *Output.StaticMeshNamePrefix, VariantIndex);
	}
}

UStaticMesh* UTerraTerrainDecorGeneratorLibrary::GenerateRidgeStaticMeshAsset(
	const FTTDGRidgeAssetBuildSettings& Settings,
	int32 VariantIndex,
	FString& OutMessage)
{
	using namespace TerraTerrainDecorGenerator;
	OutMessage.Reset();

	if (Settings.AssetType != ETTDGDecorAssetType::Ridge)
	{
		OutMessage = TEXT("Only the Ridge generator is implemented in this version.");
		return nullptr;
	}

	const FString MeshPath = BuildMeshPath(Settings.Output, VariantIndex);
	if (!ValidateObjectPath(MeshPath, TEXT("Static mesh path"), OutMessage))
	{
		return nullptr;
	}
	if (FindObject<UObject>(nullptr, *MeshPath) != nullptr || FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(MeshPath)))
	{
		OutMessage = FString::Printf(TEXT("Asset already exists; choose another index or delete it explicitly: %s"), *MeshPath);
		return nullptr;
	}

	FHeightSampler HeightSampler;
	if (!HeightSampler.Initialize(Settings.Material.HeightTexture, OutMessage))
	{
		return nullptr;
	}

	const int32 Seed = Settings.Output.BaseSeed + VariantIndex * Settings.Output.SeedStride;
	UDynamicMesh* DynamicMesh = NewObject<UDynamicMesh>(GetTransientPackage());
	DynamicMesh->EditMesh(
		[&Settings, &HeightSampler, Seed](FDynamicMesh3& Mesh)
		{
			FRidgeMeshGenerator Generator(Settings.Ridge, HeightSampler, Seed);
			Generator.BuildMesh(Mesh);
		},
		EDynamicMeshChangeType::GeneralEdit,
		EDynamicMeshAttributeChangeFlags::MeshTopology |
		EDynamicMeshAttributeChangeFlags::VertexPositions |
		EDynamicMeshAttributeChangeFlags::NormalsTangents |
		EDynamicMeshAttributeChangeFlags::UVs,
		true);

	FGeometryScriptCreateNewStaticMeshAssetOptions Options;
	Options.bEnableRecomputeNormals = true;
	Options.bEnableRecomputeTangents = true;
	Options.bEnableNanite = Settings.Output.bEnableNanite;
	Options.bEnableCollision = Settings.Output.bEnableCollision;
	Options.CollisionMode = Settings.Output.bEnableCollision ? ECollisionTraceFlag::CTF_UseComplexAsSimple : ECollisionTraceFlag::CTF_UseDefault;

	EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
	UStaticMesh* StaticMesh = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
		DynamicMesh,
		MeshPath,
		Options,
		Outcome,
		nullptr);
	if (Outcome != EGeometryScriptOutcomePins::Success || StaticMesh == nullptr)
	{
		OutMessage = FString::Printf(TEXT("Failed to create StaticMesh asset: %s"), *MeshPath);
		return nullptr;
	}

	if (UMaterialInstanceConstant* Material = CreateOrUpdateMaterialInstance(Settings.Material, OutMessage))
	{
		StaticMesh->SetMaterial(0, Material);
		StaticMesh->PostEditChange();
		StaticMesh->MarkPackageDirty();
	}
	else if (Settings.Material.bCreateMaterialInstance && Settings.Material.ParentMaterial != nullptr && !OutMessage.IsEmpty())
	{
		return StaticMesh;
	}

	OutMessage = FString::Printf(TEXT("Generated %s (seed %d)"), *MeshPath, Seed);
	return StaticMesh;
}

bool UTerraTerrainDecorGeneratorLibrary::GenerateRidgeStaticMeshAssets(
	const FTTDGRidgeAssetBuildSettings& Settings,
	TArray<UStaticMesh*>& OutAssets,
	FString& OutMessage)
{
	OutAssets.Reset();
	TArray<FString> Failures;
	const int32 Count = FMath::Clamp(Settings.Output.VariantCount, 1, 128);
	for (int32 Offset = 0; Offset < Count; ++Offset)
	{
		const int32 VariantIndex = Settings.Output.FirstVariantIndex + Offset;
		FString VariantMessage;
		if (UStaticMesh* Mesh = GenerateRidgeStaticMeshAsset(Settings, VariantIndex, VariantMessage))
		{
			OutAssets.Add(Mesh);
		}
		else
		{
			Failures.Add(FString::Printf(TEXT("%02d: %s"), VariantIndex, *VariantMessage));
		}
	}

	if (Failures.IsEmpty())
	{
		OutMessage = FString::Printf(TEXT("Generated %d ridge assets."), OutAssets.Num());
		return true;
	}

	OutMessage = FString::Printf(
		TEXT("Generated %d of %d ridge assets. Failures:\n%s"),
		OutAssets.Num(),
		Count,
		*FString::Join(Failures, TEXT("\n")));
	return false;
}
