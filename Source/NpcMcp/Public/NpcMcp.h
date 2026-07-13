#pragma once

#include "Modules/ModuleManager.h"

class NPCMCP_API FNpcMcpModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    static void RefreshInteractiveToolAvailability();

private:
    void RegisterTools_();
    void UnregisterTools_();
    void StartServerIfRequested_();
    void RefreshInteractiveToolAvailability_();

private:
    TArray<TSharedRef<struct IModelContextProtocolTool>> RegisteredTools;
    TMap<FString, TSharedRef<struct IModelContextProtocolTool>> AllToolsByName;
    TSet<FString> RegisteredToolNames;
};
