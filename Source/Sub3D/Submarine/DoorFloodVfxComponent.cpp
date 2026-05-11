#include "DoorFloodVfxComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "SubDoorActor.h"
#include "SubFloodComponent.h"
#include "SubmarineBase.h"
#include "SubmarineCompartmentComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDoorCascade, Log, All);

UDoorFloodVfxComponent::UDoorFloodVfxComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDoorFloodVfxComponent::BeginPlay()
{
	Super::BeginPlay();

	CompartmentComp = GetOwner() ? GetOwner()->FindComponentByClass<USubmarineCompartmentComponent>() : nullptr;
	SubFlood = GetOwner() ? GetOwner()->FindComponentByClass<USubFloodComponent>() : nullptr;

	if (!SubFlood)
	{
		return;
	}

	if (SubFlood->IsInitialized())
	{
		ActivateSubFloodPath();
		return;
	}

	SubFlood->OnFloodInitialized.AddDynamic(this, &UDoorFloodVfxComponent::HandleFloodInitialized);
}

void UDoorFloodVfxComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SubFlood)
	{
		SubFlood->OnFloodInitialized.RemoveDynamic(this, &UDoorFloodVfxComponent::HandleFloodInitialized);
		SubFlood->OnFloodStateUpdated.RemoveDynamic(this, &UDoorFloodVfxComponent::HandleSubFloodUpdated);
	}

	DestroyPooledCascades();
	Super::EndPlay(EndPlayReason);
}

void UDoorFloodVfxComponent::HandleFloodInitialized()
{
	if (!SubFlood || !SubFlood->IsInitialized())
	{
		return;
	}

	SubFlood->OnFloodInitialized.RemoveDynamic(this, &UDoorFloodVfxComponent::HandleFloodInitialized);
	ActivateSubFloodPath();
}

void UDoorFloodVfxComponent::ActivateSubFloodPath()
{
	SubFlood->OnFloodStateUpdated.AddDynamic(this, &UDoorFloodVfxComponent::HandleSubFloodUpdated);
	RefreshFromCurrentFloodState();
}

void UDoorFloodVfxComponent::RefreshFromCurrentFloodState()
{
	if (!SubFlood || !SubFlood->IsInitialized())
	{
		return;
	}

	TArray<FCompartmentState> States;
	SubFlood->ExportCompartmentStates(States);
	HandleSubFloodUpdated(States);
}

UNiagaraComponent* UDoorFloodVfxComponent::GetOrCreateCascadeComponent(int32 CascadeIndex)
{
	AActor* Owner = GetOwner();
	USceneComponent* AttachParent = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Owner || !AttachParent || CascadeIndex < 0)
	{
		return nullptr;
	}

	if (CascadePool.IsValidIndex(CascadeIndex) && CascadePool[CascadeIndex])
	{
		return CascadePool[CascadeIndex];
	}

	const FName ComponentName = *FString::Printf(TEXT("DoorCascade_%d"), CascadeIndex);
	UNiagaraComponent* NiagaraComponent = NewObject<UNiagaraComponent>(Owner, ComponentName, RF_Transient);
	if (!NiagaraComponent)
	{
		return nullptr;
	}

	Owner->AddInstanceComponent(NiagaraComponent);
	NiagaraComponent->SetupAttachment(AttachParent);
	NiagaraComponent->SetAutoActivate(false);
	NiagaraComponent->SetVisibility(false, true);
	NiagaraComponent->SetUsingAbsoluteLocation(false);
	NiagaraComponent->SetUsingAbsoluteRotation(false);
	NiagaraComponent->SetUsingAbsoluteScale(false);
	NiagaraComponent->RegisterComponent();

	if (!CascadePool.IsValidIndex(CascadeIndex))
	{
		CascadePool.SetNum(CascadeIndex + 1);
	}

	CascadePool[CascadeIndex] = NiagaraComponent;
	return NiagaraComponent;
}

void UDoorFloodVfxComponent::ApplyCascadeParameters(UNiagaraComponent* Component, const FDoorCascadeCandidate& Candidate) const
{
	if (!Component)
	{
		return;
	}

	const float Intensity01 = FMath::Clamp(Candidate.HeightDeltaCm / FMath::Max(1.f, MaxHeightDeltaCm), 0.f, 1.f);

	if (!FlowIntensityParam.IsNone())
	{
		Component->SetVariableFloat(FlowIntensityParam, Intensity01);
	}

	if (!FlowDirectionParam.IsNone())
	{
		Component->SetVariableVec3(FlowDirectionParam, Candidate.FlowDirection);
	}
}

