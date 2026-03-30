#include "SubCompilerPlaceholderStation.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Submarine/InteractableComponent.h"
#include "Submarine/SubCrewCharacter.h"
#include "Submarine/SubmarineBase.h"
#include "Submarine/SubmarineLayoutAsset.h"

ASubCompilerPlaceholderStation::ASubCompilerPlaceholderStation()
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(SceneRoot);
	InteractionVolume->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	InteractionVolume->SetBoxExtent(FVector(20.f, 35.f, 70.f));
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	InteractionVolume->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	InteractionVolume->SetGenerateOverlapEvents(false);
	InteractionVolume->SetCanEverAffectNavigation(false);

	LabelComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelComponent->SetupAttachment(SceneRoot);
	LabelComponent->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	LabelComponent->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	LabelComponent->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	LabelComponent->SetWorldSize(28.f);
	LabelComponent->SetTextRenderColor(FColor::White);
	LabelComponent->SetText(FText::FromString(TEXT("Station")));
	LabelComponent->SetCanEverAffectNavigation(false);

	InteractableComponent = CreateDefaultSubobject<UInteractableComponent>(TEXT("Interactable"));
	InteractableComponent->OnInteract.AddDynamic(this, &ASubCompilerPlaceholderStation::HandlePlaceholderInteract);

	bAutoResolveOwningSubmarine = false;
	bRequireOwningSubmarine = true;
	bExclusiveOccupancy = true;
}

void ASubCompilerPlaceholderStation::InitializeFromSlot(const FStationSlotDef& SlotDef, ASubmarineBase* InSubmarine)
{
	PlaceholderStationId = SlotDef.StationId;
	StationType = SlotDef.StationType;
	SetOwningSubmarine(InSubmarine);
	SetActorRelativeTransform(ComputeReadableLocalTransform(SlotDef.LocalTransform));
	UpdateLabel();
}

void ASubCompilerPlaceholderStation::HandlePlaceholderInteract(ASubCrewCharacter* Interactor)
{
	if (!Interactor || !HasAuthority())
	{
		return;
	}

	ISubStationInterface::Execute_RequestEnterStation(this, Interactor->GetController());
}

void ASubCompilerPlaceholderStation::UpdateLabel()
{
	if (!LabelComponent)
	{
		return;
	}

	const FString TypeLabel = UEnum::GetDisplayValueAsText(StationType).ToString();
	const FString StationIdLabel = PlaceholderStationId.IsNone() ? TEXT("Station") : PlaceholderStationId.ToString();
	LabelComponent->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s"), *TypeLabel, *StationIdLabel)));
}

FTransform ASubCompilerPlaceholderStation::ComputeReadableLocalTransform(const FTransform& SlotTransform) const
{
	constexpr float InwardOffsetCm = 45.f;

	FTransform AdjustedTransform = SlotTransform;
	const FVector LocalForward = SlotTransform.GetRotation().GetForwardVector();
	AdjustedTransform.AddToTranslation(LocalForward * InwardOffsetCm);
	return AdjustedTransform;
}
