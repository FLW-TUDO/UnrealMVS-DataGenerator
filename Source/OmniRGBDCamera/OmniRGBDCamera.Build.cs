// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OmniRGBDCamera : ModuleRules
{
	public OmniRGBDCamera(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Required for cereal serialization library
		bEnableExceptions = true;

		// Basalt-headers and dependencies (all header-only)
		string ThirdPartyPath = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty", "basalt-headers");

		PublicIncludePaths.AddRange(new string[] {
			Path.Combine(ThirdPartyPath, "include"),                         // basalt headers
			Path.Combine(ThirdPartyPath, "thirdparty", "Sophus"),            // Sophus (Lie groups)
			Path.Combine(ThirdPartyPath, "thirdparty", "cereal", "include"), // cereal (serialization)
			Path.Combine(ThirdPartyPath, "thirdparty", "eigen"),             // Eigen (linear algebra)
		});

		// Sophus: use basic logging to avoid fmt dependency
		PublicDefinitions.Add("SOPHUS_USE_BASIC_LOGGING");

		// MCAP (header-only, no LZ4/ZSTD compression)
		string McapPath = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty", "mcap", "include");
		if (System.IO.Directory.Exists(McapPath))
		{
			PublicIncludePaths.Add(McapPath);
			PublicDefinitions.AddRange(new string[] {
				"MCAP_COMPRESSION_NO_LZ4",
				"MCAP_COMPRESSION_NO_ZSTD",
			});
		}

		/*
		 * PublicDependencyModuleNames:
		 * - Modules that are part of your module's public API.
		 * - If your public headers include headers from these modules,
		 *   they should be listed here.
		 */
		PublicDependencyModuleNames.AddRange([
				"Core",
				"CoreUObject",
				"Engine"
			]
		);
			
		/*
		 * PrivateDependencyModuleNames:
		 * - Modules only needed internally (implementation files .cpp).
		 * - If only .cpp files include headers from these modules, put them here.
		 */
		PrivateDependencyModuleNames.AddRange([
				
				// --- Basic editor/GUI integration ---
				"Slate",        // Low-level editor UI framework (SWidgets, layout, styling)
				"SlateCore",    // Core Slate types (SWidget base, input, styling primitives)
				"ToolMenus",    // Extend editor menus/toolbars (your toolbar button)
				"UnrealEd",     // Editor-only APIs (GEditor, editor subsystems, etc.)
				
				// --- Editor Utility Widgets (EUW) support ---
				"UMG",          // UMG runtime types (UUserWidget, UWidget, etc.)
				"UMGEditor",    // Editor-side UMG/WidgetBlueprint types (WidgetBlueprint.h, EUW BP class)
				"Blutility",    // Editor Utility Widgets / editor scripting integration
				
				// --- Input ---
				"InputCore",
				
				// --- Project/Platform helpers ---
				"Projects",         // Plugin/project helpers, paths, IPluginManager etc.
				"DesktopPlatform",  // File dialogs / desktop integration (if you use it)
				
				// --- Serialization / data formats ---
				"Json",
				"JsonUtilities",
				
				// --- Sequencer / LevelSequence (only keep if you actually use it) ---
				"MovieScene",
				"MovieSceneTracks",
				"LevelSequence",

				// --- Movie Render Pipeline (for CameraPoseOutput and RosbagCreator) ---
				"MovieRenderPipelineCore",

				// --- External libs ---
				"OpenCV",
				"OpenCVHelper",
				"OpenExrWrapper",
				"UEOpenExr",
			]
		);
	}
}
