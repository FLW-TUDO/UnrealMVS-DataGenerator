#pragma once

#include "CoreMinimal.h"
#include "Blueprint/StitcherBPBase.h"
#include "Engine/Texture2D.h"
#include "Stitching/FisheyeStitcher.h"
#include "Utils/BasaltHelper.h"

#include "FisheyeStitcherBP.generated.h"

class FisheyeStitcher;

// ============================================================================================================== //
// All intrinsics types are now unified under Basalt Intrinsics (defined in BasaltHelper.h):
// - FBasaltIdealFisheyeIntrinsics (equidistant, equisolid, orthographic, stereographic)
// - FBasaltKB4Intrinsics (Kannala-Brandt)
// - FBasaltEUCMIntrinsics (Extended Unified Camera Model)
// ============================================================================================================== //

UCLASS(BlueprintType)
class OMNIRGBDCAMERA_API UFisheyeStitcherBP : public UStitcherBPBase
{
	GENERATED_BODY()

public:
	// --------------------------------- Initialization --------------------------------- //

	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye")
	bool Initialize(const TArray<FString>& Cameras,
					const FString& NameFolderImage,
					const FString& NameFolderDepthImage);

	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye")
	bool ApplySettings(const FString& InInputPath,
					   const FString& InOutputPath,
					   const FString& InOutputDirectory,
					   const FString& InFrameMin,
					   const FString& InFrameMax,
					   bool bInStitchColorImages,
					   bool bInStitchDepthImages,
					   bool bInStitchInvDepthImages);

	// --------------------------------- Resolution --------------------------------- //

	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye|Resolution")
	void SetOutputResolution(int32 InWidth, int32 InHeight);

	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye|Resolution")
	void SetOutputWidth(int32 InWidth);

	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye|Resolution")
	void SetOutputHeight(int32 InHeight);

	// --------------------------------- Ideal Fisheye Intrinsics --------------------------------- //

	/** Set ideal fisheye intrinsics for single camera mode */
	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye|Ideal")
	void SetIdealFisheyeIntrinsics(const FBasaltIdealFisheyeIntrinsics& Intrinsics);

	/** Set ideal fisheye intrinsics for multi-camera mode (array indexed by camera number) */
	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye|Ideal|MultiCamera")
	void SetIdealFisheyeIntrinsicsArray(const TArray<FBasaltIdealFisheyeIntrinsics>& IntrinsicsArray);

	// --------------------------------- KB4 Intrinsics --------------------------------- //

	/** Set KB4 intrinsics for single camera mode */
	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye|KB4")
	void SetKB4Intrinsics(const FBasaltKB4Intrinsics& Intrinsics);

	/** Set KB4 intrinsics for multi-camera mode */
	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye|KB4|MultiCamera")
	void SetKB4IntrinsicsArray(const TArray<FBasaltKB4Intrinsics>& IntrinsicsArray);

	// --------------------------------- EUCM Intrinsics --------------------------------- //

	/** Set EUCM intrinsics for single camera mode */
	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye|EUCM")
	void SetEUCMIntrinsics(const FBasaltEUCMIntrinsics& Intrinsics);

	/** Set EUCM intrinsics for multi-camera mode */
	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye|EUCM|MultiCamera")
	void SetEUCMIntrinsicsArray(const TArray<FBasaltEUCMIntrinsics>& IntrinsicsArray);

	// --------------------------------- Multi-Camera Mode --------------------------------- //

	/** Clear all multi-camera intrinsics and reset to single camera mode */
	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye|MultiCamera")
	void ClearMultiCameraIntrinsics();

	// --------------------------------- Calibration --------------------------------- //

	/**
	 * Sets the source calibration file path.
	 * After stitching, this file will be copied to the output folder.
	 * If not set, uses the default MovieRenders/calibration.json path.
	 *
	 * @param InPath Full path to the source calibration JSON file
	 */
	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye|Calibration")
	void SetSourceCalibrationPath(const FString& InPath);

	/** Returns true if multi-camera mode is active */
	UFUNCTION(BlueprintPure, Category="Stitching|Fisheye|MultiCamera")
	bool IsMultiCameraMode() const;

	// --------------------------------- Auto-Detection --------------------------------- //

	/**
	 * Scans the input path for an image and calculates optimal default parameters.
	 * Use this after setting the input path to get recommended center and focal length values.
	 *
	 * @param InputPath The path containing rendered images (e.g., frame directories)
	 * @param OutImageWidth The detected image width
	 * @param OutImageHeight The detected image height
	 * @param OutCenterX Recommended optical center X (ImageWidth / 2)
	 * @param OutCenterY Recommended optical center Y (ImageHeight / 2)
	 * @param OutFocalLength Recommended focal length based on image size
	 * @return True if an image was found and parameters were calculated
	 */
	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye|AutoDetect")
	static bool DetectImageParametersFromPath(
		const FString& InputPath,
		int32& OutImageWidth,
		int32& OutImageHeight,
		int32& OutCenterX,
		int32& OutCenterY,
		float& OutFocalLength
	);

