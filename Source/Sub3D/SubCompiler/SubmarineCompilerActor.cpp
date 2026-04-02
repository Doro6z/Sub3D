#include "SubmarineCompilerActor.h"

#include "Curves/RichCurve.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/Package.h"
#include "SubCompilerPlaceholderStation.h"
#include "SubCompilerMvpFactory.h"
#include "SubmarineBuildCompiler.h"
#include "SubmarineEnvelopeDef.h"
#include "SubmarineFunctionalGraph.h"
#include "SubmarineGeometryBuilder.h"
#include "SubmarineLayoutSolver.h"
#include "Submarine/SubDoorActor.h"
#include "Submarine/SubHullComponent.h"
#include "Submarine/SubHullVisualDamageComponent.h"
#include "Submarine/SubStationBase.h"
#include "Submarine/SubmarineLayoutAsset.h"
#include "Submarine/SubmarineStationManagerComponent.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

ASubmarineCompilerActor::ASubmarineCompilerActor()
{
	PrimaryActorTick.bCanEverTick = false;
	DoorActorClass = ASubDoorActor::StaticClass();

	ExteriorCollisionProxy = CreateDefaultSubobject<UCapsuleComponent>(TEXT("ExteriorCollisionProxy"));
	ExteriorCollisionProxy->SetupAttachment(HullMesh);
	// Rotate capsule 90° so its axis aligns with the submarine longitudinal X axis.
	ExteriorCollisionProxy->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	ConfigureExteriorCollisionProxy();
}

void ASubmarineCompilerActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshExteriorCollisionProxy();

	if (!HasAnyFlags(RF_ClassDefaultObject) && bBuildOnConstruction)
	{
		CompileAndBuild();
	}
}

void ASubmarineCompilerActor::BeginPlay()
{
	Super::BeginPlay();

	const bool bNeedsBuild =
		!CompiledLayoutAsset
		|| !LastSolution.IsValid()
		|| GeneratedInteriorMeshes.Num() == 0
		|| !GeneratedExteriorMesh;

	if (bNeedsBuild)
	{
		CompileAndBuild();
	}

	if (HasAuthority())
	{
		BuildGeneratedDoors();
		BuildGeneratedStations();
	}

	RefreshExteriorCollisionProxy();
}

UPrimitiveComponent* ASubmarineCompilerActor::GetMovementCollisionComponent() const
{
	if (bUseExteriorCollisionProxy && ExteriorCollisionProxy && ExteriorCollisionProxy->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		return ExteriorCollisionProxy;
	}

	if (bPreferGeneratedExteriorMeshCollision && GeneratedExteriorMesh && GeneratedExteriorMesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		return GeneratedExteriorMesh;
	}

	if (GeneratedExteriorMesh && GeneratedExteriorMesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		return GeneratedExteriorMesh;
	}

	if (bUseExteriorCollisionProxy && ExteriorCollisionProxy)
	{
		return ExteriorCollisionProxy;
	}

	return Super::GetMovementCollisionComponent();
}

TArray<UPrimitiveComponent*> ASubmarineCompilerActor::GetInteriorWalkableComponents() const
{
	TArray<UPrimitiveComponent*> Result;
	for (const TObjectPtr<UProceduralMeshComponent>& Mesh : GeneratedInteriorMeshes)
	{
		if (IsValid(Mesh) && Mesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			if (Mesh->GetCollisionProfileName() == TEXT("SubInteriorWalkable"))
			{
				Result.Add(Mesh);
			}
		}
	}
	return Result;
}

bool ASubmarineCompilerActor::ValidateSpawnCollision() const
{
	if (!ExteriorCollisionProxy)
	{
		return false;
	}

	if (ExteriorCollisionProxy->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SubSpawnValidation), false, this);
	const bool bOverlapping = GetWorld()->OverlapBlockingTestByProfile(
		ExteriorCollisionProxy->GetComponentLocation(),
		ExteriorCollisionProxy->GetComponentQuat(),
		TEXT("SubmarineHull"),
		FCollisionShape::MakeCapsule(
			ExteriorCollisionProxy->GetScaledCapsuleRadius(),
			ExteriorCollisionProxy->GetScaledCapsuleHalfHeight()),
		Params);

	UE_LOG(LogTemp, Log,
		TEXT("SubValidation | CollisionComp=%s | Overlapping=%d | Valid=%d"),
		*GetNameSafe(ExteriorCollisionProxy),
		bOverlapping ? 1 : 0,
		bOverlapping ? 0 : 1);

	return !bOverlapping;
}

