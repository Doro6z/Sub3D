#pragma once

#include "CoreMinimal.h"

class USubmarineEnvelopeDef;
class USubmarineFunctionalGraph;

// LEGACY (Phase 7A, 2026-04-10) — MVP bootstrap factory used by early
// SubCompiler tests and the Proto03/04 path. The SubmarineGenerator pipeline
// (Phase 5A-D) replaces this. Do not add new callers. Will be removed in
// Phase 7B once the generator path is the only init route.
class SUB3D_API FSubCompilerMvpFactory
{
public:
	static USubmarineEnvelopeDef* CreateEnvelope(UObject* Outer);
	static USubmarineFunctionalGraph* CreateFunctionalGraph(UObject* Outer);
};
