#include "SSub3DDebugPanelWidget.h"

#include "AssetRegistry/AssetData.h"
#include "Debug/Sub3DDebugSettings.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "Submarine/Generator/SubmarineDefinition.h"
#include "Submarine/Generator/SubmarineDefinitionTypes.h"
#include "SubmarineWaterBakerLibrary.h"
#include "Sub3DDebugPanelStyle.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Types/CompartmentWaterBake.h"
#include "WaterBakeViewer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
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

	LastBakeReport = TEXT("Idle. Pick a Submarine Definition + open the gameplay map (with BP_Submarine_* placed) + click Bake Water.");
	ViewerStatus = NSLOCTEXT("SSub3DDebugPanelWidget", "ViewerStatusIdle", "No viewers spawned. Run Bake Water then click Spawn All Viewers.");

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

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Debug"))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.728f, 1.0f, 0.0f, 1.0f)))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("HeaderTitle", "Sub3D"))
					.TextStyle(&FSub3DDebugPanelStyle::Get().GetWidgetStyle<FTextBlockStyle>("Sub3DDebug.HeaderText"))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("HeaderSubtitle", "Authoring commands + debug toggles. Settings persist via USub3DDebugSettings."))
				.TextStyle(&FSub3DDebugPanelStyle::Get().GetWidgetStyle<FTextBlockStyle>("Sub3DDebug.SubHeaderText"))
				.AutoWrapText(true)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4, 0, 6)
			[
				SNew(SSeparator).Thickness(1.f)
			]

			// ── Authoring section ──────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 6)
			[
				SNew(SExpandableArea)
				.InitiallyCollapsed(false)
				.HeaderContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AuthoringHeader", "Authoring"))
					.TextStyle(&FSub3DDebugPanelStyle::Get().GetWidgetStyle<FTextBlockStyle>("Sub3DDebug.HeaderText"))
				]
				.BodyContent()
				[
					SNew(SVerticalBox)

					// Submarine Definition picker
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 6, 0, 0)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0, 0, 8, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("DefinitionLabel", "Submarine Definition"))
						]

						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						[
							SNew(SObjectPropertyEntryBox)
							.AllowedClass(USubmarineDefinition::StaticClass())
							.ObjectPath_Raw(this, &SSub3DDebugPanelWidget::GetSelectedDefinitionPath)
							.OnObjectChanged_Raw(this, &SSub3DDebugPanelWidget::OnDefinitionPicked)
							.AllowClear(true)
							.DisplayUseSelected(true)
							.DisplayBrowse(true)
						]
					]

					// Bake / Copy buttons row
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 6, 0, 0)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0, 0, 8, 0)
						[
							SNew(SButton)
							.Text(LOCTEXT("BakeWaterButton", "Bake Water"))
							.ToolTipText(LOCTEXT("BakeWaterTooltip",
								"Iterate every compartment in the picked Submarine Definition, find a "
								"BP_Submarine_* actor in the active level, voxelise + Marching Squares, "
								"save CWB_<id>.uasset under Submarines/<sub>/Water/."))
							.OnClicked_Raw(this, &SSub3DDebugPanelWidget::OnBakeWaterClicked)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("CopyReportButton", "Copy Report"))
							.ToolTipText(LOCTEXT("CopyReportTooltip",
								"Copy the bake report (header + per-compartment lines + footer) to the clipboard."))
							.OnClicked_Raw(this, &SSub3DDebugPanelWidget::OnCopyReportClicked)
						]
					]

					// Multi-line read-only report (selectable / Ctrl+C-able). Height-bounded so a
					// long report doesn't push the rest of the panel off-screen.
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 6, 0, 0)
					[
						SNew(SBox)
						.HeightOverride(220.f)
						[
							SAssignNew(ReportTextBox, SMultiLineEditableTextBox)
							.IsReadOnly(true)
							.AlwaysShowScrollbars(true)
							.AutoWrapText(false)
							.Text(FText::FromString(LastBakeReport))
						]
					]

					// Bake Viewers — spawn / despawn debug actors next to the sub
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 10, 0, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BakeViewersHeader", "Bake Viewers"))
						.TextStyle(&FSub3DDebugPanelStyle::Get().GetWidgetStyle<FTextBlockStyle>("Sub3DDebug.SubHeaderText"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 4, 0, 0)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0, 0, 8, 0)
						[
							SNew(SButton)
							.Text(LOCTEXT("SpawnAllViewersButton", "Spawn All Viewers"))
							.ToolTipText(LOCTEXT("SpawnAllViewersTooltip",
								"For every CWB asset produced by the last bake, spawn an AWaterBakeViewer "
								"at the submarine's location + (0, 5000, 0) cm. All viewers share the same "
								"offset → the bake's sub-local positions stack to recreate the sub layout "
								"to the side of the real sub. Move individual viewers via gizmo."))
							.OnClicked_Raw(this, &SSub3DDebugPanelWidget::OnSpawnAllViewersClicked)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("DespawnAllViewersButton", "Despawn All Viewers"))
							.ToolTipText(LOCTEXT("DespawnAllViewersTooltip",
								"Destroy every AWaterBakeViewer spawned by this panel session."))
							.OnClicked_Raw(this, &SSub3DDebugPanelWidget::OnDespawnAllViewersClicked)
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 4, 0, 0)
					[
						SNew(STextBlock)
						.Text_Lambda([this]() { return ViewerStatus; })
						.AutoWrapText(true)
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4, 0, 6)
			[
				SNew(SSeparator).Thickness(1.f)
			]

			// ── Debug Toggles section ──────────────────────────────
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
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

