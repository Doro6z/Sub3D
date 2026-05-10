#include "CrewAnimDebugComponent.h"

#include "Sub3DDebugSettings.h"
#include "Submarine/SubCrewAnimInstance.h"
#include "Submarine/SubCrewCharacter.h"
#include "Submarine/SubCrewMovementComponent.h"
#include "Submarine/SubPlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogCrewAnimDebug, Log, All);

namespace
{
const TCHAR* ToString(ECrewDebugTraversalDomain Value)
{
	switch (Value)
	{
	case ECrewDebugTraversalDomain::Ground: return TEXT("Ground");
	case ECrewDebugTraversalDomain::InteriorWade: return TEXT("InteriorWade");
	case ECrewDebugTraversalDomain::InteriorSwim: return TEXT("InteriorSwim");
	case ECrewDebugTraversalDomain::ExteriorSwim: return TEXT("ExteriorSwim");
	case ECrewDebugTraversalDomain::Ladder: return TEXT("Ladder");
	case ECrewDebugTraversalDomain::Station: return TEXT("Station");
	default: return TEXT("Unknown");
	}
}

const TCHAR* ToString(ECrewDebugReferenceFrameState Value)
{
	switch (Value)
	{
	case ECrewDebugReferenceFrameState::SubmarineLocal: return TEXT("SubmarineLocal");
	case ECrewDebugReferenceFrameState::WorldSpace: return TEXT("WorldSpace");
	case ECrewDebugReferenceFrameState::Transition: return TEXT("Transition");
	default: return TEXT("Unknown");
	}
}

const TCHAR* ToString(ECrewDebugWaterContactState Value)
{
	switch (Value)
	{
	case ECrewDebugWaterContactState::Dry: return TEXT("Dry");
	case ECrewDebugWaterContactState::ShallowWade: return TEXT("ShallowWade");
	case ECrewDebugWaterContactState::DeepWade: return TEXT("DeepWade");
	case ECrewDebugWaterContactState::Swimming: return TEXT("Swimming");
	case ECrewDebugWaterContactState::Ocean: return TEXT("Ocean");
	default: return TEXT("Unknown");
	}
}

const TCHAR* ToString(ECrewAnimDebugWarning Value)
{
	switch (Value)
	{
	case ECrewAnimDebugWarning::OutsideNotSwimming: return TEXT("OutsideNotSwimming");
	case ECrewAnimDebugWarning::SwimmingButEmbarkedDry: return TEXT("SwimmingButEmbarkedDry");
	case ECrewAnimDebugWarning::AnimSwimMismatch: return TEXT("AnimSwimMismatch");
	case ECrewAnimDebugWarning::InvalidCompartmentOutside: return TEXT("InvalidCompartmentOutside");
	case ECrewAnimDebugWarning::NoAnimInstance: return TEXT("NoAnimInstance");
	case ECrewAnimDebugWarning::HandIKActiveDuringSwim: return TEXT("HandIKActiveDuringSwim");
	case ECrewAnimDebugWarning::FootIKActiveDuringSwim: return TEXT("FootIKActiveDuringSwim");
	case ECrewAnimDebugWarning::RestPoseDominatesArm: return TEXT("RestPoseDominatesArm");
	case ECrewAnimDebugWarning::GridVelocitySpike: return TEXT("GridVelocitySpike");
	default: return TEXT("Unknown");
	}
}

FString MovementModeToString(const UCharacterMovementComponent* MovementComponent)
{
	if (!MovementComponent)
	{
		return TEXT("<none>");
	}

	switch (MovementComponent->MovementMode)
	{
	case MOVE_None: return TEXT("None");
	case MOVE_Walking: return TEXT("Walking");
	case MOVE_NavWalking: return TEXT("NavWalking");
	case MOVE_Falling: return TEXT("Falling");
	case MOVE_Swimming: return TEXT("Swimming");
	case MOVE_Flying: return TEXT("Flying");
	case MOVE_Custom: return FString::Printf(TEXT("Custom:%d"), MovementComponent->CustomMovementMode);
	default: return TEXT("Unknown");
	}
}

FString EmbarkStateToString(const USubCrewMovementComponent* MovementComponent)
{
	if (!MovementComponent)
	{
		return TEXT("<none>");
	}

	switch (MovementComponent->EmbarkState)
	{
	case ECrewEmbarkState::Outside: return TEXT("Outside");
	case ECrewEmbarkState::Embarked: return TEXT("Embarked");
	case ECrewEmbarkState::Transitioning: return TEXT("Transitioning");
	default: return TEXT("Unknown");
	}
}

float MaxAbsDegrees(const FRotator& Rotation)
{
	return FMath::Max3(FMath::Abs(Rotation.Pitch), FMath::Abs(Rotation.Yaw), FMath::Abs(Rotation.Roll));
}

bool IsSwimContact(ECrewDebugWaterContactState WaterContactState)
{
	return WaterContactState == ECrewDebugWaterContactState::Swimming
		|| WaterContactState == ECrewDebugWaterContactState::Ocean;
}

ECrewDebugReferenceFrameState ResolveReferenceFrame(const USubCrewMovementComponent* MovementComponent)
{
	if (!MovementComponent)
	{
		return ECrewDebugReferenceFrameState::Unknown;
	}

	switch (MovementComponent->EmbarkState)
	{
	case ECrewEmbarkState::Embarked: return ECrewDebugReferenceFrameState::SubmarineLocal;
	case ECrewEmbarkState::Outside: return ECrewDebugReferenceFrameState::WorldSpace;
	case ECrewEmbarkState::Transitioning: return ECrewDebugReferenceFrameState::Transition;
	default: return ECrewDebugReferenceFrameState::Unknown;
	}
}

FCrewDebugImmersionSample BuildImmersionSample(const ASubCrewCharacter* Crew, const USubCrewMovementComponent* MovementComponent)
{
	FCrewDebugImmersionSample Sample;
	if (!Crew)
	{
		return Sample;
	}

	Sample.bValid = true;
	Sample.CompartmentId = Crew->CurrentCompartmentId;
	Sample.WaterHeightCm = Crew->CurrentWaterHeightCm;
	Sample.Immersion01 = FMath::Clamp(Crew->CurrentWaterImmersion01, 0.f, 1.f);

	if (MovementComponent && MovementComponent->EmbarkState == ECrewEmbarkState::Outside)
	{
		Sample.WaterContactState = ECrewDebugWaterContactState::Ocean;
		return Sample;
	}

	if (Sample.Immersion01 <= 0.01f)
	{
		Sample.WaterContactState = ECrewDebugWaterContactState::Dry;
	}
	else if (Sample.Immersion01 >= Crew->SwimThreshold01)
	{
		Sample.WaterContactState = ECrewDebugWaterContactState::Swimming;
	}
	else if (Sample.Immersion01 >= Crew->DeepWadeThreshold01)
	{
		Sample.WaterContactState = ECrewDebugWaterContactState::DeepWade;
	}
	else
	{
		Sample.WaterContactState = ECrewDebugWaterContactState::ShallowWade;
	}

	return Sample;
}

ECrewDebugTraversalDomain ResolveTraversalDomain(
	const ASubCrewCharacter* Crew,
	const USubCrewMovementComponent* MovementComponent,
	const UCharacterMovementComponent* CharacterMovement,
	const FCrewDebugImmersionSample& Immersion)
{
	if (!Crew || !MovementComponent)
	{
		return ECrewDebugTraversalDomain::Unknown;
	}

	if (MovementComponent->IsClimbingLadder())
	{
		return ECrewDebugTraversalDomain::Ladder;
	}

	if (const ASubPlayerController* PC = Cast<ASubPlayerController>(Crew->GetController()))
	{
		if (PC->CurrentControlMode != ECrewControlMode::OnFoot)
		{
			return ECrewDebugTraversalDomain::Station;
		}
	}

	if (MovementComponent->EmbarkState == ECrewEmbarkState::Outside)
	{
		return ECrewDebugTraversalDomain::ExteriorSwim;
	}

	if (CharacterMovement && CharacterMovement->MovementMode == MOVE_Swimming)
	{
		return ECrewDebugTraversalDomain::InteriorSwim;
	}

	if (Immersion.WaterContactState == ECrewDebugWaterContactState::ShallowWade
		|| Immersion.WaterContactState == ECrewDebugWaterContactState::DeepWade)
	{
		return ECrewDebugTraversalDomain::InteriorWade;
	}

	return ECrewDebugTraversalDomain::Ground;
}

void AppendWarning(FCrewAnimDebugSnapshot& Snapshot, ECrewAnimDebugWarning Warning)
{
	Snapshot.Warnings.AddUnique(Warning);
}

void AddBonePose(
	FCrewAnimDebugSnapshot& Snapshot,
	const USkeletalMeshComponent* Mesh,
	FName BoneName,
	const FRotator& RequestedRotation)
{
	FCrewDebugBonePose BonePose;
	BonePose.BoneName = BoneName;
	BonePose.RequestedRotation = RequestedRotation;

	if (Mesh && BoneName != NAME_None)
	{
		const int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
		if (BoneIndex != INDEX_NONE)
		{
			BonePose.bHasAppliedTransform = true;
			BonePose.AppliedTransform = Mesh->GetBoneTransform(BoneIndex);
		}
	}

	Snapshot.RequestedAndAppliedBones.Add(BonePose);
}
}

