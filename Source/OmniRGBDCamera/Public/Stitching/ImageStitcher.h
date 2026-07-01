#pragma once

#include "CoreMinimal.h"
#include "Stitching/StitchingConstants.h"

#if WITH_OPENCV
#include "PreOpenCVHeaders.h"
#include "opencv2/core.hpp"
#include "opencv2/opencv.hpp"
#include "PostOpenCVHeaders.h"
#endif

/**
 * @enum EStitchImageType
 * @brief Type of image being stitched (used for output naming and processing flags)
 */
enum class EStitchImageType : uint8
{
	Color,
	Depth,
	InvDepth
};

/**
 * @brief Returns the output file suffix for a given image type
 * @param Type The image type
 * @return File suffix including extension (e.g., ".exr", "_Depth.exr", "_InvDepth.exr")
 */
inline FString GetStitchOutputSuffix(EStitchImageType Type)
{
	switch (Type)
	{
	case EStitchImageType::Color:    return TEXT(".exr");
	case EStitchImageType::Depth:    return TEXT("_Depth.exr");
	case EStitchImageType::InvDepth: return TEXT("_InvDepth.exr");
	default:                         return TEXT(".exr");
	}
}

/**
 * @class ImageStitcher
 * @brief A class for stitching images from multiple cameras into a unified image.
 * 
 * This class provides functionality to load, process, and stitch images, as well as save the resulting images.
 */
class OMNIRGBDCAMERA_API ImageStitcher
{
public:
	
	// ------------------------------------------ Constructor / Destructor ------------------------------------------ //
	// -------------------------------------------------------------------------------------------------------------- //
	
	/**
	 * @brief Constructs an ImageStitcher object.
	 */
	ImageStitcher();

	/**
	 * @brief Constructs an ImageStitcher object with the given camera names and folder names.
	 * 
	 * @param Cameras Array of camera names.
	 * @param NameFolderImage Name of the folder containing the color images.
	 * @param NameFolderImageDepth Name of the folder containing the depth images.
	 */
	ImageStitcher(const TArray<FString>& Cameras, const FString& NameFolderImage, const FString& NameFolderImageDepth);

	/**
	 * @brief Destructs the ImageStitcher object.
	 */
	virtual ~ImageStitcher();

	// ------------------------------------------------ File Handling ----------------------------------------------- //
	// -------------------------------------------------------------------------------------------------------------- //

	/**
	 * @brief Validates input and output paths.
	 * 
	 * @return True if paths are valid, false otherwise.
	 */
	virtual bool ValidateInputOutput() const;

	/**
	 * @brief Collects image paths from a directory.
	 * 
	 * @param DirectoryPath Path to the directory containing images.
	 * @param ImagePaths Output array to store collected image paths.
	 */
	virtual void CollectImagePaths(const FString& DirectoryPath, TArray<FString>& ImagePaths);
	
	/**
	 * @brief Loads images from given paths.
	 * 
	 * @param ImagePaths Array of image file paths.
	 * @param Images Output vector to store loaded images.
	 */
	virtual void LoadImages(const TArray<FString>& ImagePaths, std::vector<cv::Mat>& Images) const;
	
	/**
	 * @brief Updates the input path for the stitching process.
	 * 
	 * @param Path The new input path.
	 * @param Type Type of the input data (e.g., equirectangular (0) or fisheye (1)).
	 */
	virtual void UpdateStitchingInputPath(const FString& Path, const int Type);

	/**
	 * @brief Updates the output path for the stitching process.
	 * 
	 * @param Path The new output path.
	 */
	virtual void UpdateStitchingOutputPath(const FString& Path);

	/**
	 * @brief Updates the output directory for the stitching process.
	 * 
	 * @param Directory The new output directory.
	 */
	virtual void UpdateStitchingOutputDirectory(const FString& Directory);

	/**
	 * @brief Gets the name of the color image folder.
	 * @return The folder name (e.g., "FinalImage")
	 */
	const FString& GetImageFolderName() const { return NameFolderImage; }

	/**
	 * @brief Gets the name of the depth image folder.
	 * @return The folder name (e.g., "FinalImageDepth")
	 */
	const FString& GetDepthFolderName() const { return NameFolderDepthImage; }

	/**
	 * @brief Updates the frame range for the stitching process.
	 *
	 * @param FrameMinSet The minimum frame number as a string.
	 * @param FrameMaxSet The maximum frame number as a string.
	 */
	virtual void UpdateFrames(const FString& FrameMinSet, const FString& FrameMaxSet);
	
