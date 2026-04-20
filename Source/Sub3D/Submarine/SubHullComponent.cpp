#include "SubHullComponent.h"

#include "DrawDebugHelpers.h"
#include "Components/PrimitiveComponent.h"
#include "Net/UnrealNetwork.h"
#include "SubmarineBase.h"
#include "SubmarineLayoutAsset.h"
#include "Data/CompiledSubmarineRuntimeAsset.h"

namespace
{
bool HasAuthoritativeOwner(const UActorComponent* Component)
{
	const AActor* Owner = Component ? Component->GetOwner() : nullptr;
	if (!Owner)
	{
		return false;
	}

	return Owner->GetLocalRole() == ROLE_Authority;
}
}

USubHullComponent::USubHullComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void USubHullComponent::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthoritativeOwner(this))
	{
		InitializeFromLayout(LayoutAsset);
		EnsureFallbackLayout();
	}
}

void USubHullComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (GetOwner() && GetOwner()->GetLevel())
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	}

	if (!HasAuthoritativeOwner(this))
	{
		return;
	}

	UpdateFlowFields();

	if (!GetWorld())
	{
		return;
	}

	if (bDrawDebug)
	{
		const FTransform ActorTransform = GetOwner()->GetActorTransform();

		for (const FBreachClusterState& Cluster : BreachClusters)
		{
			const FVector WorldCenter = ActorTransform.TransformPosition(Cluster.LocalCenter);
			DrawDebugSphere(GetWorld(), WorldCenter, Cluster.InscribedRadiusCm, 16, FColor::Cyan, false, -1.f, 0, 1.f);
		}

		for (const FBreachFlowField& Flow : FlowFields)
		{
			const FVector WorldCenter = ActorTransform.TransformPosition(Flow.LocalCenter);
			const FVector WorldDir = ActorTransform.TransformVectorNoScale(Flow.Direction).GetSafeNormal();
			DrawDebugLine(GetWorld(), WorldCenter, WorldCenter + WorldDir * 120.f, FColor::Orange, false, -1.f, 0, 2.f);
			DrawDebugSphere(GetWorld(), WorldCenter, Flow.OuterRadiusCm, 12, FColor::Green, false, -1.f, 0, 0.8f);
		}
	}

	if (bDrawDebugSheets)
	{
		DrawDebugSheets();
	}
}

