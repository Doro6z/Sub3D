#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"
#include "AnimNode_CrewProcedural.generated.h"

class UAnimInstance;
class USubCrewAnimInstance;

/**
 * Single anim node that applies all procedural bone transforms
 * from SubCrewAnimInstance. One node replaces 18 Modify Bone nodes.
 */
USTRUCT(BlueprintInternalUseOnly)
struct SUB3D_API FAnimNode_CrewProcedural : public FAnimNode_Base
{
	GENERATED_BODY()

	FAnimNode_CrewProcedural();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links)
	FPoseLink BasePose;

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

private:
	enum { ProceduralBoneCount = 18 };

	void ResolveBones(const FBoneContainer& RequiredBones);
	void ResetPoseSnapshot();
	void CopyPoseSnapshot(const USubCrewAnimInstance& AnimInstance);

	struct FBoneEntry
	{
		FCompactPoseBoneIndex CompactIndex = FCompactPoseBoneIndex(INDEX_NONE);
		int32 RotIndex = 0;
		bool bHasTranslation = false;
	};

	TArray<FBoneEntry> ResolvedBones;
	FRotator SnapshotRotations[ProceduralBoneCount];
	FVector SnapshotPelvisOffset = FVector::ZeroVector;
	bool bBonesResolved = false;
	bool bHasPoseSnapshot = false;
};
