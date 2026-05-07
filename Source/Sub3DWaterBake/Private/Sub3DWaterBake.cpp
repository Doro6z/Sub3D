#include "Sub3DWaterBake.h"

DEFINE_LOG_CATEGORY(LogWaterBake);

void FSub3DWaterBakeModule::StartupModule()
{
	UE_LOG(LogWaterBake, Display, TEXT("Sub3DWaterBake module loaded"));
}

void FSub3DWaterBakeModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FSub3DWaterBakeModule, Sub3DWaterBake)
