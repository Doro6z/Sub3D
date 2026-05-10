#include "CompartmentVolumeComponent.h"

#include "Components/ActorComponent.h"
#include "Debug/Sub3DDebugSettings.h"
#include "DrawDebugHelpers.h"
#include "DynamicMeshBuilder.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialRenderProxy.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "SubFloodComponent.h"

UCompartmentVolumeComponent::UCompartmentVolumeComponent()
{
	// Probe role: QueryOnly + overlap on Pawn channel via "CompartmentProbe" profile
	// (DefaultEngine.ini). Crew capsule responds Overlap on ECC_CompartmentProbe, so
	// OnComponentBeginOverlap / OnComponentEndOverlap fire without interfering with
	// gameplay physics.
	SetCollisionProfileName(TEXT("CompartmentProbe"));
	SetGenerateOverlapEvents(true);
	SetCanEverAffectNavigation(false);
	SetHiddenInGame(true);
	SetVisibility(true);
	ShapeColor = FColor(50, 180, 220, 255);

	// Default box extent: 100cm each axis = 200x200x200cm volume.
	// Direct assignment instead of SetBoxExtent: the setter triggers
	// UBoxComponent::UpdateBodySetup, which calls NewObject<UBodySetup> with
	// no name. That is illegal inside a UObject constructor and crashes CDO
	// construction at editor launch.
	BoxExtent = FVector(100.f, 100.f, 100.f);
	LineThickness = 2.f;

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UCompartmentVolumeComponent::OnRegister()
{
	Super::OnRegister();
	ShapeColor = VolumeColor;
}

void UCompartmentVolumeComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const USub3DDebugSettings* Settings = GetDefault<USub3DDebugSettings>();
	if (!Settings || !Settings->bDrawCompartmentVolumes)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Wireframe box of the compartment in world space, oriented with the sub.
	const FTransform WorldXform = GetComponentTransform();
	DrawDebugBox(
		World,
		WorldXform.GetLocation(),
		GetScaledBoxExtent(),
		WorldXform.GetRotation(),
		VolumeColor,
		false,
		-1.f,
		SDPG_World,
		2.f);

	// Label = compartment id + current water level (if flood initialized).
	const float Level01 = GetWaterLevel01();
	const float HeightCm = GetWaterHeightCm();
	const FString Label = FString::Printf(
		TEXT("(CV) %s  H=%.0fcm  L=%.2f"),
		*CompartmentId.ToString(),
		HeightCm,
		Level01);

	// Position label inside the top of the volume rather than floating above it — keeps it
	// inside the box wireframe so multiple compartments don't overlap labels above them.
	DrawDebugString(
		World,
		WorldXform.GetLocation() + FVector(0.f, 0.f, GetScaledBoxExtent().Z * 0.5f),
		Label,
		nullptr,
		VolumeColor,
		0.f,
		true,
		1.f);
}

#if WITH_EDITOR
void UCompartmentVolumeComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UCompartmentVolumeComponent, VolumeColor))
	{
		ShapeColor = VolumeColor;
		MarkRenderStateDirty();
	}
}
#endif

// ── Flood data provider ──────────────────────────────────────────────────────

USubFloodComponent* UCompartmentVolumeComponent::GetFlood() const
{
	if (USubFloodComponent* Cached = CachedFlood.Get())
	{
		return Cached;
	}

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	USubFloodComponent* Found = Owner->FindComponentByClass<USubFloodComponent>();
	CachedFlood = Found;
	return Found;
}

float UCompartmentVolumeComponent::GetWaterHeightCm() const
{
	if (CompartmentId.IsNone())
	{
		return 0.f;
	}
	const USubFloodComponent* Flood = GetFlood();
	return Flood ? Flood->GetCompartmentWaterHeightCm(CompartmentId) : 0.f;
}

float UCompartmentVolumeComponent::GetWaterLevel01() const
{
	if (CompartmentId.IsNone())
	{
		return 0.f;
	}
	const USubFloodComponent* Flood = GetFlood();
	return Flood ? Flood->GetCompartmentFloodLevel01(CompartmentId) : 0.f;
}

float UCompartmentVolumeComponent::GetFloodBottomLocalZCm() const
{
	const AActor* Owner = GetOwner();
	const FTransform ActorTransform = Owner ? Owner->GetActorTransform() : FTransform::Identity;
	const FVector VolumeLocalCenter = ActorTransform.InverseTransformPosition(GetComponentLocation());
	const float BoxBottomLocalZ = VolumeLocalCenter.Z - GetScaledBoxExtent().Z;

	if (!FMath::IsNearlyZero(WalkableFloorZCmOverride))
	{
		return WalkableFloorZCmOverride;
	}

	return BoxBottomLocalZ;
}

