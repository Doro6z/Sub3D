#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WorldGenTypes.h"
#include "CampaignGraphAsset.generated.h"

class URouteArchetypeDataAsset;
class UBiomeFieldProfileDataAsset;

UENUM(BlueprintType)
enum class ECampaignSegmentRole : uint8
{
	Entry,
	Main,
	SideBranch,
	Exit,
	Connector
};

USTRUCT(BlueprintType)
struct FCampaignSegmentDescriptor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	FName SegmentID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	ECampaignSegmentRole Role = ECampaignSegmentRole::Main;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	TObjectPtr<URouteArchetypeDataAsset> Archetype = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	TObjectPtr<UBiomeFieldProfileDataAsset> Biome = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	ETraversalComplexityTier ComplexityTier = ETraversalComplexityTier::Moderate;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="100.0"))
	float PreferredLengthMeters = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="0.0"))
	float DepthAtSegmentEndMeters = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	int32 SeedOffset = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	TArray<FName> NextSegmentIDs;
};

USTRUCT(BlueprintType)
struct FCampaignProgressState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign")
	int32 MasterSeed = 12345;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign")
	FName CurrentSegmentID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign")
	TSet<FName> ExploredSegmentIDs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign")
	TSet<FName> DiscoveredSegmentIDs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign")
	TMap<FName, int32> SegmentBuildHashes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign")
	float DeepestDepthMeters = 0.f;
};

UCLASS(BlueprintType)
class SUB3D_API UCampaignGraphAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	FName CampaignGraphID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	FName RootSegmentID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	FName TerminalSegmentID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="0.0"))
	float TotalDepthMeters = 4000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	TArray<FCampaignSegmentDescriptor> Segments;

	UFUNCTION(BlueprintCallable, Category="Campaign")
	bool IsValidGraph(FString& OutError) const;

	const FCampaignSegmentDescriptor* FindSegmentByID(FName SegmentID) const;
	int32 FindSegmentIndexByID(FName SegmentID) const;
	int32 CountIncomingConnections(FName SegmentID) const;
};
