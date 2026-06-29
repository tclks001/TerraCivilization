// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TerrainDefinition.h"
#include "TerrainSet.generated.h"

/**
 * UTerrainSet
 *
 * 一组 UTerrainDefinition 引用的 DataAsset 集合；FWorldGenSettings::TerrainSet 通过
 * SoftObjectPtr 显式指向（关卡可携带不同 TerrainSet 实现星球风格切换）。
 *
 * 软引用（TSoftObjectPtr）原因：避免编辑器打开 SettingsActor 时瀑布式硬加载所有 Def。
 * LoadSynchronous() 在 Step_ClassifyBiomes 入口处显式调用一次，把全部 Def 加载到 cache。
 *
 * 详见 Docs/W4_BiomeClassification.md §2.1。
 */
UCLASS(BlueprintType)
class TERRAINTAGS_API UTerrainSet : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /**
     * 持有的 UTerrainDefinition 引用。
     * 顺序 = LayerIndex 顺序（约定：第 i 项 LayerIndex = i），方便编辑器一眼对照；
     * 不强制——运行时按 Def->LayerIndex 字段为准（不是数组下标）。
     */
    UPROPERTY(EditAnywhere, Category = "Set")
    TArray<TSoftObjectPtr<UTerrainDefinition>> TerrainDefs;

    /** 同步加载全部 TerrainDefs 到 LoadedDefsCache；幂等，可多次调用。 */
    void LoadSynchronous();

    /** 获取已 cache 的 Def 列表。调用方需先调用 LoadSynchronous()。 */
    const TArray<UTerrainDefinition*>& GetLoadedDefs() const { return LoadedDefsCache; }

private:
    /** Transient cache：避免运行期反复 LoadSynchronous（GC 持有，避免 Def 被回收）。 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UTerrainDefinition>> LoadedDefsCache;
};
