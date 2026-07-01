#pragma once

#include "CoreMinimal.h"
#include "MoviePipelineOutputBase.h"
#include "Misc/FrameRate.h"
#include "ROS/RosbagTypes.h"
#include "MoviePipelineCameraPoseOutput.generated.h"

/**
 * Movie Pipeline output that exports actor poses to YAML by reading
 * transforms directly from Level Sequence keyframes.
 */
UCLASS(BlueprintType, meta=(DisplayName="Camera Pose Output"))
class OMNIRGBDCAMERA_API UMoviePipelineCameraPoseOutput : public UMoviePipelineOutputBase
{
	GENERATED_BODY()

public:
	UMoviePipelineCameraPoseOutput();

	virtual void SetupForPipelineImpl(UMoviePipeline* InPipeline) override;

protected:
	virtual void OnReceiveImageDataImpl(FMoviePipelineMergerOutputFrame* InMergedOutputFrame) override;
	virtual void BeginFinalizeImpl() override;
	virtual bool HasFinishedProcessingImpl() override { return true; }

#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "CameraPoseOutputSetting_DisplayName", "Camera Pose Output (.yaml)"); }
	virtual FText GetCategoryText() const override { return NSLOCTEXT("MovieRenderPipeline", "CameraPoseOutputSetting_Category", "Exports"); }
#endif

public:
	/** Output filename without extension */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Pose Output")
	FString OutputFilename = TEXT("camera_poses");

	/**
	 * Actor labels to track (must match the Actor Label in the Outliner exactly).
	 * The actor must have a Transform track in the Level Sequence - static actors without
	 * keyframed transforms cannot be tracked.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Pose Output")
	TArray<FString> ActorNamesToTrack;

	/** Convert from Unreal (X-forward, Y-right, Z-up, cm) to OpenCV/Basalt (X-right, Y-down, Z-forward, m) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Pose Output")
	bool bConvertToOpenCVCoordinates = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Pose Output")
	bool bConvertToMeters = true;

	/**
	 * Diagonal variance for pose covariance matrix (x, y, z, roll, pitch, yaw).
	 * 0.0 = perfect synthetic ground truth. Set small values (e.g. 1e-6) if downstream
	 * ROS nodes reject zero covariance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Pose Output", meta=(ClampMin="0.0"))
	float PoseCovarianceDiagonal = 0.0f;

	/**
	 * Diagonal variance for twist covariance matrix (vx, vy, vz, wx, wy, wz).
	 * 0.0 = perfect synthetic ground truth.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Pose Output", meta=(ClampMin="0.0"))
	float TwistCovarianceDiagonal = 0.0f;

private:
	TArray<FCameraPoseEntry> CollectedPoses;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMoviePipeline> WeakPipeline;

	TSet<int32> RenderedFrames;

	/** Effective render frame rate (MRQ override or sequence DisplayRate) */
	FFrameRate RenderFrameRate;

	/** Sequence display rate (for channel evaluation) */
	FFrameRate SequenceDisplayRate;

	void ExtractPosesFromSequence();
	void WritePosesToYAML(const FString& OutputPath);
	void ConvertToOpenCV(FVector& Position, FQuat& Orientation) const;
};