bool ASubmarineCompilerActor::CompileCurrentDefinitions()
{
	EnsureDefaultDefinitions();

	LastMessages.Reset();
	LastSolution = FSubmarineLayoutSolution();
	CompiledLayoutAsset = nullptr;
	ResolvedEnvelopeDef = nullptr;

	if (!EnvelopeDef || !FunctionalGraph)
	{
		FLayoutValidationMessage Message;
		Message.Severity = ELayoutValidationSeverity::Error;
		Message.Message = FText::FromString(TEXT("Envelope ou graphe fonctionnel manquant"));
		LastMessages.Add(MoveTemp(Message));
		LogValidationMessages();
		return false;
	}

	ResolvedEnvelopeDef = ResolveEnvelopeDefinition();
	if (!ResolvedEnvelopeDef)
	{
		FLayoutValidationMessage Message;
		Message.Severity = ELayoutValidationSeverity::Error;
		Message.Message = FText::FromString(TEXT("Resolution de l'enveloppe echouee"));
		LastMessages.Add(MoveTemp(Message));
		LogValidationMessages();
		return false;
	}

	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>(this);
	USubmarineBuildCompiler* Compiler = NewObject<USubmarineBuildCompiler>(this);
	if (!Solver || !Compiler)
	{
		FLayoutValidationMessage Message;
		Message.Severity = ELayoutValidationSeverity::Error;
		Message.Message = FText::FromString(TEXT("Creation des outils de compilation echouee"));
		LastMessages.Add(MoveTemp(Message));
		LogValidationMessages();
		return false;
	}

	TArray<FLayoutValidationMessage> SolveMessages;
	if (!Solver->Solve(ResolvedEnvelopeDef, FunctionalGraph, LastSolution, SolveMessages))
	{
		LastMessages = MoveTemp(SolveMessages);
		LogValidationMessages();
		return false;
	}

	LastMessages = MoveTemp(SolveMessages);
	TArray<FLayoutValidationMessage> CompileMessages;
	CompiledLayoutAsset = Compiler->CompileToLayoutAsset(LastSolution, this, CompileMessages, ResolvedEnvelopeDef);
	LastMessages.Append(MoveTemp(CompileMessages));
	if (!CompiledLayoutAsset)
	{
		LogValidationMessages();
		return false;
	}

	if (SubHull)
	{
		SubHull->LayoutAsset = CompiledLayoutAsset;
		SubHull->InitializeFromLayout(CompiledLayoutAsset);
	}

	RefreshSocketsFromSolution();
	RefreshExteriorCollisionProxy();
	LogValidationMessages();
	return true;
}

