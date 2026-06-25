// Copyright 2021 VICTOR HERNANDEZ MOLPECERES (Rockam). All rights reserved.

#include "PTG_EditorDetails.h"
#include "RuntimeMeshComponent.h"
#include "RuntimeMesh.h"
#include "RuntimeMeshProvider.h"
#include "PtgManager.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "IDetailCustomization.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "RawMesh.h"
#include "ImageUtils.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "PhysicsEngine/BodySetup.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopedSlowTask.h"
#include "ScopedTransaction.h"
#include "IDetailsView.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogPTGEditorDetails, Log, All);

#define LOCTEXT_NAMESPACE "PTG_Details"

TSharedRef<IDetailCustomization> FPTG_EditorDetails::MakeInstance()
{
    return MakeShareable(new FPTG_EditorDetails);
}

void FPTG_EditorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    IDetailCategoryBuilder& category = DetailBuilder.EditCategory("PTG - Editor actions");

    const FText ConvertToStaticMeshText = LOCTEXT("ConvertToStaticMesh", "Convert Terrain to Static Mesh");
    const FText CreateTerrainHeightmapText = LOCTEXT("CreateTerrainHeightmap", "Create Terrain Heightmap");

    // Store details view to re-query selection later (more robust than caching a snapshot)
    DetailsViewPtr = DetailBuilder.GetDetailsViewSharedPtr();
    SelectedObjectsList = DetailsViewPtr.IsValid() ? DetailsViewPtr.Pin()->GetSelectedObjects() : TArray<TWeakObjectPtr<UObject>>();

    // Add the Convert Terrain to StaticMesh button
    category.AddCustomRow(ConvertToStaticMeshText, false)
        .NameContent()
        [
            SNullWidget::NullWidget
        ]
        .ValueContent()
        .VAlign(VAlign_Center)
        .HAlign(EHorizontalAlignment::HAlign_Fill)
        .MaxDesiredWidth(250.0f)
        [
            SNew(SButton)
            .VAlign(VAlign_Center)
            .HAlign(EHorizontalAlignment::HAlign_Center)
            .ToolTipText(LOCTEXT("ConvertToStaticMeshTooltip", "Create a new Static Mesh asset using current geometry from generated terrain mesh. Does not modify instance."))
            .OnClicked(this, &FPTG_EditorDetails::ClickedOnConvertToStaticMesh)
            .IsEnabled(this, &FPTG_EditorDetails::ConvertToStaticMeshEnabled)
            .Content()
            [
                SNew(STextBlock)
                .Text(ConvertToStaticMeshText)
            ]
        ];

    {
        // Add all the default properties
        TArray<TSharedRef<IPropertyHandle>> AllProperties;
        const bool bSimpleProperties = true;
        const bool bAdvancedProperties = false;

        // Add all properties in the category in order
        category.GetDefaultProperties(AllProperties, bSimpleProperties, bAdvancedProperties);
        for (auto& Property : AllProperties)
        {
            category.AddProperty(Property);
        }
    }

    // Add the Create Terrain Heightmap button
    category.AddCustomRow(ConvertToStaticMeshText, false)
        .NameContent()
        [
            SNullWidget::NullWidget
        ]
        .ValueContent()
        .VAlign(VAlign_Center)
            .HAlign(EHorizontalAlignment::HAlign_Fill)
        .MaxDesiredWidth(250.0f)
        [
            SNew(SButton)
            .VAlign(VAlign_Center)
            .HAlign(EHorizontalAlignment::HAlign_Center)
            .ToolTipText(LOCTEXT("CreateTerrainHeightmapTooltip", "Create a new .png image using current vertex data from the generated terrain mesh. Only available on Plane terrain shape."))
            .OnClicked(this, &FPTG_EditorDetails::ClickedOnCreateTerrainHeightmap)
            .IsEnabled(this, &FPTG_EditorDetails::CreateTerrainHeightmapEnabled)
            .Content()
            [
                SNew(STextBlock)
                .Text(CreateTerrainHeightmapText)
            ]
        ];
}

