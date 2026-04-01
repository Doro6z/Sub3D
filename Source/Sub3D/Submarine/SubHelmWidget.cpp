#include "SubHelmWidget.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "HelmNavigationDisplayComponent.h"
#include "HelmNavigationDisplayWidget.h"
#include "ReconstructionViewWidget.h"
#include "SubPlayerController.h"
#include "SubSonarComponent.h"
#include "SubSonarDisplayWidget.h"
#include "SubSonarSystemComponent.h"
#include "SubmarineBase.h"
#include "TacticalGraphViewWidget.h"

void USubHelmWidget::NativeConstruct()
{
	Super::NativeConstruct();
	DiscoverWidgetReferencesFromTree();
	TryBindSonarDisplay();
	TryBindHelmNavigationDisplay();
	TryBindReconstructionView();
	TryBindTacticalGraphView();
}

void USubHelmWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bAllowWidgetTreeFallbackDiscovery &&
		(!SonarDisplay || !ReconstructionView || !TacticalGraphView || !HelmNavigationDisplay))
	{
		DiscoverWidgetReferencesFromTree();
	}
	TryBindSonarDisplay();
	TryBindHelmNavigationDisplay();
	TryBindReconstructionView();
	TryBindTacticalGraphView();
}

void USubHelmWidget::NativeDestruct()
{
	if (bOwnsAutoCreatedSonarDisplay && SonarDisplay)
	{
		SonarDisplay->RemoveFromParent();
		SonarDisplay = nullptr;
		bOwnsAutoCreatedSonarDisplay = false;
	}

	if (bOwnsAutoCreatedHelmNavigationDisplay && HelmNavigationDisplay)
	{
		HelmNavigationDisplay->RemoveFromParent();
		HelmNavigationDisplay = nullptr;
		bOwnsAutoCreatedHelmNavigationDisplay = false;
	}

	BoundSonar.Reset();
	BoundSonarSystem.Reset();
	BoundHelmNavigationDisplayComponent.Reset();
	Super::NativeDestruct();
}

void USubHelmWidget::DiscoverWidgetReferencesFromTree()
{
	if (!WidgetTree)
	{
		return;
	}

	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		if (!SonarDisplay)
		{
			SonarDisplay = Cast<USubSonarDisplayWidget>(Widget);
		}
		if (!HelmNavigationDisplay)
		{
			HelmNavigationDisplay = Cast<UHelmNavigationDisplayWidget>(Widget);
		}
		if (!ReconstructionView)
		{
			ReconstructionView = Cast<UReconstructionViewWidget>(Widget);
		}
		if (!TacticalGraphView)
		{
			TacticalGraphView = Cast<UTacticalGraphViewWidget>(Widget);
		}
	});
}

void USubHelmWidget::RouteSonarPing()
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteSonarPing();
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSonarPing failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSonarPingHeldStart()
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerSetSonarPingHeld(true);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSonarPingHeldStart failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSonarPingHeldStop()
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerSetSonarPingHeld(false);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSonarPingHeldStop failed: OwnerController unresolved."), *GetName());
}

bool USubHelmWidget::IsSonarDisplayBound() const
{
	return SonarDisplay && BoundSonar.IsValid() && BoundSonarSystem.IsValid();
}

bool USubHelmWidget::IsHelmNavigationDisplayBound() const
{
	return HelmNavigationDisplay && BoundHelmNavigationDisplayComponent.IsValid();
}

bool USubHelmWidget::IsReconstructionViewBound() const
{
	return ReconstructionView && BoundHelmNavigationDisplayComponent.IsValid();
}

bool USubHelmWidget::IsTacticalGraphViewBound() const
{
	return TacticalGraphView && BoundHelmNavigationDisplayComponent.IsValid() && BoundSonarSystem.IsValid();
}

void USubHelmWidget::RouteSetSonarMode(ESonarMode NewMode)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->SetSonarMode(NewMode);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetSonarMode failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetSonarFocusBearing(float BearingDeg)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->SetSonarFocusBearing(BearingDeg);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetSonarFocusBearing failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetSonarRangePreset(int32 PresetIndex)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->SetSonarRangePreset(PresetIndex);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetSonarRangePreset failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteIncreaseSonarRangePreset()
{
	RouteAdjustSonarRangePreset(+1);
}

void USubHelmWidget::RouteDecreaseSonarRangePreset()
{
	RouteAdjustSonarRangePreset(-1);
}

