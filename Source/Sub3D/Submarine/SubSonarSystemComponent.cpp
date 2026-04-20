#include "SubSonarSystemComponent.h"
#include "Sub3DDebugSettings.h"

#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UObjectIterator.h"
#include "SonarAcousticVolumeComponent.h"
#include "SonarContactTrackerComponent.h"
#include "SonarFieldComponent.h"
#include "SonarNoiseEmitterComponent.h"
#include "SubMovementComponent.h"
#include "SubSonarComponent.h"
#include "SubSonarConfigData.h"
#include "SubmarineBase.h"
#include "SubmarineSystemsComponent.h"
#include "TraversalRouteActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubSonarSystem, Log, All);

namespace
{
constexpr int32 MaxNetworkSafeReplicatedTopoCells = 1024;

float NormalizeAngleDelta(float A, float B)
{
	return FMath::Abs(FMath::FindDeltaAngleDegrees(A, B));
}
}

USubSonarSystemComponent::USubSonarSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void USubSonarSystemComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedSonar = GetOwner() ? GetOwner()->FindComponentByClass<USubSonarComponent>() : nullptr;
	EnsureTrackerComponent();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		SeedTopologyFromRoute();
		RefreshReplicatedTracks();
		RefreshReplicatedTopoWindow();
	}
}

void USubSonarSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	EnsureTrackerComponent();
	if (!ContactTracker)
	{
		return;
	}

	ProcessNewActivePing();
	CommitPendingActivePingObservations();

	const float SafeDeltaTime = FMath::Max(DeltaTime, 0.f);
	NoiseUpdateAccumulator += SafeDeltaTime;
	PassiveSweepAccumulator += SafeDeltaTime;
	TrackerUpdateAccumulator += SafeDeltaTime;

	const float NoiseInterval = SystemConfig ? FMath::Max(0.05f, SystemConfig->SelfNoiseUpdateIntervalS) : 0.2f;
	const float PassiveInterval = SystemConfig ? FMath::Max(0.1f, SystemConfig->PassiveSweepIntervalS) : 0.35f;
	const float TrackerInterval = SystemConfig ? FMath::Max(0.05f, SystemConfig->TrackMaintenanceIntervalS) : 0.15f;

	if (NoiseUpdateAccumulator >= NoiseInterval)
	{
		UpdateSelfNoise(NoiseUpdateAccumulator);
		NoiseUpdateAccumulator = 0.f;
	}

	if (PassiveSweepAccumulator >= PassiveInterval)
	{
		if (bEnablePassiveSweep)
		{
			RunPassiveSweep();
			if (CurrentMode == ESonarMode::TerrainScan || CurrentMode == ESonarMode::PassiveStandard)
			{
				RunTerrainSweep();
			}
		}
		PassiveSweepAccumulator = 0.f;
	}

	if (TrackerUpdateAccumulator >= TrackerInterval)
	{
		const float WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		const FVector SubLoc = OwnerActor->GetActorLocation();
		ContactTracker->UpdateTracker(TrackerUpdateAccumulator, WorldTime, SubLoc);
		AgeAndPruneTopology(TrackerUpdateAccumulator);
		RefreshReplicatedTracks();
		RefreshReplicatedTopoWindow();
		TrackerUpdateAccumulator = 0.f;
	}
}

void USubSonarSystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USubSonarSystemComponent, CurrentMode);
	DOREPLIFETIME(USubSonarSystemComponent, RangePresetIndex);
	DOREPLIFETIME(USubSonarSystemComponent, FocusBearingDeg);
	DOREPLIFETIME(USubSonarSystemComponent, SelfNoiseState);
	DOREPLIFETIME(USubSonarSystemComponent, AcousticClutterLevel);
	DOREPLIFETIME(USubSonarSystemComponent, bSignalUnstable);
	DOREPLIFETIME(USubSonarSystemComponent, LastProcessedPingTimestamp);
	DOREPLIFETIME(USubSonarSystemComponent, ReplicatedTracks);
	DOREPLIFETIME(USubSonarSystemComponent, ReplicatedTopoWindow);
}

void USubSonarSystemComponent::SetSonarMode(ESonarMode NewMode)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	CurrentMode = NewMode;
	NotifyRuntimeUpdated();
}

void USubSonarSystemComponent::SetFocusBearing(float NewBearingDeg)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	FocusBearingDeg = FMath::UnwindDegrees(NewBearingDeg);
	NotifyRuntimeUpdated();
}