void USubHullComponent::DrawDebugSheets() const
{
	UWorld* World = GetWorld();
	if (!World || !GetOwner())
	{
		return;
	}

	const FTransform ActorTransform = GetOwner()->GetActorTransform();

	for (int32 SheetIndex = 0; SheetIndex < StructuralSheets.Num(); ++SheetIndex)
	{
		const FStructuralSheetDef& Sheet = StructuralSheets[SheetIndex];

		// Four corners in local space: origin is the centre; offsets go ±half along each tangent.
		const FVector HalfX = Sheet.LocalTangentX * (Sheet.SizeCm.X * 0.5f);
		const FVector HalfY = Sheet.LocalTangentY * (Sheet.SizeCm.Y * 0.5f);

		const FVector LocalCorners[4] = {
			Sheet.LocalOrigin - HalfX - HalfY,
			Sheet.LocalOrigin + HalfX - HalfY,
			Sheet.LocalOrigin + HalfX + HalfY,
			Sheet.LocalOrigin - HalfX + HalfY,
		};

		FVector WorldCorners[4];
		for (int32 i = 0; i < 4; ++i)
		{
			WorldCorners[i] = ActorTransform.TransformPosition(LocalCorners[i]);
		}

		// Compute mean damage across cells so the outline color reflects sheet health.
		float MeanDamage = 0.f;
		int32 DamagedCells = 0;
		if (SheetStates.IsValidIndex(SheetIndex))
		{
			const FStructuralSheetRuntimeState& State = SheetStates[SheetIndex];
			if (State.Cells.Num() > 0)
			{
				float Sum = 0.f;
				for (const FStructuralCellState& Cell : State.Cells)
				{
					Sum += Cell.Damage01;
					if (Cell.Damage01 > 0.01f)
					{
						++DamagedCells;
					}
				}
				MeanDamage = Sum / State.Cells.Num();
			}
		}

		// Outline color: green = healthy, yellow = stressed, red = breaching threshold.
		const FColor OutlineColor = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, FMath::Clamp(MeanDamage * 2.f, 0.f, 1.f)).ToFColor(true);

		// Four edges.
		DrawDebugLine(World, WorldCorners[0], WorldCorners[1], OutlineColor, false, -1.f, 0, 2.f);
		DrawDebugLine(World, WorldCorners[1], WorldCorners[2], OutlineColor, false, -1.f, 0, 2.f);
		DrawDebugLine(World, WorldCorners[2], WorldCorners[3], OutlineColor, false, -1.f, 0, 2.f);
		DrawDebugLine(World, WorldCorners[3], WorldCorners[0], OutlineColor, false, -1.f, 0, 2.f);

		// Diagonals help read the sheet orientation at a glance.
		DrawDebugLine(World, WorldCorners[0], WorldCorners[2], OutlineColor, false, -1.f, 0, 0.8f);
		DrawDebugLine(World, WorldCorners[1], WorldCorners[3], OutlineColor, false, -1.f, 0, 0.8f);

		// Normal arrow at the centre so bow/stern/port/starboard is obvious.
		const FVector WorldOrigin = ActorTransform.TransformPosition(Sheet.LocalOrigin);
		const FVector WorldNormal = ActorTransform.TransformVectorNoScale(Sheet.LocalNormal).GetSafeNormal();
		DrawDebugDirectionalArrow(World, WorldOrigin, WorldOrigin + WorldNormal * 80.f, 20.f, FColor::White, false, -1.f, 0, 2.f);

		// Label: SheetId + damage summary.
		const FString Label = FString::Printf(
			TEXT("%s\n%.0f%% dmg | %d/%d cells"),
			*Sheet.SheetId.ToString(),
			MeanDamage * 100.f,
			DamagedCells,
			SheetStates.IsValidIndex(SheetIndex) ? SheetStates[SheetIndex].Cells.Num() : 0);
		DrawDebugString(World, WorldOrigin + WorldNormal * 30.f, Label, nullptr, FColor::White, 0.f, true, 1.2f);

		// Per-cell coloring. Skipped by default to avoid 1600 draws/frame; opt in
		// via bDrawDebugSheetCellsAlways, or the damaged ones are always drawn.
		if (!SheetStates.IsValidIndex(SheetIndex))
		{
			continue;
		}

		const FStructuralSheetRuntimeState& State = SheetStates[SheetIndex];
		const int32 ResX = FMath::Max(1, Sheet.GridResolutionX);
		const int32 ResY = FMath::Max(1, Sheet.GridResolutionY);
		const FVector CellStepX = Sheet.LocalTangentX * (Sheet.SizeCm.X / ResX);
		const FVector CellStepY = Sheet.LocalTangentY * (Sheet.SizeCm.Y / ResY);
		const FVector CellOriginLocal = Sheet.LocalOrigin - HalfX - HalfY;

		for (int32 CellIdx = 0; CellIdx < State.Cells.Num(); ++CellIdx)
		{
			const FStructuralCellState& Cell = State.Cells[CellIdx];
			const bool bDamaged = Cell.Damage01 > 0.01f;
			if (!bDamaged && !bDrawDebugSheetCellsAlways)
			{
				continue;
			}

			const int32 CellX = CellIdx % ResX;
			const int32 CellY = CellIdx / ResX;
			const FVector LocalCenter = CellOriginLocal
				+ CellStepX * (CellX + 0.5f)
				+ CellStepY * (CellY + 0.5f);
			const FVector WorldCenter = ActorTransform.TransformPosition(LocalCenter);

			const FColor CellColor = FLinearColor::LerpUsingHSV(
				FLinearColor(0.f, 1.f, 0.f, 0.25f),
				FLinearColor(1.f, 0.f, 0.f, 1.f),
				FMath::Clamp(Cell.Damage01, 0.f, 1.f)).ToFColor(true);

			DrawDebugPoint(World, WorldCenter, bDamaged ? 8.f : 4.f, CellColor, false, -1.f);
		}
	}
}

void USubHullComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USubHullComponent, SheetStates);
	DOREPLIFETIME(USubHullComponent, BreachClusters);
	DOREPLIFETIME(USubHullComponent, FlowFields);
}