UCrewAnimDebugComponent::UCrewAnimDebugComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UCrewAnimDebugComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* CharacterMovement = Crew->GetCharacterMovement())
		{
			AddTickPrerequisiteComponent(CharacterMovement);
		}
		if (USkeletalMeshComponent* Mesh = Crew->GetMesh())
		{
			AddTickPrerequisiteComponent(Mesh);
		}
	}
}

void UCrewAnimDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !UE_BUILD_SHIPPING
	const USub3DDebugSettings* Settings = GetDefault<USub3DDebugSettings>();
	const bool bNeedsSample = Settings && (
		Settings->bLogCrewAnimWarnings
		|| Settings->bDrawCrewAnimDebug
		|| Settings->bShowCrewAnimDebugPanel
		|| Settings->bDrawCrewAnimBones
		|| Settings->bDrawCrewAnimIK
		|| Settings->bDrawCrewAnimFrameAxes);

	if (!bNeedsSample)
	{
		return;
	}

	SampleTimerSeconds += DeltaTime;
	const float SampleInterval = FMath::Max(0.01f, Settings->CrewAnimDebugSampleIntervalSeconds);
	if (SampleTimerSeconds < SampleInterval)
	{
		return;
	}
	SampleTimerSeconds = 0.f;

	FCrewAnimDebugSnapshot Snapshot;
	if (!BuildSnapshot(Snapshot))
	{
		return;
	}

	CurrentSnapshot = Snapshot;
	PushSnapshot(Snapshot);

	if (Settings->bLogCrewAnimWarnings && Snapshot.Warnings.Num() > 0)
	{
		UE_LOG(LogCrewAnimDebug, Warning, TEXT("%s"), *FormatSnapshotForLog());
	}