APtgManager* FPTG_EditorDetails::GetProceduralTerrain() const
{
    APtgManager* proceduralTerrain = nullptr;

    // Re-query the currently selected objects from the details view if available
    TArray<TWeakObjectPtr<UObject>> CurrentSelection;
    if (TSharedPtr<IDetailsView> DetailsView = DetailsViewPtr.Pin())
    {
        CurrentSelection = DetailsView->GetSelectedObjects();
    }
    else
    {
        CurrentSelection = SelectedObjectsList; // fallback
    }

    // Find procedural terrain
    for (const TWeakObjectPtr<UObject>& Object : CurrentSelection)
    {
        proceduralTerrain = Cast<APtgManager>(Object.Get());

        // See if this one is good.
        if (proceduralTerrain != nullptr && !proceduralTerrain->IsTemplate())
        {
            return proceduralTerrain;
        }
    }

    return nullptr;
}

bool FPTG_EditorDetails::ConvertToStaticMeshEnabled() const
{
    const APtgManager* proceduralTerrain = GetProceduralTerrain();

    return proceduralTerrain != nullptr && proceduralTerrain->GetProcMeshTerrainComp() != nullptr;
}

bool FPTG_EditorDetails::CreateTerrainHeightmapEnabled() const
{
    const APtgManager* proceduralTerrain = GetProceduralTerrain();

    return proceduralTerrain != nullptr && proceduralTerrain->GetShape() == EPtgProcMeshShapes::Plane;
}

