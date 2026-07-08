#include "TerraNpcMcpGameplayBridge.h"

#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "TerraGameplayContainer.h"

namespace
{
    FCriticalSection GTerraNpcMcpGameplayBridgeMutex;
    const FTerraGameplayContainer* GTerraNpcMcpGameplayContainer = nullptr;
}

void FTerraNpcMcpGameplayBridge::RegisterGameplayContainer(const FTerraGameplayContainer* InGameplayContainer)
{
    FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
    GTerraNpcMcpGameplayContainer = InGameplayContainer;
}

void FTerraNpcMcpGameplayBridge::UnregisterGameplayContainer(const FTerraGameplayContainer* InGameplayContainer)
{
    FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
    if (GTerraNpcMcpGameplayContainer == InGameplayContainer)
    {
        GTerraNpcMcpGameplayContainer = nullptr;
    }
}

const FTerraGameplayContainer* FTerraNpcMcpGameplayBridge::GetGameplayContainer()
{
    FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
    return GTerraNpcMcpGameplayContainer;
}
