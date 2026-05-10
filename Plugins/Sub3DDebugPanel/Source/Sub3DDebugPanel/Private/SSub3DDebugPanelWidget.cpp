#include "SSub3DDebugPanelWidget.h"

#include "Debug/Sub3DDebugSettings.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Sub3DDebugPanelStyle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SSub3DDebugPanelWidget"

void SSub3DDebugPanelWidget::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& PropertyEditorModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bShowOptions = false;
	DetailsArgs.bShowScrollBar = false;
	DetailsArgs.bShowObjectLabel = false;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	DetailsView = PropertyEditorModule.CreateDetailView(DetailsArgs);
	DetailsView->SetObject(GetMutableDefault<USub3DDebugSettings>());

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FSub3DDebugPanelStyle::Get().GetBrush("Sub3DDebug.Section"))
		.Padding(FMargin(8))
		[
			SNew(SVerticalBox)

			// ── Header ──────────────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 6)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Debug"))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.728f, 1.0f, 0.0f, 1.0f)))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("HeaderTitle", "Sub3D"))
					.TextStyle(&FSub3DDebugPanelStyle::Get().GetWidgetStyle<FTextBlockStyle>("Sub3DDebug.HeaderText"))
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("HeaderSubtitle", "Debug toggles. Settings persist via USub3DDebugSettings. Bake authoring lives in the Sub3D Water Debug panel."))
				.TextStyle(&FSub3DDebugPanelStyle::Get().GetWidgetStyle<FTextBlockStyle>("Sub3DDebug.SubHeaderText"))
				.AutoWrapText(true)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 6)
			[
				SNew(SSeparator).Thickness(1.f)
			]

			// ── Debug Toggles section ──────────────────────────────
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					DetailsView.ToSharedRef()
				]
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
