#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CrewAnimDebugTypes.h"
#include "CrewAnimDebugComponent.generated.h"

UCLASS(ClassGroup = (Debug), meta = (BlueprintSpawnableComponent))
class SUB3D_API UCrewAnimDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCrewAnimDebugComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "Crew|AnimDebug")
	FCrewAnimDebugSnapshot GetCurrentSnapshot() const { return CurrentSnapshot; }

	UFUNCTION(BlueprintCallable, Category = "Crew|AnimDebug")
	bool BuildSnapshot(FCrewAnimDebugSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintCallable, Category = "Crew|AnimDebug")
	FString FormatSnapshotForLog() const;

	UFUNCTION(BlueprintCallable, Category = "Crew|AnimDebug")
	void DumpSnapshotToLog() const;

private:
	void PushSnapshot(const FCrewAnimDebugSnapshot& Snapshot);

	UPROPERTY(Transient)
	FCrewAnimDebugSnapshot CurrentSnapshot;

	UPROPERTY(Transient)
	TArray<FCrewAnimDebugSnapshot> RecentSnapshots;

	float SampleTimerSeconds = 0.f;
};
