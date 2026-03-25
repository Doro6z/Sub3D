#include "RuntimeSyncDiagnosticsSettings.h"

URuntimeSyncDiagnosticsSettings::URuntimeSyncDiagnosticsSettings()
{
	DefaultSampleRateHz = 10.f;
	ErrorWarningThresholdCm = 10.f;
	ErrorCriticalThresholdCm = 50.f;
	ParentDeltaWarningCm = 5.f;
	ParentDeltaYawWarningDeg = 2.f;
	bEnableOverlayByDefault = false;
	bEnableDebugDrawByDefault = false;
	DefaultCsvDirectory = TEXT("Saved/RuntimeSyncDiagnostics");
}
