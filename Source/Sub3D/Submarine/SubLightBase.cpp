#include "SubLightBase.h"

#include "Engine/TextureLightProfile.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

ASubLightBase::ASubLightBase()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	FixtureMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FixtureMesh"));
	FixtureMesh->SetupAttachment(SceneRoot);
	FixtureMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FixtureMesh->SetCanEverAffectNavigation(false);

	LightPivot = CreateDefaultSubobject<USceneComponent>(TEXT("LightPivot"));
	LightPivot->SetupAttachment(SceneRoot);

	LightComponent = CreateDefaultSubobject<USpotLightComponent>(TEXT("LightComponent"));
	LightComponent->SetupAttachment(LightPivot);
	LightComponent->SetVisibility(true);

	PointLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("PointLightComponent"));
	PointLightComponent->SetupAttachment(LightPivot);
	PointLightComponent->SetVisibility(false);
	ApplyLightRenderingSettings();
}

void ASubLightBase::BeginPlay()
{
	Super::BeginPlay();

	BasePivotRotation = LightPivot ? LightPivot->GetRelativeRotation() : FRotator::ZeroRotator;
	ApplyLightRenderingSettings();
	EnsureFixtureDynamicMaterials();
	ResetRuntimeState();
	ApplyVisualState(bLightEnabled, 1.f);
}

void ASubLightBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	BehaviorElapsedSeconds += FMath::Max(0.f, DeltaSeconds);

	bool bShouldBeLit = bLightEnabled;
	float IntensityScale = 1.f;
	FRotator DesiredPivotRotation = BasePivotRotation;

	if (!bLightEnabled)
	{
		ApplyVisualState(false, 1.f);
		if (LightPivot)
		{
			LightPivot->SetRelativeRotation(BasePivotRotation);
		}
		return;
	}

	switch (Behavior)
	{
	case ESubLightBehavior::Steady:
		break;

	case ESubLightBehavior::Blink:
		{
			const float SafeFrequency = FMath::Max(0.01f, BlinkFrequencyHz);
			const float Phase01 = FMath::Fmod(BehaviorElapsedSeconds * SafeFrequency, 1.f);
			bShouldBeLit = Phase01 <= FMath::Clamp(BlinkDutyCycle01, 0.01f, 0.99f);
		}
		break;

	case ESubLightBehavior::Faulty:
		AdvanceFaultyState(DeltaSeconds);
		bShouldBeLit = bFaultySegmentLit;
		IntensityScale = FaultyIntensityScale;
		break;

	case ESubLightBehavior::GyroSpin:
		{
			const float YawOffset = BehaviorElapsedSeconds * GyroYawRateDegPerSecond;
			DesiredPivotRotation = BasePivotRotation + FRotator(0.f, YawOffset, 0.f);
		}
		break;

	default:
		break;
	}

	if (LightPivot)
	{
		LightPivot->SetRelativeRotation(DesiredPivotRotation);
	}

	ApplyVisualState(bShouldBeLit, IntensityScale);
}

void ASubLightBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (LightPivot)
	{
		BasePivotRotation = LightPivot->GetRelativeRotation();
		LightPivot->SetRelativeRotation(BasePivotRotation);
	}

	ApplyLightRenderingSettings();
	EnsureFixtureDynamicMaterials();
	ResetRuntimeState();
	ApplyVisualState(bStartEnabled, 1.f);
}

void ASubLightBase::SetLightEnabled(bool bEnabled)
{
	bLightEnabled = bEnabled;
	ApplyVisualState(bLightEnabled, 1.f);
}

void ASubLightBase::SetLightColor(const FLinearColor& NewColor)
{
	LightColor = NewColor;
	ApplyLightRenderingSettings();
	ApplyFixtureEmissiveState(bLightEnabled, 1.f);
}

void ASubLightBase::SetBehavior(ESubLightBehavior NewBehavior)
{
	Behavior = NewBehavior;
	ResetRuntimeState();
}

void ASubLightBase::ResetRuntimeState()
{
	bLightEnabled = bStartEnabled;
	bFaultySegmentLit = true;
	FaultyIntensityScale = 1.f;
	FaultySegmentRemainingSeconds = 0.f;
	BehaviorElapsedSeconds = 0.f;
	FaultyRandom.Initialize(FaultyRandomSeed);

	if (LightPivot)
	{
		BasePivotRotation = LightPivot->GetRelativeRotation();
	}
}