void USubSonarSystemComponent::SetRangePresetIndex(int32 NewIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const int32 MaxIndex = (SystemConfig && SystemConfig->DisplayRangePresetsCm.Num() > 0)
		? (SystemConfig->DisplayRangePresetsCm.Num() - 1)
		: 0;
	RangePresetIndex = FMath::Clamp(NewIndex, 0, MaxIndex);
	RefreshReplicatedTopoWindow();
	NotifyRuntimeUpdated();
}

int32 USubSonarSystemComponent::GetRangePresetCount() const
{
	return (SystemConfig && SystemConfig->DisplayRangePresetsCm.Num() > 0)
		? SystemConfig->DisplayRangePresetsCm.Num()
		: 1;
}

float USubSonarSystemComponent::GetRangePresetValueCm(int32 PresetIndex) const
{
	if (SystemConfig && SystemConfig->DisplayRangePresetsCm.IsValidIndex(PresetIndex))
	{
		return FMath::Max(2000.f, SystemConfig->DisplayRangePresetsCm[PresetIndex]);
	}

	return GetDisplayRangeCm();
}

int32 USubSonarSystemComponent::ResolveRangePresetIndexFromNormalized(float Normalized01) const
{
	const int32 PresetCount = GetRangePresetCount();
	if (PresetCount <= 1)
	{
		return 0;
	}

	const float Clamped = FMath::Clamp(Normalized01, 0.f, 1.f);
	return FMath::Clamp(FMath::RoundToInt(Clamped * static_cast<float>(PresetCount - 1)), 0, PresetCount - 1);
}

void USubSonarSystemComponent::MarkPriorityTrack(int32 TrackId, bool bPriority)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	EnsureTrackerComponent();
	if (!ContactTracker)
	{
		return;
	}

	ContactTracker->MarkPriorityTrack(TrackId, bPriority);
	RefreshReplicatedTracks();
	NotifyRuntimeUpdated();
}

float USubSonarSystemComponent::GetCurrentRangeCm() const
{
	return GetDisplayRangeCm();
}

float USubSonarSystemComponent::GetDisplayRangeCm() const
{
	if (SystemConfig && SystemConfig->DisplayRangePresetsCm.IsValidIndex(RangePresetIndex))
	{
		return FMath::Max(2000.f, SystemConfig->DisplayRangePresetsCm[RangePresetIndex]);
	}
	return 15000.f;
}

float USubSonarSystemComponent::GetRuntimeScanRangeCm() const
{
	if (!SystemConfig)
	{
		return 15000.f;
	}

	switch (CurrentMode)
	{
	case ESonarMode::PassiveFocusSector:
		return FMath::Max(2000.f, SystemConfig->PassiveFocusScanRangeCm);
	case ESonarMode::TerrainScan:
		return FMath::Max(2000.f, SystemConfig->TerrainScanRangeCm);
	case ESonarMode::ActivePing:
		return CachedSonar ? FMath::Max(2000.f, CachedSonar->PingMaxRangeCm) : 15000.f;
	case ESonarMode::PassiveStandard:
	default:
		return FMath::Max(2000.f, SystemConfig->PassiveStandardScanRangeCm);
	}
}

bool USubSonarSystemComponent::IsPingReady() const
{
	if (!CachedSonar)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float Elapsed = World->GetTimeSeconds() - CachedSonar->GetLastPingTime();
	return Elapsed >= CachedSonar->PingCooldownS;
}

void USubSonarSystemComponent::ForceRebuildTopologyFromRoute()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	RuntimeTopoCells.Reset();
	bRouteSeedApplied = false;
	SeedTopologyFromRoute();
	RefreshReplicatedTopoWindow();
	NotifyRuntimeUpdated();
}

void USubSonarSystemComponent::OnRep_RuntimeState()
{
	NotifyRuntimeUpdated();
}

void USubSonarSystemComponent::OnRep_Tracks()
{
	NotifyRuntimeUpdated();
}

void USubSonarSystemComponent::OnRep_TopoWindow()
{
	NotifyRuntimeUpdated();
}

