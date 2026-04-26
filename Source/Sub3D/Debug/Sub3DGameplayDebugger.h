#pragma once

#include "CoreMinimal.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayDebuggerCategory.h"

class APlayerController;
class AActor;

/**
 * Sub3D Gameplay Debugger category. Activate in PIE with F1, then cycle to "Sub3D".
 * Lists all submarines, all crews (with role/embark state/grid pose), and the GameMode
 * bootstrap/run phase. Per-viewport so each PIE client sees its own view of the world.
 */
class FSub3DGameplayDebuggerCategory : public FGameplayDebuggerCategory
{
public:
	FSub3DGameplayDebuggerCategory();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();
};

#endif // WITH_GAMEPLAY_DEBUGGER
