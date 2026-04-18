#include "CrewAnimDebugWidget.h"
#include "SubCrewAnimInstance.h"
#include "SubCrewCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"

UCrewAnimDebugWidget::UCrewAnimDebugWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

USubCrewAnimInstance* UCrewAnimDebugWidget::GetAnimInstance() const
{
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Pawn) return nullptr;
	USkeletalMeshComponent* Mesh = Pawn->FindComponentByClass<USkeletalMeshComponent>();
	if (!Mesh) return nullptr;
	return Cast<USubCrewAnimInstance>(Mesh->GetAnimInstance());
}

void UCrewAnimDebugWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildWidgetTree();
}

// ═══════════════════════════════════════════════════════════
// WIDGET TREE
// ═══════════════════════════════════════════════════════════

void UCrewAnimDebugWidget::AddSectionHeader(UVerticalBox* Parent, const FText& HeaderText)
{
	UTextBlock* Header = NewObject<UTextBlock>(this);
	Header->SetText(HeaderText);
	Header->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.95f, 0.88f)));
	auto* Font = const_cast<FSlateFontInfo*>(&Header->GetFont());
	Font->Size = 11;
	auto* HeaderSlot = Parent->AddChildToVerticalBox(Header);
	HeaderSlot->SetPadding(FMargin(0, 6, 0, 2));
}

UCrewAnimDebugWidget::FSliderRow UCrewAnimDebugWidget::AddSliderRow(
	UVerticalBox* Parent, const FText& LabelText, float Min, float Max, float Current)
{
	FSliderRow Row;
	Row.MinVal = Min;
	Row.MaxVal = Max;

	UHorizontalBox* HBox = NewObject<UHorizontalBox>(this);

	// Label
	Row.Label = NewObject<UTextBlock>(this);
	Row.Label->SetText(LabelText);
	Row.Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
	auto* LabelSlot = HBox->AddChildToHorizontalBox(Row.Label);
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LabelSlot->SetHorizontalAlignment(HAlign_Left);

	// Slider
	Row.Slider = NewObject<USlider>(this);
	Row.Slider->SetMinValue(0.f);
	Row.Slider->SetMaxValue(1.f);
	const float Normalized = (Max > Min) ? (Current - Min) / (Max - Min) : 0.f;
	Row.Slider->SetValue(FMath::Clamp(Normalized, 0.f, 1.f));
	Row.Slider->SetSliderBarColor(FLinearColor(0.15f, 0.4f, 0.45f));
	Row.Slider->SetSliderHandleColor(FLinearColor(0.3f, 0.95f, 0.88f));
	auto* SliderSlot = HBox->AddChildToHorizontalBox(Row.Slider);
	SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Value text
	Row.ValueText = NewObject<UTextBlock>(this);
	Row.ValueText->SetText(FText::AsNumber(Current));
	Row.ValueText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f)));
	auto* ValSlot = HBox->AddChildToHorizontalBox(Row.ValueText);
	ValSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	ValSlot->SetPadding(FMargin(4, 0, 0, 0));

	Parent->AddChildToVerticalBox(HBox);
	return Row;
}

void UCrewAnimDebugWidget::AddCheckboxRow(UVerticalBox* Parent, const FText& LabelText, bool bCurrent, int32 ParamIndex)
{
	UHorizontalBox* HBox = NewObject<UHorizontalBox>(this);

	UTextBlock* Label = NewObject<UTextBlock>(this);
	Label->SetText(LabelText);
	Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
	auto* LSlot = HBox->AddChildToHorizontalBox(Label);
	LSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UCheckBox* CB = NewObject<UCheckBox>(this);
	CB->SetIsChecked(bCurrent);

	if (ParamIndex == 0)
		CB->OnCheckStateChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnCheckChanged_NegateLeg);
	else if (ParamIndex == 1)
		CB->OnCheckStateChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnCheckChanged_NegateArm);
	else if (ParamIndex == 2)
		CB->OnCheckStateChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnCheckChanged_NegateSpine);

	HBox->AddChildToHorizontalBox(CB);
	Parent->AddChildToVerticalBox(HBox);
}

