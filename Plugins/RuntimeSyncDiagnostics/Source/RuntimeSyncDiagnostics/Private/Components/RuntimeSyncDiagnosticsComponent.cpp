#include "Components/RuntimeSyncDiagnosticsComponent.h"
#include "RuntimeSyncDiagnosticsLog.h"
#include "RuntimeSyncDiagnosticsSettings.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Data/RuntimeSyncEvent.h"

URuntimeSyncDiagnosticsComponent::URuntimeSyncDiagnosticsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

	bEnabled = true;
	bRecordSamples = true;
	bDrawDebug = false;
	bLogEvents = true;
	
	const URuntimeSyncDiagnosticsSettings* Settings = GetDefault<URuntimeSyncDiagnosticsSettings>();
	SampleRateHz = Settings->DefaultSampleRateHz;
	ErrorWarningThresholdCm = Settings->ErrorWarningThresholdCm;

	bTrackMovementBase = true;
	bTrackFloorState = true;
	bTrackParentFrame = false;

	TimeSinceLastSample = 0.f;
	LastParentDeltaCm = 0.f;
	SnapCount = 0;
	CorrectionCount = 0;
	CapturedFrames = 0;
}

void URuntimeSyncDiagnosticsComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bEnabled)
	{
		UWorld* World = GetWorld();
		if (!World) return;

		CachedRecorder = World->GetSubsystem<URuntimeSyncRecorderSubsystem>();
		if (!CachedRecorder && bLogEvents)
		{
			UE_LOG(LogRuntimeSyncDiagnostics, Warning, TEXT("URuntimeSyncDiagnosticsComponent failed to find Recorder Subsystem on Actor %s"), *GetNameSafe(GetOwner()));
		}

		// Initialize parent frame to avoid delta spike on first tick
		if (bTrackParentFrame)
		{
			if (USceneComponent* RootComp = GetOwner()->GetRootComponent())
			{
				if (USceneComponent* AttachParent = RootComp->GetAttachParent())
				{
					LastParentTransform = AttachParent->GetComponentTransform();
				}
			}
		}
	}
}

void URuntimeSyncDiagnosticsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void URuntimeSyncDiagnosticsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnabled) return;

	// Capture current tick group for diagnostics
	if (ThisTickFunction)
	{
		static const UEnum* TickGroupEnum = StaticEnum<ETickingGroup>();
		CurrentTickGroupName = TickGroupEnum->GetNameStringByValue(ThisTickFunction->TickGroup);
	}

	UpdateMovementBaseTracking();
	UpdateFloorTracking();
	UpdateParentFrameTracking();

	if (bRecordSamples && SampleRateHz > 0.f && CachedRecorder && CachedRecorder->IsCapturing())
	{
		TimeSinceLastSample += DeltaTime;
		const float SampleInterval = 1.0f / SampleRateHz;

		if (TimeSinceLastSample >= SampleInterval)
		{
			SampleNow(DeltaTime);
			// Fix sampling drift - subtract interval instead of fmod
			TimeSinceLastSample -= SampleInterval;
		}
	}
}

void URuntimeSyncDiagnosticsComponent::StartDiagnostics()
{
	bEnabled = true;
	TimeSinceLastSample = 0.f;
	CapturedFrames = 0;
	
	// Reset base tracking to avoid huge delta on first sample
	LastBaseTransform = FTransform::Identity;
	if (ACharacter* CharObj = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* CMC = CharObj->GetCharacterMovement())
		{
			if (UPrimitiveComponent* CurrentBase = CMC->GetMovementBase())
			{
				LastBaseTransform = CurrentBase->GetComponentTransform();
			}
		}
	}
}

void URuntimeSyncDiagnosticsComponent::StopDiagnostics()
{
	bEnabled = false;
}

void URuntimeSyncDiagnosticsComponent::MarkSnap(float SnapDistanceCm)
{
	SnapCount++;
	MarkCustomEvent(FName("Snap"), FString::Printf(TEXT("Dist: %.2f"), SnapDistanceCm), SnapDistanceCm);
}

void URuntimeSyncDiagnosticsComponent::MarkCorrection(float CorrectionDistanceCm)
{
	CorrectionCount++;
	MarkCustomEvent(FName("Correction"), FString::Printf(TEXT("Dist: %.2f"), CorrectionDistanceCm), CorrectionDistanceCm);
}

