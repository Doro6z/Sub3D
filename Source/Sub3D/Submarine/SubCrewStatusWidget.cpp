#include "SubCrewStatusWidget.h"

#include "SubCrewCharacter.h"
#include "SubCrewMovementComponent.h"

#include "Fonts/SlateFontInfo.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"

namespace
{
const FSlateBrush* GetWhiteBoxBrush()
{
	return FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
}

void DrawHudBox(
	const FGeometry& Geometry,
	FSlateWindowElementList& OutDrawElements,
	int32 Layer,
	const FVector2D& Position,
	const FVector2D& Size,
	const FLinearColor& Color)
{
	if (const FSlateBrush* Brush = GetWhiteBoxBrush())
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			Layer,
			Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)),
			Brush,
			ESlateDrawEffect::None,
			Color);
	}
}

void DrawHudText(
	const FGeometry& Geometry,
	FSlateWindowElementList& OutDrawElements,
	int32 Layer,
	const FVector2D& Position,
	const FString& Text,
	const FSlateFontInfo& Font,
	const FLinearColor& Color)
{
	FSlateDrawElement::MakeText(
		OutDrawElements,
		Layer,
		Geometry.ToPaintGeometry(FVector2D(1.f, 1.f), FSlateLayoutTransform(Position)),
		Text,
		Font,
		ESlateDrawEffect::None,
		Color);
}

FString ResolveModeText(const ASubCrewCharacter* Crew, const USubCrewMovementComponent* CrewMovement)
{
	if (!Crew || !CrewMovement)
	{
		return TEXT("NO CREW");
	}

	if (CrewMovement->IsWaterSprinting())
	{
		return TEXT("WATER SPRINT");
	}

	if (Crew->IsCrewSwimming())
	{
		return TEXT("SWIMMING");
	}

	if (CrewMovement->bIsRunning)
	{
		return TEXT("RUNNING");
	}

	switch (CrewMovement->GetPostureState())
	{
	case ECrewPostureState::Prone:
		return TEXT("PRONE");
	case ECrewPostureState::Crouched:
		return TEXT("CROUCH");
	case ECrewPostureState::Standing:
	default:
		return TEXT("ON FOOT");
	}
}
}

void USubCrewStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 USubCrewStatusWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const ASubCrewCharacter* Crew = GetOwningCrewCharacter();
	const USubCrewMovementComponent* CrewMovement = Crew ? Crew->GetCrewMovement() : nullptr;
	if (!Crew || !CrewMovement)
	{
		return LayerId;
	}

	const FVector2D ViewSize = AllottedGeometry.GetLocalSize();
	const FVector2D PanelSize(FMath::Max(220.f, PanelSizePx.X), FMath::Max(96.f, PanelSizePx.Y));
	const FVector2D PanelPos(
		PanelLeftMarginPx,
		FMath::Max(10.f, ViewSize.Y - PanelBottomMarginPx - PanelSize.Y));

	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 12);
	const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9);
	const FSlateFontInfo PromptFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13);

	DrawHudBox(AllottedGeometry, OutDrawElements, LayerId++, PanelPos, PanelSize, PanelColor);
	DrawHudBox(AllottedGeometry, OutDrawElements, LayerId++, PanelPos, FVector2D(PanelSize.X, 1.f), PanelEdgeColor);

	const FString ModeText = ResolveModeText(Crew, CrewMovement);
	DrawHudText(AllottedGeometry, OutDrawElements, LayerId++, PanelPos + FVector2D(12.f, 9.f), ModeText, TitleFont, TextColor);

	const auto DrawBar = [&](
		const TCHAR* Label,
		float Value01,
		float RowY,
		const FLinearColor& FillColor,
		bool bMuted = false)
	{
		const float ClampedValue = FMath::Clamp(Value01, 0.f, 1.f);
		const FVector2D LabelPos = PanelPos + FVector2D(12.f, RowY - 3.f);
		const FVector2D BarPos = PanelPos + FVector2D(82.f, RowY);
		const FVector2D BarSize(FMath::Max(10.f, PanelSize.X - 100.f), BarHeightPx);
		const FLinearColor BackColor(0.04f, 0.05f, 0.06f, bMuted ? 0.30f : 0.74f);
		const FLinearColor EdgeColor(1.f, 1.f, 1.f, bMuted ? 0.06f : 0.12f);

		DrawHudText(AllottedGeometry, OutDrawElements, LayerId, LabelPos, FString(Label), LabelFont, bMuted ? MutedTextColor : TextColor);
		DrawHudBox(AllottedGeometry, OutDrawElements, LayerId, BarPos, BarSize, BackColor);
		DrawHudBox(AllottedGeometry, OutDrawElements, LayerId + 1, BarPos, FVector2D(BarSize.X * ClampedValue, BarSize.Y), FillColor);
		DrawHudBox(AllottedGeometry, OutDrawElements, LayerId + 2, BarPos, FVector2D(BarSize.X, 1.f), EdgeColor);
		LayerId += 3;
	};

	const float Health01 = GetHealthPercent();
	const float Immersion01 = FMath::Clamp(Crew->CurrentWaterImmersion01, 0.f, 1.f);
	const float WaterSprint01 = CrewMovement->GetWaterSprintEnergy01();
	const bool bWaterSprintRelevant = Crew->IsCrewSwimming()
		|| CrewMovement->IsWaterSprinting()
		|| Immersion01 >= FMath::Clamp(CrewMovement->WaterSprintRequiredImmersion01, 0.f, 1.f)
		|| WaterSprint01 < 0.999f;
	const float Posture01 = FMath::Clamp(CrewMovement->PostureAlpha, 0.f, 1.f);

	DrawBar(TEXT("HEALTH"), Health01, 34.f, HealthColor);
	DrawBar(TEXT("WATER"), Immersion01, 54.f, WaterColor);
	DrawBar(TEXT("BOOST"), WaterSprint01, 74.f, CrewMovement->IsWaterSprinting() ? SprintActiveColor : SprintReadyColor, !bWaterSprintRelevant);
	DrawBar(TEXT("POSTURE"), Posture01, 94.f, PostureColor);

	if (HasFocusedInteractable())
	{
		const FString Prompt = GetInteractionActionText().ToString();
		const FVector2D PromptSize(360.f, 34.f);
		const FVector2D PromptPos(
			ViewSize.X * 0.5f - PromptSize.X * 0.5f,
			FMath::Max(10.f, ViewSize.Y - InteractionPromptBottomMarginPx - PromptSize.Y));

		DrawHudBox(AllottedGeometry, OutDrawElements, LayerId++, PromptPos, PromptSize, FLinearColor(0.015f, 0.018f, 0.022f, 0.50f));
		DrawHudBox(AllottedGeometry, OutDrawElements, LayerId++, PromptPos, FVector2D(PromptSize.X, 1.f), PanelEdgeColor);
		DrawHudText(AllottedGeometry, OutDrawElements, LayerId++, PromptPos + FVector2D(16.f, 8.f), Prompt, PromptFont, TextColor);
	}

	return LayerId;
}
