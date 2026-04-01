#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HelmNavigationDisplayComponent.h"
#include "HelmNavigationDisplayWidget.generated.h"

class UHelmNavigationDisplayComponent;

UCLASS(Blueprintable)
class SUB3D_API UHelmNavigationDisplayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UFUNCTION(BlueprintCallable, Category="HelmNav")
	void InitForNavigationDisplay(UHelmNavigationDisplayComponent* InDisplayComponent);

	UFUNCTION(BlueprintPure, Category="HelmNav")
	bool IsNavigationDisplayBound() const;

	UFUNCTION(BlueprintPure, Category="HelmNav")
	UHelmNavigationDisplayComponent* GetBoundNavigationDisplay() const;

	UFUNCTION(BlueprintPure, Category="HelmNav")
	FHelmCrossSectionViewData GetCrossSectionViewData() const;

	UFUNCTION(BlueprintPure, Category="HelmNav")
	FHelmForwardAnticipationViewData GetForwardAnticipationViewData() const;

	UFUNCTION(BlueprintPure, Category="HelmNav")
	FHelmTacticalGraphViewData GetTacticalGraphViewData() const;

	UFUNCTION(BlueprintPure, Category="HelmNav")
	FTunnelNavStoppingDistanceWarning GetStoppingDistanceWarning() const;

	UFUNCTION(BlueprintPure, Category="HelmNav")
	FTunnelNavCommitmentWarning GetCommitmentWarning() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HelmNav|Style")
	bool bEnableNativePaint = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HelmNav|Style")
	float PanelPaddingPx = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HelmNav|Style")
	FLinearColor BackgroundColor = FLinearColor(0.02f, 0.03f, 0.04f, 0.94f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HelmNav|Style")
	FLinearColor PanelColor = FLinearColor(0.04f, 0.06f, 0.08f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HelmNav|Style")
	FLinearColor FrameColor = FLinearColor(0.17f, 0.45f, 0.50f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HelmNav|Style")
	FLinearColor AccentColor = FLinearColor(0.30f, 0.95f, 0.88f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HelmNav|Style")
	FLinearColor WarningColor = FLinearColor(1.f, 0.38f, 0.22f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HelmNav|Style")
	FLinearColor VelocityColor = FLinearColor(1.f, 0.84f, 0.20f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HelmNav|Style")
	FLinearColor SafeColor = FLinearColor(0.24f, 0.82f, 1.f, 1.f);

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UHelmNavigationDisplayComponent> CachedDisplayComponent;
};
