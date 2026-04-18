#include "SubmarineFeedbackDirectorComponent.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundBase.h"
#include "SubCrewCharacter.h"
#include "SubFloodComponent.h"
#include "SubHullComponent.h"
#include "SubmarineAlarmBeacon.h"
#include "SubmarineBase.h"
#include "SubmarineFloodAudioAnchor.h"
#include "SubmarineFeedbackProfile.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubmarineFeedback, Log, All);

USubmarineFeedbackDirectorComponent::USubmarineFeedbackDirectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USubmarineFeedbackDirectorComponent::BeginPlay()
{
	Super::BeginPlay();

	RefreshAlarmBeacons();
	RefreshSpatialAudioAnchors();

	ASubmarineBase* Submarine = GetOwningSubmarine();

	SubFlood = Submarine ? Submarine->SubFlood : nullptr;
	if (SubFlood)
	{
		if (SubFlood->IsInitialized())
		{
			ActivateSubFloodPath();
		}
		else
		{
			SubFlood->OnFloodInitialized.AddDynamic(this, &USubmarineFeedbackDirectorComponent::HandleFloodInitialized);
		}
	}

	// Breaches and flow fields always come from SubHull (SubFlood does not manage damage).
	if (USubHullComponent* SubHull = GetOwningHull())
	{
		SubHull->OnBreachesUpdated.AddDynamic(this, &USubmarineFeedbackDirectorComponent::HandleBreachesUpdated);
		SubHull->OnFlowFieldsUpdated.AddDynamic(this, &USubmarineFeedbackDirectorComponent::HandleFlowFieldsUpdated);
		LatestBreaches = SubHull->GetBreachClusters();
		LatestFlowFields = SubHull->GetFlowFields();
		UpdateLeakAudioRuntime();
	}
}

void USubmarineFeedbackDirectorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SubFlood)
	{
		SubFlood->OnFloodInitialized.RemoveDynamic(this, &USubmarineFeedbackDirectorComponent::HandleFloodInitialized);
		SubFlood->OnFloodStateUpdated.RemoveDynamic(this, &USubmarineFeedbackDirectorComponent::HandleSubFloodUpdated);
	}

	if (USubHullComponent* SubHull = GetOwningHull())
	{
		SubHull->OnBreachesUpdated.RemoveDynamic(this, &USubmarineFeedbackDirectorComponent::HandleBreachesUpdated);
		SubHull->OnFlowFieldsUpdated.RemoveDynamic(this, &USubmarineFeedbackDirectorComponent::HandleFlowFieldsUpdated);
	}

	Super::EndPlay(EndPlayReason);
}

void USubmarineFeedbackDirectorComponent::DispatchHullImpactFeedback(const FVector& WorldLocation, float Damage, float RadiusCm)
{
	const USubmarineFeedbackProfile* Profile = FeedbackProfile;
	UWorld* World = GetWorld();
	if (!World || !Profile || Damage <= 0.f)
	{
		return;
	}

	const float InnerRadiusCm = FMath::Max(0.f, Profile->HullImpactInnerRadiusCm);
	const float OuterRadiusCm = FMath::Max3(InnerRadiusCm + 1.f, Profile->HullImpactOuterRadiusCm, RadiusCm * 2.f);
	const float ShakeScale = FMath::Clamp(
		Damage / FMath::Max(1.f, Profile->HullImpactShakeDamageDivisor),
		Profile->HullImpactMinShakeScale,
		FMath::Max(Profile->HullImpactMinShakeScale, Profile->HullImpactMaxShakeScale));
	const float SoundVolume = FMath::Clamp(
		Damage / FMath::Max(1.f, Profile->HullImpactSoundDamageDivisor),
		Profile->HullImpactMinSoundVolume,
		FMath::Max(Profile->HullImpactMinSoundVolume, Profile->HullImpactMaxSoundVolume));

	int32 EligibleControllerCount = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (!PlayerController || !IsControllerEligible(PlayerController))
		{
			continue;
		}

		const FVector ListenerLocation = PlayerController->PlayerCameraManager
			? PlayerController->PlayerCameraManager->GetCameraLocation()
			: PlayerController->GetFocalLocation();
		const float DistanceCm = FVector::Dist(ListenerLocation, WorldLocation);
		const float DistanceAlpha = ComputeDistanceAlpha(DistanceCm, InnerRadiusCm, OuterRadiusCm);
		if (DistanceAlpha <= 0.f)
		{
			continue;
		}

		++EligibleControllerCount;

		if (Profile->HullImpactShake)
		{
			PlayerController->ClientStartCameraShake(Profile->HullImpactShake, ShakeScale * DistanceAlpha);
		}

		if (Profile->HullImpactSound)
		{
			PlayerController->ClientPlaySound(Profile->HullImpactSound, SoundVolume * DistanceAlpha, 1.f);
		}
	}

	if (bLogFeedbackDispatch)
	{
		UE_LOG(
			LogSubmarineFeedback,
			Log,
			TEXT("HullImpactFeedback | Sub=%s | Damage=%.1f | Radius=%.1f | EligibleControllers=%d"),
			*GetNameSafe(GetOwningSubmarine()),
			Damage,
			OuterRadiusCm,
			EligibleControllerCount);
	}
}

