#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SubLightBase.generated.h"

class USceneComponent;
class UPointLightComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTextureLightProfile;

UENUM(BlueprintType)
enum class ESubLightBehavior : uint8
{
	Steady      UMETA(DisplayName = "Steady"),
	Blink       UMETA(DisplayName = "Blink"),
	Faulty      UMETA(DisplayName = "Faulty"),
	GyroSpin    UMETA(DisplayName = "Gyro Spin")
};

UCLASS(Blueprintable)
class SUB3D_API ASubLightBase : public AActor
{
	GENERATED_BODY()

public:
	ASubLightBase();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FixtureMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> LightPivot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpotLightComponent> LightComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> PointLightComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|General")
	bool bStartEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|General")
	ESubLightBehavior Behavior = ESubLightBehavior::Steady;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Visual")
	FLinearColor LightColor = FLinearColor(1.f, 0.92f, 0.78f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Visual")
	bool bUseSpotLight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Visual", meta = (ClampMin = "0.0"))
	float ActiveIntensity = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Visual", meta = (ClampMin = "0.0"))
	float InactiveIntensity = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Visual", meta = (ClampMin = "0.0"))
	float AttenuationRadius = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Visual", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float InnerConeAngle = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Visual", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float OuterConeAngle = 42.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Rendering")
	bool bUseInverseSquaredFalloff = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Rendering", meta = (ClampMin = "0.0"))
	float VolumetricScatteringIntensity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Rendering")
	bool bCastSpotShadows = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Rendering")
	bool bCastPointShadows = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Rendering")
	bool bLightingChannel0 = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Rendering")
	bool bLightingChannel1 = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Rendering")
	bool bLightingChannel2 = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|IES")
	TObjectPtr<UTextureLightProfile> IESTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|IES", meta = (EditCondition = "IESTexture != nullptr"))
	bool bUseIESBrightness = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|IES", meta = (EditCondition = "IESTexture != nullptr", ClampMin = "0.0"))
	float IESBrightnessScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|LightFunction")
	TObjectPtr<UMaterialInterface> LightFunctionMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|LightFunction")
	FVector LightFunctionScale = FVector(128.f, 128.f, 256.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|LightFunction", meta = (ClampMin = "0.0"))
	float LightFunctionFadeDistance = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|LightFunction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LightFunctionDisabledBrightness = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|PointLight")
	bool bUsePointLight = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|PointLight", meta = (ClampMin = "0.0"))
	float PointActiveIntensity = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|PointLight", meta = (ClampMin = "0.0"))
	float PointInactiveIntensity = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|PointLight", meta = (ClampMin = "0.0"))
	float PointAttenuationRadius = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Emissive")
	bool bDriveFixtureEmissive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Emissive")
	bool bApplyEmissiveToAllMaterialSlots = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Emissive", meta = (ClampMin = "0"))
	int32 EmissiveMaterialSlot = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Emissive")
	FName EmissiveColorParameter = TEXT("EmissiveColor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Emissive")
	FName EmissiveIntensityParameter = TEXT("EmissiveIntensity");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Emissive", meta = (ClampMin = "0.0"))
	float EmissiveActiveIntensity = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Emissive", meta = (ClampMin = "0.0"))
	float EmissiveInactiveIntensity = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Blink", meta = (ClampMin = "0.01"))
	float BlinkFrequencyHz = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Blink", meta = (ClampMin = "0.01", ClampMax = "0.99"))
	float BlinkDutyCycle01 = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Faulty")
	FVector2D FaultyOnDurationRangeSeconds = FVector2D(0.06f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Faulty")
	FVector2D FaultyOffDurationRangeSeconds = FVector2D(0.03f, 0.20f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Faulty", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FaultyDimChance01 = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Faulty", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FaultyDimIntensityScaleMin = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Faulty", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FaultyDimIntensityScaleMax = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Faulty")
	int32 FaultyRandomSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubLight|Gyro", meta = (ClampMin = "0.0"))
	float GyroYawRateDegPerSecond = 120.f;

	UFUNCTION(BlueprintCallable, Category = "SubLight")
	void SetLightEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "SubLight")
	void SetLightColor(const FLinearColor& NewColor);

	UFUNCTION(BlueprintCallable, Category = "SubLight")
	void SetBehavior(ESubLightBehavior NewBehavior);

	UFUNCTION(BlueprintPure, Category = "SubLight")
	bool IsLightEnabled() const { return bLightEnabled; }

private:
	void ResetRuntimeState();
	void ApplyLightRenderingSettings();
	void EnsureFixtureDynamicMaterials();
	void AdvanceFaultyState(float DeltaSeconds);
	void ApplyFixtureEmissiveState(bool bShouldBeLit, float IntensityScale);
	void ApplyVisualState(bool bShouldBeLit, float IntensityScale);
	float SampleRangeSafe(const FVector2D& Range) const;

private:
	bool bLightEnabled = true;
	bool bFaultySegmentLit = true;
	float FaultySegmentRemainingSeconds = 0.f;
	float FaultyIntensityScale = 1.f;
	float BehaviorElapsedSeconds = 0.f;
	FRotator BasePivotRotation = FRotator::ZeroRotator;
	FRandomStream FaultyRandom;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FixtureDynamicMaterials;
};
