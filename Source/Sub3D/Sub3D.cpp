// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sub3D.h"
#include "Modules/ModuleManager.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#include "Debug/Sub3DGameplayDebugger.h"
#endif

class FSub3DModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();

#if WITH_GAMEPLAY_DEBUGGER
		IGameplayDebugger& Debugger = IGameplayDebugger::Get();
		Debugger.RegisterCategory(
			TEXT("Sub3D"),
			IGameplayDebugger::FOnGetCategory::CreateStatic(&FSub3DGameplayDebuggerCategory::MakeInstance),
			EGameplayDebuggerCategoryState::EnabledInGameAndSimulate,
			5);
		Debugger.NotifyCategoriesChanged();
#endif
	}

	virtual void ShutdownModule() override
	{
#if WITH_GAMEPLAY_DEBUGGER
		if (IGameplayDebugger::IsAvailable())
		{
			IGameplayDebugger::Get().UnregisterCategory(TEXT("Sub3D"));
		}
#endif
		FDefaultGameModuleImpl::ShutdownModule();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FSub3DModule, Sub3D, "Sub3D");

DEFINE_LOG_CATEGORY(LogSub3D)
