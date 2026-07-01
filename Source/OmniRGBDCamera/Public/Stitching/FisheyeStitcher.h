#pragma once

#include "CoreMinimal.h"
#include "Stitching/ImageStitcher.h"
#include "Stitching/StitchingConstants.h"
#include "Stitching/CubeMap/CubeMap.h"
#include "Utils/Depth.h"
#include "Utils/BasaltHelper.h"
#include "Misc/TVariant.h"
#include <atomic>

/**
 * @enum FisheyeProjection
 * @brief Defines the different types of fisheye projections supported.
 */
enum class FisheyeProjection {
	Equidistant,
	Equisolid,
	Orthographic,
	Stereographic,
	KannalaBrandt,
	ExtendedUnifiedCameraModel
};

// ============================================================================================================== //
// Helper functions for Basalt Intrinsics
// ============================================================================================================== //

/** Converts projection type string to FisheyeProjection enum */
inline FisheyeProjection GetProjectionEnumFromString(const FString& ProjectionType)
{
	const FString Lower = ProjectionType.ToLower();
	if (Lower == TEXT("equisolid")) return FisheyeProjection::Equisolid;
	if (Lower == TEXT("orthographic")) return FisheyeProjection::Orthographic;
	if (Lower == TEXT("stereographic")) return FisheyeProjection::Stereographic;
	if (Lower == TEXT("kb4") || Lower == TEXT("kannala brandt")) return FisheyeProjection::KannalaBrandt;
	if (Lower == TEXT("eucm") || Lower == TEXT("extended unified camera model")) return FisheyeProjection::ExtendedUnifiedCameraModel;
	return FisheyeProjection::Equidistant; // Default
}

/** Returns display name for a projection type */
inline FString GetProjectionDisplayName(const FString& ProjectionType)
{
	const FString Lower = ProjectionType.ToLower();
	if (Lower == TEXT("equisolid")) return TEXT("Equisolid");
	if (Lower == TEXT("orthographic")) return TEXT("Orthographic");
	if (Lower == TEXT("stereographic")) return TEXT("Stereographic");
	if (Lower == TEXT("kb4") || Lower == TEXT("kannala brandt")) return TEXT("Kannala-Brandt");
	if (Lower == TEXT("eucm") || Lower == TEXT("extended unified camera model")) return TEXT("EUCM");
	return TEXT("Equidistant");
}

/**
 * @struct FCameraLookupTable
 * @brief Holds lookup tables for a specific camera configuration
 */
struct FCameraLookupTable
{
	cv::Mat LookUpTableX;
	cv::Mat LookUpTableY;
	cv::Mat Vignette;
	cv::Mat ValidMask;  // Binary mask: 255 = valid pixel, 0 = outside fisheye circle
	bool bGenerated = false;

	void Release()
	{
		LookUpTableX.release();
		LookUpTableY.release();
		Vignette.release();
		ValidMask.release();
		bGenerated = false;
	}
};

/**
 * @class FisheyeStitcher
 * @brief A class for stitching fisheye images from 5 sided CubeMap projections.
 *
 * This class extends the ImageStitcher to handle various fisheye projections
 * and create stitched from a CubeMap projection.
 *
 * Supports two modes:
 * - Single Camera Mode: One set of parameters for all cameras
 * - Multi Camera Mode: Different parameters per camera (e.g., from Basalt calibration)
 */
class OMNIRGBDCAMERA_API FisheyeStitcher : public ImageStitcher
{
public:

	// ------------------------------------------ Constructor / Destructor ------------------------------------------ //
	// -------------------------------------------------------------------------------------------------------------- //

	FisheyeStitcher();

	FisheyeStitcher(const TArray<FString>& Cameras, const FString& NameFolderImage, const FString& NameFolderImageDepth);

	~FisheyeStitcher();

	// -------------------------------------------- Intrinsics Setters --------------------------------------------- //
	// -------------------------------------------------------------------------------------------------------------- //