void ASubLightBase::ApplyLightRenderingSettings()
{
	const bool bHasIESProfile = IESTexture != nullptr;
	const bool bShouldUseIESBrightness = bHasIESProfile && bUseIESBrightness;
	const bool bShouldUseInverseSquared = bUseInverseSquaredFalloff || bShouldUseIESBrightness;
	const float SafeIESBrightnessScale = FMath::Max(0.f, IESBrightnessScale);
	const FVector SafeLightFunctionScale(
		FMath::Max(KINDA_SMALL_NUMBER, LightFunctionScale.X),
		FMath::Max(KINDA_SMALL_NUMBER, LightFunctionScale.Y),
		FMath::Max(KINDA_SMALL_NUMBER, LightFunctionScale.Z));
	const float SafeLightFunctionFadeDistance = FMath::Max(0.f, LightFunctionFadeDistance);
	const float SafeLightFunctionDisabledBrightness = FMath::Clamp(LightFunctionDisabledBrightness, 0.f, 1.f);
	const float SafeVolumetricScatteringIntensity = FMath::Max(0.f, VolumetricScatteringIntensity);

	if (LightComponent)
	{
		LightComponent->SetCastShadows(bCastSpotShadows);
		LightComponent->SetUseInverseSquaredFalloff(bShouldUseInverseSquared);
		LightComponent->SetLightColor(LightColor.ToFColor(true));
		LightComponent->SetAttenuationRadius(AttenuationRadius);
		LightComponent->SetInnerConeAngle(InnerConeAngle);
		LightComponent->SetOuterConeAngle(OuterConeAngle);
		LightComponent->SetVolumetricScatteringIntensity(SafeVolumetricScatteringIntensity);
		LightComponent->SetLightingChannels(bLightingChannel0, bLightingChannel1, bLightingChannel2);
		LightComponent->SetIESTexture(IESTexture);
		LightComponent->SetUseIESBrightness(bShouldUseIESBrightness);
		LightComponent->SetIESBrightnessScale(SafeIESBrightnessScale);
		LightComponent->SetLightFunctionMaterial(LightFunctionMaterial);
		LightComponent->SetLightFunctionScale(SafeLightFunctionScale);
		LightComponent->SetLightFunctionFadeDistance(SafeLightFunctionFadeDistance);
		LightComponent->SetLightFunctionDisabledBrightness(SafeLightFunctionDisabledBrightness);
	}

	if (PointLightComponent)
	{
		PointLightComponent->SetCastShadows(bCastPointShadows);
		PointLightComponent->SetUseInverseSquaredFalloff(bShouldUseInverseSquared);
		PointLightComponent->SetLightColor(LightColor.ToFColor(true));
		PointLightComponent->SetAttenuationRadius(PointAttenuationRadius);
		PointLightComponent->SetVolumetricScatteringIntensity(SafeVolumetricScatteringIntensity);
		PointLightComponent->SetLightingChannels(bLightingChannel0, bLightingChannel1, bLightingChannel2);
		PointLightComponent->SetIESTexture(IESTexture);
		PointLightComponent->SetUseIESBrightness(bShouldUseIESBrightness);
		PointLightComponent->SetIESBrightnessScale(SafeIESBrightnessScale);
		PointLightComponent->SetLightFunctionMaterial(LightFunctionMaterial);
		PointLightComponent->SetLightFunctionScale(SafeLightFunctionScale);
		PointLightComponent->SetLightFunctionFadeDistance(SafeLightFunctionFadeDistance);
		PointLightComponent->SetLightFunctionDisabledBrightness(SafeLightFunctionDisabledBrightness);
	}
}

void ASubLightBase::EnsureFixtureDynamicMaterials()
{
	if (!bDriveFixtureEmissive || !FixtureMesh)
	{
		FixtureDynamicMaterials.Reset();
		return;
	}

	const int32 MaterialCount = FixtureMesh->GetNumMaterials();
	if (MaterialCount <= 0)
	{
		FixtureDynamicMaterials.Reset();
		return;
	}

	FixtureDynamicMaterials.SetNumZeroed(MaterialCount);

	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		const bool bTargetSlot = bApplyEmissiveToAllMaterialSlots || MaterialIndex == EmissiveMaterialSlot;
		if (!bTargetSlot)
		{
			continue;
		}

		if (UMaterialInstanceDynamic* ExistingMID = Cast<UMaterialInstanceDynamic>(FixtureMesh->GetMaterial(MaterialIndex)))
		{
			FixtureDynamicMaterials[MaterialIndex] = ExistingMID;
			continue;
		}

		if (UMaterialInstanceDynamic* CreatedMID = FixtureMesh->CreateDynamicMaterialInstance(MaterialIndex))
		{
			FixtureDynamicMaterials[MaterialIndex] = CreatedMID;
		}
	}
}

