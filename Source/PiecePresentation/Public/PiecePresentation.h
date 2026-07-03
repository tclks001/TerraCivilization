#pragma once

#include "Modules/ModuleManager.h"

class FPiecePresentationModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