void UDoorFloodVfxComponent::DeactivateUnusedCascades(int32 FirstUnusedIndex)
{
	// DEPRECATED 2026-05-10 (lifecycle refacto): replaced by per-slot door-affinity + drain
	// grace period inside HandleSubFloodUpdated. Kept declared for binary compat in case some
	// child BP overrides it; body retained for safety if called externally. New code should not
	// call this — it hard-deactivates without giving in-flight particles time to drain.
	for (int32 Index = FirstUnusedIndex; Index < CascadePool.Num(); ++Index)
	{
		UNiagaraComponent* Component = CascadePool[Index];
		if (!Component)
		{
			continue;
		}

		Component->Deactivate();
		Component->SetVisibility(false, true);
	}
}

void UDoorFloodVfxComponent::DestroyPooledCascades()
{
	for (UNiagaraComponent* Component : CascadePool)
	{
		if (IsValid(Component))
		{
			Component->DestroyComponent();
		}
	}

	CascadePool.Reset();
	SlotStates.Reset();
	ActiveCascadeCount = 0;
}

float UDoorFloodVfxComponent::GetCompartmentWaterHeightCmFromStates(const TArray<FCompartmentState>& States, FName CompartmentId) const
{
	const FCompartmentState* State = States.FindByPredicate([CompartmentId](const FCompartmentState& Candidate)
	{
		return Candidate.CompartmentId == CompartmentId;
	});

	return State ? State->WaterHeightCm : 0.f;
}