float UCompartmentVolumeComponent::GetFloodMaxHeightCm() const
{
	const AActor* Owner = GetOwner();
	const FTransform ActorTransform = Owner ? Owner->GetActorTransform() : FTransform::Identity;
	const FVector VolumeLocalCenter = ActorTransform.InverseTransformPosition(GetComponentLocation());
	const float BoxTopLocalZ = VolumeLocalCenter.Z + GetScaledBoxExtent().Z;
	return FMath::Max(1.f, BoxTopLocalZ - GetFloodBottomLocalZCm());
}

FVector UCompartmentVolumeComponent::GetWaterSurfaceWorldLocation() const
{
	const AActor* Owner = GetOwner();
	const FTransform ActorTransform = Owner ? Owner->GetActorTransform() : FTransform::Identity;
	const FVector WorldUp = Owner ? Owner->GetActorUpVector() : FVector::UpVector;
	const FVector VolumeWorldCenter = GetComponentLocation();
	const FVector VolumeLocalCenter = ActorTransform.InverseTransformPosition(VolumeWorldCenter);

	// The manual Craniata path often places compartment boxes below the visible deck.
	// Use the authored walkable floor when available so the visual water starts where
	// the player expects instead of under the floor mesh.
	const FVector FloodFloorLocal(VolumeLocalCenter.X, VolumeLocalCenter.Y, GetFloodBottomLocalZCm());
	const FVector WorldBottom = ActorTransform.TransformPosition(FloodFloorLocal);

	// Raise by the water height along the sub's up axis (follows sub pitch/roll).
	const float HeightCm = GetWaterHeightCm();
	return WorldBottom + WorldUp * HeightCm;
}

// ────────────────────────────────────────────────────────────────────────────
// Custom SceneProxy — filled translucent X-ray faces for placement aid.
// Inherits FPrimitiveSceneProxy directly (not FShapeSceneProxy) to fully control
// rendering. Draws wireframe + 6 colored translucent faces in foreground DPG.
// ────────────────────────────────────────────────────────────────────────────

class FCompartmentVolumeSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniqueHash;
		return reinterpret_cast<size_t>(&UniqueHash);
	}

	FCompartmentVolumeSceneProxy(const UCompartmentVolumeComponent* InComponent)
		: FPrimitiveSceneProxy(InComponent)
		, BoxExtent(InComponent->GetUnscaledBoxExtent())
		, LineColor(InComponent->ShapeColor)
		, LineThickness(InComponent->GetEditorLineThickness())
		, FaceOpacity(InComponent->FaceOpacity)
		, bShowFilled(InComponent->bShowFilledFaces)
		, bXRay(InComponent->bDrawXRay)
	{
		FaceColors[0] = InComponent->FaceColorXPos;
		FaceColors[1] = InComponent->FaceColorXNeg;
		FaceColors[2] = InComponent->FaceColorYPos;
		FaceColors[3] = InComponent->FaceColorYNeg;
		FaceColors[4] = InComponent->FaceColorZPos;
		FaceColors[5] = InComponent->FaceColorZNeg;
		bWillEverBeLit = false;
	}

	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override
	{
		const ESceneDepthPriorityGroup DPG = bXRay ? SDPG_Foreground : SDPG_World;
		const FMatrix& LocalToWorldMat = GetLocalToWorld();
		const FBox Box(-BoxExtent, BoxExtent);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if (!(VisibilityMap & (1 << ViewIndex)))
			{
				continue;
			}
			const FSceneView* View = Views[ViewIndex];
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

			// Wireframe always.
			DrawWireBox(PDI, LocalToWorldMat, Box, LineColor, DPG, LineThickness, 0.f, /*bScreenSpace*/ false);

			// Filled translucent faces.
			if (bShowFilled && FaceOpacity > 0.f && GEngine && GEngine->DebugMeshMaterial)
			{
				UMaterialInterface* BaseMaterial = GEngine->DebugMeshMaterial;
				for (int32 FaceIdx = 0; FaceIdx < 6; ++FaceIdx)
				{
					FLinearColor TintedColor = FaceColors[FaceIdx];
					TintedColor.A = FaceOpacity;

					FColoredMaterialRenderProxy* TintProxy = new FColoredMaterialRenderProxy(
						BaseMaterial->GetRenderProxy(),
						TintedColor);
					Collector.RegisterOneFrameMaterialProxy(TintProxy);

					FDynamicMeshBuilder MeshBuilder(View->GetFeatureLevel());
					BuildBoxFace(MeshBuilder, FaceIdx, FColor::White);
					MeshBuilder.GetMesh(LocalToWorldMat, TintProxy, DPG, /*bDisableBackfaceCulling*/ true,
						/*bReceivesDecals*/ false, ViewIndex, Collector);
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Relevance;
		Relevance.bDrawRelevance = IsShown(View);
		Relevance.bDynamicRelevance = true;
		Relevance.bShadowRelevance = false;
		Relevance.bEditorPrimitiveRelevance = UseEditorCompositing(View);
		Relevance.bSeparateTranslucency = bShowFilled;
		Relevance.bNormalTranslucency = bShowFilled;
		return Relevance;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

private:
	FVector BoxExtent;
	FColor LineColor;
	float LineThickness;
	float FaceOpacity;
	bool bShowFilled;
	bool bXRay;
	FLinearColor FaceColors[6];

	void BuildBoxFace(FDynamicMeshBuilder& MeshBuilder, int32 FaceIdx, FColor Color) const
	{
		FVector V0, V1, V2, V3, Normal, Tangent;
		const float X = BoxExtent.X;
		const float Y = BoxExtent.Y;
		const float Z = BoxExtent.Z;

		switch (FaceIdx)
		{
		case 0: // +X (bow)
			V0 = FVector(+X, -Y, -Z); V1 = FVector(+X, +Y, -Z);
			V2 = FVector(+X, +Y, +Z); V3 = FVector(+X, -Y, +Z);
			Normal = FVector(1, 0, 0); Tangent = FVector(0, 1, 0);
			break;
		case 1: // -X (stern)
			V0 = FVector(-X, +Y, -Z); V1 = FVector(-X, -Y, -Z);
			V2 = FVector(-X, -Y, +Z); V3 = FVector(-X, +Y, +Z);
			Normal = FVector(-1, 0, 0); Tangent = FVector(0, -1, 0);
			break;
		case 2: // +Y (starboard)
			V0 = FVector(+X, +Y, -Z); V1 = FVector(-X, +Y, -Z);
			V2 = FVector(-X, +Y, +Z); V3 = FVector(+X, +Y, +Z);
			Normal = FVector(0, 1, 0); Tangent = FVector(-1, 0, 0);
			break;
		case 3: // -Y (port)
			V0 = FVector(-X, -Y, -Z); V1 = FVector(+X, -Y, -Z);
			V2 = FVector(+X, -Y, +Z); V3 = FVector(-X, -Y, +Z);
			Normal = FVector(0, -1, 0); Tangent = FVector(1, 0, 0);
			break;
		case 4: // +Z (top)
			V0 = FVector(-X, -Y, +Z); V1 = FVector(+X, -Y, +Z);
			V2 = FVector(+X, +Y, +Z); V3 = FVector(-X, +Y, +Z);
			Normal = FVector(0, 0, 1); Tangent = FVector(1, 0, 0);
			break;
		case 5: // -Z (bottom)
		default:
			V0 = FVector(-X, +Y, -Z); V1 = FVector(+X, +Y, -Z);
			V2 = FVector(+X, -Y, -Z); V3 = FVector(-X, -Y, -Z);
			Normal = FVector(0, 0, -1); Tangent = FVector(1, 0, 0);
			break;
		}

		const int32 I0 = MeshBuilder.AddVertex(FDynamicMeshVertex(FVector3f(V0), FVector3f(Tangent), FVector3f(Normal), FVector2f(0, 0), Color));
		const int32 I1 = MeshBuilder.AddVertex(FDynamicMeshVertex(FVector3f(V1), FVector3f(Tangent), FVector3f(Normal), FVector2f(1, 0), Color));
		const int32 I2 = MeshBuilder.AddVertex(FDynamicMeshVertex(FVector3f(V2), FVector3f(Tangent), FVector3f(Normal), FVector2f(1, 1), Color));
		const int32 I3 = MeshBuilder.AddVertex(FDynamicMeshVertex(FVector3f(V3), FVector3f(Tangent), FVector3f(Normal), FVector2f(0, 1), Color));
		MeshBuilder.AddTriangle(I0, I1, I2);
		MeshBuilder.AddTriangle(I0, I2, I3);
	}
};

FPrimitiveSceneProxy* UCompartmentVolumeComponent::CreateSceneProxy()
{
	// Custom proxy gives us filled translucent X-ray faces for placement.
	// Falls back to standard wireframe-only behaviour (parent UBoxComponent path)
	// if both bShowFilledFaces and bDrawXRay are off — but the proxy is still ours
	// (it draws wireframe too, redundantly with parent if we'd called Super; we
	// don't, since we render everything ourselves).
	return new FCompartmentVolumeSceneProxy(this);
}
