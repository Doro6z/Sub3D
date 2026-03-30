#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SubSonarTypes.h"
#include "SubSonarDisplayWidget.generated.h"

class USubSonarComponent;
class USubSonarSystemComponent;
class UTexture2D;

UCLASS(Blueprintable)
class SUB3D_API USubSonarDisplayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UFUNCTION(BlueprintCallable, Category = "Sonar|Display")
	void InitForSonar(USubSonarComponent* SonarComponent);

	UFUNCTION(BlueprintCallable, Category = "Sonar|Display")
	void InitForSonarSources(USubSonarComponent* SonarComponent, USubSonarSystemComponent* SonarSystemComponent);

	UFUNCTION(BlueprintCallable, Category = "Sonar|Display")
	void GetDisplayPoints(int32 CanvasWidth, int32 CanvasHeight, TArray<FVector2D>& OutPositions, TArray<float>& OutAlphas) const;

	UFUNCTION(BlueprintPure, Category = "Sonar|Display")
	float ComputePointAlpha(const FSonarHitPoint& Point, float CurrentTime) const;

	UFUNCTION(BlueprintPure, Category = "Sonar|Display")
	FVector2D ProjectToDisplayNormalized(FVector WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Sonar|Display")
	bool HasActivePoints() const;

	UFUNCTION(BlueprintPure, Category = "Sonar|Display")
	USubSonarSystemComponent* GetBoundSonarSystem() const;

	UFUNCTION(BlueprintPure, Category = "Sonar|Display")
	float GetSelfNoiseAggregate() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Sonar|Display")
	void BP_OnNewPingReceived();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	bool bEnableNativePaint = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	bool bDrawGrid = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	bool bDrawSweepPulse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	bool bDrawTracks = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	bool bDrawTopologyWireframe = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	float DotSizePx = 5.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	float DotGlowScale = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	float DotGlowAlpha = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	bool bDepthAffectsDotSize = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	float NearDotScale = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	float FarDotScale = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Projection")
	bool bUsePingFrameProjection = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	float GridStepPx = 32.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	float RingCount = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	float TopologyCellSizeCm = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	float CenterTextureScale = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.05f, 0.02f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	FLinearColor GridColor = FLinearColor(0.0f, 0.25f, 0.12f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	FLinearColor DotColor = FLinearColor(0.15f, 1.0f, 0.45f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	bool bEnableSpatialColorCoding = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	FLinearColor LeftFarColor = FLinearColor(0.10f, 0.45f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	FLinearColor RightNearColor = FLinearColor(1.0f, 0.58f, 0.08f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpatialColorBlend = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LateralColorWeight = 0.70f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ProximityColorWeight = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	FLinearColor SweepColor = FLinearColor(0.20f, 1.0f, 0.55f, 0.75f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	FLinearColor CenterTint = FLinearColor(0.18f, 1.0f, 0.5f, 0.8f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	float NoiseOverlayAlpha = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Style")
	float SmudgeOverlayAlpha = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Textures")
	TObjectPtr<UTexture2D> NoiseOverlayTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Textures")
	TObjectPtr<UTexture2D> SmudgeOverlayTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Textures")
	TObjectPtr<UTexture2D> CenterSubTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Textures")
	TObjectPtr<UTexture2D> CenterReticleTexture = nullptr;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Sonar|Display")
	TWeakObjectPtr<USubSonarComponent> CachedSonar;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar|Display")
	TWeakObjectPtr<USubSonarSystemComponent> CachedSonarSystem;

private:
	FVector2D ProjectPointNormalized(const FSonarHitPoint& Point) const;
	FVector GetSonarReferenceLocation() const;
	float GetEffectiveDisplayRangeCm() const;
	FLinearColor ResolveSpatialColor(const FVector2D& Normalized, const FLinearColor& BaseColor, float Alpha) const;
	FLinearColor ResolveTrackColor(uint8 StateValue, bool bPriority) const;

	float LastKnownPingTime = -1000.f;
};