void UDoorFloodVfxComponent::HandleSubFloodUpdated(const TArray<FCompartmentState>& InStates)
{
	if (!CascadeEffect)
	{
		// No asset assigned: immediately release any in-flight cascade slots and clear affinity
		// (no drain — there's nothing valid to drain into).
		for (int32 i = 0; i < CascadePool.Num(); ++i)
		{
			if (CascadePool[i])
			{
				CascadePool[i]->DeactivateImmediate();
				CascadePool[i]->SetVisibility(false, true);
			}
			if (SlotStates.IsValidIndex(i))
			{
				SlotStates[i].AssignedDoorId = NAME_None;
			}
		}
		ActiveCascadeCount = 0;
		return;
	}

	AActor* Owner = GetOwner();
	USceneComponent* AttachParent = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Owner || !AttachParent)
	{
		ActiveCascadeCount = 0;
		return;
	}

	// Baseline debug arrows: 2 thin blue arrows (bidirectional) at every open door, drawn EVERY
	// tick regardless of flow state. Active flow gets a thicker green arrow drawn over the top
	// (see candidate loop below). This way we can visually verify door orientation independently
	// of whether there's currently flow above threshold.
	if (bDrawDebugArrows)
	{
		const UWorld* DebugWorld = GetWorld();
		const ASubmarineBase* DebugSubBase = Cast<ASubmarineBase>(Owner);
		if (DebugWorld && DebugSubBase && SubFlood)
		{
			constexpr float BaselineLengthCm = 25.f;
			constexpr float BaselineThickness = 0.8f;
			constexpr float BaselineDuration = 0.2f;
			constexpr float BaselineArrowHeadSize = 6.f;

			for (const FFloodEdgeState& DebugEdge : SubFlood->GetEdgeStates())
			{
				if (DebugEdge.bExteriorEdge || DebugEdge.OpenRatio < KINDA_SMALL_NUMBER)
				{
					continue;
				}
				const ASubDoorActor* DebugDoor = DebugSubBase->FindAttachedDoorById(DebugEdge.ClosureId);
				if (!DebugDoor)
				{
					continue;
				}
				const FVector DoorLoc = DebugDoor->GetActorLocation();
				const FVector DoorFwd = DebugDoor->GetActorForwardVector();
				DrawDebugDirectionalArrow(DebugWorld, DoorLoc, DoorLoc + DoorFwd * BaselineLengthCm,
					BaselineArrowHeadSize, FColor::Blue, false, BaselineDuration, 0, BaselineThickness);
				DrawDebugDirectionalArrow(DebugWorld, DoorLoc, DoorLoc - DoorFwd * BaselineLengthCm,
					BaselineArrowHeadSize, FColor::Blue, false, BaselineDuration, 0, BaselineThickness);
			}
		}
	}

	// Gather cascade candidates. Two paths:
	//   1. bUseFlowRateMode == true: read CurrentFlowRateLitersPerSec directly off USubFloodComponent
	//      edge states (P3.8). Single source of truth — same value the sim is using for transfer.
	//      Respects OpenRatio (partial-open door = partial flow = partial intensity) and works
	//      client-side because the field is replicated.
	//   2. Legacy fallback: water-height delta between A/B compartments via the existing
	//      USubmarineCompartmentComponent door list. Kept for A/B comparison.
	TArray<FDoorCascadeCandidate> Candidates;
	Candidates.Reset();

	const ASubmarineBase* SubBase = Cast<ASubmarineBase>(Owner);

	if (bUseFlowRateMode && SubBase && SubFlood)
	{
		const TArray<FFloodEdgeState>& Edges = SubFlood->GetEdgeStates();
		const float ThresholdLps = FMath::Max(0.f, FlowRateThresholdLps);
		const float MaxRateLps = FMath::Max(1.f, MaxFlowRateLps);

		if (bLogVfxDecisions)
		{
			UE_LOG(LogDoorCascade, Display,
				TEXT("Tick — %d edges, threshold=%.0f Lps, MaxFlow=%.0f Lps, asset=%s"),
				Edges.Num(), ThresholdLps, MaxRateLps, *GetNameSafe(CascadeEffect));
		}

		for (const FFloodEdgeState& Edge : Edges)
		{
			if (Edge.bExteriorEdge)
			{
				if (bLogVfxDecisions)
				{
					UE_LOG(LogDoorCascade, Verbose, TEXT("  edge '%s' skip: exterior"),
						*Edge.ClosureId.ToString());
				}
				continue;
			}
			if (Edge.OpenRatio < KINDA_SMALL_NUMBER)
			{
				if (bLogVfxDecisions)
				{
					UE_LOG(LogDoorCascade, Verbose, TEXT("  edge '%s' skip: OpenRatio=%.2f (closed)"),
						*Edge.ClosureId.ToString(), Edge.OpenRatio);
				}
				continue;
			}

			const float SignedRate = Edge.CurrentFlowRateLitersPerSec;
			const float AbsRate = FMath::Abs(SignedRate);
			if (AbsRate < ThresholdLps)
			{
				if (bLogVfxDecisions)
				{
					UE_LOG(LogDoorCascade, Verbose,
						TEXT("  edge '%s' skip: |flow|=%.1f Lps below threshold %.1f (OpenRatio=%.2f)"),
						*Edge.ClosureId.ToString(), AbsRate, ThresholdLps, Edge.OpenRatio);
				}
				continue;
			}

			const ASubDoorActor* DoorActor = SubBase->FindAttachedDoorById(Edge.ClosureId);
			if (!DoorActor)
			{
				if (bLogVfxDecisions)
				{
					UE_LOG(LogDoorCascade, Warning,
						TEXT("  edge '%s' skip: no ASubDoorActor with DoorId='%s' attached to %s. ")
						TEXT("Check the door BP instance carries the right DoorId."),
						*Edge.ClosureId.ToString(), *Edge.ClosureId.ToString(), *Owner->GetName());
				}
				continue;
			}

			FDoorCascadeCandidate Candidate;
			Candidate.DoorId = Edge.ClosureId;
			Candidate.WorldTransform = DoorActor->GetActorTransform();

			// Sign for direction: positive head delta (A above B) means flow A→B.
			// CurrentHeadDeltaCm is signed (+A→B, -B→A). Sign of flow rate falls back if delta
			// not populated yet (e.g. before first AdvanceFlooding tick).
			const float SignedHead = Edge.CurrentHeadDeltaCm != 0.f ? Edge.CurrentHeadDeltaCm : SignedRate;
			const float AbsHead = FMath::Abs(SignedHead);

			// Intensity01 from head-delta smoothstep (preferred) OR flow-rate ratio (fallback).
			// Smoothstep is the standard ease curve: t*t*(3-2t).
			float Intensity01 = 0.f;
			if (bUseHeadDeltaSignal)
			{
				const float StartCm = FMath::Max(0.f, HeadDeltaStartCm);
				const float MaxCm = FMath::Max(StartCm + 1.f, HeadDeltaMaxCm);
				const float Raw01 = FMath::Clamp((AbsHead - StartCm) / (MaxCm - StartCm), 0.f, 1.f);
				Intensity01 = Raw01 * Raw01 * (3.f - 2.f * Raw01);
			}
			else
			{
				Intensity01 = FMath::Clamp(AbsRate / MaxRateLps, 0.f, 1.f);
			}
			Candidate.Intensity01 = Intensity01;

			// Legacy HeightDeltaCm field used for sort and the existing "scale3D" sizing. Reuse
			// it as a derived ranking value so high-intensity doors get cascade slots first when
			// MaxActiveCascades caps the pool.
			Candidate.HeightDeltaCm = Intensity01 * FMath::Max(1.f, MaxHeightDeltaCm);

			// Source water speed at door (cm/s). Lerp baseline → max by intensity.
			Candidate.SourceSpeedCmS = FMath::Lerp(SourceSpeedMinCmS, SourceSpeedMaxCmS, Intensity01);

			// World-space flow direction = door forward (signed by Δh) + gravity bias.
			// Bias toward world-down so the cascade visibly cascades DOWN rather than shooting
			// horizontally; physics handles the rest once particles spawn.
			// SignedHead = SurfaceA - SurfaceB. Positive = A higher → water flows FROM A TO B.
			// Convention check 2026-05-11: ActorForward points TOWARD A, so flow toward B = -Forward.
			const FVector DoorDir = (SignedHead > 0.f)
				? -DoorActor->GetActorForwardVector()
				: DoorActor->GetActorForwardVector();
			const FVector WorldDown(0.f, 0.f, -1.f);
			Candidate.FlowDirection = (DoorDir + WorldDown * GravityBlendStrength).GetSafeNormal();
			if (Candidate.FlowDirection.IsNearlyZero())
			{
				Candidate.FlowDirection = WorldDown;
			}
			// FlowDirectionLocal is computed once we know which Niagara component slot will be
			// assigned (its world transform sits at the door's local-to-sub position). For now,
			// we compute it relative to the DoorActor's world transform — the slot will then
			// match that frame after SetRelativeLocation/Rotation in pass 2.
			Candidate.FlowDirectionLocal = DoorActor->GetActorTransform()
				.InverseTransformVectorNoScale(Candidate.FlowDirection).GetSafeNormal();
			if (Candidate.FlowDirectionLocal.IsNearlyZero())
			{
				Candidate.FlowDirectionLocal = FVector(0.f, 0.f, -1.f);
			}

			Candidates.Add(Candidate);

			if (bLogVfxDecisions)
			{
				UE_LOG(LogDoorCascade, Display,
					TEXT("  edge '%s' CANDIDATE: |flow|=%.1f Lps, OpenRatio=%.2f, dir=%s, intensityMapped=%.2f"),
					*Edge.ClosureId.ToString(), AbsRate, Edge.OpenRatio,
					*Candidate.FlowDirection.ToString(), Candidate.HeightDeltaCm / FMath::Max(1.f, MaxHeightDeltaCm));
			}
		}
	}
	else if (CompartmentComp && SubBase)
	{
		const TArray<FDoorState>& Doors = CompartmentComp->GetDoors();
		for (const FDoorState& Door : Doors)
		{
			if (Door.bClosed || Door.DoorId.IsNone())
			{
				continue;
			}

			const float HeightA = GetCompartmentWaterHeightCmFromStates(InStates, Door.CompartmentA);
			const float HeightB = GetCompartmentWaterHeightCmFromStates(InStates, Door.CompartmentB);
			const float Delta = FMath::Abs(HeightA - HeightB);

			if (Delta < HeightDeltaThresholdCm)
			{
				continue;
			}

			const ASubDoorActor* DoorActor = SubBase->FindAttachedDoorById(Door.DoorId);
			if (!DoorActor)
			{
				continue;
			}

			FDoorCascadeCandidate Candidate;
			Candidate.DoorId = Door.DoorId;
			Candidate.WorldTransform = DoorActor->GetActorTransform();
			Candidate.HeightDeltaCm = Delta;
			Candidate.FlowDirection = (HeightA > HeightB)
				? DoorActor->GetActorForwardVector()
				: -DoorActor->GetActorForwardVector();
			Candidates.Add(Candidate);
		}
	}

	// Sort candidates by intensity descending so high-flow doors get a slot first when MaxActiveCascades
	// is reached. Within the slot-affinity assignment below, sorted order also means re-priming an
	// existing slot is preferred for higher-priority candidates.
	Candidates.Sort([](const FDoorCascadeCandidate& A, const FDoorCascadeCandidate& B)
	{
		return A.HeightDeltaCm > B.HeightDeltaCm;
	});

	// Lifecycle pattern (door affinity + drain grace) — replaces the previous index-based assignment
	// that hard-Deactivate()d unused cascades. The Niagara Fluids docs explicitly call out that
	// `Deactivate()` stops spawning but the FLIP grid keeps ticking until particles die — so an
	// immediate Deactivate while flow visibly stops causes visual pop. Instead:
	//   1. Slots have door affinity (`SlotStates[i].AssignedDoorId`).
	//   2. A candidate matching an existing slot's DoorId continues that slot — applies params,
	//      updates LastActiveTimeSeconds.
	//   3. A slot whose DoorId is NOT in current candidates: keep component active, set
	//      FlowIntensity01 = 0 (so spawn stops, in-flight particles drain over their lifetime),
	//      until `DeactivateGracePeriodSeconds` elapses → DeactivateImmediate() to free GPU buffers.
	//   4. New candidates without an existing slot get assigned to a free slot (drained or unused),
	//      Activate(bReset=true), apply params.
	const UWorld* World = GetWorld();
	const float NowSeconds = World ? World->GetTimeSeconds() : 0.f;
	const int32 MaxSlots = FMath::Max(0, MaxActiveCascades);

	// Ensure SlotStates matches CascadePool size. SlotStates entries are added when slots are
	// allocated below; we just keep them in sync if pool was shrunk externally (rare).
	if (SlotStates.Num() < CascadePool.Num())
	{
		SlotStates.SetNum(CascadePool.Num());
	}

	// Build a quick lookup of remaining (unmatched) candidates by DoorId. Slots claim from this
	// map; whatever stays at the end goes to fresh slot allocation.
	TMap<FName, const FDoorCascadeCandidate*> UnmatchedCandidates;
	UnmatchedCandidates.Reserve(Candidates.Num());
	for (const FDoorCascadeCandidate& C : Candidates)
	{
		UnmatchedCandidates.Add(C.DoorId, &C);
	}

	// Pass 1: existing slots with affinity — apply if matching candidate, drain otherwise.
	int32 ActiveCount = 0;
	for (int32 SlotIdx = 0; SlotIdx < CascadePool.Num(); ++SlotIdx)
	{
		UNiagaraComponent* Component = CascadePool[SlotIdx];
		if (!Component || !SlotStates.IsValidIndex(SlotIdx))
		{
			continue;
		}
		FCascadeSlotState& SlotState = SlotStates[SlotIdx];

		if (SlotState.AssignedDoorId.IsNone())
		{
			continue;  // genuinely free slot, leave for pass 2
		}

		const FDoorCascadeCandidate* Match = nullptr;
		if (const FDoorCascadeCandidate* const* Found = UnmatchedCandidates.Find(SlotState.AssignedDoorId))
		{
			Match = *Found;
		}

		// Asymmetric FInterpTo smoothing (ramp-up faster than ramp-down) — Niagara params are
		// pushed from SlotState.SmoothedIntensity01 so the cascade fades gracefully on close.
		const float DeltaTime = World ? World->GetDeltaSeconds() : 0.016f;
		const float TargetIntensity = Match ? Match->Intensity01 : 0.f;
		const float InterpSpeed = (TargetIntensity > SlotState.SmoothedIntensity01)
			? IntensityRampUpSpeed
			: IntensityRampDownSpeed;
		SlotState.SmoothedIntensity01 = FMath::FInterpTo(
			SlotState.SmoothedIntensity01, TargetIntensity, DeltaTime, InterpSpeed);
		const float SmoothedIntensity = FMath::Clamp(SlotState.SmoothedIntensity01, 0.f, 1.f);

		if (Match)
		{
			// Continue serving this door — re-apply transform (door may have moved with sub).
			// Spawn = door world location + offset along FlowDir + vertical world-Z offset.
			// Offsets compensate the Niagara Sphere Location interne et placent la source au sill.
			// Rotation = orient component +X along FlowDirection (gravity-blended, world-space),
			// then converted to sub-local so the emitter émet vers l'aval du flux.
			const FVector DoorWorld = Match->WorldTransform.GetLocation();
			const FVector SpawnWorld = DoorWorld
				+ Match->FlowDirection * SpawnOffsetFlowDirCm
				+ FVector(0.f, 0.f, SpawnOffsetWorldZCm);
			const FVector LocalPosition = Owner->GetActorTransform().InverseTransformPosition(SpawnWorld);
			const FQuat WorldFlowQuat = FRotationMatrix::MakeFromX(Match->FlowDirection).ToQuat();
			const FQuat LocalFlowQuat = Owner->GetActorTransform().GetRotation().Inverse() * WorldFlowQuat;
			Component->SetRelativeLocation(LocalPosition);
			Component->SetRelativeRotation(LocalFlowQuat.Rotator());
			Component->SetRelativeScale3D(FVector(FMath::Max(0.2f, SmoothedIntensity) * CascadeScaleMultiplier));

			// Push 3 user params (head-delta signal path): smoothed intensity, local-space
			// direction, derived source speed. The legacy single-param path (FlowDirection
			// world) is kept available via the flow-rate-only branch above.
			if (!FlowIntensityParam.IsNone())
			{
				Component->SetVariableFloat(FlowIntensityParam, SmoothedIntensity);
			}
			if (!FlowDirectionLocalParam.IsNone())
			{
				Component->SetVariableVec3(FlowDirectionLocalParam, Match->FlowDirectionLocal);
			}
			if (!SourceSpeedParam.IsNone())
			{
				const float SmoothedSpeed = FMath::Lerp(SourceSpeedMinCmS, SourceSpeedMaxCmS, SmoothedIntensity);
				Component->SetVariableFloat(SourceSpeedParam, SmoothedSpeed);
			}
			// Legacy world-space direction param (kept for assets authored against the old API).
			if (!FlowDirectionParam.IsNone() && FlowDirectionParam != FlowDirectionLocalParam)
			{
				Component->SetVariableVec3(FlowDirectionParam, Match->FlowDirection);
			}

			SlotState.LastActiveTimeSeconds = NowSeconds;
			UnmatchedCandidates.Remove(SlotState.AssignedDoorId);
			++ActiveCount;

			if (bLogVfxDecisions)
			{
				UE_LOG(LogDoorCascade, Display,
					TEXT("  CONTINUE slot[%d] DoorId='%s' Target=%.2f Smoothed=%.2f Speed=%.0f"),
					SlotIdx, *SlotState.AssignedDoorId.ToString(),
					TargetIntensity, SmoothedIntensity,
					FMath::Lerp(SourceSpeedMinCmS, SourceSpeedMaxCmS, SmoothedIntensity));
			}

			if (bDrawDebugArrows && World)
			{
				const FVector ArrowStart = Match->WorldTransform.GetLocation();
				const float ArrowLen = 25.f + 60.f * SmoothedIntensity;
				const FVector ArrowEnd = ArrowStart + Match->FlowDirection * ArrowLen;
				const float ActiveThickness = 1.5f + 2.5f * SmoothedIntensity;
				DrawDebugDirectionalArrow(World, ArrowStart, ArrowEnd, 10.f,
					FColor::Green, false, 0.2f, 0, ActiveThickness);
			}
		}
		else
		{
			// No matching candidate — drain. SmoothedIntensity ramps toward 0; spawn stops.
			// Particles drain over their Niagara lifetime. After grace + smoothed≈0 → free GPU.
			if (!FlowIntensityParam.IsNone())
			{
				Component->SetVariableFloat(FlowIntensityParam, SmoothedIntensity);
			}
			if (!SourceSpeedParam.IsNone())
			{
				const float SmoothedSpeed = FMath::Lerp(SourceSpeedMinCmS, SourceSpeedMaxCmS, SmoothedIntensity);
				Component->SetVariableFloat(SourceSpeedParam, SmoothedSpeed);
			}

			const float TimeSinceActive = NowSeconds - SlotState.LastActiveTimeSeconds;
			if (TimeSinceActive >= DeactivateGracePeriodSeconds && SmoothedIntensity < 0.01f)
			{
				Component->DeactivateImmediate();
				Component->SetVisibility(false, true);

				if (bLogVfxDecisions)
				{
					UE_LOG(LogDoorCascade, Display,
						TEXT("  RELEASE slot[%d] DoorId='%s' after %.2fs grace, smoothed=%.3f"),
						SlotIdx, *SlotState.AssignedDoorId.ToString(), TimeSinceActive, SmoothedIntensity);
				}
				SlotState.AssignedDoorId = NAME_None;
				SlotState.SmoothedIntensity01 = 0.f;
			}
			else
			{
				++ActiveCount;
				if (bLogVfxDecisions)
				{
					UE_LOG(LogDoorCascade, Verbose,
						TEXT("  DRAIN slot[%d] DoorId='%s' since %.2fs smoothed=%.3f"),
						SlotIdx, *SlotState.AssignedDoorId.ToString(), TimeSinceActive, SmoothedIntensity);
				}
			}
		}
	}

	// Pass 2: assign remaining unmatched candidates to free (or newly created) slots,
	// respecting MaxActiveCascades.
	for (const TPair<FName, const FDoorCascadeCandidate*>& Pair : UnmatchedCandidates)
	{
		if (ActiveCount >= MaxSlots)
		{
			if (bLogVfxDecisions)
			{
				UE_LOG(LogDoorCascade, Verbose,
					TEXT("  SKIP candidate DoorId='%s': MaxActiveCascades (%d) reached"),
					*Pair.Key.ToString(), MaxSlots);
			}
			break;
		}

		const FDoorCascadeCandidate* Candidate = Pair.Value;

		// Find a free slot.
		int32 FreeSlotIdx = INDEX_NONE;
		for (int32 i = 0; i < SlotStates.Num(); ++i)
		{
			if (SlotStates[i].AssignedDoorId.IsNone() && CascadePool.IsValidIndex(i) && CascadePool[i])
			{
				FreeSlotIdx = i;
				break;
			}
		}
		// No free slot — create a new one if under MaxSlots.
		if (FreeSlotIdx == INDEX_NONE && CascadePool.Num() < MaxSlots)
		{
			FreeSlotIdx = CascadePool.Num();
			UNiagaraComponent* NewComp = GetOrCreateCascadeComponent(FreeSlotIdx);
			if (!NewComp)
			{
				continue;
			}
			if (SlotStates.Num() <= FreeSlotIdx)
			{
				SlotStates.SetNum(FreeSlotIdx + 1);
			}
		}
		if (FreeSlotIdx == INDEX_NONE)
		{
			continue;  // capacity reached during this pass
		}

		UNiagaraComponent* Component = CascadePool[FreeSlotIdx];
		if (!Component)
		{
			continue;
		}
		FCascadeSlotState& SlotState = SlotStates[FreeSlotIdx];

		const FVector DoorWorld = Candidate->WorldTransform.GetLocation();
		const FVector SpawnWorld = DoorWorld
			+ Candidate->FlowDirection * SpawnOffsetFlowDirCm
			+ FVector(0.f, 0.f, SpawnOffsetWorldZCm);
		const FVector LocalPosition = Owner->GetActorTransform().InverseTransformPosition(SpawnWorld);
		const FQuat WorldFlowQuat = FRotationMatrix::MakeFromX(Candidate->FlowDirection).ToQuat();
		const FQuat LocalFlowQuat = Owner->GetActorTransform().GetRotation().Inverse() * WorldFlowQuat;

		Component->SetAsset(CascadeEffect, true);
		Component->SetRelativeLocation(LocalPosition);
		Component->SetRelativeRotation(LocalFlowQuat.Rotator());

		// New slot: smoothed intensity starts at 0 then ramps up next tick. Initial-frame params
		// use 0 so the cascade doesn't pop in at full intensity — visual fade-in matches fade-out.
		SlotState.SmoothedIntensity01 = 0.f;
		Component->SetRelativeScale3D(FVector(0.2f * CascadeScaleMultiplier));

		if (!FlowIntensityParam.IsNone())
		{
			Component->SetVariableFloat(FlowIntensityParam, 0.f);
		}
		if (!FlowDirectionLocalParam.IsNone())
		{
			Component->SetVariableVec3(FlowDirectionLocalParam, Candidate->FlowDirectionLocal);
		}
		if (!SourceSpeedParam.IsNone())
		{
			Component->SetVariableFloat(SourceSpeedParam, SourceSpeedMinCmS);
		}
		if (!FlowDirectionParam.IsNone() && FlowDirectionParam != FlowDirectionLocalParam)
		{
			Component->SetVariableVec3(FlowDirectionParam, Candidate->FlowDirection);
		}

		Component->SetVisibility(true, true);
		Component->Activate(true);

		SlotState.AssignedDoorId = Candidate->DoorId;
		SlotState.LastActiveTimeSeconds = NowSeconds;
		++ActiveCount;

		if (bLogVfxDecisions)
		{
			UE_LOG(LogDoorCascade, Display,
				TEXT("  SPAWN slot[%d] DoorId='%s' WorldPos=%s TargetIntensity=%.2f"),
				FreeSlotIdx, *Candidate->DoorId.ToString(),
				*Candidate->WorldTransform.GetLocation().ToString(), Candidate->Intensity01);
		}

		if (bDrawDebugArrows && World)
		{
			const FVector ArrowStart = Candidate->WorldTransform.GetLocation();
			const float ArrowLen = 25.f + 60.f * Candidate->Intensity01;
			const FVector ArrowEnd = ArrowStart + Candidate->FlowDirection * ArrowLen;
			const float ActiveThickness = 1.5f + 2.5f * Candidate->Intensity01;
			DrawDebugDirectionalArrow(World, ArrowStart, ArrowEnd, 10.f,
				FColor::Green, false, 0.2f, 0, ActiveThickness);
		}
	}

	// Debug text per open door — drawn at end so SlotStates reflect this tick's smoothing.
	// Shows live values: Δh, FlowRate, OpenRatio, SmoothedIntensity, computed state.
	// Color: green = ACTIVE/DRAIN slot, yellow = open but below threshold (SKIP), cyan = closed.
	if (bDrawDebugValues && World && SubBase && SubFlood)
	{
		TMap<FName, float> SmoothedByDoor;
		SmoothedByDoor.Reserve(SlotStates.Num());
		for (const FCascadeSlotState& Slot : SlotStates)
		{
			if (!Slot.AssignedDoorId.IsNone())
			{
				SmoothedByDoor.Add(Slot.AssignedDoorId, Slot.SmoothedIntensity01);
			}
		}

		const float ThresholdLpsLocal = FMath::Max(0.f, FlowRateThresholdLps);
		const float StartCmLocal = FMath::Max(0.f, HeadDeltaStartCm);

		for (const FFloodEdgeState& DebugEdge : SubFlood->GetEdgeStates())
		{
			if (DebugEdge.bExteriorEdge || DebugEdge.OpenRatio < KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const ASubDoorActor* DebugDoor = SubBase->FindAttachedDoorById(DebugEdge.ClosureId);
			if (!DebugDoor)
			{
				continue;
			}

			const float AbsRate = FMath::Abs(DebugEdge.CurrentFlowRateLitersPerSec);
			const float AbsHead = FMath::Abs(DebugEdge.CurrentHeadDeltaCm);
			const float* SmoothedPtr = SmoothedByDoor.Find(DebugEdge.ClosureId);
			const float Smoothed = SmoothedPtr ? *SmoothedPtr : 0.f;

			const TCHAR* StateStr;
			FColor TextColor;
			if (SmoothedPtr && Smoothed > 0.05f)
			{
				StateStr = TEXT("ACTIVE");
				TextColor = FColor::Green;
			}
			else if (SmoothedPtr)
			{
				StateStr = TEXT("DRAIN");
				TextColor = FColor::Yellow;
			}
			else if (AbsRate < ThresholdLpsLocal)
			{
				StateStr = TEXT("SKIP_FLOW");
				TextColor = FColor(255, 140, 0);
			}
			else if (AbsHead < StartCmLocal)
			{
				StateStr = TEXT("SKIP_DH");
				TextColor = FColor(255, 140, 0);
			}
			else
			{
				StateStr = TEXT("PENDING");
				TextColor = FColor::Cyan;
			}

			const FString Text = FString::Printf(
				TEXT("%s  [%s]\nDh=%.1fcm  Q=%.0fLps\nOpen=%.0f%%  Smoothed=%.2f"),
				*DebugEdge.ClosureId.ToString(), StateStr,
				DebugEdge.CurrentHeadDeltaCm, DebugEdge.CurrentFlowRateLitersPerSec,
				DebugEdge.OpenRatio * 100.f, Smoothed);

			const FVector TextLoc = DebugDoor->GetActorLocation() + FVector(0.f, 0.f, 100.f);
			DrawDebugString(World, TextLoc, Text, nullptr, TextColor, 0.f, true, 1.0f);
		}
	}

	ActiveCascadeCount = ActiveCount;

	if (bLogVfxDecisions)
	{
		UE_LOG(LogDoorCascade, Display,
			TEXT("Tick result: %d candidates total, %d slots active (including draining)"),
			Candidates.Num(), ActiveCascadeCount);
	}
}