	/**
	 * Calculates recommended focal length for a given image size and FOV.
	 * Formula: f = (ImageWidth / 2) / tan(FOV_rad / 2) for perspective
	 * For equidistant fisheye: f = (ImageWidth / 2) / (FOV_rad / 2)
	 *
	 * @param ImageWidth The image width in pixels
	 * @param FOVDegrees The field of view in degrees
	 * @param bEquidistant If true, uses equidistant formula; if false, uses perspective formula
	 * @return Recommended focal length in pixels
	 */
	UFUNCTION(BlueprintPure, Category="Stitching|Fisheye|AutoDetect")
	static float CalculateFocalLengthFromFOV(int32 ImageWidth, float FOVDegrees, bool bEquidistant = true);

	// --------------------------------- Path Helpers --------------------------------- //

	/**
	 * Returns the OmniRGBDCamera plugin's base directory.
	 * Useful for accessing sample data shipped with the plugin.
	 * @return Full path to the plugin folder (e.g., "C:/Project/Plugins/UE5_Plugin")
	 */
	UFUNCTION(BlueprintPure, Category="Stitching|Paths")
	static FString GetPluginDirectory();

	/**
	 * Returns the OmniRGBDCamera plugin's Resources directory.
	 * @return Full path to the plugin's Resources folder
	 */
	UFUNCTION(BlueprintPure, Category="Stitching|Paths")
	static FString GetPluginResourcesDirectory();

	// --------------------------------- Vignette --------------------------------- //
	// IMPORTANT: Set vignette settings BEFORE calling ProcessDirectories or GeneratePreview!
	// The vignette mask is created during LUT generation, so settings must be applied first.

	/** Enable/disable vignette effect. MUST be called BEFORE ProcessDirectories/GeneratePreview! */
	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye|Vignette")
	void SetApplyVignette(bool bApply);

	/** Set vignette strength (higher = darker edges). Typical range: 0.5 - 3.0. MUST be called BEFORE ProcessDirectories/GeneratePreview! */
	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye|Vignette")
	void SetVignetteStrength(float Strength);

	/** Export vignette mask to output directory */
	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye|Vignette")
	void ExportVignette();

	// --------------------------------- Preview --------------------------------- //

	/**
	 * Generates a preview texture by stitching a single frame with current parameters.
	 * Use this to preview the result before processing all frames.
	 *
	 * @param SampleDirectoryPath Path to a single frame directory (e.g., "C:/renders/0001")
	 * @param CameraIndex Camera index to preview (1-based, use 1 for single camera mode)
	 * @return UTexture2D that can be displayed in UI widgets, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye|Preview")
	UTexture2D* GeneratePreview(const FString& SampleDirectoryPath, int32 CameraIndex = 1);

	/**
	 * Generates a preview texture with a specific output size (for faster preview).
	 *
	 * @param SampleDirectoryPath Path to a single frame directory
	 * @param CameraIndex Camera index to preview (1-based)
	 * @param PreviewWidth Desired preview width (0 = use full output width)
	 * @param PreviewHeight Desired preview height (0 = use full output height)
	 * @return UTexture2D that can be displayed in UI widgets, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye|Preview")
	UTexture2D* GeneratePreviewWithSize(const FString& SampleDirectoryPath, int32 CameraIndex, int32 PreviewWidth, int32 PreviewHeight);

	// --------------------------------- Processing --------------------------------- //

	/**
	 * Starts stitching asynchronously - returns immediately, UI stays responsive.
	 * Fires OnStitchingProgress during processing and OnStitchingComplete when done.
	 * Check bIsStitching to see if currently processing.
	 */
	UFUNCTION(BlueprintCallable, Category="Stitching|Fisheye")
	void ProcessDirectories(TArray<int32> CameraArray);

protected:
	// UStitcherBPBase interface
	virtual bool GetStitcherProgress(int32& OutCurrent, int32& OutTotal) const override;
	virtual FString GetLogPrefix() const override { return TEXT("[FisheyeStitcherBP]"); }

private:
	TUniquePtr<FisheyeStitcher> Stitcher;

	/** Converts cv::Mat (BGR/BGRA float or 8-bit) to UTexture2D */
	static UTexture2D* MatToTexture2D(const cv::Mat& InMat);
};