void USubSonarSystemComponent::EnsureTrackerComponent()
{
	if (ContactTracker)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	ContactTracker = OwnerActor->FindComponentByClass<USonarContactTrackerComponent>();
	if (!ContactTracker)
	{
		ContactTracker = NewObject<USonarContactTrackerComponent>(OwnerActor, TEXT("SonarContactTracker"));
		if (ContactTracker)
		{
			ContactTracker->RegisterComponent();
		}
	}
}

void USubSonarSystemComponent::UpdateSelfNoise(float DeltaTime)
{
	const ASubmarineBase* Submarine = Cast<ASubmarineBase>(GetOwner());
	const USubMovementComponent* Movement = Submarine ? Submarine->SubMovement : nullptr;
	const USubmarineSystemsComponent* Systems = Submarine ? Submarine->Systems : nullptr;

	const float SpeedCmS = Movement ? Movement->Velocity.Size() : 0.f;
	const float SpeedNormCmS = (SystemConfig && SystemConfig->SelfNoiseSpeedNormCmS > 0.f)
		? SystemConfig->SelfNoiseSpeedNormCmS
		: 600.f;
	const bool bPumpActive = Systems ? Systems->GetCommandState().bPumpActive : false;
	const bool bBallastActive = Systems ? Systems->GetCommandState().bBallastsActive : false;

	SelfNoiseState.PropulsionNoise = FMath::Clamp(SpeedCmS / SpeedNormCmS, 0.f, 1.25f);
	SelfNoiseState.PumpNoise = bPumpActive ? 0.18f : 0.f;
	SelfNoiseState.BallastNoise = bBallastActive ? 0.12f : 0.f;
	SelfNoiseState.SystemNoise = (CachedSonar && CachedSonar->IsContinuousPingActive()) ? 0.15f : 0.02f;
	SelfNoiseState.AggregateNoise =
		SelfNoiseState.PropulsionNoise +
		SelfNoiseState.PumpNoise +
		SelfNoiseState.BallastNoise +
		SelfNoiseState.DamageNoise +
		SelfNoiseState.WeaponNoise +
		SelfNoiseState.SystemNoise;

	if (GetDefault<USub3DDebugSettings>()->bLogSonar && DeltaTime > 0.f)
	{
		UE_LOG(LogSubSonarSystem, Verbose, TEXT("SelfNoise | Aggregate=%.2f"), SelfNoiseState.AggregateNoise);
	}
}

