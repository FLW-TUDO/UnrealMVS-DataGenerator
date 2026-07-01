#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Containers/Ticker.h"
#include "ROS/RosbagSettings.h"
#include "ROS/RosbagTypes.h"
#include <atomic>
#include "RosbagCreator.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRosbagComplete, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRosbagProgress, int32, CurrentFrame, int32, TotalFrames);

/**
 * Converts a CameraPoseOutput YAML + stitched EXR images into an MCAP rosbag.
 *
 * Workflow:
 *   1. Run Movie Render Queue with CameraPoseOutput → produces camera_poses.yaml
 *   2. Run stitching → produces stitched .exr files in the render directory
 *   3. Call CreateRosbag() → reads poses from YAML, reads images, writes .mcap
 *
 * Call from an Editor Utility Widget after stitching is complete.
 */
UCLASS(BlueprintType)
class OMNIRGBDCAMERA_API URosbagCreator : public UObject
{
	GENERATED_BODY()

public:
	/** Fired when async rosbag creation finishes. */
	UPROPERTY(BlueprintAssignable, Category="OmniRGBD|ROS")
	FOnRosbagComplete OnRosbagComplete;

	/** Fired periodically during async rosbag creation with frame progress. */
	UPROPERTY(BlueprintAssignable, Category="OmniRGBD|ROS")
	FOnRosbagProgress OnRosbagProgress;

	/** True while an async rosbag creation is running. */
	UPROPERTY(BlueprintReadOnly, Category="OmniRGBD|ROS")
	bool bIsRunning = false;

	/**
	 * Create an MCAP rosbag from a CameraPoseOutput YAML file and a render directory.
	 * Runs asynchronously on a worker thread. Returns immediately.
	 * Bind OnRosbagComplete to receive the result; OnRosbagProgress for frame updates.
	 *
	 * @param YamlPath        Path to the camera_poses.yaml produced by CameraPoseOutput.
	 * @param RenderDirectory Directory containing stitched .exr files.
	 * @param OutputPath      Full path to the output .mcap file.
	 *                        If empty, defaults to RenderDirectory/camera_trajectory.mcap.
	 * @param Settings        Topic names, ROS frame IDs, camera intrinsics, etc.
	 */
	UFUNCTION(BlueprintCallable, Category="OmniRGBD|ROS",
		meta=(ToolTip="Async: reads poses from camera_poses.yaml, scans RenderDirectory for images, writes .mcap. Fires OnRosbagComplete(bSuccess) when done."))
	void CreateRosbag(
		const FString& YamlPath,
		const FString& RenderDirectory,
		const FString& OutputPath,
		const FRosbagSettings& Settings);

private:
	/** Synchronous implementation, runs on the worker thread. */
	bool CreateRosbagInternal(
		const FString& YamlPath,
		const FString& RenderDirectory,
		const FString& OutputPath,
		const FRosbagSettings& Settings);


	/** Progress counters updated by WriteMcap on the worker thread. */
	mutable std::atomic<int32> ProcessedFrames{0};
	mutable std::atomic<int32> TotalFramesCount{0};

	/** Ticker handle for polling progress on the Game Thread. */
	FTSTicker::FDelegateHandle ProgressTickerHandle;

	void StartProgressPolling();
	void StopProgressPolling();

	/**
	 * Parse the YAML file produced by CameraPoseOutput into pose entries.
	 * Also extracts the coordinate_system field from the metadata section,
	 * e.g. "OpenCV/Basalt (X-right, Y-down, Z-forward)" or "Unreal Engine (X-forward, Y-right, Z-up)".
	 */
	bool ParseYaml(
		const FString& FilePath,
		const FString& ActorNameFilter,
		TArray<FCameraPoseEntry>& OutPoses,
		FString& OutCoordinateSystem) const;

	/** Scan directory for files matching Pattern; return sorted frame numbers. */
	TArray<int32> ScanFramesInDirectory(
		const FString& Dir,
		const FString& Pattern) const;

	/** Write the MCAP file with all configured topics. */
	bool WriteMcap(
		const FString& FilePath,
		const TArray<FCameraPoseEntry>& Poses,
		const FString& RenderDirectory,
		const FRosbagSettings& Settings) const;

	/** Resolve {frame:04d} etc. placeholders in a filename pattern. */
	FString ResolveFrameFilename(const FString& Pattern, int32 Frame) const;

	/**
	 * Parse T_imu_cam entries from a Basalt calibration JSON (calibration.json).
	 * Each entry is a (position, quaternion) pair in Basalt convention.
	 * OutCameras[i].Key = position (px,py,pz), OutCameras[i].Value = quaternion (qx,qy,qz,qw).
	 */
	bool ParseCalibrationJson(
		const FString& FilePath,
		TArray<TPair<FVector, FQuat>>& OutCameras) const;

	/**
	 * Serialize all rig-camera transforms as a single tf2_msgs/TFMessage for /tf_static.
	 * parent frame = Settings.CameraFrameId, children = Settings.CamFramePrefix + index.
	 */
	TArray<uint8> SerializeStaticTfMessage(
		const TArray<TPair<FVector, FQuat>>& Cameras,
		int64 TimestampNs,
		const FRosbagSettings& Settings) const;

	/**
	 * Convert a single pose from OpenCV camera convention (X-right, Y-down, Z-fwd)
	 * to ROS world convention (X-fwd, Y-left, Z-up) in-place.
	 *
	 * This is required because CameraPoseOutput writes poses in OpenCV convention
	 * (bConvertToOpenCVCoordinates = true) while ROS Odometry / TF expect poses
	 * in the world frame with Z-up.
	 */
	static void ConvertPoseOpenCVToROS(FCameraPoseEntry& Pose);

	// --- CDR serializers --------------------------------------------------

	TArray<uint8> SerializeOdometry(
		const FCameraPoseEntry& Pose,
		const FRosbagSettings& Settings) const;

	TArray<uint8> SerializePoseStamped(
		const FCameraPoseEntry& Pose,
		const FRosbagSettings& Settings) const;

	TArray<uint8> SerializeTfMessage(
		const FCameraPoseEntry& Pose,
		const FRosbagSettings& Settings) const;

	TArray<uint8> SerializeCameraInfo(
		int64 TimestampNs,
		const FRosbagSettings& Settings) const;

	/**
	 * Load depth + color EXR, unproject each pixel via equirectangular formula
	 * into the camera_link frame, and serialize as sensor_msgs/PointCloud2 (XYZRGB, float32).
	 * Azimuth convention matches EquirectangularStitcher: Phi=0 at left edge (Left camera),
	 * Phi=π/2 at u=W/4 (Front camera = +X forward in camera_link).
	 * Depth unit: cm (Unreal) → converted to m internally.
	 * Stride: every Nth pixel is sampled to keep message size manageable.
	 */
	TArray<uint8> SerializePointCloud2(
		const FString& DepthFile,
		const FString& ColorFile,
		const FCameraPoseEntry& Pose,
		const FRosbagSettings& Settings) const;

	/** Load a depth EXR (float, cm) and serialize as sensor_msgs/Image (32FC1, m). */
	TArray<uint8> SerializeDepthImage(
		const FString& FilePath,
		int64 TimestampNs,
		const FRosbagSettings& Settings) const;

	/** Load a color EXR (float HDR) and serialize as sensor_msgs/Image (bgr8). */
	TArray<uint8> SerializeColorImage(
		const FString& FilePath,
		int64 TimestampNs,
		const FRosbagSettings& Settings) const;
};