#endif
}

bool UCrewAnimDebugComponent::BuildSnapshot(FCrewAnimDebugSnapshot& OutSnapshot) const
{
	OutSnapshot = FCrewAnimDebugSnapshot();

	const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(GetOwner());
	if (!Crew)
	{
		return false;
	}

	const USubCrewMovementComponent* CrewMovement = Crew->GetCrewMovement();
	const UCharacterMovementComponent* CharacterMovement = Crew->GetCharacterMovement();
	const USkeletalMeshComponent* Mesh = Crew->GetMesh();
	const USubCrewAnimInstance* AnimInstance = Mesh ? Cast<USubCrewAnimInstance>(Mesh->GetAnimInstance()) : nullptr;
	const USub3DDebugSettings* Settings = GetDefault<USub3DDebugSettings>();

	OutSnapshot.CrewName = Crew->GetName();
	OutSnapshot.WorldTimeSeconds = Crew->GetWorld() ? Crew->GetWorld()->GetTimeSeconds() : 0.f;
	OutSnapshot.FrameNumber = static_cast<int64>(GFrameCounter);
	OutSnapshot.ReferenceFrame = ResolveReferenceFrame(CrewMovement);
	OutSnapshot.Immersion = BuildImmersionSample(Crew, CrewMovement);
	OutSnapshot.TraversalDomain = ResolveTraversalDomain(Crew, CrewMovement, CharacterMovement, OutSnapshot.Immersion);
	OutSnapshot.RawEmbarkState = EmbarkStateToString(CrewMovement);
	OutSnapshot.RawMovementMode = MovementModeToString(CharacterMovement);
	OutSnapshot.bCharacterSwimming = CharacterMovement && CharacterMovement->MovementMode == MOVE_Swimming;
	OutSnapshot.bAnimInstanceValid = AnimInstance != nullptr;

	if (CrewMovement)
	{
		OutSnapshot.MoveIntent = CrewMovement->GetLastMoveIntent();
		OutSnapshot.LocomotionFrame = CrewMovement->GetLastLocomotionFrame();
		OutSnapshot.WorldVelocity = CrewMovement->Velocity;
		OutSnapshot.LocalVelocity = CrewMovement->RelativeLinearVelocity;
		OutSnapshot.GridVelocityCmPerSec = CrewMovement->RelativeLinearVelocity.Size();
		OutSnapshot.FootIKLeftWeight = CrewMovement->FootIK_L_State.Weight;
		OutSnapshot.FootIKRightWeight = CrewMovement->FootIK_R_State.Weight;
	}

	if (AnimInstance)
	{
		OutSnapshot.bAnimSwimming = AnimInstance->bIsSwimming;
		OutSnapshot.HandIKLeftWeight = AnimInstance->HandIK_L_Weight;
		OutSnapshot.HandIKRightWeight = AnimInstance->HandIK_R_Weight;
		OutSnapshot.FootIKLeftWeight = AnimInstance->FootIK_L_State.Weight;
		OutSnapshot.FootIKRightWeight = AnimInstance->FootIK_R_State.Weight;
		OutSnapshot.StridePhase01 = AnimInstance->LocomotionState.StridePhase01;
		OutSnapshot.ArmRestR = AnimInstance->ArmRestR;
		OutSnapshot.ArmRestL = AnimInstance->ArmRestL;

		AddBonePose(OutSnapshot, Mesh, TEXT("pelvis"), AnimInstance->Proc_Pelvis_Rot);
		AddBonePose(OutSnapshot, Mesh, TEXT("spine_01"), AnimInstance->Proc_Spine01_Rot);
		AddBonePose(OutSnapshot, Mesh, TEXT("spine_02"), AnimInstance->Proc_Spine02_Rot);
		AddBonePose(OutSnapshot, Mesh, TEXT("spine_03"), AnimInstance->Proc_Spine03_Rot);
		AddBonePose(OutSnapshot, Mesh, TEXT("spine_04"), AnimInstance->Proc_Spine04_Rot);
		AddBonePose(OutSnapshot, Mesh, TEXT("spine_05"), AnimInstance->Proc_Spine05_Rot);
		AddBonePose(OutSnapshot, Mesh, TEXT("neck_01"), AnimInstance->Proc_Neck01_Rot);
		AddBonePose(OutSnapshot, Mesh, TEXT("head"), AnimInstance->Proc_Head_Rot);
		AddBonePose(OutSnapshot, Mesh, TEXT("thigh_r"), AnimInstance->Proc_ThighR_Rot);
		AddBonePose(OutSnapshot, Mesh, TEXT("thigh_l"), AnimInstance->Proc_ThighL_Rot);
		AddBonePose(OutSnapshot, Mesh, TEXT("calf_r"), AnimInstance->Proc_CalfR_Rot);
		AddBonePose(OutSnapshot, Mesh, TEXT("calf_l"), AnimInstance->Proc_CalfL_Rot);
		AddBonePose(OutSnapshot, Mesh, TEXT("foot_r"), AnimInstance->Proc_FootR_Rot);
		AddBonePose(OutSnapshot, Mesh, TEXT("foot_l"), AnimInstance->Proc_FootL_Rot);
		AddBonePose(OutSnapshot, Mesh, TEXT("upperarm_r"), AnimInstance->Proc_UpperarmR_Rot);
		AddBonePose(OutSnapshot, Mesh, TEXT("upperarm_l"), AnimInstance->Proc_UpperarmL_Rot);
		AddBonePose(OutSnapshot, Mesh, TEXT("lowerarm_r"), AnimInstance->Proc_LowerarmR_Rot);
		AddBonePose(OutSnapshot, Mesh, TEXT("lowerarm_l"), AnimInstance->Proc_LowerarmL_Rot);
	}
	else
	{
		AppendWarning(OutSnapshot, ECrewAnimDebugWarning::NoAnimInstance);
	}

	if (CrewMovement && CrewMovement->EmbarkState == ECrewEmbarkState::Outside && !OutSnapshot.bCharacterSwimming)
	{
		AppendWarning(OutSnapshot, ECrewAnimDebugWarning::OutsideNotSwimming);
	}

	if (OutSnapshot.bCharacterSwimming
		&& OutSnapshot.ReferenceFrame == ECrewDebugReferenceFrameState::SubmarineLocal
		&& OutSnapshot.Immersion.WaterContactState == ECrewDebugWaterContactState::Dry)
	{
		AppendWarning(OutSnapshot, ECrewAnimDebugWarning::SwimmingButEmbarkedDry);
	}

	if (AnimInstance && OutSnapshot.bAnimSwimming != OutSnapshot.bCharacterSwimming)
	{
		AppendWarning(OutSnapshot, ECrewAnimDebugWarning::AnimSwimMismatch);
	}

	if (CrewMovement && CrewMovement->EmbarkState == ECrewEmbarkState::Outside && Crew->CurrentCompartmentId != NAME_None)
	{
		AppendWarning(OutSnapshot, ECrewAnimDebugWarning::InvalidCompartmentOutside);
	}

	if (IsSwimContact(OutSnapshot.Immersion.WaterContactState)
		&& (OutSnapshot.HandIKLeftWeight > 0.05f || OutSnapshot.HandIKRightWeight > 0.05f))
	{
		AppendWarning(OutSnapshot, ECrewAnimDebugWarning::HandIKActiveDuringSwim);
	}

	if (IsSwimContact(OutSnapshot.Immersion.WaterContactState)
		&& (OutSnapshot.FootIKLeftWeight > 0.05f || OutSnapshot.FootIKRightWeight > 0.05f))
	{
		AppendWarning(OutSnapshot, ECrewAnimDebugWarning::FootIKActiveDuringSwim);
	}

	if (AnimInstance && Settings)
	{
		const float RestLimit = Settings->CrewAnimArmRestWarningDeg;
		if (MaxAbsDegrees(AnimInstance->ArmRestR) >= RestLimit || MaxAbsDegrees(AnimInstance->ArmRestL) >= RestLimit)
		{
			AppendWarning(OutSnapshot, ECrewAnimDebugWarning::RestPoseDominatesArm);
		}
	}

	if (Settings && OutSnapshot.GridVelocityCmPerSec >= Settings->CrewJitterWarnVelocityCmPerSec)
	{
		AppendWarning(OutSnapshot, ECrewAnimDebugWarning::GridVelocitySpike);
	}

	return true;
}

