#pragma once

#include "Modules/ModuleManager.h"

class FGameplayModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
