#include "Utils/RuntimeSyncRoleUtils.h"
#include "Engine/EngineTypes.h"

FString FRuntimeSyncRoleUtils::GetRoleString(ENetRole Role)
{
	switch (Role)
	{
	case ROLE_None:
		return TEXT("None");
	case ROLE_SimulatedProxy:
		return TEXT("SimulatedProxy");
	case ROLE_AutonomousProxy:
		return TEXT("AutonomousProxy");
	case ROLE_Authority:
		return TEXT("Authority");
	case ROLE_MAX:
		return TEXT("MAX");
	default:
		return TEXT("Unknown");
	}
}