void USubHelmWidget::RouteAdjustSonarRangePreset(int32 Delta)
{
	ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] RouteAdjustSonarRangePreset failed: OwnerController unresolved."), *GetName());
		return;
	}

	ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	USubSonarSystemComponent* SonarSystem = Submarine ? Submarine->SonarSystem : nullptr;
	if (!SonarSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] RouteAdjustSonarRangePreset failed: SonarSystem unresolved."), *GetName());
		return;
	}

	const int32 CurrentIndex = SonarSystem->GetRangePresetIndex();
	const int32 MaxIndex = FMath::Max(0, SonarSystem->GetRangePresetCount() - 1);
	const int32 NewIndex = FMath::Clamp(CurrentIndex + Delta, 0, MaxIndex);
	OwnerController->SetSonarRangePreset(NewIndex);
}

void USubHelmWidget::RouteSetSonarRangeNormalized(float Normalized01)
{
	ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetSonarRangeNormalized failed: OwnerController unresolved."), *GetName());
		return;
	}

	ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	USubSonarSystemComponent* SonarSystem = Submarine ? Submarine->SonarSystem : nullptr;
	if (!SonarSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetSonarRangeNormalized failed: SonarSystem unresolved."), *GetName());
		return;
	}

	const int32 NewIndex = SonarSystem->ResolveRangePresetIndexFromNormalized(Normalized01);
	OwnerController->SetSonarRangePreset(NewIndex);
}

int32 USubHelmWidget::GetCurrentSonarRangePresetIndex() const
{
	const_cast<USubHelmWidget*>(this)->ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		return 0;
	}

	const ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	const USubSonarSystemComponent* SonarSystem = Submarine ? Submarine->SonarSystem : nullptr;
	return SonarSystem ? SonarSystem->GetRangePresetIndex() : 0;
}

int32 USubHelmWidget::GetCurrentSonarRangePresetCount() const
{
	const_cast<USubHelmWidget*>(this)->ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		return 1;
	}

	const ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	const USubSonarSystemComponent* SonarSystem = Submarine ? Submarine->SonarSystem : nullptr;
	return SonarSystem ? SonarSystem->GetRangePresetCount() : 1;
}

float USubHelmWidget::GetCurrentSonarDisplayRangeCm() const
{
	const_cast<USubHelmWidget*>(this)->ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		return 0.f;
	}

	const ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	const USubSonarSystemComponent* SonarSystem = Submarine ? Submarine->SonarSystem : nullptr;
	return SonarSystem ? SonarSystem->GetDisplayRangeCm() : 0.f;
}

void USubHelmWidget::RouteMarkPriorityTrack(int32 TrackId, bool bPriority)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->MarkSonarPriorityTrack(TrackId, bPriority);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteMarkPriorityTrack failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::TryBindSonarDisplay()
{
	if (!SonarDisplay)
	{
		if (WidgetTree)
		{
			WidgetTree->ForEachWidget([this](UWidget* Widget)
			{
				if (!SonarDisplay)
				{
					SonarDisplay = Cast<USubSonarDisplayWidget>(Widget);
				}
			});
		}

		if (bAutoCreateSonarDisplayIfMissing)
		{
			APlayerController* PC = GetOwningPlayer();
			if (PC)
			{
				UClass* DisplayClass = SonarDisplayClass ? SonarDisplayClass.Get() : USubSonarDisplayWidget::StaticClass();
				USubSonarDisplayWidget* CreatedDisplay = CreateWidget<USubSonarDisplayWidget>(PC, DisplayClass);
				if (CreatedDisplay)
				{
					CreatedDisplay->AddToViewport(AutoCreatedSonarDisplayZOrder);
					SonarDisplay = CreatedDisplay;
					bOwnsAutoCreatedSonarDisplay = true;
				}
			}
		}

		if (!SonarDisplay)
		{
			if (!bLoggedMissingSonarDisplay)
			{
				UE_LOG(LogTemp, Warning, TEXT("[%s] TryBindSonarDisplay: no USubSonarDisplayWidget found in widget tree and auto-create disabled."), *GetName());
				bLoggedMissingSonarDisplay = true;
			}
			return;
		}
	}

	ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		return;
	}

	ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	if (!Submarine || !Submarine->Sonar || !Submarine->SonarSystem)
	{
		return;
	}

	if (BoundSonar.Get() == Submarine->Sonar && BoundSonarSystem.Get() == Submarine->SonarSystem)
	{
		return;
	}

	SonarDisplay->InitForSonarSources(Submarine->Sonar, Submarine->SonarSystem);
	BoundSonar = Submarine->Sonar;
	BoundSonarSystem = Submarine->SonarSystem;
}