bool ASubmarineCompilerActor::BuildGeneratedGeometry()
{
	DestroyGeneratedInteriorMeshes();

	if (!LastSolution.IsValid())
	{
		return false;
	}

	USubmarineGeometryBuilder* GeometryBuilder = NewObject<USubmarineGeometryBuilder>(this);
	if (!GeometryBuilder)
	{
		return false;
	}

	const float SectionExp = ResolvedEnvelopeDef ? ResolvedEnvelopeDef->SectionExponent : 2.f;
	const float WHR = ResolvedEnvelopeDef ? ResolvedEnvelopeDef->WidthToHeightRatio : 1.f;
	const int32 ArcSegs = ResolvedEnvelopeDef ? ResolvedEnvelopeDef->InteriorArcSegments : 24;
	const float WallInset = ResolvedEnvelopeDef ? ResolvedEnvelopeDef->WallThicknessCm : 12.f;
	UMaterialInterface* EffectiveWallMaterial = InteriorWallMaterial ? InteriorWallMaterial.Get() : InteriorMaterial.Get();
	UMaterialInterface* EffectiveFloorMaterial = FloorMaterial ? FloorMaterial.Get() : InteriorMaterial.Get();
	UMaterialInterface* EffectiveExteriorMaterial = ExteriorMaterial ? ExteriorMaterial.Get() : InteriorMaterial.Get();

	TArray<UProceduralMeshComponent*> BuiltMeshes = GeometryBuilder->BuildInteriorMeshes(
		LastSolution,
		this,
		EffectiveWallMaterial,
		EffectiveFloorMaterial,
		bEnableInteriorCollision,
		SectionExp,
		WHR,
		ArcSegs,
		WallInset);

	GeneratedInteriorMeshes.Reserve(BuiltMeshes.Num());
	for (UProceduralMeshComponent* Mesh : BuiltMeshes)
	{
		GeneratedInteriorMeshes.Add(Mesh);
	}

	GeneratedExteriorMesh = GeometryBuilder->BuildExteriorMesh(
		LastSolution,
		this,
		EffectiveExteriorMaterial,
		true,
		ResolvedEnvelopeDef ? ResolvedEnvelopeDef->ExteriorRadialSegments : 32,
		ResolvedEnvelopeDef ? ResolvedEnvelopeDef->ExteriorLongitudinalSubdivisionsPerSpan : 6,
		SectionExp,
		WHR,
		ResolvedEnvelopeDef);

	if (GeneratedExteriorMesh)
	{
		GeneratedExteriorMesh->SetGenerateOverlapEvents(true);
		GeneratedExteriorMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
		GeneratedExteriorMesh->SetVisibility(true);
		GeneratedExteriorMesh->SetHiddenInGame(false);
		if (EffectiveExteriorMaterial)
		{
			GeneratedExteriorMesh->SetMaterial(0, EffectiveExteriorMaterial);
		}
	}

	if (USubHullVisualDamageComponent* VisualDamageComponent = FindComponentByClass<USubHullVisualDamageComponent>())
	{
		VisualDamageComponent->RefreshFromCurrentBreaches();
	}

	RefreshExteriorCollisionProxy();
	RefreshMovementCollisionBinding();

	// Spawn compartment lights for interior visibility.
	for (UPointLightComponent* OldLight : GeneratedCompartmentLights)
	{
		if (OldLight) { OldLight->DestroyComponent(); }
	}
	GeneratedCompartmentLights.Reset();

	for (const FCompartmentPlacement& Comp : LastSolution.Compartments)
	{
		const float MidX = (Comp.SpineStartCm + Comp.SpineEndCm) * 0.5f;
		const float LightZ = Comp.FloorOffsetCm + FMath::Max(10.f, Comp.EffectiveRadiusCm * 0.6f);
		const FName LightName(*FString::Printf(TEXT("CompartmentLight_%s"), *Comp.CompartmentId.ToString()));

		UPointLightComponent* Light = NewObject<UPointLightComponent>(this, LightName);
		if (Light)
		{
			Light->RegisterComponent();
			Light->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			Light->SetRelativeLocation(FVector(MidX, 0.f, LightZ));
			Light->SetIntensity(5000.f);
			Light->SetAttenuationRadius((Comp.SpineEndCm - Comp.SpineStartCm) * 0.7f);
			Light->SetLightColor(FLinearColor(1.f, 0.9f, 0.75f)); // warm white
			Light->SetCastShadows(false);
			GeneratedCompartmentLights.Add(Light);
		}
	}

	const int32 ExpectedInteriorMeshCount = (LastSolution.Compartments.Num() * 2) + LastSolution.Bulkheads.Num();
	return GeneratedInteriorMeshes.Num() == ExpectedInteriorMeshCount
		&& GeneratedExteriorMesh != nullptr;
}

bool ASubmarineCompilerActor::CompileAndBuild()
{
	if (!CompileCurrentDefinitions())
	{
		DestroyGeneratedInteriorMeshes();
		DestroyGeneratedDoors();
		DestroyGeneratedStations();
		return false;
	}

	const bool bBuiltGeometry = BuildGeneratedGeometry();
	if (!bBuiltGeometry)
	{
		DestroyGeneratedDoors();
		DestroyGeneratedStations();
		return false;
	}

	if (HasAuthority() && HasActorBegunPlay())
	{
		BuildGeneratedDoors();
		BuildGeneratedStations();
	}

	return true;
}

bool ASubmarineCompilerActor::CompileAndBuildDirty()
{
	PreviousSolution = LastSolution;

	if (!CompileCurrentDefinitions())
	{
		DestroyGeneratedInteriorMeshes();
		DestroyGeneratedDoors();
		DestroyGeneratedStations();
		return false;
	}

	TSet<FName> DirtyIds;
	ComputeDirtyCompartments(PreviousSolution, LastSolution, DirtyIds);

	if (DirtyIds.Num() == 0 && PreviousSolution.IsValid() && GeneratedExteriorMesh)
	{
		UE_LOG(LogTemp, Log, TEXT("[SubCompiler] Dirty rebuild: 0/%d compartments dirty — skipping geometry"),
			LastSolution.Compartments.Num());
		return true;
	}

	UE_LOG(LogTemp, Log, TEXT("[SubCompiler] Dirty rebuild: %d/%d compartments dirty"),
		DirtyIds.Num(), LastSolution.Compartments.Num());

	const bool bBuiltGeometry = BuildGeneratedGeometry();
	if (!bBuiltGeometry)
	{
		DestroyGeneratedDoors();
		DestroyGeneratedStations();
		return false;
	}

	if (HasAuthority() && HasActorBegunPlay())
	{
		BuildGeneratedDoors();
		BuildGeneratedStations();
	}

	return true;
}

void ASubmarineCompilerActor::CompileAndBuildInEditor()
{
	CompileAndBuild();
}

void ASubmarineCompilerActor::CompileAndBuildDirtyInEditor()
{
	CompileAndBuildDirty();
}