void USubSonarSystemComponent::RunPassiveSweep()
{
	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	if (!World || !OwnerActor || !ContactTracker)
	{
		return;
	}

	TArray<FSonarDetectionSample> Samples;
	const FVector SubLocation = OwnerActor->GetActorLocation();
	const float MaxRange = GetRuntimeScanRangeCm();
	const float MinScore = SystemConfig ? FMath::Clamp(SystemConfig->PassiveMinDetectionScore, 0.01f, 1.f) : 0.12f;

	float AmbientNoise = 0.f;
	float Clutter = 0.f;
	float PassiveModifier = 1.f;
	float ActivePingDistortion = 0.f;
	ApplyAcousticVolumeModifiers(AmbientNoise, Clutter, PassiveModifier, ActivePingDistortion);

	const float FocusHalfAngle = SystemConfig ? FMath::Max(5.f, SystemConfig->PassiveFocusHalfAngleDeg) : 25.f;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == OwnerActor || Candidate->IsActorBeingDestroyed())
		{
			continue;
		}

		USonarNoiseEmitterComponent* Emitter = Candidate->FindComponentByClass<USonarNoiseEmitterComponent>();
		if (!Emitter || !Emitter->IsSonarRelevant())
		{
			continue;
		}

		const FVector ToTarget = Emitter->GetEmissionLocation() - SubLocation;
		const float DistanceCm = ToTarget.Size();
		if (DistanceCm <= KINDA_SMALL_NUMBER || DistanceCm > MaxRange)
		{
			continue;
		}

		const float BearingDeg = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
		float SectorModifier = 1.f;
		if (CurrentMode == ESonarMode::PassiveFocusSector)
		{
			const float Delta = NormalizeAngleDelta(BearingDeg, FocusBearingDeg);
			SectorModifier = FMath::Clamp(1.f - (Delta / FocusHalfAngle), 0.1f, 1.f);
		}

		const float RangeFactor = FMath::Clamp(1.f - (DistanceCm / MaxRange), 0.05f, 1.f);
		const float DetectionScore =
			Emitter->GetCurrentNoiseStrength() *
			RangeFactor *
			SectorModifier *
			FMath::Max(0.1f, PassiveModifier) /
			(1.f + SelfNoiseState.AggregateNoise + AmbientNoise + (Clutter * 0.5f));

		if (DetectionScore < MinScore)
		{
			continue;
		}

		FSonarDetectionSample& Sample = Samples.AddDefaulted_GetRef();
		const float Uncertainty = FMath::Clamp(1.f - DetectionScore, 0.f, 1.f);
		const float AcousticUncertainty = FMath::Clamp(Uncertainty + (Clutter * 0.35f) + (AmbientNoise * 0.20f), 0.f, 1.f);
		const FVector Jitter = FVector(
			FMath::FRandRange(-1.f, 1.f),
			FMath::FRandRange(-1.f, 1.f),
			FMath::FRandRange(-0.2f, 0.2f)) * (450.f * AcousticUncertainty);

		Sample.EstimatedWorldLocation = Emitter->GetEmissionLocation() + Jitter;
		Sample.BearingDeg = BearingDeg;
		Sample.EstimatedDistanceCm = DistanceCm;
		Sample.RawStrength = DetectionScore;
		Sample.ConfidenceDelta = FMath::Clamp((DetectionScore * 0.35f) / (1.f + (Clutter * 0.6f) + (AmbientNoise * 0.3f)), 0.02f, 0.25f);
		Sample.SuggestedClass = Emitter->ContactClass;
		Sample.bFromActivePing = false;
		Sample.Timestamp = World->GetTimeSeconds();
	}

	if (Samples.Num() > 0)
	{
		ContactTracker->DecayPerSecond = SystemConfig ? FMath::Max(0.f, SystemConfig->TrackDecayPerSecond) : ContactTracker->DecayPerSecond;
		ContactTracker->LostThresholdS = SystemConfig ? FMath::Max(0.f, SystemConfig->LostThresholdS) : ContactTracker->LostThresholdS;
		ContactTracker->LostRetentionS = SystemConfig ? FMath::Max(0.f, SystemConfig->LostRetentionS) : ContactTracker->LostRetentionS;
		ContactTracker->ConsumeSamples(Samples, World->GetTimeSeconds(), SubLocation);
	}
}

void USubSonarSystemComponent::RunTerrainSweep()
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World)
	{
		return;
	}

	const FVector Origin = OwnerActor->GetActorLocation();
	const bool bWeakPassiveTerrain = (CurrentMode == ESonarMode::PassiveStandard);
	const float ScanRange = bWeakPassiveTerrain
		? (SystemConfig ? FMath::Max(1000.f, SystemConfig->PassiveStandardTerrainRangeCm) : 4500.f)
		: (SystemConfig ? FMath::Max(2000.f, SystemConfig->TerrainScanRangeCm) : 12000.f);
	const int32 RayCount = 42;
	const float VerticalBias = bWeakPassiveTerrain
		? 0.85f
		: (SystemConfig ? FMath::Clamp(SystemConfig->PassiveTerrainWeight, 0.5f, 3.f) : 1.2f);
	float AmbientNoise = 0.f;
	float Clutter = 0.f;
	float ActivePingDistortion = 0.f;
	float UnusedPassiveModifier = 1.f;
	ApplyAcousticVolumeModifiers(AmbientNoise, Clutter, UnusedPassiveModifier, ActivePingDistortion);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SonarTerrainSweep), false, OwnerActor);
	QueryParams.bTraceComplex = false;

	for (int32 RayIndex = 0; RayIndex < RayCount; ++RayIndex)
	{
		const float Angle = (2.f * PI * static_cast<float>(RayIndex)) / static_cast<float>(RayCount);
		const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), FMath::Sin(Angle * 0.5f) * 0.2f * VerticalBias);
		const FVector End = Origin + Dir.GetSafeNormal() * ScanRange;
		const ECollisionChannel TraceChannel = CachedSonar
			? static_cast<ECollisionChannel>(CachedSonar->PingTraceChannel.GetValue())
			: ECC_Visibility;

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Origin, End, TraceChannel, QueryParams) && Hit.bBlockingHit)
		{
			const float Distortion = FMath::Clamp(ActivePingDistortion + (Clutter * 0.35f), 0.f, 1.f);
			const FVector Jitter(
				FMath::FRandRange(-1.f, 1.f),
				FMath::FRandRange(-1.f, 1.f),
				FMath::FRandRange(-0.15f, 0.15f));
			const float JitterScale = bWeakPassiveTerrain ? 70.f : 120.f;
			const FVector DistortedPoint = Hit.ImpactPoint + (Jitter * (JitterScale * Distortion));
			const float BaseConfidence = bWeakPassiveTerrain ? 0.10f : 0.22f;
			const float Confidence = FMath::Clamp(BaseConfidence / (1.f + (Clutter * 0.8f) + (AmbientNoise * 0.25f)), bWeakPassiveTerrain ? 0.03f : 0.05f, BaseConfidence);
			const float Occupancy = bWeakPassiveTerrain ? 0.35f : 0.9f;
			AddTopologyObservation(DistortedPoint, Occupancy, Confidence, false);
		}
	}
}