void USubmarineFeedbackDirectorComponent::RefreshAlarmBeacons()
{
	AlarmBeacons.Reset();
	RefreshSpatialAudioAnchors();
}

void USubmarineFeedbackDirectorComponent::RefreshSpatialAudioAnchors()
{
	AlarmBeacons.Reset();
	FloodAudioAnchors.Reset();

	ASubmarineBase* Submarine = GetOwningSubmarine();
	if (!Submarine)
	{
		return;
	}

	TArray<AActor*> AttachedActors;
	Submarine->GetAttachedActors(AttachedActors, true);
	for (AActor* AttachedActor : AttachedActors)
	{
		if (ASubmarineAlarmBeacon* AlarmBeacon = Cast<ASubmarineAlarmBeacon>(AttachedActor))
		{
			AlarmBeacons.Add(AlarmBeacon);
			continue;
		}

		if (ASubmarineFloodAudioAnchor* FloodAnchor = Cast<ASubmarineFloodAudioAnchor>(AttachedActor))
		{
			FloodAudioAnchors.Add(FloodAnchor);
		}
	}
}

float USubmarineFeedbackDirectorComponent::ComputeDistanceAlpha(float DistanceCm, float InnerRadiusCm, float OuterRadiusCm)
{
	if (DistanceCm <= InnerRadiusCm)
	{
		return 1.f;
	}

	if (OuterRadiusCm <= InnerRadiusCm || DistanceCm >= OuterRadiusCm)
	{
		return 0.f;
	}

	return 1.f - ((DistanceCm - InnerRadiusCm) / (OuterRadiusCm - InnerRadiusCm));
}

void USubmarineFeedbackDirectorComponent::HandleSubFloodUpdated(const TArray<FCompartmentState>& InStates)
{
	UpdateFloodAlarmFromStates(InStates);
	UpdateFloodInteriorAudioAnchorsFromStates(InStates);
}

void USubmarineFeedbackDirectorComponent::HandleFloodInitialized()
{
	if (!SubFlood || !SubFlood->IsInitialized())
	{
		return;
	}

	SubFlood->OnFloodInitialized.RemoveDynamic(this, &USubmarineFeedbackDirectorComponent::HandleFloodInitialized);
	ActivateSubFloodPath();
}

void USubmarineFeedbackDirectorComponent::ActivateSubFloodPath()
{
	SubFlood->OnFloodStateUpdated.AddDynamic(this, &USubmarineFeedbackDirectorComponent::HandleSubFloodUpdated);

	TArray<FCompartmentState> States;
	SubFlood->ExportCompartmentStates(States);
	UpdateFloodAlarmFromStates(States);
	UpdateFloodInteriorAudioAnchorsFromStates(States);
}

void USubmarineFeedbackDirectorComponent::HandleBreachesUpdated(const TArray<FBreachClusterState>& Breaches)
{
	LatestBreaches = Breaches;
	UpdateLeakAudioRuntime();
}

void USubmarineFeedbackDirectorComponent::HandleFlowFieldsUpdated(const TArray<FBreachFlowField>& FlowFields)
{
	LatestFlowFields = FlowFields;
	UpdateLeakAudioRuntime();
}

