#include "Slate/SCraniataMaterialEditorPanel.h"

#include "Editor.h"
#include "IDetailsView.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCraniataMaterialEditorPanel"

void SCraniataMaterialEditorPanel::Construct(const FArguments& InArgs)
{
	BuildMaterialEntries();

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bLockable = false;
	DetailsArgs.bUpdatesFromSelection = false;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsArgs);

	SelectedIndex = MaterialEntries.Num() > 0 ? 0 : INDEX_NONE;
	RefreshDetailsView();

	TSharedRef<SVerticalBox> ButtonList = SNew(SVerticalBox);
	for (int32 Index = 0; Index < MaterialEntries.Num(); ++Index)
	{
		ButtonList->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(SButton)
			.Text(FText::FromString(MaterialEntries[Index].Label))
			.OnClicked(this, &SCraniataMaterialEditorPanel::HandleSelectMaterial, Index)
		];
	}

	ChildSlot
	[
		SNew(SBorder)
		.Padding(8.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Header", "Craniata Material Editor"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Hint", "Select a Craniata material instance on the left. Edit color, roughness, and metallic in the details panel."))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Refresh", "Refresh"))
					.OnClicked(this, &SCraniataMaterialEditorPanel::HandleRefresh)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Browse", "Browse Asset"))
					.OnClicked(this, &SCraniataMaterialEditorPanel::HandleBrowseSelected)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				.Padding(12.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(this, &SCraniataMaterialEditorPanel::GetSelectedLabelText)
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SSplitter)
				+ SSplitter::Slot()
				.Value(0.28f)
				[
					SNew(SBorder)
					.Padding(6.f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							ButtonList
						]
					]
				]
				+ SSplitter::Slot()
				.Value(0.72f)
				[
					SNew(SBorder)
					.Padding(6.f)
					[
						DetailsView.IsValid()
						? StaticCastSharedRef<SWidget>(DetailsView.ToSharedRef())
						: StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(LOCTEXT("NoDetails", "Details view unavailable")))
					]
				]
			]
		]
	];
}

void SCraniataMaterialEditorPanel::BuildMaterialEntries()
{
	MaterialEntries = {
		{TEXT("Hull"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Hull.MI_Craniata_Hull")},
		{TEXT("Deck"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Deck.MI_Craniata_Deck")},
		{TEXT("Deck Upper"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_DeckUpper.MI_Craniata_DeckUpper")},
		{TEXT("Bulkhead"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Bulkhead.MI_Craniata_Bulkhead")},
		{TEXT("Bulkhead Lower"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_BulkheadLower.MI_Craniata_BulkheadLower")},
		{TEXT("Door"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Door.MI_Craniata_Door")},
		{TEXT("Door Frame"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_DoorFrame.MI_Craniata_DoorFrame")},
		{TEXT("Hatch"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Hatch.MI_Craniata_Hatch")},
		{TEXT("Fin"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Fin.MI_Craniata_Fin")},
		{TEXT("Propulsor"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Propulsor.MI_Craniata_Propulsor")},
		{TEXT("Duct"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Duct.MI_Craniata_Duct")},
		{TEXT("Pipe Water"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_PipeWater.MI_Craniata_PipeWater")},
		{TEXT("Pipe Hyd"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_PipeHyd.MI_Craniata_PipeHyd")},
		{TEXT("Pipe Reactor"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_PipeReactor.MI_Craniata_PipeReactor")},
		{TEXT("Pipe Default"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Pipe.MI_Craniata_Pipe")},
		{TEXT("Valve"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Valve.MI_Craniata_Valve")},
		{TEXT("Junction"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Junction.MI_Craniata_Junction")},
		{TEXT("Periscope"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Periscope.MI_Craniata_Periscope")},
		{TEXT("Antenna"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Antenna.MI_Craniata_Antenna")},
		{TEXT("Flag"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Flag.MI_Craniata_Flag")},
		{TEXT("Catwalk"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Catwalk.MI_Craniata_Catwalk")},
		{TEXT("Ladder"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Ladder.MI_Craniata_Ladder")},
		{TEXT("Turret"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Turret.MI_Craniata_Turret")},
		{TEXT("Mount"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Mount.MI_Craniata_Mount")},
		{TEXT("Engine"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Engine.MI_Craniata_Engine")},
		{TEXT("Reactor"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Reactor.MI_Craniata_Reactor")},
		{TEXT("Storage"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Storage.MI_Craniata_Storage")},
		{TEXT("Seal"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Seal.MI_Craniata_Seal")},
		{TEXT("UI"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_UI.MI_Craniata_UI")},
		{TEXT("Default"), TEXT("/Game/Sub3D/FirstPlayableRun/Materials/Craniata/MI_Craniata_Default.MI_Craniata_Default")},
	};
}

void SCraniataMaterialEditorPanel::RefreshDetailsView()
{
	if (!DetailsView.IsValid())
	{
		return;
	}

	if (UMaterialInstanceConstant* Material = LoadMaterialAtIndex(SelectedIndex))
	{
		DetailsView->SetObject(Material);
	}
	else
	{
		DetailsView->SetObject(nullptr);
	}
}

UMaterialInstanceConstant* SCraniataMaterialEditorPanel::LoadMaterialAtIndex(int32 Index) const
{
	if (!MaterialEntries.IsValidIndex(Index))
	{
		return nullptr;
	}

	return LoadObject<UMaterialInstanceConstant>(nullptr, *MaterialEntries[Index].AssetPath);
}

FReply SCraniataMaterialEditorPanel::HandleSelectMaterial(int32 Index)
{
	SelectedIndex = Index;
	RefreshDetailsView();
	return FReply::Handled();
}

FReply SCraniataMaterialEditorPanel::HandleRefresh()
{
	RefreshDetailsView();
	return FReply::Handled();
}

FReply SCraniataMaterialEditorPanel::HandleBrowseSelected()
{
	if (UMaterialInstanceConstant* Material = LoadMaterialAtIndex(SelectedIndex))
	{
		TArray<UObject*> Assets;
		Assets.Add(Material);
		GEditor->SyncBrowserToObjects(Assets);
	}

	return FReply::Handled();
}

FText SCraniataMaterialEditorPanel::GetSelectedLabelText() const
{
	if (MaterialEntries.IsValidIndex(SelectedIndex))
	{
		return FText::FromString(FString::Printf(TEXT("Selected: %s"), *MaterialEntries[SelectedIndex].Label));
	}

	return LOCTEXT("NoSelection", "Selected: none");
}

#undef LOCTEXT_NAMESPACE