	/**
	 * @brief Saves the stitched image to the specified output path.
	 * 
	 * @param Image The stitched image to be saved.
	 * @param Index The index of the current frame.
	 * @param OutputName The name of the output file.
	 */
	virtual void SaveStitchedImage(const cv::Mat& Image, const FString& Index, const FString& OutputName);
	
	// -------------------------------------------------- Stitching ------------------------------------------------- //
	// -------------------------------------------------------------------------------------------------------------- //

	/**
	 * @brief Converts the cubemap to a corresponding projection using the lookup table.
	 * 
	 * @param CubeMap The input cubemap.
	 * @param LookUpX The lookup table for the x-coordinates.
	 * @param LookUpY The lookup table for the y-coordinates.
	 * @return The remapped image.
	 */
	virtual cv::Mat CubeMapToImage(const cv::Mat& CubeMap, const cv::Mat& LookUpX, const cv::Mat& LookUpY);

	/**
	 * @brief Calculates Cartesian coordinates from spherical coordinates. (Physical Convention)
	 * 
	 * @param Theta The angle with respect to positive polar axis (Polar Angle)
	 * @param Phi The (angle of rotation from the initial meridian plane (Azumithal Angle)
	 * @param PX The x-coordinate (output).
	 * @param PY The y-coordinate (output).
	 * @param PZ The z-coordinate (output).
	 */
	virtual void CalculateCartesianCoordinates(const double Theta, const double Phi, double& PX, double& PY, double& PZ);
	
	// ---------------------------------------------------- Getter -------------------------------------------------- //
	// -------------------------------------------------------------------------------------------------------------- //
	
	/**
	* @brief Gets the size of the images in the specified directory.
	 * 
	 * @param Path The path to the directory.
	 * @param Type Type of the input data (e.g., equirectangular or fisheye).
	 */
	void GetImageSize(const FString& Path, const int Type);
	
	// Accessors for image properties
	int GetWidthValue() const { return this->ImageWidth; }
	int GetHeightValue() const { return this->ImageHeight; }
	int GetOpticalCenterX() const { return this->OpticalCenterX; }
	int GetOpticalCenterY() const { return this->OpticalCenterY; }
	float GetFocalLengthX() const { return this->FocalLengthX; }
	float GetFocalLengthY() const { return this->FocalLengthY; }
	
	// -------------------------------------------------- Variables ------------------------------------------------- //
	// -------------------------------------------------------------------------------------------------------------- //

	// Bool values used for activating Start Stitching Buttons
	bool bInputDirectoryIsReady = false;
	bool bOutputDirectoryIsReady = false;

	// What type of image to stitch
	bool bStitchColorImages = true;

	// Used to save the desired image size. During Input of Path, set to size without rescale
	int ImageWidth = 0;
	int ImageHeight = 0;

	// Flag indicating whether the Lookup Table has been generated. Only need one for current Stitching
	bool dGeneratedLoopUpTable = false;
	// Lookup table for the x and y coordinates.
	cv::Mat LookUpTableX; 
	cv::Mat LookUpTableY;
	
	// Optical Center used for Fisheye Stitching process. During Input of Path, set to size / 2
	int OpticalCenterX = 0;
	int OpticalCenterY = 0;

	// Focal Length in pixels
	float FocalLengthX = StitchingConstants::DefaultFocalLength;
	float FocalLengthY = StitchingConstants::DefaultFocalLength;

protected:
	// Names of the cameras used for stitching.
	TArray<FString> CameraNames;
	// Name of the folder containing the color images. Should be "FinalImage"
	FString NameFolderImage;
	// Name of the folder containing the depth images. Should be "FinalImageDepth_Metric_Inst"
	FString NameFolderDepthImage;
	
	// Input and Output Path for stitching.
	FString InputPath;
	FString OutputPath;
	// Output Directory used in combination with Output Path
	FString OutputDirectory = "Output";
	// Frame sequence for Stitching process. Indicates which folders to process
	FString FrameMin = "0000";
	FString FrameMax = "0000";

	// Calibration JSON File Name (Basalt-compatible format with extrinsics + intrinsics)
	FString CalibrationFile = "calibration.json";

	// Source calibration file path (set when loading a Basalt calibration file)
	// After stitching, this file will be copied to the output folder
	FString SourceCalibrationPath;

	// Folder Names for specific camera types
	FString EquirectangularDirectory = "Equirectangular";
	FString FisheyeDirectory = "FisheyeCamera";

	FString ImageExtension = "exr";
};
