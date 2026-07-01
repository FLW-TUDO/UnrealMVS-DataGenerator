#pragma once

#include "CoreMinimal.h"
#include "Stitching/ImageStitcher.h"
#include "Stitching/StitchingConstants.h"
#include "Stitching/CubeMap/CubeMap.h"
#include <atomic>

#include "Utils/Depth.h"

/**
 * @brief Derived class for stitching equirectangular images.
 * 
 * This class inherits from the base ImageStitcher class and implements the specific functionality
 * for stitching omnidirectional (equirectangular) images.
 */
class OMNIRGBDCAMERA_API EquirectangularStitcher : public ImageStitcher
{
public:
	
	// ------------------------------------------ Constructor / Destructor ------------------------------------------ //
	// -------------------------------------------------------------------------------------------------------------- //
	
	/**
	 * @brief Constructs an EquirectangularStitcher object.
	 */
	EquirectangularStitcher();

	/**
	 * @brief Constructs an EquirectangularStitcher object with the given camera names and folder names.
	 * 
	 * @param Cameras Array of camera names to stitch together.
	 * @param NameFolderImage Name of the folder containing the color images.
	 * @param NameFolderImageDepth Name of the folder containing the depth images.
	 */
	EquirectangularStitcher(const TArray<FString>& Cameras, const FString& NameFolderImage, const FString& NameFolderImageDepth);

	/**
	 * @brief Destructs the EquirectangularStitcher object.
	 */
	~EquirectangularStitcher() override;
	
	// -------------------------------------------------- Stitching ------------------------------------------------- //
	// -------------------------------------------------------------------------------------------------------------- //
	
	/**
	 * @brief Processes the directories containing the omnidirectional (equirectangular) images.
	 */
	void ProcessDirectories();
	
	// ------------------------------------ Public Members ------------------------------------- //

	// Vertical FOV of the Equirectangular image (degrees)
	int VerticalFOV = StitchingConstants::DefaultVerticalFOV;
	// True if user wants to restrain 2:1, false if user wants clear image
	bool bLeaveBlackBars = true;

	bool bStitchDepthImages = true;
	bool bStitchInvDepthImages = false;

	cv::Mat ValidRegionMask;

	// ------------------------------------ Progress Tracking ------------------------------------- //

	/** Current number of processed frames (thread-safe) */
	std::atomic<int32> ProcessedFrames{0};

	/** Total number of frames to process */
	int32 TotalFrames = 0;

	/** Get current progress */
	void GetProgress(int32& OutCurrent, int32& OutTotal) const
	{
		OutCurrent = ProcessedFrames.load();
		OutTotal = TotalFrames;
	}

private:
	
	// -------------------------------------------------- Stitching ------------------------------------------------- //
	// -------------------------------------------------------------------------------------------------------------- //
	
	/**
	 * @brief Stitches the images in the given directory and returns the stitched image.
	 *
	 * @param DirectoryPath The path to the directory containing the images to be stitched.
	 * @return The stitched image.
	 */
	cv::Mat StitchImagesInDirectory(const FString& DirectoryPath);

	/**
	 * @brief Generates a lookup table for converting the cubemap to an equirectangular projection.
	 *
	 * @param CubeMapAtlas The input cubemap atlas.
	 */
	void GenerateLookupTable(const cv::Mat& CubeMapAtlas);
	
	// ----------------------------------------------- File Handling ------------------------------------------------ //
	// -------------------------------------------------------------------------------------------------------------- //
	
	/**
	 * @brief Processes a subdirectory containing images.
	 * 
	 * @param Subdirectory Name of the subdirectory to process.
	 */
	void ProcessSubdirectory(const FString& Subdirectory);

	/**
	 * @brief Processes a camera folder for stitching
	 * @param CameraFolder Camera folder name (e.g., "Equirectangular")
	 * @param FolderPathBase Base path containing the camera folder
	 * @param Subdirectory Frame subdirectory name for output
	 * @param ImageType Type of image (Color, Depth, InvDepth)
	 */
	void ProcessImageFolder(const FString& CameraFolder,
							const FString& FolderPathBase,
							const FString& Subdirectory,
							EStitchImageType ImageType);
};