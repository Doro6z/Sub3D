#include "SubTurretWidget.h"

#include "SubCrewCharacter.h"
#include "SubHullComponent.h"
#include "SubPlayerController.h"
#include "SubmarineBase.h"
#include "SubmarineSystemsComponent.h"
#include "TurretActor.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"

namespace
{
const FSlateBrush* GetWhiteBoxBrush()
{
	// GenericWhiteBox is a white 9-slice brush registered in FCoreStyle and
	// available in both editor and runtime builds. Used as the base fill for
	// solid-color HUD rectangles.
	return FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
}
}

void USubTurretWidget::SetTurretAim(const FRotator& NewAim)
{
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteTurretAim(NewAim);
	}
}

void USubTurretWidget::SetTurretFireHeld(bool bHeld)
{
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteTurretFire(bHeld);
	}
}

FRotator USubTurretWidget::GetCurrentTurretAim() const
{
	const_cast<USubTurretWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->ExteriorTurret)
	{
		return OwnerCrew->CurrentSubmarine->ExteriorTurret->CurrentAim;
	}
	return FRotator::ZeroRotator;
}

bool USubTurretWidget::IsTurretOnline() const
{
	const_cast<USubTurretWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->ExteriorTurret)
	{
		return OwnerCrew->CurrentSubmarine->ExteriorTurret->bOnline;
	}
	return false;
}

float USubTurretWidget::GetAmmoPercent() const
{
	const int32 Max = GetMaxAmmo();
	if (Max <= 0)
	{
		return 0.f;
	}
	return FMath::Clamp(static_cast<float>(GetCurrentAmmo()) / static_cast<float>(Max), 0.f, 1.f);
}

int32 USubTurretWidget::GetCurrentAmmo() const
{
	const_cast<USubTurretWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->ExteriorTurret)
	{
		return OwnerCrew->CurrentSubmarine->ExteriorTurret->CurrentAmmo;
	}
	return 0;
}

int32 USubTurretWidget::GetMaxAmmo() const
{
	const_cast<USubTurretWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->ExteriorTurret)
	{
		return OwnerCrew->CurrentSubmarine->ExteriorTurret->MaxAmmo;
	}
	return 0;
}

void USubTurretWidget::NativeConstruct()
{
	Super::NativeConstruct();

	FireFlashAlpha = 0.f;
	ShakeOffsetPx = FVector2D::ZeroVector;
	LastObservedFireTime = 0.f;

	if (USubHullComponent* Hull = ResolveHullForBinding())
	{
		Hull->OnHullDamageUpdated.AddDynamic(this, &USubTurretWidget::HandleHullDamageUpdated);
		BoundHull = Hull;
	}
}

void USubTurretWidget::NativeDestruct()
{
	if (BoundHull.IsValid())
	{
		BoundHull->OnHullDamageUpdated.RemoveDynamic(this, &USubTurretWidget::HandleHullDamageUpdated);
	}
	BoundHull.Reset();

	Super::NativeDestruct();
}

void USubTurretWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	PollFireEvent();

	if (FireFlashAlpha > 0.f)
	{
		FireFlashAlpha = FMath::Max(0.f, FireFlashAlpha - InDeltaTime / FMath::Max(FireFlashDecaySeconds, KINDA_SMALL_NUMBER));
	}

	if (!ShakeOffsetPx.IsNearlyZero())
	{
		const float Decay = FMath::Exp(-ShakeDecayPerSecond * InDeltaTime);
		ShakeOffsetPx *= Decay;
		if (ShakeOffsetPx.SizeSquared() < 0.25f)
		{
			ShakeOffsetPx = FVector2D::ZeroVector;
		}
	}
}