FString UCrewAnimDebugComponent::FormatSnapshotForLog() const
{
	FCrewAnimDebugSnapshot Snapshot;
	if (!BuildSnapshot(Snapshot))
	{
		return TEXT("CrewAnimDump: no crew owner");
	}

	FString WarningText = TEXT("<none>");
	if (Snapshot.Warnings.Num() > 0)
	{
		TArray<FString> WarningNames;
		WarningNames.Reserve(Snapshot.Warnings.Num());
		for (ECrewAnimDebugWarning Warning : Snapshot.Warnings)
		{
			WarningNames.Add(ToString(Warning));
		}
		WarningText = FString::Join(WarningNames, TEXT(", "));
	}

	FString Result;
	Result += FString::Printf(
		TEXT("CrewAnimDump | Crew=%s | Time=%.2f | Frame=%lld | Domain=%s | FrameRef=%s | Embark=%s | Move=%s\n"),
		*Snapshot.CrewName,
		Snapshot.WorldTimeSeconds,
		Snapshot.FrameNumber,
		ToString(Snapshot.TraversalDomain),
		ToString(Snapshot.ReferenceFrame),
		*Snapshot.RawEmbarkState,
		*Snapshot.RawMovementMode);
	Result += FString::Printf(
		TEXT("  Water=%s Comp=%s Height=%.1fcm Immersion=%.2f | CharSwim=%d AnimSwim=%d | Speed=%.1f LocalSpeed=%.1f Stride=%.2f\n"),
		ToString(Snapshot.Immersion.WaterContactState),
		*Snapshot.Immersion.CompartmentId.ToString(),
		Snapshot.Immersion.WaterHeightCm,
		Snapshot.Immersion.Immersion01,
		Snapshot.bCharacterSwimming ? 1 : 0,
		Snapshot.bAnimSwimming ? 1 : 0,
		Snapshot.WorldVelocity.Size(),
		Snapshot.LocalVelocity.Size(),
		Snapshot.StridePhase01);
	Result += FString::Printf(
		TEXT("  IK HandL=%.2f HandR=%.2f FootL=%.2f FootR=%.2f | ArmRestR=P%.1f Y%.1f R%.1f ArmRestL=P%.1f Y%.1f R%.1f\n"),
		Snapshot.HandIKLeftWeight,
		Snapshot.HandIKRightWeight,
		Snapshot.FootIKLeftWeight,
		Snapshot.FootIKRightWeight,
		Snapshot.ArmRestR.Pitch,
		Snapshot.ArmRestR.Yaw,
		Snapshot.ArmRestR.Roll,
		Snapshot.ArmRestL.Pitch,
		Snapshot.ArmRestL.Yaw,
		Snapshot.ArmRestL.Roll);
	Result += FString::Printf(TEXT("  Warnings=%s\n"), *WarningText);
	Result += TEXT("  Requested/Applied bones:\n");

	for (const FCrewDebugBonePose& BonePose : Snapshot.RequestedAndAppliedBones)
	{
		const FVector AppliedLocation = BonePose.AppliedTransform.GetLocation();
		Result += FString::Printf(
			TEXT("    %s Req=P%.1f Y%.1f R%.1f Applied=%d Loc=(%.1f, %.1f, %.1f)\n"),
			*BonePose.BoneName.ToString(),
			BonePose.RequestedRotation.Pitch,
			BonePose.RequestedRotation.Yaw,
			BonePose.RequestedRotation.Roll,
			BonePose.bHasAppliedTransform ? 1 : 0,
			AppliedLocation.X,
			AppliedLocation.Y,
			AppliedLocation.Z);
	}

	return Result;
}

void UCrewAnimDebugComponent::DumpSnapshotToLog() const
{
	UE_LOG(LogCrewAnimDebug, Log, TEXT("%s"), *FormatSnapshotForLog());
}

void UCrewAnimDebugComponent::PushSnapshot(const FCrewAnimDebugSnapshot& Snapshot)
{
	const USub3DDebugSettings* Settings = GetDefault<USub3DDebugSettings>();
	const int32 MaxSnapshots = Settings ? FMath::Clamp(Settings->CrewAnimDebugRecentSnapshotCount, 1, 600) : 120;

	RecentSnapshots.Add(Snapshot);
	while (RecentSnapshots.Num() > MaxSnapshots)
	{
		RecentSnapshots.RemoveAt(0, 1, EAllowShrinking::No);
	}
}
