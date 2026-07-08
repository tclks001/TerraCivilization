#pragma once

#include "Modules/ModuleManager.h"

class FNpcMcpModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterTools_();
    void UnregisterTools_();
    void StartServerIfRequested_();

private:
    TArray<TSharedRef<struct IModelContextProtocolTool>> RegisteredTools;
};