void USubSonarSystemComponent::ProcessNewActivePing()
{
	if (!CachedSonar || !GetWorld())
	{
		return;
	}

	const float LatestPing = CachedSonar->GetLastPingTime();
	if (LatestPing <= LastProcessedPingTimestamp + KINDA_SMALL_NUMBER)
	{
		return;
	}

	float AmbientNoise = 0.f;
	float Clutter = 0.f;
	float ActivePingDistortion = 0.f;
	float UnusedPassiveModifier = 1.f;
	ApplyAcousticVolumeModifiers(AmbientNoise, Clutter, UnusedPassiveModifier, ActivePingDistortion);

	for (const FSonarHitPoint& HitPoint : CachedSonar->SonarPoints)
	{
		if (FMath::Abs(HitPoint.PingTimestamp - LatestPing) > 0.005f)
		{
			continue;
		}

		const float Distortion = FMath::Clamp(ActivePingDistortion + (Clutter * 0.25f), 0.f, 1.f);
		const FVector Jitter(
			FMath::FRandRange(-1.f, 1.f),
			FMath::FRandRange(-1.f, 1.f),
			FMath::FRandRange(-0.12f, 0.12f));
		const FVector DistortedPoint = HitPoint.WorldLocation + (Jitter * (180.f * Distortion));
		const float Confidence = FMath::Clamp(1.f - (Distortion * 0.45f) - (AmbientNoise * 0.10f), 0.35f, 1.f);

		FPendingActiveTopoObservation& PendingObservation = PendingActiveTopoObservations.AddDefaulted_GetRef();
		PendingObservation.WorldLocation = DistortedPoint;
		PendingObservation.Occupancy01 = 1.f;
		PendingObservation.Confidence01 = Confidence;
		PendingObservation.RevealTime = HitPoint.PingTimestamp + (HitPoint.DistanceCm / FMath::Max(CachedSonar->PropagationSpeedCmS, 1.f));
	}

	LastProcessedPingTimestamp = LatestPing;
}

void USubSonarSystemComponent::CommitPendingActivePingObservations()
{
	UWorld* World = GetWorld();
	if (!World || PendingActiveTopoObservations.Num() == 0)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	bool bCommittedAny = false;

	for (int32 Index = PendingActiveTopoObservations.Num() - 1; Index >= 0; --Index)
	{
		const FPendingActiveTopoObservation& PendingObservation = PendingActiveTopoObservations[Index];
		if (Now + KINDA_SMALL_NUMBER < PendingObservation.RevealTime)
		{
			continue;
		}

		AddTopologyObservation(
			PendingObservation.WorldLocation,
			PendingObservation.Occupancy01,
			PendingObservation.Confidence01,
			false);
		PendingActiveTopoObservations.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		bCommittedAny = true;
	}

	if (bCommittedAny)
	{
		RefreshReplicatedTopoWindow();
		NotifyRuntimeUpdated();
	}
}

