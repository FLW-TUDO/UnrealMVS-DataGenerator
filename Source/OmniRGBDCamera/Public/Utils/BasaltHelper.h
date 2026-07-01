#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BasaltHelper.generated.h"

class UEditableTextBox;

/**
 * @brief Kannala-Brandt camera intrinsics (KB4 model)
 * Parameters: fx, fy, cx, cy, k1, k2, k3, k4
 */
USTRUCT(BlueprintType)
struct FBasaltKB4Intrinsics
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Fx = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Fy = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Cx = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Cy = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float K1 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float K2 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float K3 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float K4 = 0.0f;
};

/**
 * @brief Extended Unified Camera Model intrinsics (EUCM)
 * Parameters: fx, fy, cx, cy, alpha, beta
 */
USTRUCT(BlueprintType)
struct FBasaltEUCMIntrinsics
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Fx = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Fy = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Cx = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Cy = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Alpha = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Beta = 0.0f;
};

/**
 * @brief Double Sphere camera intrinsics
 * Parameters: fx, fy, cx, cy, xi, alpha
 */
USTRUCT(BlueprintType)
struct FBasaltDoubleSphereIntrinsics
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Fx = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Fy = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Cx = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Cy = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Xi = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Alpha = 0.0f;
};

/**
 * @brief Pinhole camera intrinsics
 * Parameters: fx, fy, cx, cy
 */
USTRUCT(BlueprintType)
struct FBasaltPinholeIntrinsics
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Fx = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Fy = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Cx = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Cy = 0.0f;
};

/**
 * @brief Ideal fisheye camera intrinsics
 * For ideal models: equidistant, equisolid, orthographic, stereographic
 * Parameters: projection_type, fov, cx, cy
 */
USTRUCT(BlueprintType)
struct FBasaltIdealFisheyeIntrinsics
{
	GENERATED_BODY()

	/** Projection type: "equidistant", "equisolid", "orthographic", "stereographic" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	FString ProjectionType = TEXT("equidistant");

	/** Field of view in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	int32 FOV = 120;

	/** Optical center X (relative to image center, use 0 for centered) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Cx = 0.0f;

	/** Optical center Y (relative to image center, use 0 for centered) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Intrinsics")
	float Cy = 0.0f;
};

/**
 * @brief Camera resolution
 */
USTRUCT(BlueprintType)
struct FBasaltResolution
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Resolution")
	int32 Width = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basalt|Resolution")
	int32 Height = 0;
};

/**
 * @brief Complete camera calibration data for a single camera
 */
USTRUCT(BlueprintType)
struct FBasaltCameraCalibration
{
	GENERATED_BODY()

	/** Camera type string (kb4, eucm, ds, pinhole, etc.) */
	UPROPERTY(BlueprintReadOnly, Category="Basalt|Calibration")
	FString CameraType;

	/** Camera transform (extrinsic) in Unreal coordinate system */
	UPROPERTY(BlueprintReadOnly, Category="Basalt|Calibration")
	FTransform Transform;

	/** Camera resolution */
	UPROPERTY(BlueprintReadOnly, Category="Basalt|Calibration")
	FBasaltResolution Resolution;

	/** KB4 intrinsics (only valid if CameraType == "kb4") */
	UPROPERTY(BlueprintReadOnly, Category="Basalt|Calibration")
	FBasaltKB4Intrinsics KB4Intrinsics;

	/** EUCM intrinsics (only valid if CameraType == "eucm") */
	UPROPERTY(BlueprintReadOnly, Category="Basalt|Calibration")
	FBasaltEUCMIntrinsics EUCMIntrinsics;

	/** Double Sphere intrinsics (only valid if CameraType == "ds") */
	UPROPERTY(BlueprintReadOnly, Category="Basalt|Calibration")
	FBasaltDoubleSphereIntrinsics DoubleSphereIntrinsics;

	/** Pinhole intrinsics (only valid if CameraType == "pinhole") */
	UPROPERTY(BlueprintReadOnly, Category="Basalt|Calibration")
	FBasaltPinholeIntrinsics PinholeIntrinsics;

