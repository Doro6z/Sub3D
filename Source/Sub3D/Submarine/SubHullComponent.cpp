#include "SubHullComponent.h"

#include "DrawDebugHelpers.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Net/UnrealNetwork.h"
#include "SubMovementComponent.h"
#include "SubmarineBase.h"
#include "SubmarineCompartmentComponent.h"
#include "SubmarineLayoutAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubHullWater, Log, All);

namespace
{
constexpr float KPaPerAtm = 101.325f;

void BuildExportedCompartmentState(const FCompartmentRuntimeState& Comp, FCompartmentState& OutState)
{
	OutState.CompartmentId = Comp.CompartmentId;
	OutState.FloodLevel01 = Comp.WaterLevelNormalized;
	OutState.WaterMassLiters = Comp.CurrentWaterLiters;
	OutState.WaterHeightCm = Comp.WaterHeightCm;
	OutState.InternalPressureKPa = Comp.InternalPressureKPa;
	OutState.ExternalPressureKPa = Comp.ExternalReferencePressureKPa;
	OutState.PressureDeltaKPa = Comp.PressureDeltaKPa;
	OutState.bCritical = Comp.WaterLevelNormalized >= 0.8f || Comp.bPressureCritical;
	OutState.bElectricalsWet = Comp.WaterLevelNormalized >= 0.15f;
}

void ExpandBoundsWithSheet(FBox& InOutBounds, const FStructuralSheetDef& Sheet)
{
	const FVector TangentX = Sheet.LocalTangentX.GetSafeNormal();
	const FVector TangentY = Sheet.LocalTangentY.GetSafeNormal();
	const FVector HalfX = TangentX * (Sheet.SizeCm.X * 0.5f);
	const FVector HalfY = TangentY * (Sheet.SizeCm.Y * 0.5f);

	InOutBounds += Sheet.LocalOrigin + HalfX + HalfY;
	InOutBounds += Sheet.LocalOrigin + HalfX - HalfY;
	InOutBounds += Sheet.LocalOrigin - HalfX + HalfY;
	InOutBounds += Sheet.LocalOrigin - HalfX - HalfY;
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

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		InitializeFromLayout(LayoutAsset);
		EnsureFallbackLayout();
	}
}

void USubHullComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	UpdateCompartmentDerivedState(0.f);
	UpdateFlowFields();
	AdvanceFlooding(DeltaTime);
	MaybeLogWaterLevels(DeltaTime);

	if (!bDrawDebug || !GetWorld())
	{
		return;
	}

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

void USubHullComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USubHullComponent, SheetStates);
	DOREPLIFETIME(USubHullComponent, BreachClusters);
	DOREPLIFETIME(USubHullComponent, FlowFields);
	DOREPLIFETIME(USubHullComponent, CompartmentStates);
}

void USubHullComponent::InitializeFromLayout(const USubmarineLayoutAsset* InLayout)
{
	StructuralSheets.Reset();
	SheetStates.Reset();
	BreachClusters.Reset();
	FlowFields.Reset();
	CompartmentStates.Reset();

	if (!InLayout)
	{
		return;
	}

	StructuralSheets = InLayout->StructuralSheets;

	for (const FSubCompartmentDef& CompDef : InLayout->Compartments)
	{
		if (CompDef.CompartmentId.IsNone())
		{
			continue;
		}

		FCompartmentRuntimeState Runtime;
		Runtime.CompartmentId = CompDef.CompartmentId;
		Runtime.CapacityLiters = FMath::Max(1.f, CompDef.CapacityLiters);
		Runtime.FreeAirLiters = Runtime.CapacityLiters;
		Runtime.MaxWaterHeightCm = ComputeCompartmentMaxWaterHeightCm(CompDef.CompartmentId);
		Runtime.InternalPressureKPa = NominalInternalPressureAtm * KPaPerAtm;
		Runtime.ExternalReferencePressureKPa = Runtime.InternalPressureKPa;
		CompartmentStates.Add(Runtime);
	}

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
			Runtime.Cells.Add(Cell);
		}
		SheetStates.Add(Runtime);
	}

	UpdateCompartmentDerivedState(0.f);
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

	CompartmentStates.Reset();
	FCompartmentRuntimeState MainCompartment;
	MainCompartment.CompartmentId = FName(TEXT("HullMain"));
	MainCompartment.CapacityLiters = 15000.f;
	MainCompartment.FreeAirLiters = MainCompartment.CapacityLiters;
	MainCompartment.MaxWaterHeightCm = FMath::Max(MinCompartmentHeightCm, Extent.Z * 2.f);
	MainCompartment.InternalPressureKPa = NominalInternalPressureAtm * KPaPerAtm;
	MainCompartment.ExternalReferencePressureKPa = MainCompartment.InternalPressureKPa;
	CompartmentStates.Add(MainCompartment);

	StructuralSheets.Reset();
	auto AddFallbackSheet = [&](const TCHAR* Name, const FVector& Origin, const FVector& Normal, const FVector& TangentX, const FVector& TangentY, const FVector2D& Size, bool bExterior)
	{
		FStructuralSheetDef Sheet;
		Sheet.SheetId = FName(Name);
		Sheet.ParentCompartmentId = MainCompartment.CompartmentId;
		Sheet.LocalOrigin = Origin;
		Sheet.LocalNormal = Normal;
		Sheet.LocalTangentX = TangentX;
		Sheet.LocalTangentY = TangentY;
		Sheet.SizeCm = Size;
		Sheet.GridResolutionX = 20;
		Sheet.GridResolutionY = 20;
		Sheet.bCanOpenToExterior = bExterior;
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
		}
		SheetStates.Add(Runtime);
	}

	UpdateCompartmentDerivedState(0.f);
}