	/**
	 * @brief Set ideal fisheye intrinsics for single camera mode (applies to all cameras)
	 * @param Intrinsics The ideal fisheye intrinsics
	 */
	void SetIdealFisheyeIntrinsics(const FBasaltIdealFisheyeIntrinsics& Intrinsics);

	/**
	 * @brief Set ideal fisheye intrinsics for multi-camera mode
	 * @param IntrinsicsArray Array of intrinsics, indexed by camera number (1-based)
	 */
	void SetIdealFisheyeIntrinsicsArray(const TArray<FBasaltIdealFisheyeIntrinsics>& IntrinsicsArray);

	/**
	 * @brief Set KB4 intrinsics for single camera mode
	 * @param Intrinsics The KB4 intrinsics
	 */
	void SetKB4Intrinsics(const FBasaltKB4Intrinsics& Intrinsics);

	/**
	 * @brief Set KB4 intrinsics for multi-camera mode
	 * @param IntrinsicsArray Array of intrinsics, indexed by camera number (1-based)
	 */
	void SetKB4IntrinsicsArray(const TArray<FBasaltKB4Intrinsics>& IntrinsicsArray);

	/**
	 * @brief Set EUCM intrinsics for single camera mode
	 * @param Intrinsics The EUCM intrinsics
	 */
	void SetEUCMIntrinsics(const FBasaltEUCMIntrinsics& Intrinsics);

	/**
	 * @brief Set EUCM intrinsics for multi-camera mode
	 * @param IntrinsicsArray Array of intrinsics, indexed by camera number (1-based)
	 */
	void SetEUCMIntrinsicsArray(const TArray<FBasaltEUCMIntrinsics>& IntrinsicsArray);

	/**
	 * @brief Clear all multi-camera intrinsics and reset to single camera mode
	 */
	void ClearMultiCameraIntrinsics();

	/**
	 * @brief Invalidates all lookup tables, forcing regeneration on next use.
	 * Call this when vignette or other settings change.
	 */
	void InvalidateLookupTables();

	/**
	 * @brief Sets the source calibration file path.
	 * After stitching, this file will be copied to the output folder.
	 * @param InPath Full path to the source calibration JSON file
	 */
	void SetSourceCalibrationPath(const FString& InPath) { SourceCalibrationPath = InPath; }

	// -------------------------------------------------- Stitching ------------------------------------------------- //
	// -------------------------------------------------------------------------------------------------------------- //

	/** Process all directories containing images to be stitched */
	void ProcessDirectories(TArray<int32> ActiveCameras);

	/**
	 * @brief Pre-generates lookup tables for all specified cameras.
	 * Call this BEFORE ProcessDirectories to avoid race conditions during parallel stitching.
	 * LUTs are generated sequentially on the calling thread.
	 *
	 * @param ActiveCameras List of camera indices to pre-generate LUTs for
	 * @param SampleImagePath Path to a sample image directory (needed to determine atlas size)
	 * @return True if all LUTs were generated successfully
	 */
	bool PreGenerateLookupTables(const TArray<int32>& ActiveCameras, const FString& SampleImagePath);

	/**
	 * @brief Stitches images from a single directory (for preview/single frame processing)
	 * @param DirectoryPath Path to the directory containing the 5 cubemap images
	 * @param CameraIndex The camera index (1-based) for multi-camera mode
	 * @param bIsDepth Whether processing depth images
	 * @param bIsInvDepth Whether processing inverse depth images
	 * @return Stitched image as cv::Mat, empty on failure
	 */
	cv::Mat StitchSingleFrame(const FString& DirectoryPath, int32 CameraIndex = 1, bool bIsDepth = false, bool bIsInvDepth = false);

	// -------------------------------------------------- Vignette -------------------------------------------------- //
	// -------------------------------------------------------------------------------------------------------------- //

