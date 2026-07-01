// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/OmniRGBDCamera.h"
#include "Core/OmniRGBDCameraStyle.h"
#include "Core/OmniRGBDCameraCommands.h"

#include "Editor.h"
#include "ToolMenus.h"

// EUW spawning
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FOmniRGBDCameraModule"

static const TCHAR* OmniEUWPath = TEXT("/OmniRGBDCamera/UI/OmniRGBDCamera.OmniRGBDCamera");

void FOmniRGBDCameraModule::StartupModule()
{
    // Initialize plugin style (icons/brushes) used by menus/toolbars.
	FOmniRGBDCameraStyle::Initialize();
	FOmniRGBDCameraStyle::ReloadTextures();

	// Register UI commands (used for menu/toolbar entries).
	FOmniRGBDCameraCommands::Register();
	
	// Bind commands to actions.
	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FOmniRGBDCameraCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FOmniRGBDCameraModule::PluginButtonClicked),
		FCanExecuteAction());

	// Register menu/toolbar extensions once tool menus are ready.
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FOmniRGBDCameraModule::RegisterMenus)
	);
}

void FOmniRGBDCameraModule::ShutdownModule()
{
	// Unregister menu callbacks and ownership to avoid dangling menu entries.
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	// Shutdown style + commands.
	FOmniRGBDCameraStyle::Shutdown();
	FOmniRGBDCameraCommands::Unregister();
}

void FOmniRGBDCameraModule::PluginButtonClicked()
{
	// Load the Editor Utility Widget blueprint asset.
	UEditorUtilityWidgetBlueprint* EUW =
		LoadObject<UEditorUtilityWidgetBlueprint>(nullptr, OmniEUWPath);

	if (!EUW)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not find EUW: %s"), OmniEUWPath);
		return;
	}

	// Spawn as a dockable editor tab via the Editor Utility Subsystem.
	if (UEditorUtilitySubsystem* Subsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>())
	{
		Subsystem->SpawnAndRegisterTab(EUW);
	}
}

void FOmniRGBDCameraModule::RegisterMenus()
{
	// Used for cleanup when unregistering tool menus owner.
	FToolMenuOwnerScoped OwnerScoped(this);

	// Add entry to Window menu
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
		Section.AddMenuEntryWithCommandList(
			FOmniRGBDCameraCommands::Get().OpenPluginWindow,
			PluginCommands
		);
	}
	
	// Add toolbar button
	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.AssetsToolBar");
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Asset");
		
		FToolMenuEntry& Entry = Section.AddEntry(
			FToolMenuEntry::InitToolBarButton(FOmniRGBDCameraCommands::Get().OpenPluginWindow)
		);
		Entry.SetCommandList(PluginCommands);
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FOmniRGBDCameraModule, OmniRGBDCamera)