bool ASubmarineCompilerActor::SavePreviewToEnvelope()
{
	EnsureDefaultDefinitions();

	if (!EnvelopeDef)
	{
		return false;
	}

	EnvelopeDef->Modify();
	ApplyPreviewOverridesToEnvelope(*EnvelopeDef);
	EnvelopeDef->MarkPackageDirty();

#if WITH_EDITOR
	EnvelopeDef->PostEditChange();
#endif

	if (!HasAnyFlags(RF_ClassDefaultObject) && bBuildOnConstruction)
	{
		// Automation tests can create transient actors without a valid world.
		// In that case we only persist preview values and skip runtime mesh rebuild.
		if (GetWorld())
		{
			return CompileAndBuild();
		}
	}

	return true;
}

void ASubmarineCompilerActor::SavePreviewToEnvelopeInEditor()
{
	SavePreviewToEnvelope();
}

void ASubmarineCompilerActor::ClearGeneratedGeometry()
{
	DestroyGeneratedInteriorMeshes();
	DestroyGeneratedDoors();
	DestroyGeneratedStations();
	RefreshExteriorCollisionProxy();
	RefreshMovementCollisionBinding();
}

void ASubmarineCompilerActor::EnsureDefaultDefinitions()
{
	if (!bUseMvpDefaultsWhenUnset)
	{
		return;
	}

	if (!EnvelopeDef)
	{
		EnvelopeDef = FSubCompilerMvpFactory::CreateEnvelope(this);
	}

	if (!FunctionalGraph)
	{
		FunctionalGraph = FSubCompilerMvpFactory::CreateFunctionalGraph(this);
	}
}

USubmarineEnvelopeDef* ASubmarineCompilerActor::ResolveEnvelopeDefinition()
{
	if (!EnvelopeDef)
	{
		return nullptr;
	}

	USubmarineEnvelopeDef* WorkingEnvelope = NewObject<USubmarineEnvelopeDef>(this);
	if (!WorkingEnvelope)
	{
		return nullptr;
	}

	WorkingEnvelope->SpineLengthCm = EnvelopeDef->SpineLengthCm;
	WorkingEnvelope->RadiusProfile = EnvelopeDef->RadiusProfile;
	WorkingEnvelope->DefaultRadiusCm = EnvelopeDef->DefaultRadiusCm;
	WorkingEnvelope->FloorDropBiasCm = EnvelopeDef->FloorDropBiasCm;
	WorkingEnvelope->ExteriorLongitudinalSubdivisionsPerSpan = EnvelopeDef->ExteriorLongitudinalSubdivisionsPerSpan;
	WorkingEnvelope->ExteriorRadialSegments = EnvelopeDef->ExteriorRadialSegments;
	WorkingEnvelope->MaxCompartments = EnvelopeDef->MaxCompartments;
	WorkingEnvelope->SectionExponent = EnvelopeDef->SectionExponent;
	WorkingEnvelope->WidthToHeightRatio = EnvelopeDef->WidthToHeightRatio;
	WorkingEnvelope->BowProfile = EnvelopeDef->BowProfile;
	WorkingEnvelope->SternProfile = EnvelopeDef->SternProfile;
	WorkingEnvelope->BowTaperFraction = EnvelopeDef->BowTaperFraction;
	WorkingEnvelope->SternTaperFraction = EnvelopeDef->SternTaperFraction;
	WorkingEnvelope->ExteriorHullOffsetCm = EnvelopeDef->ExteriorHullOffsetCm;
	WorkingEnvelope->InteriorArcSegments = EnvelopeDef->InteriorArcSegments;
	WorkingEnvelope->WallThicknessCm = EnvelopeDef->WallThicknessCm;

	if (bUseEnvelopePreviewOverrides)
	{
		ApplyPreviewOverridesToEnvelope(*WorkingEnvelope);
	}

	return WorkingEnvelope;
}