	void ExportVignette();

	// ---------------------------------------------------- Masks --------------------------------------------------- //
	// -------------------------------------------------------------------------------------------------------------- //

	/**
	 * @brief Exports valid region masks for all processed cameras.
	 * Masks are saved as PNG files (255 = valid pixel, 0 = outside fisheye circle)
	 * @param ActiveCameras List of camera indices to export masks for
	 */
	void ExportValidMasks(const TArray<int32>& ActiveCameras);

	// ------------------------------------ Public Members ------------------------------------- //

	bool bStitchDepthImages = false;
	bool bStitchInvDepthImages = false;

	// Vignette for shadow effect on fisheye image
	bool bApplyVignette = false;
	float VignetteStrength = 0.0;

	/** Returns true if multi-camera mode is active */
	bool IsMultiCameraMode() const { return bMultiCameraMode; }

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

	// ------------------------------------ Private Members ------------------------------------- //

	// Field of view in degrees (single camera mode)
	int FOV = StitchingConstants::DefaultFisheyeFOV;

	// Current fisheye projection type
	FisheyeProjection CurrentProjection = FisheyeProjection::Equidistant;

	// String representation of the current fisheye projection
	FString FisheyeProjectionString = "Equidistant";

	// Coefficients for Kannala-Brandt projection model (single camera mode)
	double K[4] = {
		StitchingConstants::DefaultKB4Coefficients[0],
		StitchingConstants::DefaultKB4Coefficients[1],
		StitchingConstants::DefaultKB4Coefficients[2],
		StitchingConstants::DefaultKB4Coefficients[3]
	};

	// Alpha/Beta parameter for Extended Unified Camera Model (single camera mode)
	float Alpha = StitchingConstants::DefaultEUCMAlpha;
	float Beta = StitchingConstants::DefaultEUCMBeta;

	// --------------------------------- Multi-Camera Mode --------------------------------- //

	/** Flag indicating multi-camera mode (different intrinsics per camera) */
	bool bMultiCameraMode = false;

	/** Variant type holding any supported intrinsics type */
	using FCameraIntrinsics = TVariant<FBasaltIdealFisheyeIntrinsics, FBasaltKB4Intrinsics, FBasaltEUCMIntrinsics>;

	/** Intrinsics per camera (key = camera number 1-based) */
	TMap<int32, FCameraIntrinsics> IntrinsicsMap;

	/** Lookup tables per camera (key = camera number 1-based) */
	TMap<int32, FCameraLookupTable> CameraLUTs;

	/** Single camera LUT (used when not in multi-camera mode) */
	FCameraLookupTable SingleCameraLUT;

	// -------------------------------------------------- Stitching ------------------------------------------------- //
	// -------------------------------------------------------------------------------------------------------------- //

	/**
	 * @brief Stitches the images in the given directory
	 * @param DirectoryPath Path to the directory containing images
	 * @param CameraIndex The camera index (1-based) for multi-camera mode
	 * @param bIsDepth Whether processing depth images
	 * @param bIsInvDepth Whether processing inverse depth images
	 * @return Stitched image as cv::Mat
	 */
	cv::Mat StitchImagesInDirectory(const FString& DirectoryPath, int32 CameraIndex, bool bIsDepth, bool bIsInvDepth);

	/**
	 * @brief Generates a lookup table for the given camera
	 * @param CubeMapAtlas Input cubemap atlas (for size reference)
	 * @param CameraIndex Camera index (1-based), 0 for single camera mode
	 */
	void GenerateLookupTable(const cv::Mat& CubeMapAtlas, int32 CameraIndex);

	/**
	 * @brief Gets the lookup table for a camera, generating if needed
	 * @param CameraIndex Camera index (1-based)
	 * @param CubeMapAtlas Reference atlas for size
	 * @return Reference to the camera's lookup table
	 */
	FCameraLookupTable& GetOrCreateLUT(int32 CameraIndex, const cv::Mat& CubeMapAtlas);