FString SSub3DDebugPanelWidget::GetSelectedDefinitionPath() const
{
	return SelectedDefinition.IsValid() ? SelectedDefinition->GetPathName() : FString();
}

void SSub3DDebugPanelWidget::OnDefinitionPicked(const FAssetData& Asset)
{
	SelectedDefinition = Cast<USubmarineDefinition>(Asset.GetAsset());
}

FReply SSub3DDebugPanelWidget::OnBakeWaterClicked()
{
	auto SetReport = [this](const FString& Text)
	{
		LastBakeReport = Text;
		if (ReportTextBox.IsValid()) ReportTextBox->SetText(FText::FromString(LastBakeReport));
	};

	USubmarineDefinition* Definition = SelectedDefinition.Get();
	if (!Definition)
	{
		SetReport(TEXT("[FAIL] Pick a Submarine Definition first."));
		return FReply::Handled();
	}

	UEditorActorSubsystem* ActorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
	if (!ActorSubsystem)
	{
		SetReport(TEXT("[FAIL] EditorActorSubsystem unavailable."));
		return FReply::Handled();
	}

	TArray<AActor*> AllActors = ActorSubsystem->GetAllLevelActors();
	AActor* HullActor = nullptr;
	for (AActor* Actor : AllActors)
	{
		if (!Actor) continue;
		const FString ClassName = Actor->GetClass()->GetName();
		if (ClassName.StartsWith(TEXT("BP_Submarine_")))
		{
			HullActor = Actor;
			break;
		}
	}
	if (!HullActor)
	{
		SetReport(TEXT("[FAIL] No BP_Submarine_* actor in the active level. Open the gameplay map first."));
		return FReply::Handled();
	}

	// Iterate manually instead of calling BakeAllCompartments — that way we capture each
	// produced CWB into LastBakedAssets, which feeds Spawn All Viewers.
	FSubmarineWaterBakeParams Params; // defaults
	FString Report;
	const FString HeaderLine = FString::Printf(
		TEXT("Bake on %s — %d compartments — params: NumSlices=%d CellSize=%.1fcm RingsCount=%d ResampleN=%d Inset=%.1fcm"),
		*HullActor->GetName(), Definition->Compartments.Num(),
		Params.NumSlices, Params.CellSizeCm, Params.RingsCount, Params.PolygonResampleN, Params.CapInsetCm);
	Report += HeaderLine + TEXT("\n");

	LastBakedAssets.Reset();
	int32 SuccessCount = 0;
	for (const FGeneratedCompartmentDef& Comp : Definition->Compartments)
	{
		if (Comp.CompartmentId.IsNone()) continue;
		UCompartmentWaterBake* Bake = USubmarineWaterBakerLibrary::BakeCompartment(
			Definition, Comp.CompartmentId, HullActor, Params, Report);
		if (Bake)
		{
			++SuccessCount;
			LastBakedAssets.Add(Bake);
		}
	}

	const FString FooterLine = FString::Printf(TEXT("─ Done: %d/%d compartments baked ─"),
		SuccessCount, Definition->Compartments.Num());
	Report += FooterLine + TEXT("\n");

	// Persist the report to disk so the user can diff between bakes. Path appended to the
	// in-panel text so the user knows where to find it.
	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString ReportFolder = FPaths::ProjectSavedDir() / TEXT("Sub3D/BakeReports");
	const FString ReportPath = ReportFolder / FString::Printf(TEXT("BakeReport_%s.txt"), *Timestamp);
	if (FFileHelper::SaveStringToFile(Report, *ReportPath))
	{
		Report += FString::Printf(TEXT("Report saved to: %s\n"), *FPaths::ConvertRelativePathToFull(ReportPath));
	}

	SetReport(Report);

	ViewerStatus = FText::Format(NSLOCTEXT("SSub3DDebugPanelWidget", "ViewerStatusReady",
		"{0} bake(s) ready. Click Spawn All Viewers."),
		FText::AsNumber(LastBakedAssets.Num()));

	return FReply::Handled();
}