void USubmarineFeedbackDirectorComponent::EnsureFallbackAlarmAudio()
{
	if (FallbackAlarmAudio)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	FallbackAlarmAudio = NewObject<UAudioComponent>(Owner, TEXT("FloodAlarmFallbackAudio"));
	if (!FallbackAlarmAudio)
	{
		return;
	}

	FallbackAlarmAudio->SetAutoActivate(false);
	FallbackAlarmAudio->SetCanEverAffectNavigation(false);
	FallbackAlarmAudio->bAutoActivate = false;
	FallbackAlarmAudio->bIsUISound = false;
	Owner->AddOwnedComponent(FallbackAlarmAudio);

	if (USceneComponent* RootComponent = Owner->GetRootComponent())
	{
		FallbackAlarmAudio->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	}

	FallbackAlarmAudio->RegisterComponent();
}

void USubmarineFeedbackDirectorComponent::EnsureLeakAudioPoolSize(int32 DesiredCount)
{
	AActor* Owner = GetOwner();
	if (!Owner || DesiredCount <= 0)
	{
		return;
	}

	USceneComponent* RootComponent = Owner->GetRootComponent();
	for (int32 Index = RuntimeLeakAudioComponents.Num(); Index < DesiredCount; ++Index)
	{
		UAudioComponent* LeakAudioComponent = NewObject<UAudioComponent>(Owner, *FString::Printf(TEXT("RuntimeLeakAudio_%d"), Index));
		if (!LeakAudioComponent)
		{
			continue;
		}

		LeakAudioComponent->SetAutoActivate(false);
		LeakAudioComponent->SetCanEverAffectNavigation(false);
		LeakAudioComponent->bAutoActivate = false;
		LeakAudioComponent->bIsUISound = false;
		Owner->AddOwnedComponent(LeakAudioComponent);

		if (RootComponent)
		{
			LeakAudioComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		}

		LeakAudioComponent->RegisterComponent();
		RuntimeLeakAudioComponents.Add(LeakAudioComponent);
	}
}

void USubmarineFeedbackDirectorComponent::UpdateAlarmBeacons(bool bAlarmActive, float Severity01)
{
	const USubmarineFeedbackProfile* Profile = FeedbackProfile;
	if (!Profile)
	{
		return;
	}

	for (int32 Index = AlarmBeacons.Num() - 1; Index >= 0; --Index)
	{
		ASubmarineAlarmBeacon* Beacon = AlarmBeacons[Index];
		if (!IsValid(Beacon))
		{
			AlarmBeacons.RemoveAtSwap(Index);
			continue;
		}

		Beacon->SetAlarmState(
			bAlarmActive,
			Severity01,
			Profile->AlarmSound,
			Profile->AlarmTriggerPeriodSeconds,
			Profile->AlarmGain * FMath::Lerp(0.35f, 1.f, Severity01),
			Profile->AlarmTriggerPeriodParameter,
			Profile->AlarmGainParameter);
	}
}