int32 USubTurretWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FSlateBrush* WhiteBrush = GetWhiteBoxBrush();
	if (!WhiteBrush)
	{
		return LayerId;
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FVector2D Center = Size * 0.5f + ShakeOffsetPx;
	const bool bOnline = IsTurretOnline();
	const FLinearColor BaseColor = bOnline ? ReticleColor : FLinearColor(0.5f, 0.5f, 0.5f, 0.7f);

	// ── Reticle: outer ring (approximated with line segments) ────────────
	{
		constexpr int32 Segments = 32;
		TArray<FVector2D> RingPoints;
		RingPoints.Reserve(Segments + 1);
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float Theta = (static_cast<float>(i) / static_cast<float>(Segments)) * 2.f * PI;
			RingPoints.Add(Center + FVector2D(FMath::Cos(Theta), FMath::Sin(Theta)) * ReticleRingRadiusPx);
		}
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(),
			RingPoints,
			ESlateDrawEffect::None,
			BaseColor,
			true,
			1.5f);
	}

	// ── Reticle: crosshair ───────────────────────────────────────────────
	{
		TArray<FVector2D> HLine = { Center + FVector2D(-ReticleSizePx, 0.f), Center + FVector2D(ReticleSizePx, 0.f) };
		TArray<FVector2D> VLine = { Center + FVector2D(0.f, -ReticleSizePx), Center + FVector2D(0.f, ReticleSizePx) };
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId++, AllottedGeometry.ToPaintGeometry(), HLine, ESlateDrawEffect::None, BaseColor, true, 2.f);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId++, AllottedGeometry.ToPaintGeometry(), VLine, ESlateDrawEffect::None, BaseColor, true, 2.f);
	}

	// ── Reticle: center dot ──────────────────────────────────────────────
	{
		const FVector2D DotSize(4.f, 4.f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(DotSize, FSlateLayoutTransform(Center - DotSize * 0.5f)),
			WhiteBrush,
			ESlateDrawEffect::None,
			BaseColor);
	}

	// ── Aim offset indicator: draws a small marker offset from center
	// proportional to (TargetAim - CurrentAim). Gives visual feedback on
	// turret travel. Scaled so a 10° delta maps roughly to the ring radius.
	if (bOnline && OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->ExteriorTurret)
	{
		const ATurretActor* Turret = OwnerCrew->CurrentSubmarine->ExteriorTurret;
		const FRotator Delta = (Turret->TargetAim - Turret->CurrentAim).GetNormalized();
		const float DegreesPerRadius = 10.f;
		const FVector2D OffsetPx(
			FMath::Clamp(Delta.Yaw / DegreesPerRadius, -1.f, 1.f) * ReticleRingRadiusPx,
			FMath::Clamp(-Delta.Pitch / DegreesPerRadius, -1.f, 1.f) * ReticleRingRadiusPx);

		const FVector2D MarkerSize(6.f, 6.f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(MarkerSize, FSlateLayoutTransform(Center + OffsetPx - MarkerSize * 0.5f)),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(1.f, 1.f, 1.f, 0.85f));
	}

	// ── Ammo gauge (bottom-center) ───────────────────────────────────────
	{
		const FVector2D GaugePos(
			Size.X * 0.5f - AmmoGaugeSizePx.X * 0.5f + ShakeOffsetPx.X,
			Size.Y - AmmoGaugeBottomMarginPx + ShakeOffsetPx.Y);

		// Background
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(AmmoGaugeSizePx, FSlateLayoutTransform(GaugePos)),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.05f, 0.05f, 0.05f, 0.75f));

		// Fill
		const float Pct = GetAmmoPercent();
		if (Pct > 0.f)
		{
			const FVector2D FillSize(AmmoGaugeSizePx.X * Pct, AmmoGaugeSizePx.Y);
			FLinearColor FillColor = BaseColor;
			if (Pct < 0.25f)
			{
				FillColor = FLinearColor(1.f, 0.3f, 0.2f, 1.f);
			}
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId++,
				AllottedGeometry.ToPaintGeometry(FillSize, FSlateLayoutTransform(GaugePos)),
				WhiteBrush,
				ESlateDrawEffect::None,
				FillColor);
		}

		// Border (4 thin rects)
		const FLinearColor BorderColor = FLinearColor(0.f, 0.f, 0.f, 0.9f);
		const float BorderPx = 1.f;
		auto DrawBorderEdge = [&](const FVector2D& EdgePos, const FVector2D& EdgeSize)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(EdgeSize, FSlateLayoutTransform(EdgePos)),
				WhiteBrush,
				ESlateDrawEffect::None,
				BorderColor);
		};
		DrawBorderEdge(GaugePos, FVector2D(AmmoGaugeSizePx.X, BorderPx));
		DrawBorderEdge(GaugePos + FVector2D(0.f, AmmoGaugeSizePx.Y - BorderPx), FVector2D(AmmoGaugeSizePx.X, BorderPx));
		DrawBorderEdge(GaugePos, FVector2D(BorderPx, AmmoGaugeSizePx.Y));
		DrawBorderEdge(GaugePos + FVector2D(AmmoGaugeSizePx.X - BorderPx, 0.f), FVector2D(BorderPx, AmmoGaugeSizePx.Y));
		++LayerId;
	}

	// ── Fire flash: full-screen additive tint when a shot was just fired ─
	if (FireFlashAlpha > 0.f)
	{
		FLinearColor FlashTint = FireFlashColor;
		FlashTint.A *= FireFlashAlpha;
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2D::ZeroVector)),
			WhiteBrush,
			ESlateDrawEffect::None,
			FlashTint);
	}

	// ── Offline overlay ───────────────────────────────────────────────────
	if (!bOnline)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2D::ZeroVector)),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.f, 0.f, 0.f, 0.55f));
	}

	return LayerId;
}

void USubTurretWidget::HandleHullDamageUpdated()
{
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	ShakeOffsetPx += FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * ShakeImpulsePx;
}

USubHullComponent* USubTurretWidget::ResolveHullForBinding()
{
	ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine)
	{
		return OwnerCrew->CurrentSubmarine->SubHull;
	}
	return nullptr;
}

void USubTurretWidget::PollFireEvent()
{
	const_cast<USubTurretWidget*>(this)->ResolveRuntimeRefs();
	if (!OwnerCrew.IsValid() || !OwnerCrew->CurrentSubmarine || !OwnerCrew->CurrentSubmarine->ExteriorTurret)
	{
		return;
	}

	const float FireTime = OwnerCrew->CurrentSubmarine->ExteriorTurret->LastFireServerTime;
	if (FireTime > LastObservedFireTime + KINDA_SMALL_NUMBER)
	{
		LastObservedFireTime = FireTime;
		FireFlashAlpha = 1.f;
	}
}
