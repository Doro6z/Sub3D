#include "Sub3DDebugPanelModule.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Sub3DDebugPanelCommands.h"
#include "Sub3DDebugPanelStyle.h"
#include "SSub3DDebugPanelWidget.h"
#include "SSub3DWaterDebugPanelWidget.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

static const FName Sub3DDebugPanelTabName("Sub3DDebugPanel");
static const FName Sub3DWaterDebugPanelTabName("Sub3DWaterDebugPanel");

#define LOCTEXT_NAMESPACE "FSub3DDebugPanelModule"

void FSub3DDebugPanelModule::StartupModule()
{
	FSub3DDebugPanelStyle::Initialize();
	FSub3DDebugPanelStyle::ReloadTextures();

	FSub3DDebugPanelCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FSub3DDebugPanelCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FSub3DDebugPanelModule::PluginButtonClicked),
		FCanExecuteAction());

	FGlobalTabmanager::Get()
		->RegisterNomadTabSpawner(
			Sub3DDebugPanelTabName,
			FOnSpawnTab::CreateRaw(this, &FSub3DDebugPanelModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("TabDisplayName", "Sub3D Debug"))
		.SetTooltipText(LOCTEXT("TabTooltip", "All Sub3D debug toggles in one dockable panel."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Debug"))
		.SetMenuType(ETabSpawnerMenuType::Enabled);

	FGlobalTabmanager::Get()
		->RegisterNomadTabSpawner(
			Sub3DWaterDebugPanelTabName,
			FOnSpawnTab::CreateRaw(this, &FSub3DDebugPanelModule::OnSpawnWaterDebugTab))
		.SetDisplayName(LOCTEXT("WaterTabDisplayName", "Sub3D Water Debug"))
		.SetTooltipText(LOCTEXT("WaterTabTooltip", "Water heightfield + breach test commands per compartment."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Visibility"))
		.SetMenuType(ETabSpawnerMenuType::Enabled);

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSub3DDebugPanelModule::RegisterMenus));
}

void FSub3DDebugPanelModule::ShutdownModule()
{
	UToolMenus::UnregisterOwner(this);

	FSub3DDebugPanelStyle::Shutdown();
	FSub3DDebugPanelCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(Sub3DDebugPanelTabName);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(Sub3DWaterDebugPanelTabName);
}

TSharedRef<SDockTab> FSub3DDebugPanelModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("DockTabLabel", "Sub3D Debug"))
		[
			SNew(SSub3DDebugPanelWidget)
		];
}

TSharedRef<SDockTab> FSub3DDebugPanelModule::OnSpawnWaterDebugTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("WaterDockTabLabel", "Sub3D Water Debug"))
		[
			SNew(SSub3DWaterDebugPanelWidget)
		];
}

void FSub3DDebugPanelModule::WaterDebugButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(Sub3DWaterDebugPanelTabName);
}

void FSub3DDebugPanelModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* ToolbarMenu =
		UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.AssetsToolBar");
	FToolMenuSection& ToolbarSection = ToolbarMenu->FindOrAddSection("Content");
	FToolMenuEntry ToolbarEntry = FToolMenuEntry::InitToolBarButton(
		"Sub3DDebugPanelToolbar",
		FUIAction(FExecuteAction::CreateRaw(this, &FSub3DDebugPanelModule::PluginButtonClicked)),
		LOCTEXT("ToolbarButtonLabel", "Sub3D Debug"),
		LOCTEXT("ToolbarButtonTooltip", "Open the Sub3D Debug Panel"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Debug"));
	ToolbarEntry.StyleNameOverride = "AssetEditorToolbar";
	ToolbarSection.AddEntry(ToolbarEntry);

	FToolMenuEntry WaterToolbarEntry = FToolMenuEntry::InitToolBarButton(
		"Sub3DWaterDebugPanelToolbar",
		FUIAction(FExecuteAction::CreateRaw(this, &FSub3DDebugPanelModule::WaterDebugButtonClicked)),
		LOCTEXT("WaterToolbarButtonLabel", "Water Debug"),
		LOCTEXT("WaterToolbarButtonTooltip", "Open the Sub3D Water Debug Panel"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Visibility"));
	WaterToolbarEntry.StyleNameOverride = "AssetEditorToolbar";
	ToolbarSection.AddEntry(WaterToolbarEntry);
}

void FSub3DDebugPanelModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(Sub3DDebugPanelTabName);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSub3DDebugPanelModule, Sub3DDebugPanel)