void USubHullComponent::ApplyHullImpact(const FVector& LocalHitPosition, float Damage, float RadiusCm)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Damage <= 0.f || StructuralSheets.Num() == 0)
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
	if (!GetOwner() || !GetOwner()->HasAuthority() || RepairStrength <= 0.f || StructuralSheets.Num() == 0)
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

void USubHullComponent::SetCompartmentPumpState(FName CompartmentId, bool bActive, float PumpRateOutLitersPerSec)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	FCompartmentRuntimeState* Comp = FindCompartmentState(CompartmentId);
	if (!Comp)
	{
		return;
	}

	Comp->bPumpActive = bActive;
	Comp->PumpRateOut = bActive ? FMath::Max(0.f, PumpRateOutLitersPerSec) : 0.f;
}

void USubHullComponent::SetAllPumpsActive(bool bActive, float PumpRateOutLitersPerSec, FName PreferredCompartmentId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	bool bAppliedPreferred = false;
	if (!PreferredCompartmentId.IsNone())
	{
		if (FCompartmentRuntimeState* Preferred = FindCompartmentState(PreferredCompartmentId))
		{
			Preferred->bPumpActive = bActive;
			Preferred->PumpRateOut = bActive ? FMath::Max(0.f, PumpRateOutLitersPerSec) : 0.f;
			bAppliedPreferred = true;
		}
	}

	if (bAppliedPreferred)
	{
		for (FCompartmentRuntimeState& Comp : CompartmentStates)
		{
			if (Comp.CompartmentId == PreferredCompartmentId)
			{
				continue;
			}

			Comp.bPumpActive = false;
			Comp.PumpRateOut = 0.f;
			Comp.FloodRateOut = 0.f;
		}
		return;
	}

	for (FCompartmentRuntimeState& Comp : CompartmentStates)
	{
		Comp.bPumpActive = bActive;
		Comp.PumpRateOut = bActive ? FMath::Max(0.f, PumpRateOutLitersPerSec) : 0.f;
	}
}

float USubHullComponent::GetTotalWaterLiters() const
{
	float Total = 0.f;
	for (const FCompartmentRuntimeState& Comp : CompartmentStates)
	{
		Total += FMath::Max(0.f, Comp.CurrentWaterLiters);
	}
	return Total;
}

void USubHullComponent::ExportCompartmentStates(TArray<FCompartmentState>& OutStates) const
{
	OutStates.Reset();
	OutStates.Reserve(CompartmentStates.Num());

	for (const FCompartmentRuntimeState& Comp : CompartmentStates)
	{
		FCompartmentState State;
		BuildExportedCompartmentState(Comp, State);
		OutStates.Add(State);
	}
}