void USubHullComponent::InitializeFromCompiledHull(const UCompiledSubmarineRuntimeAsset* Asset)
{
	StructuralSheets.Reset();
	SheetStates.Reset();
	BreachClusters.Reset();
	FlowFields.Reset();

	if (!Asset)
	{
		return;
	}

	// Resolve the first compartment ID from the flood graph for sheet parenting.
	FName MainCompId = NAME_None;
	for (const FDerivedFloodVolume& Volume : Asset->FloodGraph.Volumes)
	{
		if (!Volume.VolumeId.IsNone())
		{
			MainCompId = Volume.VolumeId;
			break;
		}
	}

	// Create fallback sheets for impact projection.
	if (!MainCompId.IsNone())
	{
		const float HullLength = FMath::Max(100.f, Asset->HullLengthCm);
		
		float MaxWidth = 100.f;
		float MaxHeight = 100.f;
		for (const FSub3DCompiledHullSection& Section : Asset->HullData.Sections)
		{
			MaxWidth = FMath::Max(MaxWidth, Section.HalfWidthCm * 2.f);
			MaxHeight = FMath::Max(MaxHeight, Section.HalfHeightCm * 2.f);
		}

		auto AddProceduralSheet = [&](const TCHAR* Name, const FVector& Origin, const FVector& Normal, const FVector& TangentX, const FVector& TangentY, const FVector2D& Size)
		{
			FStructuralSheetDef Sheet;
			Sheet.SheetId = FName(Name);
			Sheet.ParentCompartmentId = MainCompId;
			Sheet.LocalOrigin = Origin;
			Sheet.LocalNormal = Normal;
			Sheet.LocalTangentX = TangentX;
			Sheet.LocalTangentY = TangentY;
			Sheet.SizeCm = Size;
			Sheet.GridResolutionX = 20;
			Sheet.GridResolutionY = 20;
			Sheet.bCanOpenToExterior = true;
			Sheet.bSupportsVisualRupture = true;
			Sheet.MaterialStrength = 1.0f;
			Sheet.ThicknessCm = 15.0f;
			StructuralSheets.Add(Sheet);
		};

		const float HalfW = MaxWidth * 0.5f;
		const float HalfH = MaxHeight * 0.5f;

		AddProceduralSheet(TEXT("Hull_Port"),      FVector(0.f, -HalfW, 0.f), FVector(0.f, -1.f, 0.f), FVector(1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f), FVector2D(HullLength, MaxHeight));
		AddProceduralSheet(TEXT("Hull_Starboard"), FVector(0.f, HalfW, 0.f),  FVector(0.f, 1.f, 0.f),  FVector(1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f), FVector2D(HullLength, MaxHeight));
		AddProceduralSheet(TEXT("Hull_Top"),       FVector(0.f, 0.f, HalfH),  FVector(0.f, 0.f, 1.f),  FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), FVector2D(HullLength, MaxWidth));
		AddProceduralSheet(TEXT("Hull_Bottom"),    FVector(0.f, 0.f, -HalfH), FVector(0.f, 0.f, -1.f), FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), FVector2D(HullLength, MaxWidth));

		for (const FStructuralSheetDef& Sheet : StructuralSheets)
		{
			const int32 NumCells = Sheet.GridResolutionX * Sheet.GridResolutionY;
			FStructuralSheetRuntimeState Runtime;
			Runtime.SheetId = Sheet.SheetId;
			Runtime.Cells.SetNum(NumCells);
			for (int32 CellIndex = 0; CellIndex < NumCells; ++CellIndex)
			{
				Runtime.Cells[CellIndex].CellIndex = static_cast<uint16>(CellIndex);
				Runtime.Cells[CellIndex].ThicknessRemaining = Sheet.ThicknessCm;
			}
			SheetStates.Add(Runtime);
		}
	}

}

void USubHullComponent::InitializeFromLayout(const USubmarineLayoutAsset* InLayout)
{
	StructuralSheets.Reset();
	SheetStates.Reset();
	BreachClusters.Reset();
	FlowFields.Reset();

	if (!InLayout)
	{
		return;
	}

	StructuralSheets = InLayout->StructuralSheets;

	for (const FStructuralSheetDef& Sheet : StructuralSheets)
	{
		const int32 NumCells = FMath::Max(1, Sheet.GridResolutionX) * FMath::Max(1, Sheet.GridResolutionY);
		FStructuralSheetRuntimeState Runtime;
		Runtime.SheetId = Sheet.SheetId;
		Runtime.Cells.Reserve(NumCells);
		for (int32 CellIndex = 0; CellIndex < NumCells; ++CellIndex)
		{
			FStructuralCellState Cell;
			Cell.CellIndex = static_cast<uint16>(CellIndex);
			Cell.ThicknessRemaining = Sheet.ThicknessCm;
			Runtime.Cells.Add(Cell);
		}
		SheetStates.Add(Runtime);
	}
}