void URuntimeSyncDiagnosticsComponent::MarkCustomEvent(FName EventType, const FString& Context, float NumericValue)
{
	if (!bEnabled || !CachedRecorder || !CachedRecorder->IsCapturing()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	FRuntimeSyncEvent NewEvent;
	NewEvent.Timestamp = World->GetTimeSeconds();
	NewEvent.EventType = EventType;
	NewEvent.Context = FString::Printf(TEXT("[%s] %s"), *GetNameSafe(GetOwner()), *Context);
	NewEvent.NumericValue = NumericValue;

	CachedRecorder->RecordEvent(NewEvent);
}

void URuntimeSyncDiagnosticsComponent::UpdateMovementBaseTracking()
{
	if (!bTrackMovementBase) return;

	if (ACharacter* CharObj = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* CMC = CharObj->GetCharacterMovement())
		{
			UPrimitiveComponent* CurrentBase = CMC->GetMovementBase();
			
			// Detect base changes
			if (CurrentBase != LastMovementBase.Get())
			{
				FString BaseName = CurrentBase ? CurrentBase->GetName() : TEXT("None");
				MarkCustomEvent(FName("BaseChange"), FString::Printf(TEXT("NewBase: %s"), *BaseName));
				LastMovementBase = CurrentBase;
				
				// Reset transform tracking on base change
				if (CurrentBase)
				{
					LastBaseTransform = CurrentBase->GetComponentTransform();
				}
			}

			// Track base displacement
			if (bTrackBaseDeltas && CurrentBase)
			{
				FTransform CurrentBaseTransform = CurrentBase->GetComponentTransform();
				if (!LastBaseTransform.GetLocation().IsNearlyZero())
				{
					LastBaseDeltaCm = FVector::Dist(CurrentBaseTransform.GetLocation(), LastBaseTransform.GetLocation());
				}
				else
				{
					LastBaseDeltaCm = 0.f;
				}
				LastBaseTransform = CurrentBaseTransform;
			}
			else
			{
				LastBaseDeltaCm = 0.f;
			}
		}
	}
}

void URuntimeSyncDiagnosticsComponent::UpdateFloorTracking()
{
	if (!bTrackFloorState) return;

	if (ACharacter* CharObj = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* CMC = CharObj->GetCharacterMovement())
		{
			const FFindFloorResult& CurrentFloor = CMC->CurrentFloor;
			FString CurrentStateStr = CurrentFloor.IsWalkableFloor() ? TEXT("Walking") : (CurrentFloor.bBlockingHit ? TEXT("HitNotWalkable") : TEXT("NoFloor"));
			
			if (CurrentStateStr != LastFloorStateString && !LastFloorStateString.IsEmpty())
			{
				MarkCustomEvent(FName("FloorChange"), CurrentStateStr);
			}
			LastFloorStateString = CurrentStateStr;
		}
	}
}

void URuntimeSyncDiagnosticsComponent::UpdateParentFrameTracking()
{
	// Legacy stub replaced by UpdateMovementBaseTracking
}

bool URuntimeSyncDiagnosticsComponent::BuildSample(FRuntimeSyncSample& OutSample) const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	OutSample.Timestamp = World->GetTimeSeconds();
	OutSample.FrameNumber = CapturedFrames;
	OutSample.ActorName = OwnerActor->GetName();
	OutSample.TickGroup = CurrentTickGroupName;
	OutSample.ActualTransform = OwnerActor->GetActorTransform();
	
	if (OwnerActor->HasAuthority())
	{
		OutSample.RoleContext = TEXT("Authority");
	}
	else
	{
		OutSample.RoleContext = (OwnerActor->GetLocalRole() == ROLE_AutonomousProxy) ? TEXT("Autonomous") : TEXT("Simulated");
	}

	if (ExpectedTransformProvider)
	{
		if (ExpectedTransformProvider->GetExpectedTransform(OwnerActor, OutSample.ExpectedTransform))
		{
			OutSample.bHasExpectedTransform = true;
			OutSample.ErrorDistanceCm = FVector::Dist(OutSample.ActualTransform.GetLocation(), OutSample.ExpectedTransform.GetLocation());
		}
		else
		{
			OutSample.bHasExpectedTransform = false;
			OutSample.ErrorDistanceCm = 0.f;
		}
	}
	else
	{
		OutSample.bHasExpectedTransform = false;
		OutSample.ExpectedTransform = OutSample.ActualTransform; 
		OutSample.ErrorDistanceCm = 0.f;
	}

	if (LastMovementBase.IsValid())
	{
		OutSample.MovementBaseName = LastMovementBase->GetName();
	}
	else
	{
		OutSample.MovementBaseName = TEXT("None");
	}

	OutSample.FloorStateContext = LastFloorStateString;

	if (bTrackBaseDeltas)
	{
		OutSample.MovementBaseTransform = LastBaseTransform;
		OutSample.MovementBaseDeltaCm = LastBaseDeltaCm;
	}

	return true;
}

void URuntimeSyncDiagnosticsComponent::SampleNow(float DeltaTime)
{
	// Fix frame indexing - increment before assignment in BuildSample
	CapturedFrames++;

	FRuntimeSyncSample NewSample;
	if (BuildSample(NewSample))
	{
		CachedRecorder->RecordSample(NewSample);

		if (bLogEvents && NewSample.bHasExpectedTransform && NewSample.ErrorDistanceCm > ErrorWarningThresholdCm)
		{
			UE_LOG(LogRuntimeSyncDiagnostics, Warning, TEXT("Sync Error Warning [%s]: %.2f cm (Threshold: %.2f)"), 
				*GetNameSafe(GetOwner()), NewSample.ErrorDistanceCm, ErrorWarningThresholdCm);
			MarkCustomEvent(FName("ThresholdExceeded"), FString::Printf(TEXT("Error: %.2f"), NewSample.ErrorDistanceCm), NewSample.ErrorDistanceCm);
		}
	}
}
