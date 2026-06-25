#pragma once
#include "Modules/ModuleManager.h"

class GRID_API FGridModule : public IModuleInterface
{
public:
    virtual void StartupModule() override; // 模块加载时调用
    virtual void ShutdownModule() override; // 模块卸载时调用
};