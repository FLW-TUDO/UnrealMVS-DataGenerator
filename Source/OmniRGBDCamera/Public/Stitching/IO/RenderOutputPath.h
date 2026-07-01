#pragma once

#include "CoreMinimal.h"

/**
 * @class FRenderOutputPath
 * @brief Utility class for navigating Movie Render Queue output directory structure.
 *
 * Expected directory structure:
 * MovieRenders/
 * +-- 0000/                    (Frame directories)
 * |   +-- FinalImage/          (Type folders)
 * |   |   +-- Equirectangular/ (Camera folders)
 * |   |   +-- FisheyeCamera_1/
 * |   +-- FinalImageDepth/
 * +-- 0001/
 * +-- ...
 */
class OMNIRGBDCAMERA_API FRenderOutputPath
{
public:
	// --------------------------------- Frame Directories --------------------------------- //

	/**
	 * @brief Scans for frame directories within a path and filters by frame range.
	 * @param BasePath Root path containing numbered frame directories.
	 * @param FrameMin Minimum frame number (inclusive), e.g., "0000".
	 * @param FrameMax Maximum frame number (inclusive), e.g., "0100".
	 * @return Array of frame directory names (folder names only, not full paths).
	 */
	static TArray<FString> GetFrameDirectories(
		const FString& BasePath,
		const FString& FrameMin,
		const FString& FrameMax);

	/**
	 * @brief Gets the minimum and maximum frame numbers in a directory.
	 * @param BasePath Root path containing numbered frame directories.
	 * @param OutMin Output: minimum frame number found.
	 * @param OutMax Output: maximum frame number found.
	 * @return True if at least one frame directory was found.
	 */
	static bool GetFrameRange(
		const FString& BasePath,
		int32& OutMin,
		int32& OutMax);

	// --------------------------------- Camera Directories --------------------------------- //

	/**
	 * @brief Finds all camera directories matching a prefix within a type folder.
	 * @param TypeFolderPath Full path to the type folder (e.g., .../0000/FinalImage/).
	 * @param CameraPrefix Prefix to match (e.g., "FisheyeCamera_" or "Equirectangular").
	 * @return Array of camera directory names.
	 */
	static TArray<FString> GetCameraDirectories(
		const FString& TypeFolderPath,
		const FString& CameraPrefix);

	// --------------------------------- Image Paths --------------------------------- //

	/**
	 * @brief Collects image paths for specified camera names in a directory.
	 * @param DirectoryPath Path to the directory containing images.
	 * @param CameraNames Array of camera names (without extension).
	 * @param Extension File extension without dot (e.g., "exr").
	 * @param OutImagePaths Output array to store found image paths.
	 */
	static void CollectImagePaths(
		const FString& DirectoryPath,
		const TArray<FString>& CameraNames,
		const FString& Extension,
		TArray<FString>& OutImagePaths);

	/**
	 * @brief Gets the path to the first image file in a directory.
	 * @param DirectoryPath Path to search for images.
	 * @param Extension Image extension without dot (e.g., "exr").
	 * @return Full path to the first image found, or empty string if none.
	 */
	static FString GetFirstImagePath(
		const FString& DirectoryPath,
		const FString& Extension);

	// --------------------------------- Path Building --------------------------------- //

	/**
	 * @brief Builds the full path to a camera folder within the render output structure.
	 * @param BasePath Root render output path.
	 * @param FrameDir Frame directory name (e.g., "0000").
	 * @param TypeFolder Type folder name (e.g., "FinalImage").
	 * @param CameraFolder Camera folder name (e.g., "Equirectangular").
	 * @return Combined path.
	 */
	static FString BuildCameraPath(
		const FString& BasePath,
		const FString& FrameDir,
		const FString& TypeFolder,
		const FString& CameraFolder);

	// --------------------------------- Validation --------------------------------- //

	/**
	 * @brief Checks if a directory contains at least one file with the given extension.
	 * @param DirectoryPath Path to check.
	 * @param Extension File extension without dot.
	 * @return True if at least one matching file exists.
	 */
	static bool HasFilesWithExtension(
		const FString& DirectoryPath,
		const FString& Extension);

private:
	/**
	 * @brief Checks if a string represents a numeric directory name.
	 * @param DirectoryName Name to check.
	 * @return True if the name is purely numeric (e.g., "0000", "0001").
	 */
	static bool IsNumericDirectory(const FString& DirectoryName);
};