void USubHullComponent::EnsureFallbackLayout()
{
	if (StructuralSheets.Num() > 0)
	{
		return;
	}

	const ASubmarineBase* SubBase = Cast<ASubmarineBase>(GetOwner());
	if (!SubBase)
	{
		return;
	}

	UPrimitiveComponent* BoundsComponent = SubBase->GetMovementCollisionComponent();
	if (!BoundsComponent)
	{
		BoundsComponent = SubBase->HullMesh;
	}
	if (!BoundsComponent)
	{
		return;
	}

	const FBoxSphereBounds Bounds = BoundsComponent->CalcBounds(BoundsComponent->GetComponentTransform());
	const FVector Extent = Bounds.BoxExtent;
	const FVector LocalCenter = GetOwner()->GetActorTransform().InverseTransformPosition(Bounds.Origin);

	const FName MainCompId = FName(TEXT("HullMain"));

	StructuralSheets.Reset();
	auto AddFallbackSheet = [&](const TCHAR* Name, const FVector& Origin, const FVector& Normal, const FVector& TangentX, const FVector& TangentY, const FVector2D& Size, bool bExterior)
	{
		FStructuralSheetDef Sheet;
		Sheet.SheetId = FName(Name);
		Sheet.ParentCompartmentId = MainCompId;
		Sheet.LocalOrigin = Origin;
		Sheet.LocalNormal = Normal;
		Sheet.LocalTangentX = TangentX;
		Sheet.LocalTangentY = TangentY;
		Sheet.SizeCm = Size;
		Sheet.GridResolutionX = 20;
		Sheet.GridResolutionY = 20;
		Sheet.bCanOpenToExterior = bExterior;
		Sheet.bSupportsVisualRupture = bExterior;
		Sheet.ExteriorVisualMaterialSlot = 0;
		Sheet.VisualLocalOrigin = Origin;
		Sheet.VisualLocalTangentX = TangentX;
		Sheet.VisualLocalTangentY = TangentY;
		Sheet.VisualProjectionSizeCm = Size;
		Sheet.MaxVisibleRuptureRadiusCm = bExterior ? FMath::Max(40.f, 0.35f * FMath::Min(Size.X, Size.Y)) : 0.f;
		Sheet.PreferredRuptureBorderScale = 1.f;
		StructuralSheets.Add(Sheet);
	};

	// Four main lateral sheets to allow impacts anywhere around the hull.
	AddFallbackSheet(TEXT("HullSheet_Port"),      LocalCenter + FVector(0.f, -Extent.Y, 0.f), FVector(0.f, -1.f, 0.f), FVector(1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f), FVector2D(Extent.X * 2.f, Extent.Z * 2.f), true);
	AddFallbackSheet(TEXT("HullSheet_Starboard"), LocalCenter + FVector(0.f,  Extent.Y, 0.f), FVector(0.f,  1.f, 0.f), FVector(1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f), FVector2D(Extent.X * 2.f, Extent.Z * 2.f), true);
	AddFallbackSheet(TEXT("HullSheet_Top"),       LocalCenter + FVector(0.f, 0.f,  Extent.Z), FVector(0.f, 0.f,  1.f), FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), FVector2D(Extent.X * 2.f, Extent.Y * 2.f), true);
	AddFallbackSheet(TEXT("HullSheet_Bottom"),    LocalCenter + FVector(0.f, 0.f, -Extent.Z), FVector(0.f, 0.f, -1.f), FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), FVector2D(Extent.X * 2.f, Extent.Y * 2.f), true);

	SheetStates.Reset();
	for (const FStructuralSheetDef& Sheet : StructuralSheets)
	{
		const int32 NumCells = FMath::Max(1, Sheet.GridResolutionX) * FMath::Max(1, Sheet.GridResolutionY);
		FStructuralSheetRuntimeState Runtime;
		Runtime.SheetId = Sheet.SheetId;
		Runtime.Cells.SetNum(NumCells);
		for (int32 CellIndex = 0; CellIndex < NumCells; ++CellIndex)
		{
			Runtime.Cells[CellIndex].CellIndex = static_cast<uint16>(CellIndex);
			Runtime.Cells[CellIndex].ThicknessRemaining = Sheet.ThicknessCm;
		}
		SheetStates.Add(Runtime);
	}

}

void USubHullComponent::ApplyHullImpact(const FVector& LocalHitPosition, float Damage, float RadiusCm)
{
	if (!HasAuthoritativeOwner(this) || Damage <= 0.f || StructuralSheets.Num() == 0)
	{
		return;
	}

	int32 SheetIndex = INDEX_NONE;
	FVector2D UV(0.5f, 0.5f);
	if (!ProjectImpactToSheet(LocalHitPosition, SheetIndex, UV))
	{
		return;
	}

	ApplyImpactToSheet(SheetIndex, UV, Damage, RadiusCm);
	BroadcastHullDamageUpdated();
	RebuildBreachClusters();
}