void USubSonarSystemComponent::AddTopologyObservation(const FVector& WorldLocation, float Occupancy01, float Confidence01, bool bFromSeed)
{
	const float CellSize = SystemConfig ? FMath::Max(100.f, SystemConfig->TopologyCellSizeCm) : 500.f;
	const int32 GridX = FMath::FloorToInt(WorldLocation.X / CellSize);
	const int32 GridY = FMath::FloorToInt(WorldLocation.Y / CellSize);
	const int64 Key = MakeTopoKey(GridX, GridY);

	FRuntimeTopoCell& Cell = RuntimeTopoCells.FindOrAdd(Key);
	if (Cell.Confidence01 <= KINDA_SMALL_NUMBER)
	{
		Cell.HeightCm = WorldLocation.Z;
	}
	else
	{
		const float Blend = FMath::Clamp(Confidence01, 0.05f, 1.f);
		Cell.HeightCm = FMath::Lerp(Cell.HeightCm, WorldLocation.Z, Blend);
	}

	Cell.Occupancy01 = FMath::Clamp(FMath::Max(Cell.Occupancy01, Occupancy01), 0.f, 1.f);
	Cell.Confidence01 = FMath::Clamp(FMath::Max(Cell.Confidence01 * 0.96f, Confidence01), 0.f, 1.f);
	Cell.LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	Cell.bFromSeed = Cell.bFromSeed || bFromSeed;
}

void USubSonarSystemComponent::AgeAndPruneTopology(float DeltaTime)
{
	const float LifeTime = SystemConfig ? FMath::Max(1.f, SystemConfig->TopologyCellLifetimeS) : 45.f;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	for (auto It = RuntimeTopoCells.CreateIterator(); It; ++It)
	{
		FRuntimeTopoCell& Cell = It.Value();
		const float Age = FMath::Max(0.f, Now - Cell.LastUpdateTime);
		if (Age > LifeTime)
		{
			It.RemoveCurrent();
			continue;
		}

		Cell.Confidence01 = FMath::Clamp(Cell.Confidence01 - (0.015f * DeltaTime), 0.f, 1.f);
	}
}

void USubSonarSystemComponent::RefreshReplicatedTracks()
{
	if (!ContactTracker)
	{
		ReplicatedTracks.Reset();
		return;
	}

	ReplicatedTracks = ContactTracker->GetTracks();
}

void USubSonarSystemComponent::RefreshReplicatedTopoWindow()
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		ReplicatedTopoWindow.Reset();
		return;
	}

	const float CellSize = SystemConfig ? FMath::Max(100.f, SystemConfig->TopologyCellSizeCm) : 500.f;
	const int32 HalfWindow = SystemConfig ? FMath::Max(4, SystemConfig->TopologyHalfWindowCells) : 44;
	const int32 ConfiguredMaxCells = SystemConfig ? FMath::Max(64, SystemConfig->MaxReplicatedTopoCells) : 1500;
	const int32 MaxCells = FMath::Min(ConfiguredMaxCells, MaxNetworkSafeReplicatedTopoCells);
	if (ConfiguredMaxCells > MaxNetworkSafeReplicatedTopoCells && !bLoggedReplicatedTopoCap)
	{
		UE_LOG(
			LogSubSonarSystem,
			Warning,
			TEXT("[%s] MaxReplicatedTopoCells=%d exceeds the safe replicated sonar topology budget. Clamping to %d to avoid oversized net bunches."),
			*GetNameSafe(OwnerActor),
			ConfiguredMaxCells,
			MaxNetworkSafeReplicatedTopoCells);
		bLoggedReplicatedTopoCap = true;
	}

	const FVector SubLoc = OwnerActor->GetActorLocation();
	const int32 SubCellX = FMath::FloorToInt(SubLoc.X / CellSize);
	const int32 SubCellY = FMath::FloorToInt(SubLoc.Y / CellSize);

	TArray<FSonarTopoCell> Cells;
	Cells.Reserve(FMath::Min(MaxCells, RuntimeTopoCells.Num()));

	for (const TPair<int64, FRuntimeTopoCell>& Pair : RuntimeTopoCells)
	{
		const int32 GridX = static_cast<int32>(Pair.Key >> 32);
		const int32 GridY = static_cast<int32>(static_cast<uint32>(Pair.Key & 0xffffffff));
		if (FMath::Abs(GridX - SubCellX) > HalfWindow || FMath::Abs(GridY - SubCellY) > HalfWindow)
		{
			continue;
		}

		const FRuntimeTopoCell& RuntimeCell = Pair.Value;
		FSonarTopoCell& NetCell = Cells.AddDefaulted_GetRef();
		NetCell.GridX = GridX;
		NetCell.GridY = GridY;
		NetCell.HeightDm = FMath::RoundToInt(RuntimeCell.HeightCm / 10.f);
		NetCell.Occupancy01Byte = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(RuntimeCell.Occupancy01 * 255.f), 0, 255));
		NetCell.Confidence01Byte = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(RuntimeCell.Confidence01 * 255.f), 0, 255));
		NetCell.bFromSeed = RuntimeCell.bFromSeed;
	}

	if (Cells.Num() > MaxCells)
	{
		Cells.Sort([](const FSonarTopoCell& A, const FSonarTopoCell& B)
		{
			if (A.Confidence01Byte != B.Confidence01Byte)
			{
				return A.Confidence01Byte > B.Confidence01Byte;
			}
			return A.Occupancy01Byte > B.Occupancy01Byte;
		});
		Cells.SetNum(MaxCells, EAllowShrinking::No);
	}

	ReplicatedTopoWindow = MoveTemp(Cells);
}

