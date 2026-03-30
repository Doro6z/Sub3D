#include "SubHelmWidget.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "SubPlayerController.h"
#include "SubSonarComponent.h"
#include "SubSonarDisplayWidget.h"
#include "SubSonarSystemComponent.h"
#include "SubmarineBase.h"

void USubHelmWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TryBindSonarDisplay();
}

void USubHelmWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TryBindSonarDisplay();
}

void USubHelmWidget::NativeDestruct()
{
	if (bOwnsAutoCreatedSonarDisplay && SonarDisplay)
	{
		SonarDisplay->RemoveFromParent();
		SonarDisplay = nullptr;
		bOwnsAutoCreatedSonarDisplay = false;
	}

	BoundSonar.Reset();
	BoundSonarSystem.Reset();
	Super::NativeDestruct();
}

void USubHelmWidget::RouteSonarPing()
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteSonarPing();
	}
}

void USubHelmWidget::RouteSonarPingHeldStart()
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerSetSonarPingHeld(true);
	}
}

void USubHelmWidget::RouteSonarPingHeldStop()
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerSetSonarPingHeld(false);
	}
}

bool USubHelmWidget::IsSonarDisplayBound() const
{
	return SonarDisplay && BoundSonar.IsValid() && BoundSonarSystem.IsValid();
}

void USubHelmWidget::RouteSetSonarMode(ESonarMode NewMode)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->SetSonarMode(NewMode);
	}
}

void USubHelmWidget::RouteSetSonarFocusBearing(float BearingDeg)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->SetSonarFocusBearing(BearingDeg);
	}
}

void USubHelmWidget::RouteSetSonarRangePreset(int32 PresetIndex)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->SetSonarRangePreset(PresetIndex);
	}
}

void USubHelmWidget::RouteMarkPriorityTrack(int32 TrackId, bool bPriority)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->MarkSonarPriorityTrack(TrackId, bPriority);
	}
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