bool USubHullComponent::RepairAtLocalPoint(const FVector& LocalRepairPosition, float RepairStrength, float RadiusCm)
{
	if (!HasAuthoritativeOwner(this) || RepairStrength <= 0.f || StructuralSheets.Num() == 0)
	{
		return false;
	}

	int32 SheetIndex = INDEX_NONE;
	FVector2D UV(0.5f, 0.5f);
	if (!ProjectImpactToSheet(LocalRepairPosition, SheetIndex, UV))
	{
		return false;
	}

	if (!StructuralSheets.IsValidIndex(SheetIndex) || !SheetStates.IsValidIndex(SheetIndex))
	{
		return false;
	}

	const FStructuralSheetDef& Sheet = StructuralSheets[SheetIndex];
	FStructuralSheetRuntimeState& Runtime = SheetStates[SheetIndex];
	const int32 GridX = FMath::Max(1, Sheet.GridResolutionX);
	const int32 GridY = FMath::Max(1, Sheet.GridResolutionY);
	const float RadiusU = RadiusCm / FMath::Max(1.f, Sheet.SizeCm.X);
	const float RadiusV = RadiusCm / FMath::Max(1.f, Sheet.SizeCm.Y);
	const float RadiusNorm = FMath::Max(RadiusU, RadiusV);
	bool bChanged = false;

	for (int32 Y = 0; Y < GridY; ++Y)
	{
		for (int32 X = 0; X < GridX; ++X)
		{
			const float CellU = (static_cast<float>(X) + 0.5f) / static_cast<float>(GridX);
			const float CellV = (static_cast<float>(Y) + 0.5f) / static_cast<float>(GridY);
			const float Dist = FVector2D::Distance(UV, FVector2D(CellU, CellV));
			if (Dist > RadiusNorm)
			{
				continue;
			}

			const float Falloff = 1.f - (Dist / FMath::Max(KINDA_SMALL_NUMBER, RadiusNorm));
			const float AppliedRepair = RepairStrength * Falloff * 0.01f;
			const int32 CellIndex = Y * GridX + X;
			FStructuralCellState& Cell = Runtime.Cells[CellIndex];

			const float PrevDamage = Cell.Damage01;
			Cell.Damage01 = FMath::Max(0.f, Cell.Damage01 - AppliedRepair);
			Cell.ThicknessRemaining = FMath::Min(Sheet.ThicknessCm, Cell.ThicknessRemaining + AppliedRepair * DamageToThicknessScale * 50.f);
			Cell.bLeaking = Cell.Damage01 >= LeakThreshold;
			Cell.bOpen = (Cell.Damage01 >= OpenThreshold) || (Cell.ThicknessRemaining <= 0.f);
			bChanged |= !FMath::IsNearlyEqual(PrevDamage, Cell.Damage01);
		}
	}

	if (bChanged)
	{
		BroadcastHullDamageUpdated();
		RebuildBreachClusters();
	}

	return bChanged;
}

bool USubHullComponent::RepairAtWorldPoint(const FVector& WorldRepairPosition, float RepairStrength, float RadiusCm)
{
	if (!GetOwner())
	{
		return false;
	}

	const FVector LocalRepairPosition = GetOwner()->GetActorTransform().InverseTransformPosition(WorldRepairPosition);
	return RepairAtLocalPoint(LocalRepairPosition, RepairStrength, RadiusCm);
}

bool USubHullComponent::SampleSuctionAtWorldLocation(const FVector& WorldLocation, FVector& OutWorldDirection, float& OutForceScale, EBreachPassageState& OutPassageState) const
{
	OutWorldDirection = FVector::ZeroVector;
	OutForceScale = 0.f;
	OutPassageState = EBreachPassageState::LeakOnly;

	if (!GetOwner() || FlowFields.Num() == 0)
	{
		return false;
	}

	const FTransform ActorTransform = GetOwner()->GetActorTransform();
	const FVector LocalPoint = ActorTransform.InverseTransformPosition(WorldLocation);

	int32 BestRank = 0;
	auto PassageRank = [](EBreachPassageState State) -> int32
	{
		switch (State)
		{
		case EBreachPassageState::CreatureEnterable: return 4;
		case EBreachPassageState::ActorEjectable: return 3;
		case EBreachPassageState::StrongSuction: return 2;
		case EBreachPassageState::LeakOnly: default: return 1;
		}
	};

	for (const FBreachFlowField& Flow : FlowFields)
	{
		const FVector ToPoint = LocalPoint - Flow.LocalCenter;
		const float Distance = ToPoint.Size();
		if (Distance > Flow.OuterRadiusCm)
		{
			continue;
		}

		const float T = FMath::Clamp((Distance - Flow.InnerRadiusCm) / FMath::Max(1.f, Flow.OuterRadiusCm - Flow.InnerRadiusCm), 0.f, 1.f);
		const float Falloff = 1.f - T;
		const FVector LocalDir = Flow.Direction.GetSafeNormal();
		const FVector WorldDir = ActorTransform.TransformVectorNoScale(LocalDir).GetSafeNormal();
		OutWorldDirection += WorldDir * Falloff;
		OutForceScale += Flow.ForceScale * Falloff;

		const int32 Rank = PassageRank(Flow.PassageState);
		if (Rank > BestRank)
		{
			BestRank = Rank;
			OutPassageState = Flow.PassageState;
		}
	}

	if (OutForceScale <= KINDA_SMALL_NUMBER || OutWorldDirection.IsNearlyZero())
	{
		OutWorldDirection = FVector::ZeroVector;
		OutForceScale = 0.f;
		OutPassageState = EBreachPassageState::LeakOnly;
		return false;
	}

	OutWorldDirection.Normalize();
	return true;
}

