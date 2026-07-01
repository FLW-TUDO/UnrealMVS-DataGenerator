#pragma once

#include "CoreMinimal.h"
#include "RosbagSettings.generated.h"

/**
 * Configuration for URosbagCreator::CreateRosbag.
 *
 * All topic strings: leave empty to skip writing that topic.
 * File patterns support {frame:04d} / {frame:05d} / {frame} placeholders.
 */
USTRUCT(BlueprintType)
struct OMNIRGBDCAMERA_API FRosbagSettings
{
	GENERATED_BODY()

	// -------------------------------------------------------------------------
	// Actor tracking

	/** Actor label (Outliner name) that has a Transform track in the LevelSequence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tracking")
	FString ActorName = TEXT("Equirectangular Camera");

	/**
	 * Whether the input YAML poses are in OpenCV/Basalt convention
	 * (X-right, Y-down, Z-forward) — the default output of CameraPoseOutput
	 * when bConvertToOpenCVCoordinates = true.
	 *
	 * Enable this to automatically re-express poses in the ROS world frame
	 * (X-forward, Y-left, Z-up) before writing Odometry and TF messages.
	 * Leave disabled only if CameraPoseOutput was configured with
	 * bConvertToOpenCVCoordinates = false (raw Unreal world coordinates).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tracking")
	bool bInputIsOpenCVCoordinates = true;

	// -------------------------------------------------------------------------
	// Frame rate
	// Set both to 0 to use the LevelSequence display rate automatically.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing",
		meta=(ToolTip="Render frame rate numerator. 0 = use LevelSequence display rate."))
	int32 FrameRateNumerator = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing")
	int32 FrameRateDenominator = 1;

	// -------------------------------------------------------------------------
	// ROS frame IDs

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ROS Frames")
	FString MapFrameId = TEXT("map");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ROS Frames")
	FString CameraFrameId = TEXT("camera_link");

	// -------------------------------------------------------------------------
	// Topics (empty string = topic is not written)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Topics")
	FString OdometryTopic = TEXT("/camera/odometry");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Topics")
	FString PoseStampedTopic = TEXT("/camera/pose");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Topics")
	FString PointCloudTopic = TEXT("/camera/pointcloud");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Topics")
	FString TfTopic = TEXT("/tf");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Topics")
	FString DepthTopic = TEXT("/camera/depth/image_raw");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Topics")
	FString ColorTopic = TEXT("/camera/color/image_raw");

	/** Leave empty to skip writing CameraInfo (e.g. for equirectangular which has no standard intrinsics). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Topics")
	FString CameraInfoTopic = TEXT("");

	// -------------------------------------------------------------------------
	// Camera rig calibration (optional, Basalt JSON format)

	/** Path to a Basalt calibration JSON (calibration.json).
	 *  When set, one /tf_static message is written with the T_imu_cam transforms
	 *  for all cameras in the rig (parent = CameraFrameId, children = <CamFramePrefix>0, 1, ...).
	 *  Leave empty to skip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera Rig")
	FString CalibrationFilePath = TEXT("");

	/** Prefix for the child frame IDs generated from T_imu_cam.
	 *  "cam" produces cam0, cam1, cam2, ... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera Rig")
	FString CamFramePrefix = TEXT("cam");

	/** Topic for static camera-rig TF messages. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera Rig")
	FString TfStaticTopic = TEXT("/tf_static");

	// -------------------------------------------------------------------------
	// Image file patterns
	// Placeholders: {frame:04d}, {frame:05d}, {frame}
	// The render output uses numbered sub-directories: 0000/Equirectangular.exr

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Images")
	FString DepthFilePattern = TEXT("{frame:04d}/Equirectangular_Depth.exr");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Images")
	FString ColorFilePattern = TEXT("{frame:04d}/Equirectangular.exr");

	// -------------------------------------------------------------------------
	// Camera intrinsics for sensor_msgs/CameraInfo

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Intrinsics")
	int32 ImageWidth = 2048;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Intrinsics")
	int32 ImageHeight = 1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Intrinsics")
	double FocalLengthX = 350.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Intrinsics")
	double FocalLengthY = 350.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Intrinsics")
	double PrincipalPointX = 1024.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Intrinsics")
	double PrincipalPointY = 512.0;

	// -------------------------------------------------------------------------
	// Covariance (0.0 = perfect synthetic ground truth)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Covariance", meta=(ClampMin="0.0"))
	float PoseCovarianceDiagonal = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Covariance", meta=(ClampMin="0.0"))
	float TwistCovarianceDiagonal = 0.0f;
};
