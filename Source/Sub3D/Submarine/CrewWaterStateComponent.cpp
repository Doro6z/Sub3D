#include "CrewWaterStateComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "CompartmentVolumeComponent.h"
#include "Debug/Sub3DDebugSettings.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "SubCrewCharacter.h"
#include "SubCrewMovementComponent.h"
#include "SubFloodComponent.h"
#include "SubmarineBase.h"

UCrewWaterStateComponent::UCrewWaterStateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UCrewWaterStateComponent::BeginPlay()
{
	Super::BeginPlay();
	CachedCrew = Cast<ASubCrewCharacter>(GetOwner());
	EnsurePostProcessComponent();
}

void UCrewWaterStateComponent::EnsurePostProcessComponent()
{
	if (PostProcessComp)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	PostProcessComp = NewObject<UPostProcessComponent>(Owner, TEXT("UnderwaterPostProcess"));
	if (!PostProcessComp)
	{
		return;
	}

	PostProcessComp->bUnbound = true;            // applies globally when owning camera renders
	PostProcessComp->BlendWeight = 0.f;
	PostProcessComp->bEnabled = true;
	PostProcessComp->Priority = 5.f;              // above default ambient PPVs

	// Attach under the owner's root for actor-lifetime management.
	if (USceneComponent* Root = Owner->GetRootComponent())
	{
		PostProcessComp->SetupAttachment(Root);
	}
	PostProcessComp->RegisterComponent();

	if (UnderwaterPostProcessMaterial)
	{
		PostProcessMID = UMaterialInstanceDynamic::Create(UnderwaterPostProcessMaterial, this);
		if (PostProcessMID)
		{
			PostProcessComp->AddOrUpdateBlendable(PostProcessMID, 1.f);
		}
	}
}

void UCrewWaterStateComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ASubCrewCharacter* Crew = CachedCrew.Get();
	if (!Crew)
	{
		Crew = Cast<ASubCrewCharacter>(GetOwner());
		CachedCrew = Crew;
	}
	if (!Crew)
	{
		return;
	}

	FCrewWaterSample TickSample;
	const bool bHasFreshSample = RefreshWaterState(TickSample);
	DrawDebugWaterSample(bHasFreshSample ? TickSample : LastSample, bHasFreshSample);

	if (!PostProcessComp)
	{
		return;
	}

	// Local effect — only meaningful on the owning/autonomous client.
	if (!Crew->IsLocallyControlled())
	{
		PostProcessComp->BlendWeight = 0.f;
		return;
	}

	// ── Resolve camera Z vs water surface Z ─────────────────────────────────
	const FCrewWaterProbe& View = bHasFreshSample ? TickSample.View : LastSample.View;
	const float CameraZ = View.WorldLocation.Z;
	const float SurfaceZ = bHasFreshSample
		? View.SurfaceWorldZ
		: ResolveWaterSurfaceZ();

	LastDistanceCm = CameraZ - SurfaceZ;  // negative = underwater
	bIsUnderwater = bHasFreshSample ? View.bSubmerged : (LastDistanceCm < 0.f);

	// ── Blend alpha toward target ───────────────────────────────────────────
	const float Target = bIsUnderwater ? 1.f : 0.f;
	const float Speed = bIsUnderwater ? BlendInSpeed : BlendOutSpeed;
	CurrentBlendAlpha = FMath::FInterpTo(CurrentBlendAlpha, Target, DeltaTime, Speed);
	PostProcessComp->BlendWeight = CurrentBlendAlpha;

	// ── Push enriched material parameters (Phase D) ──────────────────────────
	// Material may consume any subset — unused params are silently ignored by UMID.
	// Conventions :
	//   UnderwaterAlpha       : [0..1] smoothed PP blend (driver for tint/fog/vignette intensity)
	//   DistanceToSurfaceCm   : signed (negative = camera below surface) — for waterline effects
	//   ViewDepthCm           : positive depth of the View probe below its surface (0 if dry)
	//   HeadDepthCm           : positive depth of the Head probe (drives breath-hold visuals)
	//   BodyFractionSubmerged : [0..1] fraction of probes submerged (0 = dry, 0.25 = feet only,
	//                            0.5 = feet+torso, 0.75 = +head, 1 = +view) — useful for
	//                            modulating fog density / vignette as the player goes deeper
	//   WaterMode             : enum index (0=Dry, 1=Wading, 2=Swimming, 3=FullySubmerged)
	if (PostProcessMID)
	{
		const int32 SubmergedCount =
			(LastSample.View.bSubmerged  ? 1 : 0) +
			(LastSample.Head.bSubmerged  ? 1 : 0) +
			(LastSample.Torso.bSubmerged ? 1 : 0) +
			(LastSample.Feet.bSubmerged  ? 1 : 0);
		const float BodyFraction = SubmergedCount * 0.25f;

		PostProcessMID->SetScalarParameterValue(TEXT("UnderwaterAlpha"), CurrentBlendAlpha);
		PostProcessMID->SetScalarParameterValue(TEXT("DistanceToSurfaceCm"), LastDistanceCm);
		PostProcessMID->SetScalarParameterValue(TEXT("ViewDepthCm"), LastSample.View.DepthBelowSurfaceCm);
		PostProcessMID->SetScalarParameterValue(TEXT("HeadDepthCm"), LastSample.Head.DepthBelowSurfaceCm);
		PostProcessMID->SetScalarParameterValue(TEXT("BodyFractionSubmerged"), BodyFraction);
		PostProcessMID->SetScalarParameterValue(TEXT("WaterMode"), static_cast<float>(LastSample.Mode));
	}

	// Waterline proximity: fire continuously while camera is close to surface.
	if (FMath::Abs(LastDistanceCm) < WaterlineCrossThresholdCm)
	{
		BP_OnWaterlineProximity(LastDistanceCm);
	}
}

