#include "AnimNode_CrewProcedural.h"
#include "SubCrewAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"

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

void FAnimNode_CrewProcedural::ResolveBones(const FBoneContainer& RequiredBones)
{
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

	const USubCrewAnimInstance* AnimInst = Cast<USubCrewAnimInstance>(
		Output.AnimInstanceProxy->GetAnimInstanceObject());
	if (!AnimInst)
	{
		return;
	}

	const FRotator Rotations[] = {
		AnimInst->Proc_Pelvis_Rot,
		AnimInst->Proc_Spine01_Rot,
		AnimInst->Proc_Spine02_Rot,
		AnimInst->Proc_Spine03_Rot,
		AnimInst->Proc_Spine04_Rot,
		AnimInst->Proc_Spine05_Rot,
		AnimInst->Proc_Neck01_Rot,
		AnimInst->Proc_Head_Rot,
		AnimInst->Proc_ThighR_Rot,
		AnimInst->Proc_ThighL_Rot,
		AnimInst->Proc_CalfR_Rot,
		AnimInst->Proc_CalfL_Rot,
		AnimInst->Proc_FootR_Rot,
		AnimInst->Proc_FootL_Rot,
		AnimInst->Proc_UpperarmR_Rot,
		AnimInst->Proc_UpperarmL_Rot,
		AnimInst->Proc_LowerarmR_Rot,
		AnimInst->Proc_LowerarmL_Rot,
	};

	const FVector PelvisOffset = AnimInst->Proc_Pelvis_Offset;

	for (const FBoneEntry& Entry : ResolvedBones)
	{
		if (!Output.Pose.IsValidIndex(Entry.CompactIndex))
		{
			continue;
		}

		FTransform& BoneXform = Output.Pose[Entry.CompactIndex];

		const FRotator& AddRot = Rotations[Entry.RotIndex];
		if (!AddRot.IsNearlyZero(0.01f))
		{
			BoneXform.SetRotation(AddRot.Quaternion() * BoneXform.GetRotation());
		}

		if (Entry.bHasTranslation && !PelvisOffset.IsNearlyZero(0.01f))
		{
			BoneXform.AddToTranslation(PelvisOffset);
		}
	}
}

void FAnimNode_CrewProcedural::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT(" (%d bones)"), ResolvedBones.Num());
	DebugData.AddDebugItem(DebugLine);
	BasePose.GatherDebugData(DebugData);
}
