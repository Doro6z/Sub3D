#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubmarineRuntimeTypes.h"
#include "SubmarineTypes.h"
#include "SubMovementComponent.generated.h"

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float BaseMass = 200000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float WaterDensity = 1025.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float SubmergedVolume = 205.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	bool bAutoNeutralBuoyancyOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NeutralBuoyancyFill01 = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float NeutralBuoyancyMassBiasKg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	FVector DragCoefficients = FVector(0.18f, 1.2f, 1.1f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	FVector CrossSections = FVector(2.f, 20.f, 20.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float MaxThrust = 25000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float MaxForwardSpeed = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float MaxReverseSpeed = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float MaxVerticalSpeed = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float IdleForwardSpeedDamping = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float LateralSpeedDamping = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float ContactVelocityDamping = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float RudderTurnRate = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float DivePlanePitchRate = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float HydroplaneAuthoritySpeed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float PitchRateDamping = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float PitchFromHydroplaneAccel = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float VerticalFromPitchFactor = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float MaxDivePlanePitch = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float BallastPitchFactor = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	bool bEnableBallastTrimPitch = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float BallastTrimPitchRate = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Flooding")
	float FloodedMassInfluence = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Ballasts")
	TArray<FBallastTank> Ballasts;

	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Ballasts")
	float GlobalTargetFill = 0.5f;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	float CurrentDepth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	float FloodedMassKg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Net")
	float FixedSimulationHz = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Net")
	float InterpSnapDistanceCm = 500.f;

	void SetThrustInput(float Value) { ThrustInput = FMath::Clamp(Value, -1.f, 1.f); }
	void SetRudderInput(float Value) { RudderInput = FMath::Clamp(Value, -1.f, 1.f); }
	void SetDivePlaneInput(float Value) { DivePlaneInput = FMath::Clamp(Value, -1.f, 1.f); }

	void SetBallastTarget(int32 Index, float Target);
	void ResyncAllBallasts();
	void ApplyCommandState(const FSubmarineCommandState& CommandState);

	UFUNCTION(BlueprintPure, Category = "Submarine|Physics")
	float ComputeTotalMass() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Physics")
	float ComputeBuoyancyForce() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Physics")
	float ComputeCenterOfMassXOffset() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Physics")
	float GetPressureAtDepth(float DepthMeters) const;

	void HandleReplicatedNetState(const FSubmarineNetState& NewState);
	int32 GetSimFrameCounter() const { return SimFrameCounter; }

private:
	float ThrustInput = 0.f;
	float RudderInput = 0.f;
	float DivePlaneInput = 0.f;
	float PitchRateDegPerSec = 0.f;
	float SimAccumulator = 0.f;
	int32 SimFrameCounter = 0;

	FSubmarineNetState PrevSnapshot;
	FSubmarineNetState TargetSnapshot;
	float InterpAlpha = 1.f;
	float InterpDuration = 1.f / 20.f;
	bool bHasReceivedSnapshot = false;

	void UpdateBallasts(float DeltaTime);
	void SimulateStep(float DeltaTime);
	void ApplyPhysics(float DeltaTime);
	float ComputeFloodedMassKg() const;
	void InterpolateClient(float DeltaTime);
	void InitializeNeutralBuoyancy();
};