void ASubmarineCompilerActor::ApplyPreviewOverridesToEnvelope(USubmarineEnvelopeDef& Envelope) const
{
	Envelope.SpineLengthCm = PreviewSpineLengthCm;
	Envelope.DefaultRadiusCm = PreviewDefaultRadiusCm;
	Envelope.FloorDropBiasCm = PreviewFloorDropBiasCm;
	Envelope.ExteriorLongitudinalSubdivisionsPerSpan = PreviewExteriorLongitudinalSubdivisionsPerSpan;
	Envelope.ExteriorRadialSegments = PreviewExteriorRadialSegments;
	Envelope.SectionExponent = PreviewSectionExponent;
	Envelope.WidthToHeightRatio = PreviewWidthToHeightRatio;
	Envelope.BowProfile = PreviewBowProfile;
	Envelope.SternProfile = PreviewSternProfile;
	Envelope.BowTaperFraction = PreviewBowTaperFraction;
	Envelope.SternTaperFraction = PreviewSternTaperFraction;
	Envelope.ExteriorHullOffsetCm = PreviewExteriorHullOffsetCm;
	Envelope.InteriorArcSegments = PreviewInteriorArcSegments;
	Envelope.WallThicknessCm = PreviewWallThicknessCm;

	FRichCurve* Curve = Envelope.RadiusProfile.GetRichCurve();
	if (!Curve)
	{
		return;
	}

	Curve->Reset();
	Curve->AddKey(0.00f, PreviewBowRadiusCm);
	Curve->AddKey(0.18f, PreviewForeShoulderRadiusCm);
	Curve->AddKey(0.50f, PreviewMidBodyRadiusCm);
	Curve->AddKey(0.82f, PreviewAftShoulderRadiusCm);
	Curve->AddKey(1.00f, PreviewSternRadiusCm);
}

void ASubmarineCompilerActor::RefreshSocketsFromSolution()
{
	const FCompartmentPlacement* HelmCompartment = LastSolution.Compartments.FindByPredicate([](const FCompartmentPlacement& Placement)
	{
		return Placement.Type == ECompartmentType::Helm;
	});

	if (HelmSocket && HelmCompartment)
	{
		const float MidX = (HelmCompartment->SpineStartCm + HelmCompartment->SpineEndCm) * 0.5f;
		HelmSocket->SetRelativeLocation(FVector(MidX, 0.f, HelmCompartment->FloorOffsetCm + 90.f));

		if (CrewSpawnSocketP1)
		{
			// Capsule half-height is ~88-96cm. Place actor center at floor + half-height
			// so capsule bottom sits exactly on the floor surface.
			CrewSpawnSocketP1->SetRelativeLocation(FVector(MidX - 160.f, 0.f, HelmCompartment->FloorOffsetCm + 92.f));
		}
	}

	const FStationPlacement* TurretStation = LastSolution.Stations.FindByPredicate([](const FStationPlacement& Placement)
	{
		return Placement.StationType == ESubStationType::Turret;
	});

	if (TurretHardpoint)
	{
		if (TurretStation)
		{
			const FCompartmentPlacement* StationCompartment = LastSolution.Compartments.FindByPredicate([TurretStation](const FCompartmentPlacement& Placement)
			{
				return Placement.CompartmentId == TurretStation->CompartmentId;
			});

			const float HardpointZ = StationCompartment
				? StationCompartment->EffectiveRadiusCm + 40.f
				: 220.f;
			TurretHardpoint->SetRelativeLocation(FVector(TurretStation->LocalTransform.GetLocation().X, 0.f, HardpointZ));
		}
		else if (LastSolution.Compartments.Num() > 0)
		{
			const FCompartmentPlacement& AftCompartment = LastSolution.Compartments.Last();
			const float MidX = (AftCompartment.SpineStartCm + AftCompartment.SpineEndCm) * 0.5f;
			TurretHardpoint->SetRelativeLocation(FVector(MidX, 0.f, AftCompartment.EffectiveRadiusCm + 40.f));
		}
	}
}

void ASubmarineCompilerActor::RefreshExteriorCollisionProxy()
{
	ConfigureExteriorCollisionProxy();

	if (!ExteriorCollisionProxy)
	{
		return;
	}

	float MinX = 0.f;
	float MaxX = PreviewSpineLengthCm;
	float MaxRadiusCm = PreviewDefaultRadiusCm;

	if (LastSolution.IsValid() && LastSolution.Compartments.Num() > 0)
	{
		MinX = LastSolution.Compartments[0].SpineStartCm;
		MaxX = LastSolution.Compartments[0].SpineEndCm;
		MaxRadiusCm = FMath::Max(1.f, LastSolution.Compartments[0].EffectiveRadiusCm);

		for (const FCompartmentPlacement& Compartment : LastSolution.Compartments)
		{
			MinX = FMath::Min(MinX, Compartment.SpineStartCm);
			MaxX = FMath::Max(MaxX, Compartment.SpineEndCm);
			MaxRadiusCm = FMath::Max(MaxRadiusCm, Compartment.EffectiveRadiusCm);
		}
	}

	const float WidthToHeightRatio = ResolvedEnvelopeDef
		? FMath::Max(0.5f, ResolvedEnvelopeDef->WidthToHeightRatio)
		: FMath::Max(0.5f, PreviewWidthToHeightRatio);

	const float HalfLength = FMath::Max(50.f, (MaxX - MinX) * 0.5f) + ExteriorCollisionPaddingCm;
	const float CapsuleRadius = FMath::Max(50.f, FMath::Max(MaxRadiusCm * WidthToHeightRatio, MaxRadiusCm)) + ExteriorCollisionPaddingCm;
	const float CenterX = (MinX + MaxX) * 0.5f;

	ExteriorCollisionProxy->SetRelativeLocation(FVector(CenterX, 0.f, 0.f));
	// Capsule is rotated 90° so its internal axis maps to X. HalfHeight = half-length.
	ExteriorCollisionProxy->SetCapsuleSize(CapsuleRadius, HalfLength);
	ExteriorCollisionProxy->SetVisibility(bShowExteriorCollisionProxyInEditor);

	if (HullMesh)
	{
		const bool bUseMovementProxy = bUseExteriorCollisionProxy
			&& ExteriorCollisionProxy
			&& ExteriorCollisionProxy->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
		const bool bUseGeneratedExteriorCollision = !bUseMovementProxy
			&& bPreferGeneratedExteriorMeshCollision
			&& GeneratedExteriorMesh
			&& GeneratedExteriorMesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
		HullMesh->SetCollisionEnabled((bUseGeneratedExteriorCollision || bUseMovementProxy)
			? ECollisionEnabled::NoCollision
			: ECollisionEnabled::QueryAndPhysics);
	}

	if (bDebugLogExteriorCollisionProxy)
	{
		LogExteriorCollisionProxyState();
	}
}

