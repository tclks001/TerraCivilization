#include "TerraSphericalTileGeneratorLibrary.h"

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

namespace TerraSphericalTileGenerator
{
	struct FHeightSampler
	{
		UTexture2D* Texture = nullptr;
		TArray64<uint8> MipData;
		int32 SizeX = 0;
		int32 SizeY = 0;
		ETextureSourceFormat Format = TSF_Invalid;
		bool bValid = false;

		bool Initialize(UTexture2D* InTexture, FString& OutErrorMessage)
		{
			Texture = InTexture;
			if (Texture == nullptr)
			{
				return true;
			}

			if (!Texture->Source.IsValid())
			{
				OutErrorMessage = FString::Printf(TEXT("HeightTexture '%s' 没有有效 Source 数据，无法在编辑器侧采样。"), *Texture->GetName());
				return false;
			}

			SizeX = Texture->Source.GetSizeX();
			SizeY = Texture->Source.GetSizeY();
			Format = Texture->Source.GetFormat();

			if (SizeX <= 0 || SizeY <= 0)
			{
				OutErrorMessage = FString::Printf(TEXT("HeightTexture '%s' 尺寸无效。"), *Texture->GetName());
				return false;
			}

			if (!Texture->Source.GetMipData(MipData, 0))
			{
				OutErrorMessage = FString::Printf(TEXT("无法读取 HeightTexture '%s' 的顶层 Mip 数据。"), *Texture->GetName());
				return false;
			}

			bValid = true;
			return true;
		}

		float Sample(float U, float V) const
		{
			if (!bValid || SizeX <= 0 || SizeY <= 0 || MipData.Num() == 0)
			{
				return 0.0f;
			}

			const float X = FMath::Clamp(U, 0.0f, 1.0f) * static_cast<float>(SizeX - 1);
			const float Y = FMath::Clamp(V, 0.0f, 1.0f) * static_cast<float>(SizeY - 1);
			const int32 X0 = FMath::FloorToInt(X);
			const int32 Y0 = FMath::FloorToInt(Y);
			const int32 X1 = FMath::Min(X0 + 1, SizeX - 1);
			const int32 Y1 = FMath::Min(Y0 + 1, SizeY - 1);
			const float TX = X - static_cast<float>(X0);
			const float TY = Y - static_cast<float>(Y0);

			const float H00 = SampleNearest(X0, Y0);
			const float H10 = SampleNearest(X1, Y0);
			const float H01 = SampleNearest(X0, Y1);
			const float H11 = SampleNearest(X1, Y1);

			const float H0 = FMath::Lerp(H00, H10, TX);
			const float H1 = FMath::Lerp(H01, H11, TX);
			return FMath::Lerp(H0, H1, TY);
		}

		float SampleNearest(int32 X, int32 Y) const
		{
			const int64 PixelIndex = static_cast<int64>(Y) * SizeX + X;
			switch (Format)
			{
			case TSF_G8:
				return static_cast<float>(MipData[PixelIndex]) / 255.0f;
			case TSF_G16:
			{
				const uint16* Data = reinterpret_cast<const uint16*>(MipData.GetData());
				return static_cast<float>(Data[PixelIndex]) / 65535.0f;
			}
			case TSF_BGRA8:
			{
				const FColor* Data = reinterpret_cast<const FColor*>(MipData.GetData());
				const FColor Color = Data[PixelIndex];
				return (0.2126f * Color.R + 0.7152f * Color.G + 0.0722f * Color.B) / 255.0f;
			}
			case TSF_RGBA16:
			{
				const uint16* Data = reinterpret_cast<const uint16*>(MipData.GetData());
				const int64 ComponentIndex = PixelIndex * 4;
				const float R = static_cast<float>(Data[ComponentIndex + 0]) / 65535.0f;
				const float G = static_cast<float>(Data[ComponentIndex + 1]) / 65535.0f;
				const float B = static_cast<float>(Data[ComponentIndex + 2]) / 65535.0f;
				return 0.2126f * R + 0.7152f * G + 0.0722f * B;
			}
			case TSF_RGBA16F:
			{
				const FFloat16Color* Data = reinterpret_cast<const FFloat16Color*>(MipData.GetData());
				const FLinearColor Color = Data[PixelIndex].GetFloats();
				return FMath::Clamp(0.2126f * Color.R + 0.7152f * Color.G + 0.0722f * Color.B, 0.0f, 1.0f);
			}
			default:
				return 0.0f;
			}
		}
	};