void UCrewAnimDebugWidget::BuildWidgetTree()
{
	USubCrewAnimInstance* AI = GetAnimInstance();

	// Background border
	UBorder* BG = NewObject<UBorder>(this);
	BG->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.90f));
	BG->SetPadding(FMargin(8.f));

	UScrollBox* Scroll = NewObject<UScrollBox>(this);
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);

	RootBox = NewObject<UVerticalBox>(this);
	Scroll->AddChild(RootBox);
	BG->AddChild(Scroll);

	if (WidgetTree)
	{
		WidgetTree->RootWidget = BG;
	}

	// Title
	UTextBlock* Title = NewObject<UTextBlock>(this);
	Title->SetText(FText::FromString(TEXT("=== CREW ANIM TUNER ===")));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.95f, 0.88f)));
	RootBox->AddChildToVerticalBox(Title);

	float Def_LegSwing = AI ? AI->WalkLegSwingDeg : 30.f;
	float Def_ArmSwing = AI ? AI->WalkArmSwingDeg : 20.f;
	float Def_Bob = AI ? AI->WalkPelvisBobCm : 2.f;
	float Def_Rate = AI ? AI->WalkCycleRate : 0.04f;
	float Def_Calf = AI ? AI->WalkCalfBendMultiplier : 1.2f;
	float Def_ArmR = AI ? AI->ArmRestR.Roll : -85.f;
	float Def_ForeR = AI ? AI->ForearmRestR.Roll : -10.f;
	float Def_BAmp = AI ? AI->BreathingAmplitudeDeg : 1.5f;
	float Def_BRate = AI ? AI->BreathingRate : 0.4f;
	float Def_PBend = AI ? AI->MaxPostureBendDeg : 80.f;
	float Def_SLean = AI ? AI->SubLeanMultiplier : 0.3f;
	float Def_SStum = AI ? AI->SubStumbleMultiplier : 0.01f;
	int32 Def_LA = AI ? AI->LegSwingAxis : 1;
	int32 Def_AA = AI ? AI->ArmSwingAxis : 1;
	int32 Def_ARA = AI ? AI->ElbowBendAxis : 2;
	int32 Def_SBA = AI ? AI->SpineBendAxis : 1;
	int32 Def_STA = AI ? AI->SpineTwistAxis : 2;
	bool Def_NL = AI ? AI->bNegateLegSwing : false;
	bool Def_NA = AI ? AI->bNegateArmSwing : false;
	bool Def_NS = AI ? AI->bNegateSpineBend : false;

	// AXES (0-2 mapped to sliders 0.0-1.0 → round to int)
	AddSectionHeader(RootBox, FText::FromString(TEXT("─── AXES (0=P 1=Y 2=R) ───")));
	SR_LegAxis = AddSliderRow(RootBox, FText::FromString(TEXT("LegSwing")), 0, 2, Def_LA);
	SR_LegAxis.Slider->OnValueChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnSliderChanged_LegAxis);
	SR_ArmAxis = AddSliderRow(RootBox, FText::FromString(TEXT("ArmSwing")), 0, 2, Def_AA);
	SR_ArmAxis.Slider->OnValueChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnSliderChanged_ArmAxis);
	SR_ArmRestAxis = AddSliderRow(RootBox, FText::FromString(TEXT("ArmRest")), 0, 2, Def_ARA);
	SR_ArmRestAxis.Slider->OnValueChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnSliderChanged_ArmRestAxis);
	SR_SpineBendAxis = AddSliderRow(RootBox, FText::FromString(TEXT("SpineBend")), 0, 2, Def_SBA);
	SR_SpineBendAxis.Slider->OnValueChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnSliderChanged_SpineBendAxis);
	SR_SpineTwistAxis = AddSliderRow(RootBox, FText::FromString(TEXT("SpineTwist")), 0, 2, Def_STA);
	SR_SpineTwistAxis.Slider->OnValueChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnSliderChanged_SpineTwistAxis);

	// NEGATE
	AddSectionHeader(RootBox, FText::FromString(TEXT("─── NEGATE ───")));
	AddCheckboxRow(RootBox, FText::FromString(TEXT("Neg Leg")), Def_NL, 0);
	AddCheckboxRow(RootBox, FText::FromString(TEXT("Neg Arm")), Def_NA, 1);
	AddCheckboxRow(RootBox, FText::FromString(TEXT("Neg Spine")), Def_NS, 2);

	// WALK
	AddSectionHeader(RootBox, FText::FromString(TEXT("─── WALK ───")));
	SR_LegSwing = AddSliderRow(RootBox, FText::FromString(TEXT("LegSwingDeg")), 0, 60, Def_LegSwing);
	SR_LegSwing.Slider->OnValueChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnSliderChanged_LegSwingDeg);
	SR_ArmSwing = AddSliderRow(RootBox, FText::FromString(TEXT("ArmSwingDeg")), 0, 40, Def_ArmSwing);
	SR_ArmSwing.Slider->OnValueChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnSliderChanged_ArmSwingDeg);
	SR_PelvisBob = AddSliderRow(RootBox, FText::FromString(TEXT("PelvisBob")), 0, 5, Def_Bob);
	SR_PelvisBob.Slider->OnValueChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnSliderChanged_PelvisBob);
	SR_CycleRate = AddSliderRow(RootBox, FText::FromString(TEXT("CycleRate")), 0.01f, 0.1f, Def_Rate);
	SR_CycleRate.Slider->OnValueChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnSliderChanged_CycleRate);
	SR_CalfBend = AddSliderRow(RootBox, FText::FromString(TEXT("CalfBend")), 0, 3, Def_Calf);
	SR_CalfBend.Slider->OnValueChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnSliderChanged_CalfBend);

	// REST
	AddSectionHeader(RootBox, FText::FromString(TEXT("─── REST POSE ───")));
	SR_ArmRest = AddSliderRow(RootBox, FText::FromString(TEXT("ArmRestDeg")), -90, 0, Def_ArmR);
	SR_ArmRest.Slider->OnValueChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnSliderChanged_ArmRest);
	SR_ForearmRest = AddSliderRow(RootBox, FText::FromString(TEXT("ForearmRest")), -60, 0, Def_ForeR);
	SR_ForearmRest.Slider->OnValueChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnSliderChanged_ForearmRest);

	// IDLE
	AddSectionHeader(RootBox, FText::FromString(TEXT("─── IDLE ───")));
	SR_BreathAmp = AddSliderRow(RootBox, FText::FromString(TEXT("BreathAmp")), 0, 5, Def_BAmp);
	SR_BreathAmp.Slider->OnValueChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnSliderChanged_BreathAmp);
	SR_BreathRate = AddSliderRow(RootBox, FText::FromString(TEXT("BreathRate")), 0.1f, 1.f, Def_BRate);
	SR_BreathRate.Slider->OnValueChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnSliderChanged_BreathRate);

	// POSTURE
	AddSectionHeader(RootBox, FText::FromString(TEXT("─── POSTURE ───")));
	SR_PostureBend = AddSliderRow(RootBox, FText::FromString(TEXT("MaxBendDeg")), 0, 90, Def_PBend);
	SR_PostureBend.Slider->OnValueChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnSliderChanged_PostureBend);

	// SUB MOTION
	AddSectionHeader(RootBox, FText::FromString(TEXT("─── SUB MOTION ───")));
	SR_SubLean = AddSliderRow(RootBox, FText::FromString(TEXT("LeanMult")), 0, 1, Def_SLean);
	SR_SubLean.Slider->OnValueChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnSliderChanged_SubLean);
	SR_SubStumble = AddSliderRow(RootBox, FText::FromString(TEXT("StumbleMult")), 0, 0.05f, Def_SStum);
	SR_SubStumble.Slider->OnValueChanged.AddDynamic(this, &UCrewAnimDebugWidget::OnSliderChanged_SubStumble);

	// DEBUG READOUT
	AddSectionHeader(RootBox, FText::FromString(TEXT("─── LIVE ───")));
	DebugText = NewObject<UTextBlock>(this);
	DebugText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 1.f, 0.5f)));
	RootBox->AddChildToVerticalBox(DebugText);
}

