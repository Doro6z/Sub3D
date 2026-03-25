#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SubmarineRuntimeTypes.h"
#include "SubDoorActor.generated.h"

class ASubCrewCharacter;
class ASubmarineBase;
class UInteractableComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class SUB3D_API ASubDoorActor : public AActor
{
	GENERATED_BODY()

public:
	ASubDoorActor();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInteractableComponent* Interactable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FName DoorId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FName CompartmentA = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FName CompartmentB = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bStartsClosed = false;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DoorClosed, Category = "Door")
	bool bClosed = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Door")
	TObjectPtr<ASubmarineBase> OwningSubmarine = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Door")
	void SetDoorClosed(bool bNewClosed);

	UFUNCTION(BlueprintCallable, Category = "Door")
	void ToggleDoor();

	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsDoorClosed() const { return bClosed; }

protected:
	UFUNCTION()
	void HandleInteract(ASubCrewCharacter* Interactor);

	UFUNCTION()
	void OnRep_DoorClosed();

	UFUNCTION(BlueprintImplementableEvent, Category = "Door")
	void BP_OnDoorStateChanged(bool bNowClosed);

private:
	void TryResolveOwningSubmarine();
	void ApplyDoorState();
	void RegisterWithCompartments();
};
