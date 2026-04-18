#pragma once

#include "Modules/ModuleInterface.h"

class FSub3DRuntimeModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};