void ASubmarineCompilerActor::LogExteriorCollisionProxyState() const
{
	if (!ExteriorCollisionProxy)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SubCompiler][CollisionProxy] Missing proxy component"));
		return;
	}

	const float CapsuleRadius = ExteriorCollisionProxy->GetUnscaledCapsuleRadius();
	const float CapsuleHalfHeight = ExteriorCollisionProxy->GetUnscaledCapsuleHalfHeight();
	const FVector WorldLocation = ExteriorCollisionProxy->GetComponentLocation();
	const FVector LocalLocation = ExteriorCollisionProxy->GetRelativeLocation();
	const UPrimitiveComponent* ActiveMovementCollision = GetMovementCollisionComponent();
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[SubCompiler][CollisionProxy] UseProxy=%d | PreferGenerated=%d | Active=%s | ProxyCollision=%s | GeneratedCollision=%s | LocalCenter=%s | WorldCenter=%s | Radius=%.1f | HalfHeight=%.1f | HullCollision=%s"),
		bUseExteriorCollisionProxy ? 1 : 0,
		bPreferGeneratedExteriorMeshCollision ? 1 : 0,
		*GetNameSafe(ActiveMovementCollision),
		ExteriorCollisionProxy ? *UEnum::GetValueAsString(ExteriorCollisionProxy->GetCollisionEnabled()) : TEXT("None"),
		GeneratedExteriorMesh ? *UEnum::GetValueAsString(GeneratedExteriorMesh->GetCollisionEnabled()) : TEXT("None"),
		*LocalLocation.ToCompactString(),
		*WorldLocation.ToCompactString(),
		CapsuleRadius,
		CapsuleHalfHeight,
		HullMesh ? *UEnum::GetValueAsString(HullMesh->GetCollisionEnabled()) : TEXT("NoHullMesh"));
}

void ASubmarineCompilerActor::ConfigureExteriorCollisionProxy()
{
	if (!ExteriorCollisionProxy)
	{
		return;
	}

	ExteriorCollisionProxy->SetCollisionProfileName(TEXT("SubmarineHull"));
	ExteriorCollisionProxy->SetCollisionEnabled(
		bUseExteriorCollisionProxy
			? ECollisionEnabled::QueryAndPhysics
			: ECollisionEnabled::NoCollision);
	ExteriorCollisionProxy->SetGenerateOverlapEvents(true);
	ExteriorCollisionProxy->SetNotifyRigidBodyCollision(false);
	ExteriorCollisionProxy->SetCanEverAffectNavigation(false);
	ExteriorCollisionProxy->SetMobility(EComponentMobility::Movable);
	ExteriorCollisionProxy->ShapeColor = FColor(0, 220, 255);
	ExteriorCollisionProxy->SetHiddenInGame(true);
}

void ASubmarineCompilerActor::LogValidationMessages() const
{
	if (!bLogValidationMessages || LastMessages.Num() == 0)
	{
		return;
	}

	for (const FLayoutValidationMessage& Message : LastMessages)
	{
		const TCHAR* Prefix = TEXT("Info");
		switch (Message.Severity)
		{
		case ELayoutValidationSeverity::Warning:
			Prefix = TEXT("Warning");
			break;
		case ELayoutValidationSeverity::Error:
			Prefix = TEXT("Error");
			break;
		case ELayoutValidationSeverity::OK:
		default:
			break;
		}

		UE_LOG(
			LogTemp,
			Log,
			TEXT("[SubCompiler][%s] %s (%s)"),
			Prefix,
			*Message.Message.ToString(),
			*Message.RelatedId.ToString());
	}
}

