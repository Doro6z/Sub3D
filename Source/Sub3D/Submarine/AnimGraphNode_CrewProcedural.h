#pragma once

#include "CoreMinimal.h"

#if WITH_EDITORONLY_DATA

#include "AnimGraphNode_Base.h"
#include "AnimNode_CrewProcedural.h"
#include "AnimGraphNode_CrewProcedural.generated.h"

UCLASS()
class SUB3D_API UAnimGraphNode_CrewProcedural : public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_CrewProcedural Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override
	{
		return FText::FromString(TEXT("Crew Procedural Animation"));
	}

	virtual FText GetTooltipText() const override
	{
		return FText::FromString(TEXT("Applies all procedural bone transforms from SubCrewAnimInstance."));
	}

	virtual FString GetNodeCategory() const override
	{
		return TEXT("Sub3D");
	}
};

#endif // WITH_EDITORONLY_DATA
