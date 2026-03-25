#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TraversalTypes.generated.h"

UENUM(BlueprintType)
enum class EConnectorShape : uint8
{
    Oval        UMETA(DisplayName = "Oval"),
    Wide        UMETA(DisplayName = "Wide"),
    Vertical    UMETA(DisplayName = "Vertical"),
    Spheroidal  UMETA(DisplayName = "Spheroidal")
};

UENUM(BlueprintType)
enum class ECarveShape : uint8
{
    OvalTunnel   UMETA(DisplayName = "OvalTunnel"),
    Spheroid     UMETA(DisplayName = "Spheroid"),
    VerticalDrop UMETA(DisplayName = "VerticalDrop")
};

UENUM(BlueprintType)
enum class ENodeType : uint8
{
    StartBuffer         UMETA(DisplayName = "StartBuffer"),
    OpenWaterTransit    UMETA(DisplayName = "OpenWaterTransit"),
    CanyonWide          UMETA(DisplayName = "CanyonWide"),
    CanyonNarrow        UMETA(DisplayName = "CanyonNarrow"),
    VerticalDrop        UMETA(DisplayName = "VerticalDrop"),
    HubCavern           UMETA(DisplayName = "HubCavern"),
    AmbushChoke         UMETA(DisplayName = "AmbushChoke"),
    DeadEndReward       UMETA(DisplayName = "DeadEndReward"),
    ExitRelief          UMETA(DisplayName = "ExitRelief")
};

UENUM(BlueprintType)
enum class EValidationResult : uint8
{
    Valid       UMETA(DisplayName = "Valid"),
    SoftFail    UMETA(DisplayName = "SoftFail"),
    HardFail    UMETA(DisplayName = "HardFail")
};

// Spécification d'entrée du générateur
USTRUCT(BlueprintType)
struct FTraversalGenSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CampaignSeed = 42;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RouteID = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TargetDistanceMeters = 3000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BiomeID = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Difficulty = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CrewSize = 1;
};

// Connecteur entre chunks
USTRUCT(BlueprintType)
struct FChunkConnector
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) EConnectorShape Shape = EConnectorShape::Oval;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float UsableWidth = 8000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float UsableHeight = 6000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TurnRadiusMin = 3500.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BiomeMask = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform LocalTransform = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct FChunkCarveParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) ECarveShape PrimaryShape = ECarveShape::OvalTunnel;
    
    // X=longueur, Y=largeur, Z=hauteur (cm)
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector BaseExtents = FVector(5000.f, 8000.f, 6000.f);
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float NoiseAmplitude = 500.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float NoiseFrequency = 0.001f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 NoiseOctaves = 3;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RadialSegments = 16;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 LengthSegments = 20;
};

// Template de chunk
USTRUCT(BlueprintType)
struct FChunkTemplate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ChunkID;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<ENodeType> CompatibleNodeTypes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FChunkConnector ConnectorIn;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FChunkConnector ConnectorOut;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float LengthMeters = 50.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FChunkCarveParams CarveParams;
};

// Instance de chunk résolue
USTRUCT(BlueprintType)
struct FChunkInstance
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName ChunkID;
    UPROPERTY(BlueprintReadOnly) FTransform WorldTransform;
    UPROPERTY(BlueprintReadOnly) FChunkConnector ConnectorIn;
    UPROPERTY(BlueprintReadOnly) FChunkConnector ConnectorOut;
    UPROPERTY(BlueprintReadOnly) int32 ChunkSeed = 0;
};

// Nœud du graphe
USTRUCT(BlueprintType)
struct FTraversalGraphNode
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 NodeID = 0;
    UPROPERTY(BlueprintReadOnly) ENodeType NodeType = ENodeType::OpenWaterTransit;
    UPROPERTY(BlueprintReadOnly) int32 NodeSeed = 0;
    UPROPERTY(BlueprintReadOnly) float PacingPosition = 0.0f;
    UPROPERTY(BlueprintReadOnly) TArray<int32> NextNodeIDs;
    UPROPERTY(BlueprintReadOnly) bool bIsFork = false;
    UPROPERTY(BlueprintReadOnly) bool bIsDeadEnd = false;
};
