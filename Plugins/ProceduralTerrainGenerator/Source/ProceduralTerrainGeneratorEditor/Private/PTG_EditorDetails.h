// Copyright 2021 VICTOR HERNANDEZ MOLPECERES (Rockam). All rights reserved.

#pragma once

#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"

class APtgManager;
class IDetailsView; // forward declaration

class FPTG_EditorDetails : public IDetailCustomization
{
public:

    /** Makes a new instance of this detail layout class for a specific detail view requesting it. */
    static TSharedRef<IDetailCustomization> MakeInstance();

    /** IDetailCustomization interface. */
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

    /** Handle clicking on the Convert to Static Mesh button. */
    FReply ClickedOnConvertToStaticMesh();

    /** Handle clicking on the Create Terrain Heightmap button. */
    FReply ClickedOnCreateTerrainHeightmap();

    /** Returns if the Convert to Static Mesh button enabled. */
    bool ConvertToStaticMeshEnabled() const;

    /** Returns if the Create Terrain Heightmap button enabled. */
    bool CreateTerrainHeightmapEnabled() const; 

    /** Util to get the Procedural Terrain we want to convert to static mesh. */
    APtgManager* GetProceduralTerrain() const;

    /** Cached array of selected objects (fallback). */
    TArray<TWeakObjectPtr<UObject>> SelectedObjectsList;

private:

    bool SaveImage(const FString& fullFilePath, int32 resolution, const TArray<FColor>& imagePixels);
    bool CreateDirectory(FString FolderToMake);

    // Hold a weak pointer to the details view so we can re-query selected objects when needed
    TWeakPtr<IDetailsView> DetailsViewPtr;
};
