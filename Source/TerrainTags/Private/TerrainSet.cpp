// Copyright Epic Games, Inc. All Rights Reserved.

#include "TerrainSet.h"

void UTerrainSet::LoadSynchronous()
{
    // 由于 TObjectPtr 不会被 GC 在 cache 持有期间回收，重新构建即可。
    // Reset() 用 Slack 而不是 Empty()——多次调用时 cache 容量稳定。
    LoadedDefsCache.Reset(TerrainDefs.Num());
    for (const TSoftObjectPtr<UTerrainDefinition>& SoftDef : TerrainDefs)
    {
        if (UTerrainDefinition* Def = SoftDef.LoadSynchronous())
        {
            LoadedDefsCache.Add(Def);
        }
    }
}