void UCrewWaterStateComponent::DrawDebugWaterSample(const FCrewWaterSample& Sample, bool bSampleValid) const
{
	const USub3DDebugSettings* DebugSettings = GetDefault<USub3DDebugSettings>();
	if (!DebugSettings || !DebugSettings->bDrawCrewGridAuthority)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	auto DrawProbe = [World](const FCrewWaterProbe& Probe, const TCHAR* Label, float ZOffset)
	{
		const FColor Color = Probe.bSubmerged ? FColor(255, 80, 80) : FColor(80, 255, 80);
		DrawDebugSphere(World, Probe.WorldLocation, 7.f, 10, Color, false, 0.f, 0, 1.5f);

		const float SignedDelta = Probe.SurfaceWorldZ - Probe.WorldLocation.Z;
		const FString CompStr = Probe.CompartmentId.IsNone()
			? FString(TEXT("<none>"))
			: Probe.CompartmentId.ToString();
		const FString Text = FString::Printf(
			TEXT("%s | %s | submerged=%d | d=%+.1fcm"),
			Label,
			*CompStr,
			Probe.bSubmerged ? 1 : 0,
			SignedDelta);
		DrawDebugString(World, Probe.WorldLocation + FVector(0.f, 0.f, ZOffset), Text, nullptr, Color, 0.f, true, 0.9f);
	};

	if (bSampleValid)
	{
		DrawProbe(Sample.View, TEXT("ViewProbe"), 18.f);
		DrawProbe(Sample.Head, TEXT("HeadProbe"), 10.f);
		DrawProbe(Sample.Torso, TEXT("TorsoProbe"), 4.f);
		DrawProbe(Sample.Feet, TEXT("FeetProbe"), -12.f);
	}

	const ASubCrewCharacter* Crew = CachedCrew.Get();
	if (!GEngine || !Crew || !Crew->IsLocallyControlled())
	{
		return;
	}

	auto FmtVec = [](const FVector& V)
	{
		return FString::Printf(TEXT("(%7.1f,%7.1f,%7.1f)"), V.X, V.Y, V.Z);
	};

	const float DisplayTime = 0.f;
	const FString ModeName = UEnum::GetValueAsString(Sample.Mode);
	GEngine->AddOnScreenDebugMessage(1005, DisplayTime, bSampleValid ? FColor::Cyan : FColor::Red,
		FString::Printf(TEXT("WaterSample=%s  Mode=%s  Imm01=%.2f  AnySub=%d"),
			bSampleValid ? TEXT("OK") : TEXT("FAILED"),
			*ModeName,
			Sample.WaterImmersion01,
			Sample.bAnySubmerged ? 1 : 0));
	GEngine->AddOnScreenDebugMessage(1006, DisplayTime, FColor::Cyan,
		FString::Printf(TEXT("Submerged: V=%d H=%d T=%d F=%d  HeadDepth=%5.1f  FeetDepth=%5.1f"),
			Sample.View.bSubmerged ? 1 : 0,
			Sample.Head.bSubmerged ? 1 : 0,
			Sample.Torso.bSubmerged ? 1 : 0,
			Sample.Feet.bSubmerged ? 1 : 0,
			Sample.Head.DepthBelowSurfaceCm,
			Sample.Feet.DepthBelowSurfaceCm));
	GEngine->AddOnScreenDebugMessage(1007, DisplayTime, FColor(180, 220, 255),
		FString::Printf(TEXT("ViewProbe  W=%s  SurfZ=%.1f"), *FmtVec(Sample.View.WorldLocation), Sample.View.SurfaceWorldZ));
	GEngine->AddOnScreenDebugMessage(1008, DisplayTime, FColor(180, 220, 255),
		FString::Printf(TEXT("HeadProbe  W=%s  SurfZ=%.1f"), *FmtVec(Sample.Head.WorldLocation), Sample.Head.SurfaceWorldZ));
	GEngine->AddOnScreenDebugMessage(1009, DisplayTime, FColor(180, 220, 255),
		FString::Printf(TEXT("TorsoProbe W=%s  SurfZ=%.1f"), *FmtVec(Sample.Torso.WorldLocation), Sample.Torso.SurfaceWorldZ));
	GEngine->AddOnScreenDebugMessage(1010, DisplayTime, FColor(180, 220, 255),
		FString::Printf(TEXT("FeetProbe  W=%s  SurfZ=%.1f"), *FmtVec(Sample.Feet.WorldLocation), Sample.Feet.SurfaceWorldZ));
}