void USubHelmWidget::TryBindHelmNavigationDisplay()
{
	if (!HelmNavigationDisplay)
	{
		if (WidgetTree)
		{
			WidgetTree->ForEachWidget([this](UWidget* Widget)
			{
				if (!HelmNavigationDisplay)
				{
					HelmNavigationDisplay = Cast<UHelmNavigationDisplayWidget>(Widget);
				}
			});
		}

		if (bAutoCreateHelmNavigationDisplayIfMissing)
		{
			APlayerController* PC = GetOwningPlayer();
			if (PC)
			{
				UClass* DisplayClass = HelmNavigationDisplayClass ? HelmNavigationDisplayClass.Get() : UHelmNavigationDisplayWidget::StaticClass();
				UHelmNavigationDisplayWidget* CreatedDisplay = CreateWidget<UHelmNavigationDisplayWidget>(PC, DisplayClass);
				if (CreatedDisplay)
				{
					CreatedDisplay->AddToViewport(AutoCreatedHelmNavigationDisplayZOrder);
					HelmNavigationDisplay = CreatedDisplay;
					bOwnsAutoCreatedHelmNavigationDisplay = true;
				}
			}
		}

		if (!HelmNavigationDisplay)
		{
			if (!bLoggedMissingHelmNavigationDisplay)
			{
				UE_LOG(LogTemp, Warning, TEXT("[%s] TryBindHelmNavigationDisplay: no UHelmNavigationDisplayWidget found in widget tree and auto-create disabled."), *GetName());
				bLoggedMissingHelmNavigationDisplay = true;
			}
			return;
		}
	}

	ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		return;
	}

	ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	if (!Submarine || !Submarine->HelmNavigationDisplay)
	{
		return;
	}

	if (BoundHelmNavigationDisplayComponent.Get() == Submarine->HelmNavigationDisplay &&
		HelmNavigationDisplay->IsNavigationDisplayBound())
	{
		return;
	}

	HelmNavigationDisplay->InitForNavigationDisplay(Submarine->HelmNavigationDisplay);
	BoundHelmNavigationDisplayComponent = Submarine->HelmNavigationDisplay;
}

void USubHelmWidget::TryBindReconstructionView()
{
	if (!ReconstructionView)
	{
		if (WidgetTree)
		{
			WidgetTree->ForEachWidget([this](UWidget* Widget)
			{
				if (!ReconstructionView)
				{
					ReconstructionView = Cast<UReconstructionViewWidget>(Widget);
				}
			});
		}

		if (!ReconstructionView)
		{
			if (!bLoggedMissingReconstructionView)
			{
				UE_LOG(LogTemp, Warning, TEXT("[%s] TryBindReconstructionView: no UReconstructionViewWidget found in widget tree."), *GetName());
				bLoggedMissingReconstructionView = true;
			}
			return;
		}
	}

	ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		return;
	}

	ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	if (!Submarine || !Submarine->HelmNavigationDisplay)
	{
		return;
	}

	if (BoundHelmNavigationDisplayComponent.Get() == Submarine->HelmNavigationDisplay && ReconstructionView->IsReconstructionViewBound())
	{
		return;
	}

	ReconstructionView->InitForReconstructionView(Submarine->HelmNavigationDisplay);
	BoundHelmNavigationDisplayComponent = Submarine->HelmNavigationDisplay;
}

void USubHelmWidget::TryBindTacticalGraphView()
{
	if (!TacticalGraphView)
	{
		if (WidgetTree)
		{
			WidgetTree->ForEachWidget([this](UWidget* Widget)
			{
				if (!TacticalGraphView)
				{
					TacticalGraphView = Cast<UTacticalGraphViewWidget>(Widget);
				}
			});
		}

		if (!TacticalGraphView)
		{
			if (!bLoggedMissingTacticalGraphView)
			{
				UE_LOG(LogTemp, Warning, TEXT("[%s] TryBindTacticalGraphView: no UTacticalGraphViewWidget found in widget tree."), *GetName());
				bLoggedMissingTacticalGraphView = true;
			}
			return;
		}
	}

	ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		return;
	}

	ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	if (!Submarine || !Submarine->HelmNavigationDisplay || !Submarine->SonarSystem)
	{
		return;
	}

	if (BoundHelmNavigationDisplayComponent.Get() == Submarine->HelmNavigationDisplay &&
		BoundSonarSystem.Get() == Submarine->SonarSystem &&
		TacticalGraphView->IsTacticalGraphViewBound())
	{
		return;
	}

	TacticalGraphView->InitForTacticalGraphSources(Submarine->HelmNavigationDisplay, Submarine->SonarSystem);
	BoundHelmNavigationDisplayComponent = Submarine->HelmNavigationDisplay;
	BoundSonarSystem = Submarine->SonarSystem;
}
