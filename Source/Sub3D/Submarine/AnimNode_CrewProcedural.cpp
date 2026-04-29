#include "AnimNode_CrewProcedural.h"
#include "SubCrewAnimInstance.h"

namespace
{
	struct FBoneDef
	{
		FName Name;
		bool bHasTranslation;
	};

	static const FBoneDef GBoneDefs[] = {
		{ "pelvis",      true  },
		{ "spine_01",    false },
		{ "spine_02",    false },
		{ "spine_03",    false },
		{ "spine_04",    false },
		{ "spine_05",    false },
		{ "neck_01",     false },
		{ "head",        false },
		{ "thigh_r",     false },
		{ "thigh_l",     false },
		{ "calf_r",      false },
		{ "calf_l",      false },
		{ "foot_r",      false },
		{ "foot_l",      false },
		{ "upperarm_r",  false },
		{ "upperarm_l",  false },
		{ "lowerarm_r",  false },
		{ "lowerarm_l",  false },
	};
}

FAnimNode_CrewProcedural::FAnimNode_CrewProcedural()
{
	ResetPoseSnapshot();
}

void FAnimNode_CrewProcedural::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	BasePose.Initialize(Context);
	bBonesResolved = false;
}

void FAnimNode_CrewProcedural::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	BasePose.CacheBones(Context);
	bBonesResolved = false;
}

void FAnimNode_CrewProcedural::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	FAnimNode_Base::Update_AnyThread(Context);
	BasePose.Update(Context);
}

void FAnimNode_CrewProcedural::PreUpdate(const UAnimInstance* InAnimInstance)
{
	const USubCrewAnimInstance* AnimInst = Cast<USubCrewAnimInstance>(InAnimInstance);
	if (!AnimInst)
	{
		ResetPoseSnapshot();
		return;
	}

	CopyPoseSnapshot(*AnimInst);
}

void FAnimNode_CrewProcedural::ResetPoseSnapshot()
{
	for (FRotator& Rotation : SnapshotRotations)
	{
		Rotation = FRotator::ZeroRotator;
	}

	SnapshotPelvisOffset = FVector::ZeroVector;
	bHasPoseSnapshot = false;
}

void FAnimNode_CrewProcedural::CopyPoseSnapshot(const USubCrewAnimInstance& AnimInstance)
{
	SnapshotRotations[0] = AnimInstance.Proc_Pelvis_Rot;
	SnapshotRotations[1] = AnimInstance.Proc_Spine01_Rot;
	SnapshotRotations[2] = AnimInstance.Proc_Spine02_Rot;
	SnapshotRotations[3] = AnimInstance.Proc_Spine03_Rot;
	SnapshotRotations[4] = AnimInstance.Proc_Spine04_Rot;
	SnapshotRotations[5] = AnimInstance.Proc_Spine05_Rot;
	SnapshotRotations[6] = AnimInstance.Proc_Neck01_Rot;
	SnapshotRotations[7] = AnimInstance.Proc_Head_Rot;
	SnapshotRotations[8] = AnimInstance.Proc_ThighR_Rot;
	SnapshotRotations[9] = AnimInstance.Proc_ThighL_Rot;
	SnapshotRotations[10] = AnimInstance.Proc_CalfR_Rot;
	SnapshotRotations[11] = AnimInstance.Proc_CalfL_Rot;
	SnapshotRotations[12] = AnimInstance.Proc_FootR_Rot;
	SnapshotRotations[13] = AnimInstance.Proc_FootL_Rot;
	SnapshotRotations[14] = AnimInstance.Proc_UpperarmR_Rot;
	SnapshotRotations[15] = AnimInstance.Proc_UpperarmL_Rot;
	SnapshotRotations[16] = AnimInstance.Proc_LowerarmR_Rot;
	SnapshotRotations[17] = AnimInstance.Proc_LowerarmL_Rot;

	SnapshotPelvisOffset = AnimInstance.Proc_Pelvis_Offset;
	bHasPoseSnapshot = true;
}

void FAnimNode_CrewProcedural::ResolveBones(const FBoneContainer& RequiredBones)
{
	static_assert(UE_ARRAY_COUNT(GBoneDefs) == ProceduralBoneCount, "Crew procedural bone table must match the pose snapshot.");

	ResolvedBones.Reset();

	for (int32 i = 0; i < UE_ARRAY_COUNT(GBoneDefs); ++i)
	{
		const int32 MeshIndex = RequiredBones.GetPoseBoneIndexForBoneName(GBoneDefs[i].Name);
		if (MeshIndex != INDEX_NONE)
		{
			FBoneEntry Entry;
			Entry.CompactIndex = RequiredBones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
			Entry.RotIndex = i;
			Entry.bHasTranslation = GBoneDefs[i].bHasTranslation;
			ResolvedBones.Add(Entry);
		}
	}

	bBonesResolved = true;
}

void FAnimNode_CrewProcedural::Evaluate_AnyThread(FPoseContext& Output)
{
	BasePose.Evaluate(Output);

	// Lazy resolve bones on first evaluate
	if (!bBonesResolved)
	{
		ResolveBones(Output.Pose.GetBoneContainer());
	}

	if (ResolvedBones.Num() == 0)
	{
		return;
	}

	if (!bHasPoseSnapshot)
	{
		return;
	}

	for (const FBoneEntry& Entry : ResolvedBones)
	{
		if (!Output.Pose.IsValidIndex(Entry.CompactIndex))
		{
			continue;
		}

		FTransform& BoneXform = Output.Pose[Entry.CompactIndex];

		const FRotator& AddRot = SnapshotRotations[Entry.RotIndex];
		if (!AddRot.IsNearlyZero(0.01f))
		{
			BoneXform.SetRotation(AddRot.Quaternion() * BoneXform.GetRotation());
		}

		if (Entry.bHasTranslation && !SnapshotPelvisOffset.IsNearlyZero(0.01f))
		{
			BoneXform.AddToTranslation(SnapshotPelvisOffset);
		}
	}
}

void FAnimNode_CrewProcedural::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT(" (%d bones, Snapshot=%d)"), ResolvedBones.Num(), bHasPoseSnapshot ? 1 : 0);
	DebugData.AddDebugItem(DebugLine);
	BasePose.GatherDebugData(DebugData);
}