FReply FPTG_EditorDetails::ClickedOnConvertToStaticMesh()
{
    // Find procedural terrain RuntimeMeshComp
    const APtgManager* proceduralTerrain = GetProceduralTerrain();
    URuntimeMeshComponent* runtimeMeshComp = IsValid(proceduralTerrain) ? proceduralTerrain->GetProcMeshTerrainComp() : nullptr;
    UPtgRuntimeMesh* runtimeMesh = (runtimeMeshComp != nullptr) ? runtimeMeshComp->GetRuntimeMesh() : nullptr;
    URuntimeMeshProvider* meshProvider = (runtimeMesh != nullptr) ? runtimeMesh->GetProviderPtr() : nullptr;

    if (runtimeMeshComp == nullptr || runtimeMesh == nullptr || meshProvider == nullptr)
    {
        UE_LOG(LogPTGEditorDetails, Warning, TEXT("ConvertToStaticMesh: Missing runtime mesh or provider"));
        return FReply::Handled();
    }

    const FString NewNameSuggestion = FString(TEXT("PTG_Mesh"));
    FString PackageName = FString(TEXT("/Game/PTG_Converted_Meshes/")) + NewNameSuggestion;
    FString Name;
    FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
    AssetToolsModule.Get().CreateUniqueAssetName(PackageName, TEXT(""), PackageName, Name);

    TSharedPtr<SDlgPickAssetPath> PickAssetPathWidget =
        SNew(SDlgPickAssetPath)
        .Title(LOCTEXT("ConvertToStaticMeshPickName", "Choose new StaticMesh location"))
        .DefaultAssetPath(FText::FromString(PackageName));

    if (!PickAssetPathWidget.IsValid())
    {
        UE_LOG(LogPTGEditorDetails, Error, TEXT("ConvertToStaticMesh: Failed to create asset path widget"));
        return FReply::Handled();
    }

    if (PickAssetPathWidget->ShowModal() == EAppReturnType::Ok)
    {
        // Get the full name of where we want to create the physics asset.
        FString UserPackageName = PickAssetPathWidget->GetFullAssetPath().ToString();
        FName MeshName(*FPackageName::GetLongPackageAssetName(UserPackageName));

        // Check if the user inputed a valid asset name, if they did not, give it the generated default name
        if (MeshName == NAME_None)
        {
            // Use the defaults that were already generated.
            UserPackageName = PackageName;
            MeshName = *Name;
        }

        // Create the package to save the static mesh
        UPackage* Package = CreatePackage(*UserPackageName);
        check(Package);

        // Create StaticMesh object
        UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, MeshName, RF_Public | RF_Standalone);
        StaticMesh->InitResources();

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 26
        StaticMesh->LightingGuid = FGuid::NewGuid();
#else
        StaticMesh->SetLightingGuid(FGuid::NewGuid());
#endif

        // Copy the material slots
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 26
        TArray<FStaticMaterial>& Materials = StaticMesh->StaticMaterials;
#else
        TArray<FStaticMaterial>& Materials = StaticMesh->GetStaticMaterials();
#endif
        const auto RMCMaterialSlots = runtimeMesh->GetMaterialSlots();
        Materials.SetNum(RMCMaterialSlots.Num());
        for (int32 Index = 0; Index < RMCMaterialSlots.Num(); Index++)
        {
            UMaterialInterface* Mat = runtimeMeshComp->OverrideMaterials.Num() > Index ? runtimeMeshComp->OverrideMaterials[Index] : nullptr;
            Mat = Mat ? Mat : RMCMaterialSlots[Index].Material;
            Materials[Index] = FStaticMaterial(Mat, RMCMaterialSlots[Index].SlotName);
        }

        const auto LODConfig = runtimeMesh->GetCopyOfConfiguration();

        // Compute an approximate total work amount for the progress dialog
        int32 TotalWork = 0;
        for (const auto& LOD : LODConfig)
        {
            TotalWork += FMath::Max(1, LOD.Sections.Num());
        }

        FScopedSlowTask SlowTask(FMath::Max(1, TotalWork), LOCTEXT("ConvertingToStaticMeshProgress", "Converting Procedural Terrain to Static Mesh"));
        SlowTask.MakeDialog(true);

        // Use a transaction to allow the editor to track the change (optional)
        FScopedTransaction Transaction(LOCTEXT("ConvertToStaticMeshTransaction", "Convert Procedural Terrain to Static Mesh"));

        for (int32 LODIndex = 0; LODIndex < LODConfig.Num(); LODIndex++)
        {
            const auto& LOD = LODConfig[LODIndex];

            // Raw mesh data we are filling in
            FRawMesh RawMesh;
            bool bUseHighPrecisionTangents = false;
            bool bUseFullPrecisionUVs = false;
            int32 MaxUVs = 1;

            int32 VertexBase = 0;

            for (const auto& SectionEntry : LOD.Sections)
            {
                const int32 SectionId = SectionEntry.Key;
                const auto& Section = SectionEntry.Value;

                // Here we need to direct query the provider the mesh is using
                FRuntimeMeshRenderableMeshData MeshData(true, true, Section.NumTexCoords, true);
                if (meshProvider->GetSectionMeshForLOD(LODIndex, SectionEntry.Key, MeshData))
                {
                    MaxUVs = FMath::Max<int32>(MaxUVs, Section.NumTexCoords);

                    // Copy the vertex positions (reserve to reduce reallocations)
                    int32 NumVertices = MeshData.Positions.Num();
                    RawMesh.VertexPositions.Reserve(RawMesh.VertexPositions.Num() + NumVertices);
                    for (int32 Index = 0; Index < NumVertices; Index++)
                    {
                        RawMesh.VertexPositions.Add(FVector3f((MeshData.Positions.GetPosition(Index))));
                    }

                    // Copy wedges (reserve sizes)
                    int32 NumTris = MeshData.Triangles.Num();
                    RawMesh.WedgeIndices.Reserve(RawMesh.WedgeIndices.Num() + NumTris);
                    RawMesh.WedgeTangentX.Reserve(RawMesh.WedgeTangentX.Num() + NumTris);
                    RawMesh.WedgeTangentY.Reserve(RawMesh.WedgeTangentY.Num() + NumTris);
                    RawMesh.WedgeTangentZ.Reserve(RawMesh.WedgeTangentZ.Num() + NumTris);
                    RawMesh.WedgeColors.Reserve(RawMesh.WedgeColors.Num() + NumTris);

                    for (int32 Index = 0; Index < NumTris; Index++)
                    {
                        int32 VertexIndex = MeshData.Triangles.GetVertexIndex(Index);
                        RawMesh.WedgeIndices.Add(VertexIndex + VertexBase);

                        FVector3f TangentX, TangentY, TangentZ;
                        MeshData.Tangents.GetTangents(VertexIndex, TangentX, TangentY, TangentZ);
                        RawMesh.WedgeTangentX.Add(FVector3f(TangentX));
                        RawMesh.WedgeTangentY.Add(FVector3f(TangentY));
                        RawMesh.WedgeTangentZ.Add(FVector3f(TangentZ));

                        // Reserve UV channel arrays as needed and copy
                        for (int32 UVIndex = 0; UVIndex < Section.NumTexCoords; UVIndex++)
                        {
                            RawMesh.WedgeTexCoords[UVIndex].Reserve(RawMesh.WedgeTexCoords[UVIndex].Num() + 1);
                            RawMesh.WedgeTexCoords[UVIndex].Add(FVector2f(MeshData.TexCoords.GetTexCoord(VertexIndex, UVIndex)));
                        }

                        RawMesh.WedgeColors.Add(MeshData.Colors.GetColor(VertexIndex));
                    }

                    // Copy face info
                    for (int32 TriIdx = 0; TriIdx < NumTris / 3; TriIdx++)
                    {
                        RawMesh.FaceMaterialIndices.Add(SectionId);
                        RawMesh.FaceSmoothingMasks.Add(0); // Assume ignored as bRecomputeNormals is false
                    }

                    // Update offset for creating one big index/vertex buffer
                    VertexBase += NumVertices;

                    // Update progress for this section
                    SlowTask.EnterProgressFrame(1);
                }
            }

            // Fill out the UV channels to the same length as the indices
            for (int32 Index = 0; Index < MaxUVs; Index++)
            {
                RawMesh.WedgeTexCoords[Index].SetNumZeroed(RawMesh.WedgeIndices.Num());
            }

            // If we got some valid data.
            if (RawMesh.IsValid())
            {
                // Add source to new StaticMesh
                FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
                SrcModel.BuildSettings.bRecomputeNormals = false;
                SrcModel.BuildSettings.bRecomputeTangents = false;
                SrcModel.BuildSettings.bRemoveDegenerates = true;
                SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = bUseHighPrecisionTangents;
                SrcModel.BuildSettings.bUseFullPrecisionUVs = bUseFullPrecisionUVs;
                SrcModel.BuildSettings.bGenerateLightmapUVs = true;
                SrcModel.BuildSettings.SrcLightmapIndex = 0;
                SrcModel.BuildSettings.DstLightmapIndex = 1;
                SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);

                SrcModel.ScreenSize = LOD.Properties.ScreenSize;

                // Set the materials used for this static mesh
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 26
                int32 NumMaterials = StaticMesh->StaticMaterials.Num();
#else
                int32 NumMaterials = StaticMesh->GetStaticMaterials().Num();
#endif

                // Set up the SectionInfoMap to enable collision
                for (int32 SectionIdx = 0; SectionIdx < NumMaterials; SectionIdx++)
                {
                    FMeshSectionInfoMap& SectionInfoMap = StaticMesh->GetSectionInfoMap();
                    FMeshSectionInfo Info = SectionInfoMap.Get(LODIndex, SectionIdx);
                    Info.MaterialIndex = SectionIdx;
                    // Turn on collision on top LOD only
                    Info.bEnableCollision = LODIndex == 0;
                    SectionInfoMap.Set(LODIndex, SectionIdx, Info);
                }
            }
        }

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 26
        StaticMesh->StaticMaterials = Materials;