float UCrewWaterStateComponent::ResolveWaterSurfaceZ() const
{
	const ASubCrewCharacter* Crew = CachedCrew.Get();
	if (!Crew)
	{
		return OceanSurfaceZ;
	}

	// EVA: always underwater (stub ocean) — surface is the global ocean Z.
	if (const USubCrewMovementComponent* CrewMov = Crew->GetCrewMovement())
	{
		if (CrewMov->EmbarkState == ECrewEmbarkState::Outside)
		{
			return OceanSurfaceZ;
		}
	}

	// Interior: surface of current compartment via SubFlood (tilt-correct, water-horizontal).
	if (const UCompartmentVolumeComponent* Vol = Crew->CurrentCompartment.Get())
	{
		if (const ASubmarineBase* Sub = Cast<ASubmarineBase>(Vol->GetOwner()))
		{
			if (Sub->SubFlood)
			{
				return Sub->SubFlood->GetCompartmentSurfaceWorldZ(Vol->CompartmentId);
			}
		}
	}

	// No compartment (e.g. ghost state) — default to ocean.
	return OceanSurfaceZ;
}

// ── Service : multi-probe water sample (synchronous, tilt-correct, hysteresis-aware) ─────
//
// Called per tick by ASubCrewCharacter::UpdateEnvironmentalEffects so the movement layer
// consumes a tilt-correct WaterImmersion01. Each probe resolves its own compartment and
// surface Z. Per-probe hysteresis prevents flicker at the waterline.
//
// `WaterImmersion01` (consumed by ApplyWaterMovementState) uses the **crew's CurrentCompartment**
// surface as the reference (D10), not per-probe surfaces. This avoids the "sub in deep water
// + probe out of bounds → fallback OceanSurfaceZ → spurious immersion" bug class.
//
// Per-probe `bSubmerged` flags + Mode are informative outputs (events / animation / audio).
//
// Cost per call : ~4 compartment lookups + 4 SubFlood queries + 4 comparisons. Trivial.