	bool ValidateAssetPath(const FString& AssetPathAndName, const TCHAR* Label, FString& OutErrorMessage)
	{
		if (AssetPathAndName.IsEmpty())
		{
			OutErrorMessage = FString::Printf(TEXT("%s 不能为空。"), Label);
			return false;
		}

		if (!FPackageName::IsValidObjectPath(AssetPathAndName))
		{
			OutErrorMessage = FString::Printf(TEXT("%s 不是有效对象路径：%s。示例：/Game/Generated/SphericalTiles/SM_SphericalTile"), Label, *AssetPathAndName);
			return false;
		}

		return true;
	}

	void BuildTileMesh(const FTSTGSphericalTileAssetBuildSettings& Settings, const FHeightSampler& HeightSampler, UE::Geometry::FDynamicMesh3& Mesh)
	{
		using namespace UE::Geometry;

		Mesh.Clear();
		Mesh.EnableAttributes();
		Mesh.Attributes()->SetNumUVLayers(1);
		Mesh.Attributes()->SetNumNormalLayers(1);

		FDynamicMeshUVOverlay* UVOverlay = Mesh.Attributes()->PrimaryUV();
		FDynamicMeshNormalOverlay* NormalOverlay = Mesh.Attributes()->PrimaryNormals();

		const int32 N = FMath::Clamp(Settings.SubdivisionsPerSide, 1, 512);
		const int32 VertexCountPerSide = N + 1;
		const float HalfAngularSizeRadians = FMath::DegreesToRadians(FMath::Clamp(Settings.PatchAngularSizeDegrees, 0.1f, 179.0f)) * 0.5f;
		const float PatchExtent = FMath::Tan(HalfAngularSizeRadians);

		TArray<int32> VertexIds;
		VertexIds.SetNum(VertexCountPerSide * VertexCountPerSide);

		for (int32 Y = 0; Y <= N; ++Y)
		{
			for (int32 X = 0; X <= N; ++X)
			{
				const float U = static_cast<float>(X) / static_cast<float>(N);
				const float V = static_cast<float>(Y) / static_cast<float>(N);

				const double SX = (static_cast<double>(U) - 0.5) * 2.0 * PatchExtent;
				const double SY = (static_cast<double>(V) - 0.5) * 2.0 * PatchExtent;
				const FVector3d Normal = FVector3d(SX, SY, 1.0).GetSafeNormal();

				const FVector2f UvVector(U, V);
				const FVector2f CenterVector(0.5f, 0.5f);
				const FVector2f CenterDelta = UvVector - CenterVector;
				const float CenterDistance01 = FMath::Clamp(FMath::Sqrt(FVector2f::DotProduct(CenterDelta, CenterDelta)) / 0.5f, 0.0f, 1.0f);
				const float EdgeOcclusionOffset = Settings.SphereExtensionAmplitude * (1.0f - 2.0f * CenterDistance01);
				const float HeightOffset = Settings.HeightmapExtensionAmplitude * HeightSampler.Sample(U, V);
				const double Radius = FMath::Max(0.001, static_cast<double>(Settings.BaseRadius + EdgeOcclusionOffset + HeightOffset));

				const int32 VertexId = Mesh.AppendVertex(Normal * Radius);
				VertexIds[Y * VertexCountPerSide + X] = VertexId;
			}
		}

		for (int32 Y = 0; Y < N; ++Y)
		{
			for (int32 X = 0; X < N; ++X)
			{
				const int32 V00 = VertexIds[Y * VertexCountPerSide + X];
				const int32 V10 = VertexIds[Y * VertexCountPerSide + X + 1];
				const int32 V01 = VertexIds[(Y + 1) * VertexCountPerSide + X];
				const int32 V11 = VertexIds[(Y + 1) * VertexCountPerSide + X + 1];

				const int32 T0 = Mesh.AppendTriangle(V00, V01, V10);
				const int32 T1 = Mesh.AppendTriangle(V10, V01, V11);

				if (T0 >= 0 && UVOverlay != nullptr && NormalOverlay != nullptr)
				{
					const FVector2f UV00(static_cast<float>(X) / N, static_cast<float>(Y) / N);
					const FVector2f UV01(static_cast<float>(X) / N, static_cast<float>(Y + 1) / N);
					const FVector2f UV10(static_cast<float>(X + 1) / N, static_cast<float>(Y) / N);
					const int32 E00 = UVOverlay->AppendElement(UV00);
					const int32 E01 = UVOverlay->AppendElement(UV01);
					const int32 E10 = UVOverlay->AppendElement(UV10);
					UVOverlay->SetTriangle(T0, FIndex3i(E00, E01, E10));

					const FVector3f N00 = FVector3f(Mesh.GetVertex(V00).GetSafeNormal());
					const FVector3f N01 = FVector3f(Mesh.GetVertex(V01).GetSafeNormal());
					const FVector3f N10 = FVector3f(Mesh.GetVertex(V10).GetSafeNormal());
					const int32 NE00 = NormalOverlay->AppendElement(N00);
					const int32 NE01 = NormalOverlay->AppendElement(N01);
					const int32 NE10 = NormalOverlay->AppendElement(N10);
					NormalOverlay->SetTriangle(T0, FIndex3i(NE00, NE01, NE10));
				}

				if (T1 >= 0 && UVOverlay != nullptr && NormalOverlay != nullptr)
				{
					const FVector2f UV10(static_cast<float>(X + 1) / N, static_cast<float>(Y) / N);
					const FVector2f UV01(static_cast<float>(X) / N, static_cast<float>(Y + 1) / N);
					const FVector2f UV11(static_cast<float>(X + 1) / N, static_cast<float>(Y + 1) / N);
					const int32 E10 = UVOverlay->AppendElement(UV10);
					const int32 E01 = UVOverlay->AppendElement(UV01);
					const int32 E11 = UVOverlay->AppendElement(UV11);
					UVOverlay->SetTriangle(T1, FIndex3i(E10, E01, E11));

					const FVector3f N10 = FVector3f(Mesh.GetVertex(V10).GetSafeNormal());
					const FVector3f N01 = FVector3f(Mesh.GetVertex(V01).GetSafeNormal());
					const FVector3f N11 = FVector3f(Mesh.GetVertex(V11).GetSafeNormal());
					const int32 NE10 = NormalOverlay->AppendElement(N10);
					const int32 NE01 = NormalOverlay->AppendElement(N01);
					const int32 NE11 = NormalOverlay->AppendElement(N11);
					NormalOverlay->SetTriangle(T1, FIndex3i(NE10, NE01, NE11));
				}
			}
		}
	}