void USubmarineFeedbackDirectorComponent::UpdateLeakAudioRuntime()
{
	const USubmarineFeedbackProfile* Profile = FeedbackProfile;
	const ASubmarineBase* Submarine = GetOwningSubmarine();
	if (!Profile || !Submarine)
	{
		return;
	}

	const int32 DesiredSourceCount = FMath::Clamp(Profile->MaxRuntimeLeakSources, 1, 8);
	EnsureLeakAudioPoolSize(DesiredSourceCount);

	struct FLeakAudioCandidate
	{
		FVector LocalCenter = FVector::ZeroVector;
		float LeakRate01 = 0.f;
		float Pressure01 = 0.f;
		float Severity01 = 0.f;
	};

	TArray<FLeakAudioCandidate> Candidates;
	Candidates.Reserve(LatestBreaches.Num());

	for (const FBreachClusterState& Breach : LatestBreaches)
	{
		FLeakAudioCandidate Candidate;
		Candidate.LocalCenter = Breach.LocalCenter;
		Candidate.LeakRate01 = FMath::Clamp(Breach.InscribedRadiusCm / FMath::Max(0.1f, Profile->LeakRadiusToRateDivisorCm), 0.f, 1.f);

		const FBreachFlowField* MatchingFlowField = LatestFlowFields.FindByPredicate([&Breach](const FBreachFlowField& FlowField)
		{
			return FlowField.SheetId == Breach.SheetId;
		});
		Candidate.Pressure01 = MatchingFlowField
			? FMath::Clamp(MatchingFlowField->ForceScale / FMath::Max(0.1f, Profile->LeakForceToPressureDivisor), 0.f, 1.f)
			: 0.f;
		Candidate.Severity01 = FMath::Max(Candidate.LeakRate01, Candidate.Pressure01);

		if (Candidate.Severity01 > KINDA_SMALL_NUMBER)
		{
			Candidates.Add(Candidate);
		}
	}

	Candidates.Sort([](const FLeakAudioCandidate& A, const FLeakAudioCandidate& B)
	{
		return A.Severity01 > B.Severity01;
	});

	const int32 ActiveSourceCount = FMath::Min(Candidates.Num(), RuntimeLeakAudioComponents.Num());
	for (int32 Index = 0; Index < RuntimeLeakAudioComponents.Num(); ++Index)
	{
		UAudioComponent* LeakAudioComponent = RuntimeLeakAudioComponents[Index];
		if (!LeakAudioComponent)
		{
			continue;
		}

		if (Index >= ActiveSourceCount || !Profile->LeakSound)
		{
			if (LeakAudioComponent->IsPlaying())
			{
				LeakAudioComponent->Stop();
			}
			continue;
		}

		const FLeakAudioCandidate& Candidate = Candidates[Index];
		if (LeakAudioComponent->Sound != Profile->LeakSound)
		{
			LeakAudioComponent->SetSound(Profile->LeakSound);
		}

		LeakAudioComponent->SetRelativeLocation(Candidate.LocalCenter);
		if (!Profile->LeakRateParameter.IsNone())
		{
			LeakAudioComponent->SetFloatParameter(Profile->LeakRateParameter, Candidate.LeakRate01);
		}
		if (!Profile->LeakPressureParameter.IsNone())
		{
			LeakAudioComponent->SetFloatParameter(Profile->LeakPressureParameter, Candidate.Pressure01);
		}
		if (!Profile->LeakGainParameter.IsNone())
		{
			LeakAudioComponent->SetFloatParameter(Profile->LeakGainParameter, Profile->LeakBaseGain * Candidate.Severity01);
		}

		if (!LeakAudioComponent->IsPlaying())
		{
			LeakAudioComponent->Play();
		}
	}
}

void USubmarineFeedbackDirectorComponent::UpdateFloodAlarmFromStates(const TArray<FCompartmentState>& States)
{
	const USubmarineFeedbackProfile* Profile = FeedbackProfile;
	if (!Profile)
	{
		return;
	}

	float MaxWaterLevel01 = 0.f;
	for (const FCompartmentState& State : States)
	{
		MaxWaterLevel01 = FMath::Max(MaxWaterLevel01, State.FloodLevel01);
	}

	const bool bShouldAlarm = Profile->AlarmSound && MaxWaterLevel01 >= Profile->AlarmFloodThreshold01;
	const float Severity01 = bShouldAlarm
		? FMath::Clamp(
			(MaxWaterLevel01 - Profile->AlarmFloodThreshold01)
			/ FMath::Max(0.01f, 1.f - Profile->AlarmFloodThreshold01),
			0.f,
			1.f)
		: 0.f;

	UpdateAlarmBeacons(bShouldAlarm, Severity01);

	if (bUseFallbackAlarmAudioWhenNoBeacon && AlarmBeacons.Num() == 0)
	{
		EnsureFallbackAlarmAudio();
	}

	if (!FallbackAlarmAudio)
	{
		bFloodAlarmActive = bShouldAlarm;
		return;
	}

	if (!bShouldAlarm)
	{
		if (FallbackAlarmAudio->IsPlaying())
		{
			FallbackAlarmAudio->Stop();
		}
		bFloodAlarmActive = false;
		return;
	}

	if (FallbackAlarmAudio->Sound != Profile->AlarmSound)
	{
		FallbackAlarmAudio->SetSound(Profile->AlarmSound);
	}

	if (!Profile->AlarmTriggerPeriodParameter.IsNone())
	{
		FallbackAlarmAudio->SetIntParameter(Profile->AlarmTriggerPeriodParameter, FMath::Max(1, Profile->AlarmTriggerPeriodSeconds));
	}

	if (!Profile->AlarmGainParameter.IsNone())
	{
		FallbackAlarmAudio->SetFloatParameter(Profile->AlarmGainParameter, Profile->AlarmGain * FMath::Lerp(0.35f, 1.f, Severity01));
	}

	if (!FallbackAlarmAudio->IsPlaying())
	{
		FallbackAlarmAudio->Play();
	}

	if (bLogFeedbackDispatch && !bFloodAlarmActive)
	{
		UE_LOG(
			LogSubmarineFeedback,
			Log,
			TEXT("FloodAlarmFeedback(SubFlood) | Sub=%s | Threshold=%.2f | Severity=%.2f | Beacons=%d"),
			*GetNameSafe(GetOwningSubmarine()),
			Profile->AlarmFloodThreshold01,
			Severity01,
			AlarmBeacons.Num());
	}

	bFloodAlarmActive = true;
}

