#pragma once

#include "CoreMinimal.h"

class USubmarineEnvelopeDef;
class USubmarineFunctionalGraph;

class SUB3D_API FSubCompilerMvpFactory
{
public:
	static USubmarineEnvelopeDef* CreateEnvelope(UObject* Outer);
	static USubmarineFunctionalGraph* CreateFunctionalGraph(UObject* Outer);
};