#else
        StaticMesh->SetStaticMaterials(Materials);
#endif

        // Configure body setup for working collision.
        StaticMesh->CreateBodySetup();
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 26
        StaticMesh->BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
#else
        StaticMesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
#endif

        // Build mesh from source
        StaticMesh->Build(false);

        // Make package dirty.
        StaticMesh->MarkPackageDirty();

        StaticMesh->PostEditChange();

        // Notify asset registry of new asset
        FAssetRegistryModule::AssetCreated(StaticMesh);

        UE_LOG(LogPTGEditorDetails, Log, TEXT("Successfully converted procedural terrain to static mesh: %s"), *UserPackageName);
    }

    return FReply::Handled();
}

FReply FPTG_EditorDetails::ClickedOnCreateTerrainHeightmap()
{
    if (const APtgManager* proceduralTerrain = GetProceduralTerrain())
    {
        const TArray<float>& vertexHeightData = proceduralTerrain->GetVertexHeightData();
        const int32 numVertices = vertexHeightData.Num();

        if (numVertices > 0)
        {
            const int32 resolution = FMath::RoundToInt(FMath::Sqrt((float)numVertices));

            // Validate that resolution is exact
            if (resolution <= 0 || resolution * resolution != numVertices)
            {
                const FString error = TEXT("Cannot create heightmap: vertex count is not a perfect square.");
                UE_LOG(LogPTGEditorDetails, Warning, TEXT("%s"), *error);
                if (GEngine != nullptr) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, error);
                return FReply::Handled();
            }

            const FString windowTitle = FString(TEXT("Choose new Heightmap location"));
            const FString suggestedLocation = FString(TEXT(""));
            const FString newNameSuggestion = FString(TEXT("PTG_Heightmap.png"));
            const FString format = FString(TEXT("png Image|*.png"));
            TArray<FString> filenames;

            if (FDesktopPlatformModule::Get()->SaveFileDialog(nullptr, windowTitle, suggestedLocation, newNameSuggestion, format, 0, filenames) && filenames.Num() > 0)
            {
                // Fill pixels with vertex data
                const FVector2D heightRange = FVector2D(proceduralTerrain->GetLowestGeneratedHeight(), proceduralTerrain->GetHighestGeneratedHeight());
                const FVector2D colorRange = FVector2D(0.0f, 1.0f);
                TArray<FColor> pixels;
                pixels.Reserve(numVertices);

                for (const float currentHeight : vertexHeightData)
                {
                    const float rgbColor = FMath::GetMappedRangeValueClamped(heightRange, colorRange, currentHeight);
                    pixels.Emplace(FLinearColor(rgbColor, rgbColor, rgbColor).ToFColor(true));
                }

                // Save heightmap values to png image
                SaveImage(filenames[0], resolution, pixels);
            }
        }
    }

    return FReply::Handled();
}

