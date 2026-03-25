#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RuntimeSyncDiagnosticsSettings.generated.h"

/**
 * Settings for the Runtime Sync Diagnostics plugin.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Runtime Sync Diagnostics"))
class RUNTIMESYNCDIAGNOSTICS_API URuntimeSyncDiagnosticsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URuntimeSyncDiagnosticsSettings();

	/** The default sampling rate in Hz for diagnostics. */
	UPROPERTY(Config, EditAnywhere, Category="Diagnostics")
	float DefaultSampleRateHz;

	/** Threshold in centimeters to trigger a warning for sync errors. */
	UPROPERTY(Config, EditAnywhere, Category="Diagnostics")
	float ErrorWarningThresholdCm;

	/** Threshold in centimeters to trigger a critical error for sync errors. */
	UPROPERTY(Config, EditAnywhere, Category="Diagnostics")
	float ErrorCriticalThresholdCm;

	/** Warning threshold in centimeters for parent frame delta changes. */
	UPROPERTY(Config, EditAnywhere, Category="Diagnostics")
	float ParentDeltaWarningCm;

	/** Warning threshold in degrees for parent frame yaw changes. */
	UPROPERTY(Config, EditAnywhere, Category="Diagnostics")
	float ParentDeltaYawWarningDeg;

	/** Whether the on-screen overlay is enabled by default. */
	UPROPERTY(Config, EditAnywhere, Category="Visuals")
	bool bEnableOverlayByDefault;

	/** Whether debug drawing is enabled by default. */
	UPROPERTY(Config, EditAnywhere, Category="Visuals")
	bool bEnableDebugDrawByDefault;

	/** Default directory path for exporting CSV reports. */
	UPROPERTY(Config, EditAnywhere, Category="Export")
	FString DefaultCsvDirectory;
};