	/**
	 * @brief Applies current parameters for the given camera index
	 * Sets internal state (FOV, OpticalCenter, Projection, etc.) based on camera index
	 */
	void ApplyParamsForCamera(int32 CameraIndex);

	/**
	 * @brief Sets the fisheye projection type and updates the projection string
	 * @param Projection The projection type to set
	 */
	void SetFisheyeProjection(FisheyeProjection Projection);

	/**
	 * @brief Processes a single pixel with offset scaling for non-square outputs.
	 * The circle radius determines the R <= 1.0 boundary check.
	 * The offset scale factor scales pixel offsets before passing to camera models (EUCM/KB4).
	 */
	void ProcessPixelScaled(int x, int y, float CircleRadius, float OffsetScaleFactor, const cv::Point2f& Center, float FovRad,
		int FaceWidth, int FaceHeight, const cv::Mat& CubeMapAtlas, cv::Mat& OutLUTX, cv::Mat& OutLUTY, cv::Mat& OutValidMask);

	/**
	 * @brief Calculates the theta angle for the fisheye projection
	 * Used by Equidistant, Equisolid, Orthographic and Stereographic
	 */
	double CalculateTheta(float R, float FovRad);

	/**
	 * @brief Transforms fisheye coordinate system to CubeMap coordinate system.
	 *
	 * FisheyeStitcher uses: Z+ = Forward, Y- = Up, X+ = Right
	 * CubeMap class uses:   Y+ = Forward, Z+ = Up, X- = Right
	 *
	 * @param InX, InY, InZ Input coordinates in fisheye coordinate system
	 * @param OutX, OutY, OutZ Output coordinates in CubeMap coordinate system
	 */
	static void TransformToCubeMapCoords(double InX, double InY, double InZ,
										 double& OutX, double& OutY, double& OutZ);

	// ---------------------------------- Kannala Brandt ---------------------------------- //

	void CalculateCartesianCoordinatesKB(const float DX, const float DY, double& PX, double& PY, double& PZ);

	// --------------------------- Extended unified camera Model -------------------------- //

	void CalculateCartesianCoordinatesEucm(const float DX, const float DY, double& PX, double& PY, double& PZ);

	// -------------------------------------------------- Vignette -------------------------------------------------- //
	// -------------------------------------------------------------------------------------------------------------- //

	cv::Mat CreateVignetteMask(int Width, int Height, double Strength, int32 CameraIndex);

	cv::Mat ApplyVignette(const cv::Mat& StitchedImage, const cv::Mat& VignetteMask);

	// ----------------------------------------------- File Handling ------------------------------------------------ //
	// -------------------------------------------------------------------------------------------------------------- //

	void ProcessSubdirectory(const FString& Subdirectory, TArray<int32> ActiveCameras);

	/**
	 * @brief Processes a single camera folder for stitching
	 * @param CameraFolder Camera folder name (e.g., "FisheyeCamera_1")
	 * @param FolderPathBase Base path containing the camera folder
	 * @param Subdirectory Frame subdirectory name for output
	 * @param CameraNumber Camera number for LUT selection
	 * @param ImageType Type of image (Color, Depth, InvDepth)
	 */
	void ProcessImageFolder(const FString& CameraFolder,
							const FString& FolderPathBase,
							const FString& Subdirectory,
							int32 CameraNumber,
							EStitchImageType ImageType);

	// ------------------------------------------------ Calibration ------------------------------------------------- //
	// -------------------------------------------------------------------------------------------------------------- //

	void ExportCameraIntrinsicToJson(TArray<int32> ActiveCameras);

	// --------------------------------------------------- Cleanup -------------------------------------------------- //
	// -------------------------------------------------------------------------------------------------------------- //

	/** Releases all lookup tables */
	void ReleaseLookupTables();
};
