#pragma once

#include "Modules/ModuleInterface.h"

// LEGACY (Phase 7A, 2026-04-10) — FP usage of the Sub3DBake pipeline is
// superseded by the SubmarineGenerator path (Phase 5D). This module stays
// present for the Proto03/04 authoring flow only. Do not add new FP-path
// consumers. Will be pruned from the FP build in Phase 7B once the generator
// path is the only init route.
class FSub3DBakeModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};