FReply SSub3DDebugPanelWidget::OnSpawnAllViewersClicked()
{
	if (LastBakedAssets.Num() == 0)
	{
		ViewerStatus = NSLOCTEXT("SSub3DDebugPanelWidget", "ViewerStatusNoBakes",
			"No bakes captured. Run Bake Water first.");
		return FReply::Handled();
	}

	UEditorActorSubsystem* ActorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
	if (!ActorSubsystem)
	{
		ViewerStatus = NSLOCTEXT("SSub3DDebugPanelWidget", "ViewerStatusNoSubsystem",
			"EditorActorSubsystem unavailable.");
		return FReply::Handled();
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		ViewerStatus = NSLOCTEXT("SSub3DDebugPanelWidget", "ViewerStatusNoWorld",
			"No editor world.");
		return FReply::Handled();
	}

	// Locate hull actor for the spawn anchor (so the viewer copies render alongside the sub).
	TArray<AActor*> AllActors = ActorSubsystem->GetAllLevelActors();
	AActor* HullActor = nullptr;
	for (AActor* Actor : AllActors)
	{
		if (Actor && Actor->GetClass()->GetName().StartsWith(TEXT("BP_Submarine_")))
		{
			HullActor = Actor;
			break;
		}
	}
	const FVector AnchorLocation = HullActor ? HullActor->GetActorLocation() : FVector::ZeroVector;
	const FRotator AnchorRotation = HullActor ? HullActor->GetActorRotation() : FRotator::ZeroRotator;
	// All viewers share the same offset (lateral on Y) → the bake's sub-local positions stack
	// to recreate the sub layout to the side of the real sub.
	const FVector SpawnLocation = AnchorLocation + FVector(0.f, 5000.f, 0.f);

	int32 Spawned = 0;
	for (const TWeakObjectPtr<UCompartmentWaterBake>& WeakBake : LastBakedAssets)
	{
		UCompartmentWaterBake* Bake = WeakBake.Get();
		if (!Bake) continue;

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AWaterBakeViewer* Viewer = World->SpawnActor<AWaterBakeViewer>(
			AWaterBakeViewer::StaticClass(), SpawnLocation, AnchorRotation, SpawnParams);
		if (!Viewer) continue;

		Viewer->BakeToView = Bake;
#if WITH_EDITOR
		Viewer->SetActorLabel(FString::Printf(TEXT("BakeViewer_%s"), *Bake->CompartmentId.ToString()));
#endif
		SpawnedViewers.Add(Viewer);
		++Spawned;
	}

	ViewerStatus = FText::Format(NSLOCTEXT("SSub3DDebugPanelWidget", "ViewerStatusSpawned",
		"Spawned {0} viewer(s) at sub + (0, 5000, 0). Move them via gizmo to inspect."),
		FText::AsNumber(Spawned));
	return FReply::Handled();
}

FReply SSub3DDebugPanelWidget::OnDespawnAllViewersClicked()
{
	int32 Destroyed = 0;
	for (const TWeakObjectPtr<AWaterBakeViewer>& WeakViewer : SpawnedViewers)
	{
		if (AWaterBakeViewer* Viewer = WeakViewer.Get())
		{
			Viewer->Destroy();
			++Destroyed;
		}
	}
	SpawnedViewers.Reset();
	ViewerStatus = FText::Format(NSLOCTEXT("SSub3DDebugPanelWidget", "ViewerStatusDespawned",
		"Despawned {0} viewer(s)."), FText::AsNumber(Destroyed));
	return FReply::Handled();
}

FReply SSub3DDebugPanelWidget::OnCopyReportClicked()
{
	FPlatformApplicationMisc::ClipboardCopy(*LastBakeReport);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