	/** Ideal fisheye intrinsics (only valid if CameraType == "equidistant", "equisolid", "orthographic", "stereographic") */
	UPROPERTY(BlueprintReadOnly, Category="Basalt|Calibration")
	FBasaltIdealFisheyeIntrinsics IdealFisheyeIntrinsics;
};

/**
 * @brief Blueprint function library for reading Basalt calibration files
 *
 * Uses Basalt's native Cereal serialization to parse calibration JSON files.
 * Supports KB4, EUCM, Double Sphere, and Pinhole camera models.
 */
UCLASS()
class OMNIRGBDCAMERA_API UBasaltCalibrationReader : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ---------------------------------------- File Dialog ---------------------------------------- //

	/**
	 * Opens a file dialog to select a JSON calibration file
	 * @param DialogTitle Title of the dialog window
	 * @param DefaultPath Default path to open
	 * @param TargetTextBox Optional text box to update with selected path
	 * @param OutFilePath The selected file path
	 * @return True if a file was selected
	 */
	UFUNCTION(BlueprintCallable, Category="Basalt|FileIO")
	static bool OpenCalibrationFileDialog(
		const FString& DialogTitle,
		const FString& DefaultPath,
		UEditableTextBox* TargetTextBox,
		FString& OutFilePath
	);

	// ---------------------------------------- Load Calibration ---------------------------------------- //

	/**
	 * Loads a complete Basalt calibration file and returns all camera data
	 * @param JsonFilePath Path to the calibration JSON file
	 * @param bConvertToUnrealCoordinates If true, converts OpenCV coordinates to Unreal
	 * @param bMetersToCentimeters If true, converts positions from meters to centimeters
	 * @param OutCameras Array of camera calibrations
	 * @return True if loading was successful
	 */
	UFUNCTION(BlueprintCallable, Category="Basalt|Calibration")
	static bool LoadCalibration(
		const FString& JsonFilePath,
		bool bConvertToUnrealCoordinates,
		bool bMetersToCentimeters,
		TArray<FBasaltCameraCalibration>& OutCameras
	);

	// ---------------------------------------- Camera Count ---------------------------------------- //

	/**
	 * Returns the number of cameras in a calibration file
	 * @param JsonFilePath Path to the calibration JSON file
	 * @param OutCameraCount Number of cameras found
	 * @return True if file was read successfully
	 */
	UFUNCTION(BlueprintCallable, Category="Basalt|Calibration")
	static bool GetCameraCount(
		const FString& JsonFilePath,
		int32& OutCameraCount
	);

	// ---------------------------------------- Camera Type ---------------------------------------- //

	/**
	 * Returns the camera type for a specific camera index
	 * @param JsonFilePath Path to the calibration JSON file
	 * @param CameraIndex Index of the camera (0-based)
	 * @param OutCameraType Camera type string (kb4, eucm, ds, pinhole)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category="Basalt|Calibration")
	static bool GetCameraType(
		const FString& JsonFilePath,
		int32 CameraIndex,
		FString& OutCameraType
	);

	// ---------------------------------------- Transforms ---------------------------------------- //

	/**
	 * Returns the transforms for all cameras
	 * @param JsonFilePath Path to the calibration JSON file
	 * @param bConvertToUnrealCoordinates If true, converts OpenCV to Unreal coordinates
	 * @param bMetersToCentimeters If true, converts meters to centimeters
	 * @param bCenterTransforms If true, centers all transforms around their centroid
	 * @param OutTransforms Array of camera transforms
	 * @param OutCenter The centroid of all camera positions
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category="Basalt|Calibration")
	static bool GetCameraTransforms(
		const FString& JsonFilePath,
		bool bConvertToUnrealCoordinates,
		bool bMetersToCentimeters,
		bool bCenterTransforms,
		TArray<FTransform>& OutTransforms,
		FVector& OutCenter
	);

	/**
	 * Returns the transform for a specific camera
	 * @param JsonFilePath Path to the calibration JSON file
	 * @param CameraIndex Index of the camera (0-based)
	 * @param bConvertToUnrealCoordinates If true, converts OpenCV to Unreal coordinates
	 * @param bMetersToCentimeters If true, converts meters to centimeters
	 * @param OutTransform The camera transform
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category="Basalt|Calibration")
	static bool GetCameraTransform(
		const FString& JsonFilePath,
		int32 CameraIndex,
		bool bConvertToUnrealCoordinates,
		bool bMetersToCentimeters,
		FTransform& OutTransform
	);

	// ---------------------------------------- Resolutions ---------------------------------------- //

	/**
	 * Returns the resolutions for all cameras
	 * @param JsonFilePath Path to the calibration JSON file
	 * @param OutResolutions Array of camera resolutions
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category="Basalt|Calibration")
	static bool GetCameraResolutions(
		const FString& JsonFilePath,
		TArray<FBasaltResolution>& OutResolutions
	);

	/**
	 * Returns the resolution for a specific camera
	 * @param JsonFilePath Path to the calibration JSON file
	 * @param CameraIndex Index of the camera (0-based)
	 * @param OutResolution The camera resolution
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category="Basalt|Calibration")
	static bool GetCameraResolution(
		const FString& JsonFilePath,
		int32 CameraIndex,
		FBasaltResolution& OutResolution
	);

	// ---------------------------------------- KB4 Intrinsics ---------------------------------------- //

	/**
	 * Returns KB4 intrinsics for a specific camera
	 * @param JsonFilePath Path to the calibration JSON file
	 * @param CameraIndex Index of the camera (0-based)
	 * @param OutIntrinsics The KB4 intrinsics
	 * @return True if successful and camera is KB4 type
	 */
	UFUNCTION(BlueprintCallable, Category="Basalt|Intrinsics")
	static bool GetKB4Intrinsics(
		const FString& JsonFilePath,
		int32 CameraIndex,
		FBasaltKB4Intrinsics& OutIntrinsics
	);

	// ---------------------------------------- EUCM Intrinsics ---------------------------------------- //

	/**
	 * Returns EUCM intrinsics for a specific camera
	 * @param JsonFilePath Path to the calibration JSON file
	 * @param CameraIndex Index of the camera (0-based)
	 * @param OutIntrinsics The EUCM intrinsics
	 * @return True if successful and camera is EUCM type
	 */
	UFUNCTION(BlueprintCallable, Category="Basalt|Intrinsics")
	static bool GetEUCMIntrinsics(
		const FString& JsonFilePath,
		int32 CameraIndex,
		FBasaltEUCMIntrinsics& OutIntrinsics
	);

	// ---------------------------------------- Double Sphere Intrinsics ---------------------------------------- //

	/**
	 * Returns Double Sphere intrinsics for a specific camera
	 * @param JsonFilePath Path to the calibration JSON file
	 * @param CameraIndex Index of the camera (0-based)
	 * @param OutIntrinsics The Double Sphere intrinsics
	 * @return True if successful and camera is DS type
	 */
	UFUNCTION(BlueprintCallable, Category="Basalt|Intrinsics")
	static bool GetDoubleSphereIntrinsics(
		const FString& JsonFilePath,
		int32 CameraIndex,
		FBasaltDoubleSphereIntrinsics& OutIntrinsics
	);

	// ---------------------------------------- Pinhole Intrinsics ---------------------------------------- //

	/**
	 * Returns Pinhole intrinsics for a specific camera
	 * @param JsonFilePath Path to the calibration JSON file
	 * @param CameraIndex Index of the camera (0-based)
	 * @param OutIntrinsics The Pinhole intrinsics
	 * @return True if successful and camera is Pinhole type
	 */
	UFUNCTION(BlueprintCallable, Category="Basalt|Intrinsics")
	static bool GetPinholeIntrinsics(
		const FString& JsonFilePath,
		int32 CameraIndex,
		FBasaltPinholeIntrinsics& OutIntrinsics
	);

	// ---------------------------------------- Save/Update Calibration ---------------------------------------- //

	/**
	 * Saves camera calibration data to a Basalt-compatible JSON file.
	 * The output format matches Basalt's Cereal JSON serialization with value0 wrapper.
	 *
	 * @param JsonFilePath Path where to save the calibration JSON file
	 * @param Cameras Array of camera calibrations to save
	 * @param bConvertFromUnrealCoordinates If true, converts Unreal coordinates to OpenCV
	 * @param bCentimetersToMeters If true, converts positions from centimeters to meters
	 * @return True if saving was successful
	 */
	UFUNCTION(BlueprintCallable, Category="Basalt|Calibration")
	static bool SaveCalibration(
		const FString& JsonFilePath,
		const TArray<FBasaltCameraCalibration>& Cameras,
		bool bConvertFromUnrealCoordinates,
		bool bCentimetersToMeters
	);

	/**
	 * Saves only extrinsics (transforms) to a Basalt-compatible JSON file.
	 * Use this when setting up cameras before stitching. Intrinsics will be added
	 * automatically after stitching via UpdateCalibrationWithIntrinsics.
	 *
	 * @param JsonFilePath Path where to save the calibration JSON file. If empty, uses default Movie Render path.
	 * @param Transforms Array of camera transforms (extrinsics)
	 * @param bConvertFromUnrealCoordinates If true, converts Unreal coordinates to OpenCV
	 * @param bCentimetersToMeters If true, converts positions from centimeters to meters
	 * @return True if saving was successful
	 */
	UFUNCTION(BlueprintCallable, Category="Basalt|Calibration")
	static bool SaveExtrinsics(
		const FString& JsonFilePath,
		const TArray<FTransform>& Transforms,
		bool bConvertFromUnrealCoordinates,
		bool bCentimetersToMeters
	);

	/**
	 * Returns the default calibration file path (in Movie Render output folder)
	 * @return Default path: [Project]/Saved/MovieRenders/calibration.json
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Basalt|Calibration")
	static FString GetDefaultCalibrationPath();
	
	/**
	 * Returns the default calibration file path (in Movie Render output folder)
	 * @return Default path: [Project]/Saved/MovieRenders/calibration.json
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="File|Path")
	static FString GetDefaultMovieRenderPath();

	/**
	 * Updates an existing Basalt calibration file with intrinsics data.
	 * The file must already exist (created with SaveExtrinsics or SaveCalibration).
	 * This merges the new intrinsics with existing data.
	 *
	 * @param JsonFilePath Path to the existing calibration JSON file
	 * @param Cameras Array of camera calibrations with intrinsics to add/update
	 * @return True if update was successful
	 */
	UFUNCTION(BlueprintCallable, Category="Basalt|Calibration")
	static bool UpdateCalibrationWithIntrinsics(
		const FString& JsonFilePath,
		const TArray<FBasaltCameraCalibration>& Cameras
	);

	/**
	 * Saves intrinsics to a Basalt calibration file.
	 * If the file exists, preserves extrinsics and updates intrinsics for all cameras.
	 * If the file doesn't exist, creates a new file with identity transforms.
	 * Call this whenever an intrinsic parameter changes.
	 *
	 * @param JsonFilePath Path to the calibration JSON file. If empty, uses default path.
	 * @param CameraType Camera model type: "kb4", "eucm", "ds", "pinhole", "equidistant", "equisolid", "orthographic", "stereographic"
	 * @param KB4Params KB4 intrinsic parameters (used if CameraType == "kb4")
	 * @param EUCMParams EUCM intrinsic parameters (used if CameraType == "eucm")
	 * @param IdealParams Ideal fisheye parameters (used if CameraType == "equidistant", "equisolid", "orthographic", "stereographic")
	 * @param Resolution Image resolution
	 * @return True if saving was successful
	 */
	UFUNCTION(BlueprintCallable, Category="Basalt|Calibration")
	static bool SaveIntrinsics(
		const FString& JsonFilePath,
		const FString& CameraType,
		const FBasaltKB4Intrinsics& KB4Params,
		const FBasaltEUCMIntrinsics& EUCMParams,
		const FBasaltIdealFisheyeIntrinsics& IdealParams,
		const FBasaltResolution& Resolution
	);

	/**
	 * Copies a calibration file from source to destination.
	 * Use this when changing the output path or when copying a loaded Basalt file to output.
	 *
	 * @param SourcePath Path to the source calibration file
	 * @param DestinationPath Path where to copy the file
	 * @return True if copy was successful
	 */
	UFUNCTION(BlueprintCallable, Category="Basalt|Calibration")
	static bool CopyCalibrationFile(
		const FString& SourcePath,
		const FString& DestinationPath
	);
};