void USubSonarSystemComponent::ApplyAcousticVolumeModifiers(float& OutAmbientNoiseBias, float& OutClutterBias, float& OutPassiveModifier, float& OutActivePingDistortion) const
{
	OutAmbientNoiseBias = 0.f;
	OutClutterBias = 0.f;
	OutPassiveModifier = 1.f;
	OutActivePingDistortion = 0.f;

	const AActor* OwnerActor = GetOwner();
	const UWorld* World = GetWorld();
	if (!OwnerActor || !World)
	{
		return;
	}

	const FVector SubLocation = OwnerActor->GetActorLocation();
	for (TObjectIterator<USonarAcousticVolumeComponent> It; It; ++It)
	{
		const USonarAcousticVolumeComponent* VolumeComp = *It;
		if (!VolumeComp || VolumeComp->GetWorld() != World || !VolumeComp->IsLocationInside(SubLocation))
		{
			continue;
		}

		OutAmbientNoiseBias += VolumeComp->AmbientNoiseBias;
		OutClutterBias += VolumeComp->ClutterBias;
		OutPassiveModifier *= FMath::Clamp(VolumeComp->PassiveDetectionModifier, 0.1f, 10.f);
		OutActivePingDistortion += VolumeComp->ActivePingDistortion;
	}

	USubSonarSystemComponent* MutableThis = const_cast<USubSonarSystemComponent*>(this);
	MutableThis->AcousticClutterLevel = FMath::Clamp(OutClutterBias + (OutAmbientNoiseBias * 0.5f) + FMath::Max(0.f, 1.f - OutPassiveModifier), 0.f, 2.5f);
	MutableThis->bSignalUnstable = (MutableThis->AcousticClutterLevel >= 0.35f) || (OutActivePingDistortion >= 0.20f);
}

void USubSonarSystemComponent::SeedTopologyFromRoute()
{
	if (bRouteSeedApplied || !bEnableRouteCoarseSeed)
	{
		return;
	}

	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	if (!World || !OwnerActor)
	{
		return;
	}

	ATraversalRouteActor* RouteActor = nullptr;
	for (TActorIterator<ATraversalRouteActor> It(World); It; ++It)
	{
		RouteActor = *It;
		break;
	}

	if (!RouteActor)
	{
		return;
	}

	USonarFieldComponent* SonarField = RouteActor->GetSonarFieldComponent();
	if (!SonarField)
	{
		return;
	}

	TArray<FVector> SeedPoints;
	const float SeedRange = SystemConfig ? FMath::Max(2000.f, SystemConfig->RouteSeedRangeCm) : 18000.f;
	const int32 SeedBudget = SystemConfig ? FMath::Max(128, SystemConfig->TopologySeedPointBudget) : 1600;
	SonarField->CollectCoarseWaterSurfacePoints(OwnerActor->GetActorLocation(), SeedRange, SeedBudget, SeedPoints);

	for (const FVector& Point : SeedPoints)
	{
		AddTopologyObservation(Point, 0.65f, 0.35f, true);
	}

	bRouteSeedApplied = true;
	if (GetDefault<USub3DDebugSettings>()->bLogSonar)
	{
		UE_LOG(LogSubSonarSystem, Log, TEXT("Route seed applied | Points=%d"), SeedPoints.Num());
	}
}

void USubSonarSystemComponent::NotifyRuntimeUpdated()
{
	BP_OnSonarSystemUpdated();
}

int64 USubSonarSystemComponent::MakeTopoKey(int32 GridX, int32 GridY)
{
	const uint64 PackedX = static_cast<uint32>(GridX);
	const uint64 PackedY = static_cast<uint32>(GridY);
	return static_cast<int64>((PackedX << 32) | PackedY);
}
