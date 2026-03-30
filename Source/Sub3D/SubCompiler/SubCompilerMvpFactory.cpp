#include "SubCompilerMvpFactory.h"

#include "Curves/RichCurve.h"
#include "SubCompilerTypes.h"
#include "SubmarineEnvelopeDef.h"
#include "SubmarineFunctionalGraph.h"

USubmarineEnvelopeDef* FSubCompilerMvpFactory::CreateEnvelope(UObject* Outer)
{
	UObject* EffectiveOuter = Outer ? Outer : GetTransientPackage();
	USubmarineEnvelopeDef* Envelope = NewObject<USubmarineEnvelopeDef>(EffectiveOuter);
	if (!Envelope)
	{
		return nullptr;
	}

	Envelope->SpineLengthCm = 2100.f;
	Envelope->DefaultRadiusCm = 250.f;
	Envelope->FloorDropBiasCm = 90.f;
	Envelope->ExteriorLongitudinalSubdivisionsPerSpan = 6;
	Envelope->ExteriorRadialSegments = 32;
	Envelope->MaxCompartments = 6;
	Envelope->SectionExponent = 2.f;
	Envelope->WidthToHeightRatio = 1.f;
	Envelope->BowProfile = EBowSternProfile::Rounded;
	Envelope->SternProfile = EBowSternProfile::Tapered;
	Envelope->BowTaperFraction = 0.12f;
	Envelope->SternTaperFraction = 0.15f;

	FRichCurve* Curve = Envelope->RadiusProfile.GetRichCurve();
	if (Curve)
	{
		Curve->Reset();
		Curve->AddKey(0.00f, 175.f);
		Curve->AddKey(0.12f, 210.f);
		Curve->AddKey(0.28f, 260.f);
		Curve->AddKey(0.48f, 290.f);
		Curve->AddKey(0.68f, 290.f);
		Curve->AddKey(0.84f, 235.f);
		Curve->AddKey(1.00f, 160.f);
	}

	return Envelope;
}

USubmarineFunctionalGraph* FSubCompilerMvpFactory::CreateFunctionalGraph(UObject* Outer)
{
	UObject* EffectiveOuter = Outer ? Outer : GetTransientPackage();
	USubmarineFunctionalGraph* Graph = NewObject<USubmarineFunctionalGraph>(EffectiveOuter);
	if (!Graph)
	{
		return nullptr;
	}

	FCompartmentNode Ballast;
	Ballast.CompartmentId = TEXT("Ballast_Fwd");
	Ballast.Type = ECompartmentType::Ballast;
	Ballast.MinLengthCm = 250.f;
	Ballast.Priority = 0;
	Ballast.RequiredSystems = { ESubStationType::Ballast };

	FCompartmentNode Helm;
	Helm.CompartmentId = TEXT("Helm");
	Helm.Type = ECompartmentType::Helm;
	Helm.MinLengthCm = 280.f;
	Helm.Priority = 1;
	Helm.RequiredSystems = { ESubStationType::Helm };

	FCompartmentNode Engine;
	Engine.CompartmentId = TEXT("Engine");
	Engine.Type = ECompartmentType::Engine;
	Engine.MinLengthCm = 320.f;
	Engine.Priority = 2;
	Engine.RequiredSystems = { ESubStationType::Engine, ESubStationType::Pump };

	FCompartmentNode Airlock;
	Airlock.CompartmentId = TEXT("Airlock_Aft");
	Airlock.Type = ECompartmentType::Airlock;
	Airlock.MinLengthCm = 200.f;
	Airlock.Priority = 3;
	Airlock.RequiredSystems = { ESubStationType::Turret };

	Graph->Compartments = { Ballast, Helm, Engine, Airlock };

	FPassageEdge D1;
	D1.FromCompartmentId = Ballast.CompartmentId;
	D1.ToCompartmentId = Helm.CompartmentId;
	D1.Type = EPassageType::WatertightDoor;
	D1.MinWidthCm = 90.f;
	D1.MinHeightCm = 180.f;

	FPassageEdge D2;
	D2.FromCompartmentId = Helm.CompartmentId;
	D2.ToCompartmentId = Engine.CompartmentId;
	D2.Type = EPassageType::WatertightDoor;
	D2.MinWidthCm = 90.f;
	D2.MinHeightCm = 180.f;

	FPassageEdge D3;
	D3.FromCompartmentId = Engine.CompartmentId;
	D3.ToCompartmentId = Airlock.CompartmentId;
	D3.Type = EPassageType::WatertightDoor;
	D3.MinWidthCm = 90.f;
	D3.MinHeightCm = 180.f;

	Graph->Passages = { D1, D2, D3 };
	return Graph;
}