bool FPTG_EditorDetails::SaveImage(const FString& fullFilePath, int32 resolution, const TArray<FColor>& imagePixels)
{
    FString errorString = TEXT("Success creating heightmap!");

    if (fullFilePath.Len() < 1)
    {
        errorString = TEXT("Error in SaveImage: no file path");
        UE_LOG(LogPTGEditorDetails, Error, TEXT("%s"), *errorString);
        return false;
    }

    // Ensure target directory exists or can be created
    FString newAbsoluteFolderPath = FPaths::GetPath(fullFilePath);
    FPaths::NormalizeDirectoryName(newAbsoluteFolderPath);

    if (!CreateDirectory(newAbsoluteFolderPath))
    {
        errorString = TEXT("Error in SaveImage: folder could not be created, check read/write permissions~ ") + newAbsoluteFolderPath;
        UE_LOG(LogPTGEditorDetails, Error, TEXT("%s"), *errorString);
        return false;
    }

    if (imagePixels.Num() != resolution * resolution)
    {
        errorString = TEXT("Error in SaveImage: resolution x resolution is not equal to the total pixel array length!");
        UE_LOG(LogPTGEditorDetails, Error, TEXT("%s"), *errorString);
        return false;
    }

    // Preserve path and change extension to .png
    FString FinalFilename = FPaths::ChangeExtension(fullFilePath, TEXT("png"));
    TArray64<uint8> CompressedPNG;

    FImageUtils::PNGCompressImageArray(resolution, resolution, imagePixels, CompressedPNG);

    bool saveResult = FFileHelper::SaveArrayToFile(CompressedPNG, *FinalFilename);

    if (!saveResult)
    {
        errorString = TEXT("Error in SaveImage: saving of file to disk did not succeed for File IO reasons");
        UE_LOG(LogPTGEditorDetails, Error, TEXT("%s"), *errorString);
    }

    if (GEngine != nullptr) GEngine->AddOnScreenDebugMessage(-1, 5.0f, saveResult ? FColor::Green : FColor::Red, errorString);

    return saveResult;
}

bool FPTG_EditorDetails::CreateDirectory(FString FolderToMake)
{
    if (FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*FolderToMake))
    {
        return true;
    }

    // Normalize the directory name
    FPaths::NormalizeDirectoryName(FolderToMake);

    // Use platform file helper to create the full tree (safer and simpler)
    if (FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FolderToMake))
    {
        return true;
    }

    return false;
}

#undef LOCTEXT_NAMESPACE
