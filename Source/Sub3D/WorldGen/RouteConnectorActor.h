#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RouteConnectorGenerator.h"
#include "RouteConnectorActor.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;

UCLASS()
class SUB3D_API ARouteConnectorActor : public AActor
{
	GENERATED_BODY()

public:
	ARouteConnectorActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connector")
	FRouteConnectorBuildSettings BuildSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connector")
	TObjectPtr<UMaterialInterface> MaterialOverride = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Connector")
	TObjectPtr<UProceduralMeshComponent> MeshComponent = nullptr;

	UFUNCTION(BlueprintCallable, Category="Connector")
	bool BuildConnector(const FTransform& StartWorld, float StartRadiusCm, const FTransform& EndWorld, float EndRadiusCm);

	UFUNCTION(BlueprintCallable, Category="Connector")
	void ClearConnector();
};