// ═══════════════════════════════════════════════════════════
// TICK — update value displays
// ═══════════════════════════════════════════════════════════

static void UpdateRowDisplay(UCrewAnimDebugWidget::FSliderRow& Row, float Value) // FSliderRow is public
{
	if (Row.ValueText)
	{
		Row.ValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), Value)));
	}
}

void UCrewAnimDebugWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	USubCrewAnimInstance* AI = GetAnimInstance();
	if (!AI || !DebugText) return;

	DebugText->SetText(FText::FromString(FString::Printf(
		TEXT("Spd:%.0f Dir:%.0f Mv:%d Phase:%.1f\nPosture:%.2f YawOff:%.1f"),
		AI->Speed, AI->Direction, AI->bIsMoving ? 1 : 0, AI->WalkPhase,
		AI->PostureAlpha, AI->UpperBodyYawOffset)));
}

// ═══════════════════════════════════════════════════════════
// SLIDER CALLBACKS — write directly to AnimInstance
// ═══════════════════════════════════════════════════════════

#define SLIDER_WRITE(Prop, Row) \
	if (USubCrewAnimInstance* AI = GetAnimInstance()) { \
		const float Val = FMath::Lerp(Row.MinVal, Row.MaxVal, Value); \
		AI->Prop = Val; \
		UpdateRowDisplay(Row, Val); \
	}

