#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HelmStatusStripWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UVerticalBox;
class UTextBlock;
class USubHelmWidget;

UCLASS(Blueprintable)
class SUB3D_API UHelmStatusStripWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Helm")
	void InitForHelmShell(USubHelmWidget* InHelmShell);

	UFUNCTION(BlueprintPure, Category = "Helm")
	bool IsBoundToHelmShell() const;

private:
	void BuildWidgetTree();
	void RefreshFromHelmData();
	void TryResolveHelmShellFromOuter();

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<USubHelmWidget> CachedHelmShell;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> RootRow;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MotionHeaderText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MotionValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SonarHeaderText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SonarValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ControlHeaderText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ControlValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WarningHeaderText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WarningValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WarningDetailText;
};
