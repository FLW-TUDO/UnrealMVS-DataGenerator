// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/OmniRGBDCameraCommands.h"

#define LOCTEXT_NAMESPACE "FOmniRGBDCameraModule"

void FOmniRGBDCameraCommands::RegisterCommands()
{
	UI_COMMAND(
		OpenPluginWindow,
		"OmniRGBDCamera",
		"Open the OmniRGBDCamera tool window",
		EUserInterfaceActionType::Button,
		FInputChord()
	);
}

#undef LOCTEXT_NAMESPACE