	UMaterialInstanceConstant* CreateMaterialInstance(const FTSTGSphericalTileAssetBuildSettings& Settings, FString& OutErrorMessage)
	{
		if (!Settings.bCreateMaterialInstance)
		{
			return nullptr;
		}

		if (Settings.ParentMaterial == nullptr)
		{
			return nullptr;
		}

		if (!ValidateAssetPath(Settings.MaterialInstanceAssetPathAndName, TEXT("MaterialInstanceAssetPathAndName"), OutErrorMessage))
		{
			return nullptr;
		}

		const FString PackageName = FPackageName::ObjectPathToPackageName(Settings.MaterialInstanceAssetPathAndName);
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		UPackage* Package = CreatePackage(*PackageName);
		if (Package == nullptr)
		{
			OutErrorMessage = FString::Printf(TEXT("无法创建材质实例 Package：%s"), *PackageName);
			return nullptr;
		}

		UMaterialInstanceConstant* MaterialInstance = NewObject<UMaterialInstanceConstant>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		MaterialInstance->SetParentEditorOnly(Settings.ParentMaterial);

		if (Settings.BaseColorTexture != nullptr)
		{
			MaterialInstance->SetTextureParameterValueEditorOnly(Settings.TextureParameterNames.BaseColor, Settings.BaseColorTexture);
		}
		if (Settings.NormalTexture != nullptr)
		{
			MaterialInstance->SetTextureParameterValueEditorOnly(Settings.TextureParameterNames.Normal, Settings.NormalTexture);
		}
		if (Settings.RoughnessTexture != nullptr)
		{
			MaterialInstance->SetTextureParameterValueEditorOnly(Settings.TextureParameterNames.Roughness, Settings.RoughnessTexture);
		}
		if (Settings.HeightTexture != nullptr)
		{
			MaterialInstance->SetTextureParameterValueEditorOnly(Settings.TextureParameterNames.Height, Settings.HeightTexture);
		}

		MaterialInstance->PostEditChange();
		MaterialInstance->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(MaterialInstance);
		return MaterialInstance;
	}
}

