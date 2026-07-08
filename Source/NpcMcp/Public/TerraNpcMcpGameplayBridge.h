#pragma once

#include "CoreMinimal.h"

class FTerraGameplayContainer;

class NPCMCP_API FTerraNpcMcpGameplayBridge
{
public:
    static void RegisterGameplayContainer(const FTerraGameplayContainer* InGameplayContainer);
    static void UnregisterGameplayContainer(const FTerraGameplayContainer* InGameplayContainer);
    static const FTerraGameplayContainer* GetGameplayContainer();
};