void ASubmarineCompilerActor::DestroyGeneratedInteriorMeshes()
{
	TInlineComponentArray<UProceduralMeshComponent*> MeshComponents(this);

#if WITH_EDITOR
	if (GEditor && (MeshComponents.Num() > 0 || GeneratedCompartmentLights.Num() > 0))
	{
		GEditor->SelectNone(true, true);
	}
#endif

	for (UProceduralMeshComponent* MeshComponent : MeshComponents)
	{
		if (!MeshComponent)
		{
			continue;
		}

		const FString ComponentName = MeshComponent->GetName();
		if (ComponentName.StartsWith(TEXT("PMC_Interior_"))
			|| ComponentName.StartsWith(TEXT("PMC_Bulkhead_"))
			|| ComponentName.StartsWith(TEXT("PMC_ExteriorHull")))
		{
			MeshComponent->DestroyComponent();
		}
	}

	GeneratedInteriorMeshes.Reset();
	GeneratedExteriorMesh = nullptr;

	// Destroy compartment lights.
	for (UPointLightComponent* Light : GeneratedCompartmentLights)
	{
		if (Light) { Light->DestroyComponent(); }
	}
	GeneratedCompartmentLights.Reset();
}

bool ASubmarineCompilerActor::BuildGeneratedStations()
{
	DestroyGeneratedStations();

	if (!HasAuthority() || !CompiledLayoutAsset || !GetWorld())
	{
		return false;
	}

	GeneratedStations.Reserve(CompiledLayoutAsset->StationSlots.Num());
	for (const FStationSlotDef& SlotDef : CompiledLayoutAsset->StationSlots)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		const FTransform WorldTransform = SlotDef.LocalTransform * GetActorTransform();
		ASubCompilerPlaceholderStation* StationActor = GetWorld()->SpawnActor<ASubCompilerPlaceholderStation>(
			ASubCompilerPlaceholderStation::StaticClass(),
			WorldTransform,
			SpawnParams);
		if (!StationActor)
		{
			continue;
		}

		StationActor->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		StationActor->SetActorRelativeTransform(SlotDef.LocalTransform);
		StationActor->InitializeFromSlot(SlotDef, this);
		GeneratedStations.Add(StationActor);
	}

	if (StationManager)
	{
		StationManager->DiscoverAttachedStations();
	}

	return GeneratedStations.Num() == CompiledLayoutAsset->StationSlots.Num();
}

bool ASubmarineCompilerActor::BuildGeneratedDoors()
{
	DestroyGeneratedDoors();

	if (!HasAuthority() || !CompiledLayoutAsset || !GetWorld())
	{
		return false;
	}

	if (CompiledLayoutAsset->Doors.Num() == 0)
	{
		return true;
	}

	if (!DoorActorClass)
	{
		return false;
	}

	GeneratedDoors.Reserve(CompiledLayoutAsset->Doors.Num());
	for (const FDoorDef& DoorDef : CompiledLayoutAsset->Doors)
	{
		const FStructuralSheetDef* SheetDef = CompiledLayoutAsset->StructuralSheets.FindByPredicate([&DoorDef](const FStructuralSheetDef& Candidate)
		{
			return Candidate.SheetId == DoorDef.BulkheadSheetId;
		});
		if (!SheetDef)
		{
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		const FTransform WorldTransform = DoorDef.LocalTransform * GetActorTransform();
		ASubDoorActor* DoorActor = GetWorld()->SpawnActor<ASubDoorActor>(
			DoorActorClass,
			WorldTransform,
			SpawnParams);
		if (!DoorActor)
		{
			continue;
		}

		DoorActor->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DoorActor->SetActorRelativeTransform(DoorDef.LocalTransform);
		DoorActor->InitializeFromDoorDef(DoorDef, SheetDef->ParentCompartmentId, SheetDef->AdjacentCompartmentId, this);
		GeneratedDoors.Add(DoorActor);
	}

	return GeneratedDoors.Num() == CompiledLayoutAsset->Doors.Num();
}

void ASubmarineCompilerActor::DestroyGeneratedDoors()
{
	for (ASubDoorActor* Door : GeneratedDoors)
	{
		if (IsValid(Door))
		{
			Door->Destroy();
		}
	}

	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors, true);
	for (AActor* AttachedActor : AttachedActors)
	{
		if (AttachedActor && AttachedActor->IsA<ASubDoorActor>())
		{
			AttachedActor->Destroy();
		}
	}

	GeneratedDoors.Reset();
}

