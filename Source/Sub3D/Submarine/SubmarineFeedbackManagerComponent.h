#pragma once

#include "SubmarineFeedbackDirectorComponent.h"
#include "SubmarineFeedbackManagerComponent.generated.h"

UCLASS(meta = (DeprecatedNode, DeprecationMessage = "Use USubmarineFeedbackDirectorComponent instead."))
class SUB3D_API USubmarineFeedbackManagerComponent : public USubmarineFeedbackDirectorComponent
{
	GENERATED_BODY()
};
