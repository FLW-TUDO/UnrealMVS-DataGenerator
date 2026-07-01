// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/OmniRGBDCameraStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FOmniRGBDCameraStyle::StyleInstance = nullptr;

void FOmniRGBDCameraStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FOmniRGBDCameraStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FOmniRGBDCameraStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("OmniRGBDCameraStyle"));
	return StyleSetName;
}

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon128x128(128.0f, 128.0f);

TSharedRef< FSlateStyleSet > FOmniRGBDCameraStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("OmniRGBDCameraStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("OmniRGBDCamera")->GetBaseDir() / TEXT("Resources"));

	Style->Set("OmniRGBDCamera.OpenPluginWindow", new IMAGE_BRUSH_SVG(TEXT("Icon128"), Icon128x128));

	return Style;
}

void FOmniRGBDCameraStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FOmniRGBDCameraStyle::Get()
{
	return *StyleInstance;
}