bool USubHullComponent::GetCompartmentLocalBounds(FName CompartmentId, FBox& OutBounds) const
{
	if (LayoutAsset)
	{
		if (const FSubCompartmentDef* CompartmentDef = LayoutAsset->Compartments.FindByPredicate([CompartmentId](const FSubCompartmentDef& Candidate)
		{
			return Candidate.CompartmentId == CompartmentId;
		}))
		{
			const FBox HydroBounds(CompartmentDef->HydroBoundsMin, CompartmentDef->HydroBoundsMax);
			if (HydroBounds.IsValid)
			{
				OutBounds = HydroBounds;
				return true;
			}
		}
	}

	OutBounds = FBox(EForceInit::ForceInit);
	bool bFoundAnySheet = false;

	for (const FStructuralSheetDef& Sheet : StructuralSheets)
	{
		if (Sheet.ParentCompartmentId != CompartmentId && Sheet.AdjacentCompartmentId != CompartmentId)
		{
			continue;
		}

		ExpandBoundsWithSheet(OutBounds, Sheet);
		bFoundAnySheet = true;
	}

	return bFoundAnySheet && OutBounds.IsValid;
}

bool USubHullComponent::SampleCompartmentStateAtLocalLocation(const FVector& LocalLocation, FCompartmentState& OutState, FBox* OutLocalBounds) const
{
	const FCompartmentRuntimeState* BestCompartment = nullptr;
	FBox BestBounds(EForceInit::ForceInit);
	float BestVolume = TNumericLimits<float>::Max();

	for (const FCompartmentRuntimeState& Compartment : CompartmentStates)
	{
		FBox CompartmentBounds(EForceInit::ForceInit);
		if (!GetCompartmentLocalBounds(Compartment.CompartmentId, CompartmentBounds))
		{
			continue;
		}

		const FBox ExpandedBounds = CompartmentBounds.ExpandBy(25.f);
		if (!ExpandedBounds.IsInsideOrOn(LocalLocation))
		{
			continue;
		}

		const float BoundsVolume = FMath::Max(1.f, ExpandedBounds.GetVolume());
		if (BoundsVolume < BestVolume)
		{
			BestVolume = BoundsVolume;
			BestCompartment = &Compartment;
			BestBounds = CompartmentBounds;
		}
	}

	if (!BestCompartment)
	{
		return false;
	}

	BuildExportedCompartmentState(*BestCompartment, OutState);
	if (OutLocalBounds)
	{
		*OutLocalBounds = BestBounds;
	}

	return true;
}