float USubHullComponent::GetLargestOpenRadiusCm() const
{
	float MaxRadius = 0.f;
	for (const FBreachClusterState& Cluster : BreachClusters)
	{
		MaxRadius = FMath::Max(MaxRadius, Cluster.InscribedRadiusCm);
	}
	return MaxRadius;
}

bool USubHullComponent::ProjectImpactToSheet(const FVector& LocalHitPosition, int32& OutSheetIndex, FVector2D& OutUV) const
{
	OutSheetIndex = INDEX_NONE;
	float BestDistance = TNumericLimits<float>::Max();

	for (int32 SheetIndex = 0; SheetIndex < StructuralSheets.Num(); ++SheetIndex)
	{
		const FStructuralSheetDef& Sheet = StructuralSheets[SheetIndex];
		const FVector N = Sheet.LocalNormal.GetSafeNormal();
		const FVector Tx = Sheet.LocalTangentX.GetSafeNormal();
		const FVector Ty = Sheet.LocalTangentY.GetSafeNormal();
		const FVector Delta = LocalHitPosition - Sheet.LocalOrigin;

		const float PlaneDistance = FMath::Abs(FVector::DotProduct(Delta, N));
		const float U = FVector::DotProduct(Delta, Tx) / FMath::Max(1.f, Sheet.SizeCm.X) + 0.5f;
		const float V = FVector::DotProduct(Delta, Ty) / FMath::Max(1.f, Sheet.SizeCm.Y) + 0.5f;

		if (U < 0.f || U > 1.f || V < 0.f || V > 1.f)
		{
			continue;
		}

		if (PlaneDistance < BestDistance)
		{
			BestDistance = PlaneDistance;
			OutSheetIndex = SheetIndex;
			OutUV = FVector2D(U, V);
		}
	}

	return OutSheetIndex != INDEX_NONE;
}

void USubHullComponent::ApplyImpactToSheet(int32 SheetIndex, const FVector2D& UV, float Damage, float RadiusCm)
{
	if (!StructuralSheets.IsValidIndex(SheetIndex) || !SheetStates.IsValidIndex(SheetIndex))
	{
		return;
	}

	const FStructuralSheetDef& Sheet = StructuralSheets[SheetIndex];
	FStructuralSheetRuntimeState& Runtime = SheetStates[SheetIndex];
	const int32 GridX = FMath::Max(1, Sheet.GridResolutionX);
	const int32 GridY = FMath::Max(1, Sheet.GridResolutionY);

	const float RadiusU = RadiusCm / FMath::Max(1.f, Sheet.SizeCm.X);
	const float RadiusV = RadiusCm / FMath::Max(1.f, Sheet.SizeCm.Y);
	const float RadiusNorm = FMath::Max(RadiusU, RadiusV);

	for (int32 Y = 0; Y < GridY; ++Y)
	{
		for (int32 X = 0; X < GridX; ++X)
		{
			const float CellU = (static_cast<float>(X) + 0.5f) / static_cast<float>(GridX);
			const float CellV = (static_cast<float>(Y) + 0.5f) / static_cast<float>(GridY);
			const float Dist = FVector2D::Distance(UV, FVector2D(CellU, CellV));
			if (Dist > RadiusNorm)
			{
				continue;
			}

			const float Falloff = 1.f - (Dist / FMath::Max(KINDA_SMALL_NUMBER, RadiusNorm));
			const float AppliedDamage = Damage * Falloff * Sheet.MaterialStrength;
			const int32 CellIndex = Y * GridX + X;
			FStructuralCellState& Cell = Runtime.Cells[CellIndex];
			Cell.Damage01 = FMath::Max(Cell.Damage01, FMath::Min(2.f, Cell.Damage01 + AppliedDamage * 0.01f));
			Cell.ThicknessRemaining = FMath::Max(0.f, Cell.ThicknessRemaining - AppliedDamage * DamageToThicknessScale * 0.01f);
			Cell.bLeaking = Cell.Damage01 >= LeakThreshold;
			Cell.bOpen = (Cell.Damage01 >= OpenThreshold) || (Cell.ThicknessRemaining <= 0.f);
		}
	}
}

