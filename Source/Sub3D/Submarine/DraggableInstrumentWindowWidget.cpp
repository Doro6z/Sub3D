#include "DraggableInstrumentWindowWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "Input/Reply.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	FSlateLayoutTransform MakeLayoutTransform(const FVector2D& Position)
	{
		return FSlateLayoutTransform(FVector2f(static_cast<float>(Position.X), static_cast<float>(Position.Y)));
	}
}

UDraggableInstrumentWindowWidget::UDraggableInstrumentWindowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

int32 UDraggableInstrumentWindowWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	int32 NextLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X <= 4.f || Size.Y <= 4.f)
	{
		return NextLayer;
	}

	static const FSlateColorBrush WhiteBrush(FLinearColor::White);
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 11);
	const FSlateFontInfo GlyphFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10);

	const auto MakePaintGeometry = [&AllottedGeometry](const FVector2D& LocalPos, const FVector2D& LocalSize)
	{
		return AllottedGeometry.ToPaintGeometry(
			FVector2f(static_cast<float>(LocalSize.X), static_cast<float>(LocalSize.Y)),
			MakeLayoutTransform(LocalPos));
	};

	const FSlateRect TitleRect = GetTitleBarLocalRect(Size);
	const FSlateRect CollapseRect = GetCollapseButtonLocalRect(Size);
	const FSlateRect ContentRect = GetContentLocalRect(Size);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		NextLayer++,
		MakePaintGeometry(FVector2D::ZeroVector, Size),
		&WhiteBrush,
		ESlateDrawEffect::None,
		WindowBackgroundColor);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		NextLayer++,
		MakePaintGeometry(FVector2D(TitleRect.Left, TitleRect.Top), FVector2D(TitleRect.Right - TitleRect.Left, TitleRect.Bottom - TitleRect.Top)),
		&WhiteBrush,
		ESlateDrawEffect::None,
		TitleBarColor);

	TArray<FVector2f> FramePoints;
	FramePoints.Add(FVector2f(0.f, 0.f));
	FramePoints.Add(FVector2f(Size.X, 0.f));
	FramePoints.Add(FVector2f(Size.X, Size.Y));
	FramePoints.Add(FVector2f(0.f, Size.Y));
	FramePoints.Add(FVector2f(0.f, 0.f));
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		NextLayer++,
		AllottedGeometry.ToPaintGeometry(),
		FramePoints,
		ESlateDrawEffect::None,
		FrameColor,
		true,
		1.1f);

	const FString CollapseGlyph = bCollapsed ? TEXT("+") : TEXT("-");
	FSlateDrawElement::MakeText(
		OutDrawElements,
		NextLayer,
		MakePaintGeometry(FVector2D(CollapseRect.Left + 5.f, CollapseRect.Top + 3.f), FVector2D(CollapseRect.Right - CollapseRect.Left, CollapseRect.Bottom - CollapseRect.Top)),
		CollapseGlyph,
		GlyphFont,
		ESlateDrawEffect::None,
		AccentColor);

	FSlateDrawElement::MakeText(
		OutDrawElements,
		NextLayer++,
		MakePaintGeometry(FVector2D(8.f, 4.f), FVector2D(Size.X - CollapseButtonWidthPx - 18.f, TitleBarHeightPx - 6.f)),
		WindowTitle,
		TitleFont,
		ESlateDrawEffect::None,
		AccentColor);

	if (!bCollapsed && (ContentRect.Right - ContentRect.Left) > 8.f && (ContentRect.Bottom - ContentRect.Top) > 8.f)
	{
		NextLayer = PaintInstrumentContent(Args, AllottedGeometry, MyCullingRect, OutDrawElements, NextLayer, InWidgetStyle, bParentEnabled, ContentRect);
	}

	return NextLayer;
}

FReply UDraggableInstrumentWindowWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D Size = InGeometry.GetLocalSize();

	if (WantsContentInteraction() && !bCollapsed && IsPointInsideLocalRect(LocalPos, GetContentLocalRect(Size)))
	{
		if (FReply ContentReply = HandleContentMouseButtonDown(InGeometry, InMouseEvent, LocalPos); ContentReply.IsEventHandled())
		{
			return ContentReply;
		}
	}

	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (bCollapsible && IsPointInsideLocalRect(LocalPos, GetCollapseButtonLocalRect(Size)))
	{
		bPendingCollapseToggle = true;
		return MakeHandledReply(true);
	}

	if (bDraggable && IsPointInsideLocalRect(LocalPos, GetTitleBarLocalRect(Size)))
	{
		bDraggingWindow = true;
		DragStartScreenPosition = InMouseEvent.GetScreenSpacePosition();
		DragStartRenderTranslation = GetRenderTransform().Translation;
		return MakeHandledReply(true);
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UDraggableInstrumentWindowWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D Size = InGeometry.GetLocalSize();

	if (WantsContentInteraction())
	{
		if (FReply ContentReply = HandleContentMouseButtonUp(InGeometry, InMouseEvent, LocalPos); ContentReply.IsEventHandled())
		{
			return ContentReply;
		}
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (bPendingCollapseToggle)
		{
			bPendingCollapseToggle = false;
			if (bCollapsible && IsPointInsideLocalRect(LocalPos, GetCollapseButtonLocalRect(Size)))
			{
				SetWindowCollapsed(!bCollapsed);
			}
			return MakeHandledReply(false);
		}

		if (bDraggingWindow)
		{
			bDraggingWindow = false;
			return MakeHandledReply(false);
		}
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UDraggableInstrumentWindowWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

	if (WantsContentInteraction())
	{
		if (FReply ContentReply = HandleContentMouseMove(InGeometry, InMouseEvent, LocalPos); ContentReply.IsEventHandled())
		{
			return ContentReply;
		}
	}

	if (bDraggingWindow)
	{
		const FVector2D Delta = InMouseEvent.GetScreenSpacePosition() - DragStartScreenPosition;
		SetRenderTranslation(DragStartRenderTranslation + Delta);
		return MakeHandledReply(true);
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UDraggableInstrumentWindowWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	HandleContentMouseLeave(InMouseEvent);
	Super::NativeOnMouseLeave(InMouseEvent);
}

void UDraggableInstrumentWindowWidget::SetWindowCollapsed(bool bInCollapsed)
{
	if (bCollapsed == bInCollapsed)
	{
		return;
	}

	bCollapsed = bInCollapsed;
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

void UDraggableInstrumentWindowWidget::SetWindowTitle(const FText& InTitle)
{
	WindowTitle = InTitle;
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 UDraggableInstrumentWindowWidget::PaintInstrumentContent(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled,
	const FSlateRect& ContentRect) const
{
	return LayerId;
}

FReply UDraggableInstrumentWindowWidget::HandleContentMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, const FVector2D& LocalMousePosition)
{
	return FReply::Unhandled();
}

FReply UDraggableInstrumentWindowWidget::HandleContentMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, const FVector2D& LocalMousePosition)
{
	return FReply::Unhandled();
}

FReply UDraggableInstrumentWindowWidget::HandleContentMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, const FVector2D& LocalMousePosition)
{
	return FReply::Unhandled();
}

void UDraggableInstrumentWindowWidget::HandleContentMouseLeave(const FPointerEvent& InMouseEvent)
{
}

FSlateRect UDraggableInstrumentWindowWidget::GetTitleBarLocalRect(const FVector2D& LocalSize) const
{
	return FSlateRect(0.f, 0.f, LocalSize.X, FMath::Min(LocalSize.Y, TitleBarHeightPx));
}

FSlateRect UDraggableInstrumentWindowWidget::GetCollapseButtonLocalRect(const FVector2D& LocalSize) const
{
	const float Width = FMath::Clamp(CollapseButtonWidthPx, 8.f, LocalSize.X);
	return FSlateRect(LocalSize.X - Width, 0.f, LocalSize.X, FMath::Min(LocalSize.Y, TitleBarHeightPx));
}

FSlateRect UDraggableInstrumentWindowWidget::GetContentLocalRect(const FVector2D& LocalSize) const
{
	const float Left = ContentPaddingPx;
	const float Top = TitleBarHeightPx + ContentPaddingPx;
	const float Right = FMath::Max(Left, LocalSize.X - ContentPaddingPx);
	const float Bottom = FMath::Max(Top, LocalSize.Y - ContentPaddingPx);
	return FSlateRect(Left, Top, Right, Bottom);
}

bool UDraggableInstrumentWindowWidget::IsPointInsideLocalRect(const FVector2D& LocalPoint, const FSlateRect& LocalRect) const
{
	return LocalPoint.X >= LocalRect.Left && LocalPoint.X <= LocalRect.Right &&
		LocalPoint.Y >= LocalRect.Top && LocalPoint.Y <= LocalRect.Bottom;
}

FReply UDraggableInstrumentWindowWidget::MakeHandledReply(bool bCaptureMouse) const
{
	FReply Reply = FReply::Handled();
	if (bCaptureMouse)
	{
		if (const TSharedPtr<SWidget> CachedWidget = GetCachedWidget())
		{
			Reply = Reply.CaptureMouse(CachedWidget.ToSharedRef());
		}
	}
	else
	{
		Reply = Reply.ReleaseMouseCapture();
	}
	return Reply;
}