UStaticMesh* UTerraSphericalTileGeneratorLibrary::GenerateSphericalTileStaticMeshAsset(
	const FTSTGSphericalTileAssetBuildSettings& Settings,
	FString& OutErrorMessage)
{
	using namespace TerraSphericalTileGenerator;

	OutErrorMessage.Reset();

	if (!ValidateAssetPath(Settings.StaticMeshAssetPathAndName, TEXT("StaticMeshAssetPathAndName"), OutErrorMessage))
	{
		return nullptr;
	}

	FHeightSampler HeightSampler;
	if (!HeightSampler.Initialize(Settings.HeightTexture, OutErrorMessage))
	{
		return nullptr;
	}

	UDynamicMesh* DynamicMesh = NewObject<UDynamicMesh>(GetTransientPackage());
	DynamicMesh->EditMesh(
		[&Settings, &HeightSampler](UE::Geometry::FDynamicMesh3& Mesh)
		{
			BuildTileMesh(Settings, HeightSampler, Mesh);
		},
		EDynamicMeshChangeType::GeneralEdit,
		EDynamicMeshAttributeChangeFlags::MeshTopology |
		EDynamicMeshAttributeChangeFlags::VertexPositions |
		EDynamicMeshAttributeChangeFlags::NormalsTangents |
		EDynamicMeshAttributeChangeFlags::UVs,
		true);

	FGeometryScriptCreateNewStaticMeshAssetOptions StaticMeshOptions;
	StaticMeshOptions.bEnableRecomputeNormals = false;
	StaticMeshOptions.bEnableRecomputeTangents = true;
	StaticMeshOptions.bEnableNanite = Settings.bEnableNanite;
	StaticMeshOptions.bEnableCollision = Settings.bEnableCollision;
	StaticMeshOptions.CollisionMode = Settings.bEnableCollision ? ECollisionTraceFlag::CTF_UseComplexAsSimple : ECollisionTraceFlag::CTF_UseDefault;

	EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
	UStaticMesh* StaticMesh = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
		DynamicMesh,
		Settings.StaticMeshAssetPathAndName,
		StaticMeshOptions,
		Outcome,
		nullptr);

	if (Outcome != EGeometryScriptOutcomePins::Success || StaticMesh == nullptr)
	{
		OutErrorMessage = FString::Printf(TEXT("创建 StaticMesh 资产失败：%s"), *Settings.StaticMeshAssetPathAndName);
		return nullptr;
	}

	if (UMaterialInstanceConstant* MaterialInstance = CreateMaterialInstance(Settings, OutErrorMessage))
	{
		StaticMesh->SetMaterial(0, MaterialInstance);
		StaticMesh->PostEditChange();
		StaticMesh->MarkPackageDirty();
	}
	else if (!OutErrorMessage.IsEmpty())
	{
		return StaticMesh;
	}

	OutErrorMessage = TEXT("OK");
	return StaticMesh;
}