void ASubmarineCompilerActor::DestroyGeneratedStations()
{
	for (ASubStationBase* Station : GeneratedStations)
	{
		if (IsValid(Station))
		{
			Station->Destroy();
		}
	}

	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors, true);
	for (AActor* AttachedActor : AttachedActors)
	{
		if (AttachedActor && AttachedActor->IsA<ASubCompilerPlaceholderStation>())
		{
			AttachedActor->Destroy();
		}
	}

	GeneratedStations.Reset();
}

void ASubmarineCompilerActor::ComputeDirtyCompartments(
	const FSubmarineLayoutSolution& OldSolution,
	const FSubmarineLayoutSolution& NewSolution,
	TSet<FName>& OutDirtyIds) const
{
	OutDirtyIds.Reset();

	if (!OldSolution.IsValid())
	{
		for (const FCompartmentPlacement& Comp : NewSolution.Compartments)
		{
			OutDirtyIds.Add(Comp.CompartmentId);
		}
		return;
	}

	TMap<FName, const FCompartmentPlacement*> OldMap;
	for (const FCompartmentPlacement& Comp : OldSolution.Compartments)
	{
		OldMap.Add(Comp.CompartmentId, &Comp);
	}

	TSet<FName> NewIds;
	for (const FCompartmentPlacement& NewComp : NewSolution.Compartments)
	{
		NewIds.Add(NewComp.CompartmentId);
		const FCompartmentPlacement* const* OldCompPtr = OldMap.Find(NewComp.CompartmentId);
		if (!OldCompPtr)
		{
			OutDirtyIds.Add(NewComp.CompartmentId);
			continue;
		}

		const FCompartmentPlacement& OldComp = **OldCompPtr;
		if (!FMath::IsNearlyEqual(OldComp.SpineStartCm, NewComp.SpineStartCm, 0.01f)
			|| !FMath::IsNearlyEqual(OldComp.SpineEndCm, NewComp.SpineEndCm, 0.01f)
			|| !FMath::IsNearlyEqual(OldComp.EffectiveRadiusCm, NewComp.EffectiveRadiusCm, 0.01f)
			|| !FMath::IsNearlyEqual(OldComp.FloorOffsetCm, NewComp.FloorOffsetCm, 0.01f))
		{
			OutDirtyIds.Add(NewComp.CompartmentId);
		}
	}

	for (const FCompartmentPlacement& OldComp : OldSolution.Compartments)
	{
		if (!NewIds.Contains(OldComp.CompartmentId))
		{
			OutDirtyIds.Add(OldComp.CompartmentId);
		}
	}

	// Expand dirty set to neighbors (for bulkhead consistency)
	TSet<FName> ExpandedDirty = OutDirtyIds;
	for (int32 i = 0; i < NewSolution.Compartments.Num(); ++i)
	{
		if (OutDirtyIds.Contains(NewSolution.Compartments[i].CompartmentId))
		{
			if (i > 0)
			{
				ExpandedDirty.Add(NewSolution.Compartments[i - 1].CompartmentId);
			}
			if (i < NewSolution.Compartments.Num() - 1)
			{
				ExpandedDirty.Add(NewSolution.Compartments[i + 1].CompartmentId);
			}
		}
	}
	OutDirtyIds = MoveTemp(ExpandedDirty);
}

void ASubmarineCompilerActor::DestroyDirtyInteriorMeshes(const TSet<FName>& DirtyIds)
{
	TInlineComponentArray<UProceduralMeshComponent*> MeshComponents(this);
	for (UProceduralMeshComponent* MeshComponent : MeshComponents)
	{
		if (!MeshComponent)
		{
			continue;
		}

		const FString ComponentName = MeshComponent->GetName();

		if (ComponentName.StartsWith(TEXT("PMC_Interior_")))
		{
			const FString IdStr = ComponentName.Mid(13);
			if (DirtyIds.Contains(FName(*IdStr)))
			{
				MeshComponent->DestroyComponent();
			}
		}
		else if (ComponentName.StartsWith(TEXT("PMC_Bulkhead_")))
		{
			const FString Suffix = ComponentName.Mid(13);
			int32 SepIdx = INDEX_NONE;
			Suffix.FindChar(TEXT('_'), SepIdx);
			if (SepIdx != INDEX_NONE)
			{
				const FName ForeId(*Suffix.Left(SepIdx));
				const FName AftId(*Suffix.Mid(SepIdx + 1));
				if (DirtyIds.Contains(ForeId) || DirtyIds.Contains(AftId))
				{
					MeshComponent->DestroyComponent();
				}
			}
		}
	}

	GeneratedInteriorMeshes.RemoveAll([](const TObjectPtr<UProceduralMeshComponent>& Comp)
	{
		return !IsValid(Comp);
	});
}