void USubHullComponent::RebuildBreachClusters()
{
	BreachClusters.Reset();

	for (int32 SheetIndex = 0; SheetIndex < StructuralSheets.Num(); ++SheetIndex)
	{
		const FStructuralSheetDef& Sheet = StructuralSheets[SheetIndex];
		const FStructuralSheetRuntimeState& Runtime = SheetStates[SheetIndex];
		const int32 GridX = FMath::Max(1, Sheet.GridResolutionX);
		const int32 GridY = FMath::Max(1, Sheet.GridResolutionY);
		if (Runtime.Cells.Num() != GridX * GridY)
		{
			continue;
		}

		TArray<uint8> Visited;
		Visited.Init(0, Runtime.Cells.Num());
		const float CellWidthCm = Sheet.SizeCm.X / static_cast<float>(GridX);
		const float CellHeightCm = Sheet.SizeCm.Y / static_cast<float>(GridY);
		const float CellAreaCm2 = CellWidthCm * CellHeightCm;
		const FVector Tx = Sheet.LocalTangentX.GetSafeNormal();
		const FVector Ty = Sheet.LocalTangentY.GetSafeNormal();

		for (int32 CellIndex = 0; CellIndex < Runtime.Cells.Num(); ++CellIndex)
		{
			if (Visited[CellIndex] || !Runtime.Cells[CellIndex].bOpen)
			{
				continue;
			}

			TArray<int32> Queue;
			Queue.Add(CellIndex);
			Visited[CellIndex] = 1;

			TArray<int32> ClusterCells;
			FVector AccumCenter = FVector::ZeroVector;

			while (Queue.Num() > 0)
			{
				const int32 Current = Queue.Pop();
				ClusterCells.Add(Current);

				const int32 CX = Current % GridX;
				const int32 CY = Current / GridX;
				const float U = (static_cast<float>(CX) + 0.5f) / static_cast<float>(GridX);
				const float V = (static_cast<float>(CY) + 0.5f) / static_cast<float>(GridY);
				const FVector LocalPos = Sheet.LocalOrigin
					+ Tx * ((U - 0.5f) * Sheet.SizeCm.X)
					+ Ty * ((V - 0.5f) * Sheet.SizeCm.Y);
				AccumCenter += LocalPos;

				const int32 Neighbors[4][2] = {
					{CX + 1, CY},
					{CX - 1, CY},
					{CX, CY + 1},
					{CX, CY - 1}
				};

				for (int32 N = 0; N < 4; ++N)
				{
					const int32 NX = Neighbors[N][0];
					const int32 NY = Neighbors[N][1];
					if (NX < 0 || NX >= GridX || NY < 0 || NY >= GridY)
					{
						continue;
					}
					const int32 NeighborIndex = NY * GridX + NX;
					if (Visited[NeighborIndex] || !Runtime.Cells[NeighborIndex].bOpen)
					{
						continue;
					}
					Visited[NeighborIndex] = 1;
					Queue.Add(NeighborIndex);
				}
			}

			if (ClusterCells.Num() == 0)
			{
				continue;
			}

			FBreachClusterState Cluster;
			Cluster.SheetId = Sheet.SheetId;
			Cluster.LocalCenter = AccumCenter / static_cast<float>(ClusterCells.Num());
			Cluster.LocalNormal = Sheet.LocalNormal.GetSafeNormal();
			Cluster.OpenAreaCm2 = ClusterCells.Num() * CellAreaCm2;
			Cluster.InscribedRadiusCm = FMath::Sqrt(FMath::Max(1.f, Cluster.OpenAreaCm2) / PI);
			Cluster.bTouchesExterior = Sheet.bCanOpenToExterior;
			BreachClusters.Add(Cluster);
		}
	}

	BroadcastBreachesUpdated();
}