#define SLIDER_WRITE_INT(Prop, Row) \
	if (USubCrewAnimInstance* AI = GetAnimInstance()) { \
		const int32 Val = FMath::RoundToInt(FMath::Lerp(Row.MinVal, Row.MaxVal, Value)); \
		AI->Prop = Val; \
		UpdateRowDisplay(Row, (float)Val); \
	}

void UCrewAnimDebugWidget::OnSliderChanged_LegSwingDeg(float Value) { SLIDER_WRITE(WalkLegSwingDeg, SR_LegSwing); }
void UCrewAnimDebugWidget::OnSliderChanged_ArmSwingDeg(float Value) { SLIDER_WRITE(WalkArmSwingDeg, SR_ArmSwing); }
void UCrewAnimDebugWidget::OnSliderChanged_PelvisBob(float Value)   { SLIDER_WRITE(WalkPelvisBobCm, SR_PelvisBob); }
void UCrewAnimDebugWidget::OnSliderChanged_CycleRate(float Value)   { SLIDER_WRITE(WalkCycleRate, SR_CycleRate); }
void UCrewAnimDebugWidget::OnSliderChanged_CalfBend(float Value)    { SLIDER_WRITE(WalkCalfBendMultiplier, SR_CalfBend); }
void UCrewAnimDebugWidget::OnSliderChanged_ArmRest(float Value)     { if (USubCrewAnimInstance* AI = GetAnimInstance()) { const float V = FMath::Lerp(SR_ArmRest.MinVal, SR_ArmRest.MaxVal, Value); AI->ArmRestR.Roll = V; AI->ArmRestL.Roll = -V; UpdateRowDisplay(SR_ArmRest, V); } }
void UCrewAnimDebugWidget::OnSliderChanged_ForearmRest(float Value) { if (USubCrewAnimInstance* AI = GetAnimInstance()) { const float V = FMath::Lerp(SR_ForearmRest.MinVal, SR_ForearmRest.MaxVal, Value); AI->ForearmRestR.Roll = V; AI->ForearmRestL.Roll = -V; UpdateRowDisplay(SR_ForearmRest, V); } }
void UCrewAnimDebugWidget::OnSliderChanged_BreathAmp(float Value)   { SLIDER_WRITE(BreathingAmplitudeDeg, SR_BreathAmp); }
void UCrewAnimDebugWidget::OnSliderChanged_BreathRate(float Value)  { SLIDER_WRITE(BreathingRate, SR_BreathRate); }
void UCrewAnimDebugWidget::OnSliderChanged_PostureBend(float Value) { SLIDER_WRITE(MaxPostureBendDeg, SR_PostureBend); }
void UCrewAnimDebugWidget::OnSliderChanged_SubLean(float Value)     { SLIDER_WRITE(SubLeanMultiplier, SR_SubLean); }
void UCrewAnimDebugWidget::OnSliderChanged_SubStumble(float Value)  { SLIDER_WRITE(SubStumbleMultiplier, SR_SubStumble); }

void UCrewAnimDebugWidget::OnSliderChanged_LegAxis(float Value)       { SLIDER_WRITE_INT(LegSwingAxis, SR_LegAxis); }
void UCrewAnimDebugWidget::OnSliderChanged_ArmAxis(float Value)       { SLIDER_WRITE_INT(ArmSwingAxis, SR_ArmAxis); }
void UCrewAnimDebugWidget::OnSliderChanged_ArmRestAxis(float Value)   { SLIDER_WRITE_INT(ElbowBendAxis, SR_ArmRestAxis); }
void UCrewAnimDebugWidget::OnSliderChanged_SpineBendAxis(float Value) { SLIDER_WRITE_INT(SpineBendAxis, SR_SpineBendAxis); }
void UCrewAnimDebugWidget::OnSliderChanged_SpineTwistAxis(float Value){ SLIDER_WRITE_INT(SpineTwistAxis, SR_SpineTwistAxis); }

void UCrewAnimDebugWidget::OnCheckChanged_NegateLeg(bool bChecked)
{
	if (USubCrewAnimInstance* AI = GetAnimInstance()) AI->bNegateLegSwing = bChecked;
}
void UCrewAnimDebugWidget::OnCheckChanged_NegateArm(bool bChecked)
{
	if (USubCrewAnimInstance* AI = GetAnimInstance()) { AI->bNegateArmSwing = bChecked; }
}
void UCrewAnimDebugWidget::OnCheckChanged_NegateSpine(bool bChecked)
{
	if (USubCrewAnimInstance* AI = GetAnimInstance()) AI->bNegateSpineBend = bChecked;
}

#undef SLIDER_WRITE
#undef SLIDER_WRITE_INT
