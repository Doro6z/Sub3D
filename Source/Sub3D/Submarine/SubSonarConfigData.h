#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SubSonarV2Types.h"
#include "SubSonarConfigData.generated.h"

UCLASS(BlueprintType)
class SUB3D_API USonarSystemConfigData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar|Ranges")
	TArray<float> RangePresetsCm = { 9000.f, 15000.f, 22000.f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar|Passive")
	float PassiveSweepIntervalS = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar|Passive")
	float PassiveMinDetectionScore = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar|Passive")
	float PassiveFocusHalfAngleDeg = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar|Passive")
	float PassiveTerrainWeight = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar|Tracker")
	float TrackMaintenanceIntervalS = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar|Tracker")
	float TrackDecayPerSecond = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar|Tracker")
	float LostThresholdS = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar|Tracker")
	float LostRetentionS = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar|Noise")
	float SelfNoiseUpdateIntervalS = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar|Noise")
	float SelfNoiseSpeedNormCmS = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar|Topo")
	float TopologyCellSizeCm = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar|Topo")
	int32 TopologyHalfWindowCells = 44;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar|Topo")
	int32 MaxReplicatedTopoCells = 1500;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar|Topo")
	float TopologyCellLifetimeS = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar|Topo")
	int32 TopologySeedPointBudget = 1600;
};

UCLASS(BlueprintType)
class SUB3D_API USonarSignatureData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar")
	ESonarContactClass ContactClass = ESonarContactClass::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar")
	float BaseNoiseStrength = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar")
	float ActiveReflectivity = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar")
	float SignalStability = 0.5f;
};

UCLASS(BlueprintType)
class SUB3D_API USonarEnvironmentProfileData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar")
	float AmbientNoiseBias = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar")
	float ClutterBias = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar")
	float PassiveDetectionModifier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar")
	float ActivePingDistortion = 0.f;
};

UCLASS(BlueprintType)
class SUB3D_API USonarUpgradeData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar")
	float RangeMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar")
	float ClassificationMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar")
	float SelfNoiseResistance = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar")
	float ActivePingCooldownMultiplier = 1.f;
};
