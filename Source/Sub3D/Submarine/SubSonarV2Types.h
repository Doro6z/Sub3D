#pragma once

#include "CoreMinimal.h"
#include "SubSonarV2Types.generated.h"

UENUM(BlueprintType)
enum class ESonarMode : uint8
{
	PassiveStandard  UMETA(DisplayName = "Passive Standard"),
	PassiveFocusSector UMETA(DisplayName = "Passive Focus Sector"),
	ActivePing       UMETA(DisplayName = "Active Ping"),
	TerrainScan      UMETA(DisplayName = "Terrain Scan")
};

UENUM(BlueprintType)
enum class ESonarTrackState : uint8
{
	None        UMETA(DisplayName = "None"),
	Suspected   UMETA(DisplayName = "Suspected"),
	Tracked     UMETA(DisplayName = "Tracked"),
	Classified  UMETA(DisplayName = "Classified"),
	Confirmed   UMETA(DisplayName = "Confirmed"),
	Lost        UMETA(DisplayName = "Lost")
};

UENUM(BlueprintType)
enum class ESonarContactClass : uint8
{
	Unknown           UMETA(DisplayName = "Unknown"),
	EnvironmentStatic UMETA(DisplayName = "Environment Static"),
	StructureActive   UMETA(DisplayName = "Structure Active"),
	MobileUnknown     UMETA(DisplayName = "Mobile Unknown"),
	MobileNeutral     UMETA(DisplayName = "Mobile Neutral"),
	MobileThreat      UMETA(DisplayName = "Mobile Threat"),
	Anomaly           UMETA(DisplayName = "Anomaly")
};

USTRUCT(BlueprintType)
struct FSonarDetectionSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	FVector EstimatedWorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float BearingDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float EstimatedDistanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float RawStrength = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float ConfidenceDelta = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	ESonarContactClass SuggestedClass = ESonarContactClass::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	bool bFromActivePing = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float Timestamp = 0.f;
};

USTRUCT(BlueprintType)
struct FSonarTrack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	int32 TrackId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	ESonarTrackState State = ESonarTrackState::None;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	ESonarContactClass ProbableClass = ESonarContactClass::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	FVector_NetQuantize100 EstimatedWorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	FVector_NetQuantize10 EstimatedVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float BearingDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float EstimatedDistanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float Confidence = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float LastUpdateTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float LostTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	bool bPriority = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	bool bLikelyHostile = false;
};

USTRUCT(BlueprintType)
struct FSonarSelfNoiseState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float PropulsionNoise = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float PumpNoise = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float BallastNoise = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float DamageNoise = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float WeaponNoise = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float SystemNoise = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float AggregateNoise = 0.f;
};

USTRUCT(BlueprintType)
struct FSonarTopoCell
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	int32 GridX = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	int32 GridY = 0;

	// Height quantized in decimeters for compact replication.
	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	int32 HeightDm = 0;

	// 0..255 occupancy confidence.
	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	uint8 Occupancy01Byte = 0;

	// 0..255 signal confidence.
	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	uint8 Confidence01Byte = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	bool bFromSeed = false;
};