bool UCrewWaterStateComponent::RefreshWaterState(FCrewWaterSample& Out)
{
	Out = FCrewWaterSample{};  // reset (zero-initialized)

	ASubCrewCharacter* Crew = CachedCrew.Get();
	if (!Crew)
	{
		Crew = Cast<ASubCrewCharacter>(GetOwner());
		CachedCrew = Crew;
	}
	if (!Crew)
	{
		if (!bLoggedMissingCrew)
		{
			bLoggedMissingCrew = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[CrewWaterState] RefreshWaterState failed on %s: owner is not ASubCrewCharacter"),
				*GetNameSafe(GetOwner()));
		}
		return false;
	}

	const UCapsuleComponent* Capsule = Crew->GetCapsuleComponent();
	if (!Capsule)
	{
		if (!bLoggedMissingCapsule)
		{
			bLoggedMissingCapsule = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[CrewWaterState] RefreshWaterState failed on %s: missing capsule"),
				*GetNameSafe(Crew));
		}
		return false;
	}

	// Probe positions (world space). View follows the active view camera. Head/Torso/Feet read
	// their world location directly from the USceneComponent on the crew (BP-editable child of
	// the capsule). NO fallback to capsule-math : if a probe is null at runtime, that's a BP
	// data integrity issue and we want to know about it (fail loud, return false → sample stays
	// zero → HUD shows all-zero, log shows the warning).
	if (!Crew->ViewProbe || !Crew->HeadProbe || !Crew->TorsoProbe || !Crew->FeetProbe)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CrewWaterState] Probe(s) NULL on %s : View=%p Head=%p Torso=%p Feet=%p"),
			*Crew->GetName(),
			Crew->ViewProbe.Get(),
			Crew->HeadProbe.Get(),
			Crew->TorsoProbe.Get(),
			Crew->FeetProbe.Get());
		return false;
	}
	const FVector ViewLoc = Crew->ViewProbe->GetComponentLocation();
	const FVector HeadLoc = Crew->HeadProbe->GetComponentLocation();
	const FVector TorsoLoc = Crew->TorsoProbe->GetComponentLocation();
	const FVector FeetLoc = Crew->FeetProbe->GetComponentLocation();

	// Resolve the sub. Primary source = Crew->CurrentSubmarine ; fallback = CurrentCompartment's
	// owner. The fallback covers the (rare) state where the crew has overlapped a compartment
	// volume but `CurrentSubmarine` hasn't been propagated yet — typically 1-2 ticks at spawn /
	// EVA → Embarked transition. Without the fallback, FillProbe gets Sub=null, every probe
	// resolves to CompId=NAME_None, the sentinel sets SurfaceWorldZ=WorldZ-1, and bSubmerged
	// stays false even when the compartment is fully flooded. Observed 2026-05-11 in PIE.
	const ASubmarineBase* Sub = Crew->CurrentSubmarine;
	if (!Sub)
	{
		if (const UCompartmentVolumeComponent* CompVol = Crew->CurrentCompartment.Get())
		{
			Sub = Cast<ASubmarineBase>(CompVol->GetOwner());
		}
	}
	const FTransform SubXf = Sub ? Sub->GetActorTransform() : FTransform::Identity;

	FillProbe(Out.View, ViewLoc, Sub, SubXf, bLastViewSubmerged);
	FillProbe(Out.Head, HeadLoc, Sub, SubXf, bLastHeadSubmerged);
	FillProbe(Out.Torso, TorsoLoc, Sub, SubXf, bLastTorsoSubmerged);
	FillProbe(Out.Feet, FeetLoc, Sub, SubXf, bLastFeetSubmerged);

	// ── WaterImmersion01 computation (D10) ────────────────────────────────────
	// Use the **crew's CurrentCompartment** surface as the single reference Z. This avoids
	// per-probe inconsistencies (Feet probe sliding under floor → CompId NAME_None → fallback
	// OceanSurfaceZ → bogus large negative DeltaZ when sub is deep).
	float SurfaceReferenceZ = 0.f;
	bool bHaveSurfaceReference = false;
	if (const UCompartmentVolumeComponent* Vol = Crew->CurrentCompartment.Get())
	{
		if (Sub && Sub->SubFlood)
		{
			SurfaceReferenceZ = Sub->SubFlood->GetCompartmentSurfaceWorldZ(Vol->CompartmentId);
			bHaveSurfaceReference = true;
		}
	}

	if (bHaveSurfaceReference)
	{
		const float BodyHeight = FMath::Max(KINDA_SMALL_NUMBER,
			Out.Head.WorldLocation.Z - Out.Feet.WorldLocation.Z);
		const float Numerator = SurfaceReferenceZ - Out.Feet.WorldLocation.Z;
		Out.WaterImmersion01 = FMath::Clamp(Numerator / BodyHeight, 0.f, 1.f);
	}
	else
	{
		// Safe default: no compartment context (transient ghost state or EVA fallback handled
		// upstream by UpdateEnvironmentalEffects which forces 1.0 itself). Keep 0 here.
		Out.WaterImmersion01 = 0.f;
	}

	Out.bAnySubmerged = Out.View.bSubmerged || Out.Head.bSubmerged
	                 || Out.Torso.bSubmerged || Out.Feet.bSubmerged;

	// ── Mode derivation (Phase B) ─────────────────────────────────────────────
	Out.Mode = DeriveMode(Out);

	// ── Fire BP transition events (Phase B) ───────────────────────────────────
	if (Out.View.bSubmerged && !bLastViewSubmerged)   { BP_OnEnterWater(); }       // legacy compat
	if (!Out.View.bSubmerged && bLastViewSubmerged)   { BP_OnExitWater(); }
	if (Out.Head.bSubmerged && !bLastHeadSubmerged)   { BP_OnHeadEnterWater(); }
	if (!Out.Head.bSubmerged && bLastHeadSubmerged)   { BP_OnHeadExitWater(); }
	if (Out.Torso.bSubmerged && !bLastTorsoSubmerged) { BP_OnTorsoEnterWater(); }
	if (!Out.Torso.bSubmerged && bLastTorsoSubmerged) { BP_OnTorsoExitWater(); }
	if (Out.Feet.bSubmerged && !bLastFeetSubmerged)   { BP_OnFeetEnterWater(); }
	if (!Out.Feet.bSubmerged && bLastFeetSubmerged)   { BP_OnFeetExitWater(); }
	if (Out.Mode != LastMode)                         { BP_OnModeChanged(Out.Mode); }

	// Update last-tick tracking for next call (hysteresis source for next FillProbe pass).
	bLastViewSubmerged = Out.View.bSubmerged;
	bLastHeadSubmerged = Out.Head.bSubmerged;
	bLastTorsoSubmerged = Out.Torso.bSubmerged;
	bLastFeetSubmerged = Out.Feet.bSubmerged;
	LastMode = Out.Mode;

	// Cache the sample so TickComponent can read it (push enriched MID params) without
	// re-computing the probes.
	LastSample = Out;

	return true;
}

