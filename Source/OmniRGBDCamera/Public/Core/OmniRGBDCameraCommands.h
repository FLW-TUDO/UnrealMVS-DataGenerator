// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "OmniRGBDCameraStyle.h"

class FOmniRGBDCameraCommands : public TCommands<FOmniRGBDCameraCommands>
{
public:

	FOmniRGBDCameraCommands()
		: TCommands<FOmniRGBDCameraCommands>(TEXT("OmniRGBDCamera"), NSLOCTEXT("Contexts", "OmniRGBDCamera", "OmniRGBDCamera Plugin"), NAME_None, FOmniRGBDCameraStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};