#pragma once

#include "CoreMinimal.h"
#include "DraggableInstrumentWindowWidget.h"
#include "SubSonarV2Types.h"
#include "TacticalGraphViewWidget.generated.h"

class UHelmNavigationDisplayComponent;
class USubSonarSystemComponent;

UCLASS(Blueprintable)
class SUB3D_API UTacticalGraphViewWidget : public UDraggableInstrumentWindowWidget
{
	GENERATED_BODY()

public:
	UTacticalGraphViewWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "TacticalGraph")
	void InitForTacticalGraphSources(UHelmNavigationDisplayComponent* InDisplayComponent, USubSonarSystemComponent* InSonarSystem);

	UFUNCTION(BlueprintPure, Category = "TacticalGraph")
	bool IsTacticalGraphViewBound() const;

	UFUNCTION(BlueprintPure, Category = "TacticalGraph")
	UHelmNavigationDisplayComponent* GetBoundNavigationDisplay() const;

	UFUNCTION(BlueprintPure, Category = "TacticalGraph")
	USubSonarSystemComponent* GetBoundSonarSystem() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TacticalGraph")
	bool bDrawSonarTracks = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TacticalGraph", meta = (ClampMin = "3.0"))
	float BaseTrackMarkerSizePx = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TacticalGraph|Style")
	FLinearColor SafeColor = FLinearColor(0.24f, 0.82f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TacticalGraph|Style")
	FLinearColor PriorityColor = FLinearColor(1.f, 0.84f, 0.20f, 1.f);

protected:
	virtual int32 PaintInstrumentContent(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled,
		const FSlateRect& ContentRect) const override;

private:
	FLinearColor GetTrackColor(const FSonarTrack& Track) const;
	float GetTrackMarkerSize(const FSonarTrack& Track) const;

	TWeakObjectPtr<UHelmNavigationDisplayComponent> CachedDisplayComponent;
	TWeakObjectPtr<USubSonarSystemComponent> CachedSonarSystem;
};
