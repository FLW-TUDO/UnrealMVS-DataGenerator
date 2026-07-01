#pragma once

#include "CoreMinimal.h"

#if WITH_OPENCV
#include "PreOpenCVHeaders.h"
#include "opencv2/core.hpp"
#include "PostOpenCVHeaders.h"
#endif

/**
 * @class FImageIO
 * @brief Unified image I/O class for loading and saving images (EXR format).
 *
 * Combines single-image and batch operations for the stitching pipeline.
 * Handles OpenCV EXR support initialization automatically.
 */
class OMNIRGBDCAMERA_API FImageIO
{
public:
	// --------------------------------- Single Image Operations --------------------------------- //

	/**
	 * @brief Loads a single image from disk.
	 * @param Path Full path to the image file.
	 * @return Loaded image as cv::Mat, empty on failure.
	 */
	static cv::Mat Load(const FString& Path);

	/**
	 * @brief Saves a single image to disk.
	 * @param Image The image to save.
	 * @param Path Full path for the output file.
	 * @return True if successful.
	 */
	static bool Save(const cv::Mat& Image, const FString& Path);

	// --------------------------------- Batch Operations --------------------------------- //

	/**
	 * @brief Loads multiple images from given paths.
	 * @param ImagePaths Array of full paths to image files.
	 * @param OutImages Output vector to store loaded images.
	 * @return Number of successfully loaded images.
	 */
	static int32 LoadBatch(const TArray<FString>& ImagePaths, std::vector<cv::Mat>& OutImages);

	/**
	 * @brief Saves an image to a directory, creating the directory if needed.
	 * @param Image The image to save.
	 * @param OutputDir Directory path (will be created if missing).
	 * @param Filename Name of the output file (with extension).
	 * @return True if successful.
	 */
	static bool SaveToDirectory(const cv::Mat& Image, const FString& OutputDir, const FString& Filename);

private:
	/** Ensures OpenCV EXR support is enabled via environment variable. */
	static void EnsureExrEnabled();

	/** Flag to track if EXR support has been initialized. */
	static bool bExrEnabled;
};