void UCrewWaterStateComponent::FillProbe(FCrewWaterProbe& Probe, const FVector& WorldLocation,
	const ASubmarineBase* Sub, const FTransform& SubXf, bool bPreviousSubmerged) const
{
	Probe.WorldLocation = WorldLocation;

	// Resolve compartment per-probe (NOT reusing CachedCrew->CurrentCompartment — that's a
	// capsule-centre-based cache that doesn't reflect head/feet position).
	if (Sub)
	{
		const FVector LocalPos = SubXf.InverseTransformPosition(WorldLocation);
		Probe.CompartmentId = Sub->FindCompartmentIdAtLocalLocation(LocalPos);
	}

	// Surface Z resolution per §14.4.2 rule :
	//  - Valid CompartmentId   → SubFlood->GetCompartmentSurfaceWorldZ(Id) (tilt-correct)
	//  - NAME_None + EVA       → handled upstream (Outside forces WaterImmersion01=1)
	//  - NAME_None + Embarked  → use Probe.Z - 1.f as sentinel → bSubmerged = false naturally
	//                             (prevents the "sub deep → OceanSurfaceZ=0 → giant positive delta" bug)
	if (!Probe.CompartmentId.IsNone() && Sub && Sub->SubFlood)
	{
		Probe.SurfaceWorldZ = Sub->SubFlood->GetCompartmentSurfaceWorldZ(Probe.CompartmentId);
	}
	else
	{
		// Sentinel : surface "just above" the probe → DeltaZ becomes ~ -1cm → not submerged
		// after hysteresis. This is the safe default for "probe outside any compartment".
		Probe.SurfaceWorldZ = Probe.WorldLocation.Z - 1.f;
	}

	// Submergence test with hysteresis (Phase B). Margins prevent flicker at the waterline :
	//   - If previously dry  : enter only when probe is firmly below (Z < Surface - EnterMargin)
	//   - If previously wet  : stay wet until firmly above (Z > Surface + ExitMargin)
	// Note ExitMargin is signed: probe.Z > Surface + ExitMargin → DeltaZ < -ExitMargin → dry.
	const float DeltaZ = Probe.SurfaceWorldZ - Probe.WorldLocation.Z;  // positive = submerged
	if (bPreviousSubmerged)
	{
		Probe.bSubmerged = DeltaZ > -ExitMarginCm;
	}
	else
	{
		Probe.bSubmerged = DeltaZ > EnterMarginCm;
	}
	Probe.DepthBelowSurfaceCm = FMath::Max(0.f, DeltaZ);
}

ECrewWaterMode UCrewWaterStateComponent::DeriveMode(const FCrewWaterSample& Sample)
{
	// Head precedence : if Head is submerged, the player is "fully under water" regardless of
	// the other probes (breath-hold timer should run).
	if (Sample.Head.bSubmerged)
	{
		return ECrewWaterMode::FullySubmerged;
	}
	if (Sample.Torso.bSubmerged)
	{
		return ECrewWaterMode::Swimming;
	}
	if (Sample.Feet.bSubmerged)
	{
		return ECrewWaterMode::Wading;
	}
	return ECrewWaterMode::Dry;
}