void USubmarineFeedbackDirectorComponent::UpdateFloodInteriorAudioAnchorsFromStates(const TArray<FCompartmentState>& States)
{
	const USubmarineFeedbackProfile* Profile = FeedbackProfile;
	if (!Profile)
	{
		return;
	}

	for (int32 Index = FloodAudioAnchors.Num() - 1; Index >= 0; --Index)
	{
		ASubmarineFloodAudioAnchor* Anchor = FloodAudioAnchors[Index];
		if (!IsValid(Anchor))
		{
			FloodAudioAnchors.RemoveAtSwap(Index);
			continue;
		}

		const FCompartmentState* State = States.FindByPredicate([Anchor](const FCompartmentState& S)
		{
			return Anchor->TargetCompartmentId == NAME_None || S.CompartmentId == Anchor->TargetCompartmentId;
		});

		const float WaterLevel01 = State ? State->FloodLevel01 : 0.f;
		const float Turbulence01 = State
			? FMath::Clamp(State->FloodRateIn / 150.f, 0.f, 1.f)
			: 0.f;
		const bool bActive = WaterLevel01 >= Profile->FloodInteriorActivationThreshold01;

		Anchor->SetFloodState(
			bActive,
			WaterLevel01,
			Turbulence01,
			Profile->FloodInteriorBaseGain * FMath::Max(WaterLevel01, Turbulence01),
			Profile->FloodInteriorSound,
			Profile->FloodInteriorWaterLevelParameter,
			Profile->FloodInteriorTurbulenceParameter,
			Profile->FloodInteriorGainParameter);
	}
}

ASubmarineBase* USubmarineFeedbackDirectorComponent::GetOwningSubmarine() const
{
	return Cast<ASubmarineBase>(GetOwner());
}

USubHullComponent* USubmarineFeedbackDirectorComponent::GetOwningHull() const
{
	const ASubmarineBase* Submarine = GetOwningSubmarine();
	return Submarine ? Submarine->SubHull : nullptr;
}

bool USubmarineFeedbackDirectorComponent::IsControllerEligible(const APlayerController* PlayerController) const
{
	if (!PlayerController)
	{
		return false;
	}

	const USubmarineFeedbackProfile* Profile = FeedbackProfile;
	if (!Profile || !Profile->bRestrictInteriorFeedbackToEmbarkedCrew)
	{
		return true;
	}

	const ASubCrewCharacter* CrewCharacter = Cast<ASubCrewCharacter>(PlayerController->GetPawn());
	const ASubmarineBase* OwningSubmarine = GetOwningSubmarine();
	return CrewCharacter && OwningSubmarine && CrewCharacter->CurrentSubmarine == OwningSubmarine;
}

FName USubmarineFeedbackDirectorComponent::FindCompartmentIdForSheet(FName SheetId) const
{
	const USubHullComponent* SubHull = GetOwningHull();
	if (!SubHull)
	{
		return NAME_None;
	}

	const FStructuralSheetDef* SheetDef = SubHull->GetStructuralSheets().FindByPredicate([SheetId](const FStructuralSheetDef& Candidate)
	{
		return Candidate.SheetId == SheetId;
	});
	return SheetDef ? SheetDef->ParentCompartmentId : NAME_None;
}