bool USubHullComponent::SampleCompartmentStateAtWorldLocation(const FVector& WorldLocation, FCompartmentState& OutState, FBox* OutLocalBounds) const
{
	if (!GetOwner())
	{
		return false;
	}

	const FVector LocalLocation = GetOwner()->GetActorTransform().InverseTransformPosition(WorldLocation);
	return SampleCompartmentStateAtLocalLocation(LocalLocation, OutState, OutLocalBounds);
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
		Flow.ForceScale = Cluster.OpenAreaCm2 * BaseLeakFlowLitersPerSec;

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

void USubHullComponent::AdvanceFlooding(float DeltaTime)
{
	const ASubmarineBase* SubBase = Cast<ASubmarineBase>(GetOwner());

	for (FCompartmentRuntimeState& Comp : CompartmentStates)
	{
		Comp.FloodRateIn = 0.f;
		Comp.FloodRateOut = 0.f;
		Comp.PumpRateOut = 0.f;
	}

	for (int32 ClusterIndex = 0; ClusterIndex < BreachClusters.Num(); ++ClusterIndex)
	{
		const FBreachClusterState& Cluster = BreachClusters[ClusterIndex];
		const FStructuralSheetDef* Sheet = StructuralSheets.FindByPredicate([&](const FStructuralSheetDef& S)
		{
			return S.SheetId == Cluster.SheetId;
		});

		if (!Sheet)
		{
			continue;
		}

		FCompartmentRuntimeState* ParentComp = FindCompartmentState(Sheet->ParentCompartmentId);
		if (!ParentComp)
		{
			continue;
		}

		if (Cluster.bTouchesExterior)
		{
			const float AreaScale = FMath::Max(0.f, Cluster.OpenAreaCm2 / FMath::Max(1.f, ExteriorFloodAreaDivisorCm2));
			const float Inflow = FMath::Clamp(
				BaseLeakFlowLitersPerSec * AreaScale,
				0.f,
				FMath::Max(0.f, MaxExteriorFloodInLitersPerSec));
			ParentComp->FloodRateIn += Inflow;
		}
	}

	for (const FStructuralSheetDef& Sheet : StructuralSheets)
	{
		if (Sheet.AdjacentCompartmentId.IsNone())
		{
			continue;
		}

		const FCompartmentRuntimeState* CompARead = FindCompartmentState(Sheet.ParentCompartmentId);
		const FCompartmentRuntimeState* CompBRead = FindCompartmentState(Sheet.AdjacentCompartmentId);
		if (!CompARead || !CompBRead)
		{
			continue;
		}

		bool bConnectionOpen = false;
		if (SubBase && SubBase->Compartments)
		{
			FDoorState DoorState;
			if (SubBase->Compartments->TryGetDoorState(Sheet.SheetId, DoorState))
			{
				bConnectionOpen = !DoorState.bClosed;
			}
		}

		if (!bConnectionOpen)
		{
			continue;
		}

		FCompartmentRuntimeState* CompA = FindCompartmentState(Sheet.ParentCompartmentId);
		FCompartmentRuntimeState* CompB = FindCompartmentState(Sheet.AdjacentCompartmentId);
		if (!CompA || !CompB)
		{
			continue;
		}

		const float ConnectionAreaScale = FMath::Clamp(
			(Sheet.SizeCm.X * Sheet.SizeCm.Y) / FMath::Max(1.f, InternalConnectionAreaDivisorCm2),
			0.f,
			8.f);
		const float HeightDeltaCm = CompA->WaterHeightCm - CompB->WaterHeightCm;
		if (FMath::IsNearlyZero(HeightDeltaCm, KINDA_SMALL_NUMBER))
		{
			continue;
		}

		FCompartmentRuntimeState* SourceComp = HeightDeltaCm > 0.f ? CompA : CompB;
		FCompartmentRuntimeState* DestComp = HeightDeltaCm > 0.f ? CompB : CompA;
		const float SourceLitersPerCm = SourceComp->MaxWaterHeightCm > KINDA_SMALL_NUMBER
			? SourceComp->CapacityLiters / SourceComp->MaxWaterHeightCm
			: 0.f;
		const float DestLitersPerCm = DestComp->MaxWaterHeightCm > KINDA_SMALL_NUMBER
			? DestComp->CapacityLiters / DestComp->MaxWaterHeightCm
			: 0.f;
		const float HeightResponsePerLiter = (SourceLitersPerCm > KINDA_SMALL_NUMBER ? 1.f / SourceLitersPerCm : 0.f)
			+ (DestLitersPerCm > KINDA_SMALL_NUMBER ? 1.f / DestLitersPerCm : 0.f);
		if (HeightResponsePerLiter <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float DesiredTransferLiters = FMath::Abs(HeightDeltaCm) / HeightResponsePerLiter;
		const float MaxTransferLiters = FMath::Min(
			SourceComp->CurrentWaterLiters,
			FMath::Max(0.f, DestComp->CapacityLiters - DestComp->CurrentWaterLiters));
		const float FlowMagnitude = FMath::Clamp(
			(DesiredTransferLiters / FMath::Max(KINDA_SMALL_NUMBER, DeltaTime)) * ConnectionAreaScale,
			0.f,
			FMath::Min(FMath::Max(0.f, MaxInternalConnectionFlowLitersPerSec), MaxTransferLiters / FMath::Max(KINDA_SMALL_NUMBER, DeltaTime)));
		if (FlowMagnitude <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		SourceComp->FloodRateOut += FlowMagnitude;
		DestComp->FloodRateIn += FlowMagnitude;
	}

	for (FCompartmentRuntimeState& Comp : CompartmentStates)
	{
		const float DeltaLiters = (Comp.FloodRateIn - Comp.FloodRateOut) * DeltaTime;
		Comp.CurrentWaterLiters = FMath::Clamp(Comp.CurrentWaterLiters + DeltaLiters, 0.f, Comp.CapacityLiters);
	}

	UpdateCompartmentDerivedState(DeltaTime);
	BroadcastCompartmentFloodUpdated();
}

void USubHullComponent::UpdateCompartmentDerivedState(float DeltaTime)
{
	(void)DeltaTime;
	const float NominalPressureKPa = NominalInternalPressureAtm * KPaPerAtm;

	for (FCompartmentRuntimeState& Comp : CompartmentStates)
	{
		Comp.WaterLevelNormalized = Comp.CapacityLiters > 0.f
			? FMath::Clamp(Comp.CurrentWaterLiters / Comp.CapacityLiters, 0.f, 1.f)
			: 0.f;
		Comp.MaxWaterHeightCm = FMath::Max(MinCompartmentHeightCm, Comp.MaxWaterHeightCm);
		Comp.WaterHeightCm = Comp.WaterLevelNormalized * Comp.MaxWaterHeightCm;
		Comp.FreeAirLiters = FMath::Max(0.f, Comp.CapacityLiters - Comp.CurrentWaterLiters);
		Comp.ExternalReferencePressureKPa = NominalPressureKPa;
		Comp.bFullyFlooded = Comp.WaterLevelNormalized >= (1.f - KINDA_SMALL_NUMBER);
		Comp.InternalPressureKPa = NominalPressureKPa;
		Comp.PressureDeltaKPa = 0.f;
		Comp.bPressureCritical = false;
	}
}

void USubHullComponent::MaybeLogWaterLevels(float DeltaTime)
{
	if (!bLogWaterLevels || CompartmentStates.Num() == 0)
	{
		WaterLevelLogAccumulatorSeconds = 0.f;
		return;
	}

	WaterLevelLogAccumulatorSeconds += DeltaTime;
	if (WaterLevelLogAccumulatorSeconds < FMath::Max(0.1f, WaterLevelLogIntervalSeconds))
	{
		return;
	}

	WaterLevelLogAccumulatorSeconds = 0.f;

	FString Summary;
	Summary.Reserve(CompartmentStates.Num() * 48);
	for (int32 Index = 0; Index < CompartmentStates.Num(); ++Index)
	{
		const FCompartmentRuntimeState& Comp = CompartmentStates[Index];
		if (Index > 0)
		{
			Summary += TEXT(" | ");
		}

		Summary += FString::Printf(
			TEXT("%s H=%.1fcm L=%.2f W=%.0fL"),
			*Comp.CompartmentId.ToString(),
			Comp.WaterHeightCm,
			Comp.WaterLevelNormalized,
			Comp.CurrentWaterLiters);
	}

	UE_LOG(LogSubHullWater, Log, TEXT("WaterLevels | Sub=%s | %s"), *GetOwner()->GetName(), *Summary);
}

float USubHullComponent::ComputeCompartmentMaxWaterHeightCm(FName CompartmentId) const
{
	if (LayoutAsset)
	{
		if (const FSubCompartmentDef* CompartmentDef = LayoutAsset->Compartments.FindByPredicate([CompartmentId](const FSubCompartmentDef& Candidate)
		{
			return Candidate.CompartmentId == CompartmentId;
		}))
		{
			const float HydroHeight = CompartmentDef->HydroBoundsMax.Z - CompartmentDef->HydroBoundsMin.Z;
			if (HydroHeight > KINDA_SMALL_NUMBER)
			{
				return FMath::Max(MinCompartmentHeightCm, HydroHeight);
			}
		}
	}

	FBox Bounds(ForceInit);
	for (const FStructuralSheetDef& Sheet : StructuralSheets)
	{
		if (Sheet.ParentCompartmentId != CompartmentId)
		{
			continue;
		}

		ExpandBoundsWithSheet(Bounds, Sheet);
	}

	if (!Bounds.IsValid)
	{
		return MinCompartmentHeightCm;
	}

	return FMath::Max(MinCompartmentHeightCm, Bounds.Max.Z - Bounds.Min.Z);
}

FCompartmentRuntimeState* USubHullComponent::FindCompartmentState(FName CompartmentId)
{
	return CompartmentStates.FindByPredicate([&](const FCompartmentRuntimeState& C)
	{
		return C.CompartmentId == CompartmentId;
	});
}

const FCompartmentRuntimeState* USubHullComponent::FindCompartmentState(FName CompartmentId) const
{
	return CompartmentStates.FindByPredicate([&](const FCompartmentRuntimeState& C)
	{
		return C.CompartmentId == CompartmentId;
	});
}

void USubHullComponent::BroadcastHullDamageUpdated()
{
	OnHullDamageUpdated.Broadcast();
}

void USubHullComponent::BroadcastBreachesUpdated()
{
	OnBreachesUpdated.Broadcast(BreachClusters);
}

void USubHullComponent::BroadcastFlowFieldsUpdated()
{
	OnFlowFieldsUpdated.Broadcast(FlowFields);
}

void USubHullComponent::BroadcastCompartmentFloodUpdated()
{
	OnCompartmentFloodUpdated.Broadcast(CompartmentStates);
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

void USubHullComponent::OnRep_CompartmentStates()
{
	BroadcastCompartmentFloodUpdated();
}