void USubHullComponent::UpdateFlowFields()
{
	FlowFields.Reset();

	for (const FBreachClusterState& Cluster : BreachClusters)
	{
		const FStructuralSheetDef* Sheet = StructuralSheets.FindByPredicate([&Cluster](const FStructuralSheetDef& Candidate)
		{
			return Candidate.SheetId == Cluster.SheetId;
		});

		FBreachFlowField Flow;
		Flow.SheetId = Cluster.SheetId;
		Flow.LocalCenter = Cluster.LocalCenter;
		Flow.Direction = -Cluster.LocalNormal.GetSafeNormal();
		Flow.InnerRadiusCm = FMath::Clamp(Cluster.InscribedRadiusCm * 0.6f, 20.f, 300.f);
		Flow.OuterRadiusCm = BaseSuctionRadiusCm + Cluster.InscribedRadiusCm * 1.8f;
		Flow.ForceScale = FMath::Min(Cluster.OpenAreaCm2 * BaseLeakFlowLitersPerSec, 50000.f);

		if (Cluster.InscribedRadiusCm >= CreatureEnterRadiusCm)
		{
			Flow.PassageState = EBreachPassageState::CreatureEnterable;
		}
		else if (Cluster.InscribedRadiusCm >= ActorEjectRadiusCm)
		{
			Flow.PassageState = EBreachPassageState::ActorEjectable;
		}
		else if (Cluster.InscribedRadiusCm >= 20.f)
		{
			Flow.PassageState = EBreachPassageState::StrongSuction;
		}
		else
		{
			Flow.PassageState = EBreachPassageState::LeakOnly;
		}

		FlowFields.Add(Flow);
	}

	BroadcastFlowFieldsUpdated();
}

void USubHullComponent::BroadcastHullDamageUpdated()
{
	OnHullDamageUpdated.Broadcast();
}

int32 USubHullComponent::ClearAllBreaches()
{
	const int32 Removed = BreachClusters.Num();
	if (Removed == 0 && SheetStates.Num() == 0)
	{
		return 0;
	}

	// Durable repair: reset sheet-level damage so the next RebuildBreachClusters()
	// cannot resurrect the clusters from per-cell bLeaking / bOpen flags.
	for (FStructuralSheetRuntimeState& Sheet : SheetStates)
	{
		for (FStructuralCellState& Cell : Sheet.Cells)
		{
			Cell.Damage01 = 0.f;
			Cell.ThicknessRemaining = 1.f;
			Cell.bLeaking = false;
			Cell.bOpen = false;
		}
	}

	BreachClusters.Reset();
	FlowFields.Reset();

	BroadcastHullDamageUpdated();
	BroadcastBreachesUpdated();
	BroadcastFlowFieldsUpdated();
	return Removed;
}

int32 USubHullComponent::ClearBreachNearLocation(const FVector& WorldLocation, float Radius)
{
	if (BreachClusters.Num() == 0 || Radius <= 0.f)
	{
		return 0;
	}

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return 0;
	}

	const FTransform ActorTransform = Owner->GetActorTransform();
	const FVector LocalQuery = ActorTransform.InverseTransformPosition(WorldLocation);
	const float RadiusSq = Radius * Radius;

	// First pass: remove clusters within radius and record affected sheet ids.
	// We heal the whole sheet (not just the cluster cells) so the heal is
	// guaranteed durable against the next RebuildBreachClusters pass; sheets
	// far from the query location are left untouched.
	TSet<FName> AffectedSheetIds;
	int32 Removed = 0;
	for (int32 i = BreachClusters.Num() - 1; i >= 0; --i)
	{
		const FVector Delta = BreachClusters[i].LocalCenter - LocalQuery;
		if (Delta.SizeSquared() <= RadiusSq)
		{
			AffectedSheetIds.Add(BreachClusters[i].SheetId);
			BreachClusters.RemoveAt(i);
			++Removed;
		}
	}

	if (Removed == 0)
	{
		return 0;
	}

	// Heal cells of affected sheets.
	for (FStructuralSheetRuntimeState& Sheet : SheetStates)
	{
		if (!AffectedSheetIds.Contains(Sheet.SheetId))
		{
			continue;
		}
		for (FStructuralCellState& Cell : Sheet.Cells)
		{
			Cell.Damage01 = 0.f;
			Cell.ThicknessRemaining = 1.f;
			Cell.bLeaking = false;
			Cell.bOpen = false;
		}
	}

	// Drop stale flow fields associated with healed sheets.
	for (int32 i = FlowFields.Num() - 1; i >= 0; --i)
	{
		if (AffectedSheetIds.Contains(FlowFields[i].SheetId))
		{
			FlowFields.RemoveAt(i);
		}
	}

	BroadcastHullDamageUpdated();
	BroadcastBreachesUpdated();
	BroadcastFlowFieldsUpdated();
	return Removed;
}

void USubHullComponent::BroadcastBreachesUpdated()
{
	OnBreachesUpdated.Broadcast(BreachClusters);
}

void USubHullComponent::BroadcastFlowFieldsUpdated()
{
	OnFlowFieldsUpdated.Broadcast(FlowFields);
}

void USubHullComponent::OnRep_SheetStates()
{
	BroadcastHullDamageUpdated();
}

void USubHullComponent::OnRep_BreachClusters()
{
	BroadcastBreachesUpdated();
}

void USubHullComponent::OnRep_FlowFields()
{
	BroadcastFlowFieldsUpdated();
}