void ASubLightBase::AdvanceFaultyState(float DeltaSeconds)
{
	FaultySegmentRemainingSeconds -= FMath::Max(0.f, DeltaSeconds);
	if (FaultySegmentRemainingSeconds > 0.f)
	{
		return;
	}

	bFaultySegmentLit = !bFaultySegmentLit;

	if (bFaultySegmentLit)
	{
		FaultySegmentRemainingSeconds = SampleRangeSafe(FaultyOnDurationRangeSeconds);

		const bool bDimThisPulse = FaultyRandom.FRand() <= FMath::Clamp(FaultyDimChance01, 0.f, 1.f);
		if (bDimThisPulse)
		{
			const float MinScale = FMath::Min(FaultyDimIntensityScaleMin, FaultyDimIntensityScaleMax);
			const float MaxScale = FMath::Max(FaultyDimIntensityScaleMin, FaultyDimIntensityScaleMax);
			FaultyIntensityScale = FaultyRandom.FRandRange(MinScale, MaxScale);
		}
		else
		{
			FaultyIntensityScale = 1.f;
		}
	}
	else
	{
		FaultySegmentRemainingSeconds = SampleRangeSafe(FaultyOffDurationRangeSeconds);
		FaultyIntensityScale = 1.f;
	}
}

void ASubLightBase::ApplyFixtureEmissiveState(bool bShouldBeLit, float IntensityScale)
{
	if (!bDriveFixtureEmissive || !FixtureMesh)
	{
		return;
	}

	EnsureFixtureDynamicMaterials();

	const FLinearColor EmissiveColorValue = bShouldBeLit ? LightColor : FLinearColor::Black;
	const float BaseEmissiveIntensity = bShouldBeLit ? EmissiveActiveIntensity : EmissiveInactiveIntensity;
	const float FinalEmissiveIntensity = FMath::Max(0.f, BaseEmissiveIntensity * FMath::Max(0.f, IntensityScale));

	for (UMaterialInstanceDynamic* MID : FixtureDynamicMaterials)
	{
		if (!MID)
		{
			continue;
		}

		if (!EmissiveColorParameter.IsNone())
		{
			MID->SetVectorParameterValue(EmissiveColorParameter, EmissiveColorValue);
		}
		if (!EmissiveIntensityParameter.IsNone())
		{
			MID->SetScalarParameterValue(EmissiveIntensityParameter, FinalEmissiveIntensity);
		}
	}
}

void ASubLightBase::ApplyVisualState(bool bShouldBeLit, float IntensityScale)
{
	const float SafeIntensityScale = FMath::Max(0.f, IntensityScale);
	ApplyLightRenderingSettings();

	if (LightComponent)
	{
		const float BaseIntensity = bShouldBeLit ? ActiveIntensity : InactiveIntensity;
		const float FinalIntensity = FMath::Max(0.f, BaseIntensity * SafeIntensityScale);
		LightComponent->SetIntensity(FinalIntensity);
		LightComponent->SetVisibility(bUseSpotLight && FinalIntensity > KINDA_SMALL_NUMBER);
	}

	if (PointLightComponent)
	{
		const float BasePointIntensity = bShouldBeLit ? PointActiveIntensity : PointInactiveIntensity;
		const float FinalPointIntensity = FMath::Max(0.f, BasePointIntensity * SafeIntensityScale);
		PointLightComponent->SetIntensity(FinalPointIntensity);
		PointLightComponent->SetVisibility(bUsePointLight && FinalPointIntensity > KINDA_SMALL_NUMBER);
	}

	ApplyFixtureEmissiveState(bShouldBeLit, SafeIntensityScale);
}

float ASubLightBase::SampleRangeSafe(const FVector2D& Range) const
{
	const float MinValue = FMath::Max(0.0f, FMath::Min(Range.X, Range.Y));
	const float MaxValue = FMath::Max(MinValue, FMath::Max(Range.X, Range.Y));
	return FaultyRandom.FRandRange(MinValue, MaxValue);
}
