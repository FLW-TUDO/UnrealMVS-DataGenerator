#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Containers/Ticker.h"
#include <atomic>

#include "StitcherBPBase.generated.h"

/** Delegate for async stitching completion */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStitchingComplete, bool, bSuccess);

/** Delegate for progress updates */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStitchingProgress, int32, CurrentFrame, int32, TotalFrames);

/**
 * @class UStitcherBPBase
 * @brief Abstract base class for Blueprint stitcher wrappers.
 *
 * Provides common functionality for async stitching operations:
 * - Progress polling and delegates
 * - Cancellation support
 * - Stitching state management
 *
 * Derived classes must implement GetStitcherProgress().
 */
UCLASS(Abstract, BlueprintType)
class OMNIRGBDCAMERA_API UStitcherBPBase : public UObject
{
	GENERATED_BODY()

public:
	/** Event fired when async stitching completes */
	UPROPERTY(BlueprintAssignable, Category="Stitching")
	FOnStitchingComplete OnStitchingComplete;

	/** Event fired periodically during async stitching with progress info */
	UPROPERTY(BlueprintAssignable, Category="Stitching")
	FOnStitchingProgress OnStitchingProgress;

	/** Returns true if stitching is currently in progress */
	UPROPERTY(BlueprintReadOnly, Category="Stitching")
	bool bIsStitching = false;

	/** Cancel ongoing async stitching (if possible) */
	UFUNCTION(BlueprintCallable, Category="Stitching")
	virtual void CancelStitching();

protected:
	/** Flag to request cancellation of async stitching */
	std::atomic<bool> bCancelRequested{false};

	/** Ticker handle for progress polling (works in Editor) */
	FTSTicker::FDelegateHandle ProgressTickerHandle;

	/** Starts polling progress and firing OnStitchingProgress */
	void StartProgressPolling();

	/** Stops progress polling */
	void StopProgressPolling();

	/** Broadcasts progress to OnStitchingProgress delegate */
	void BroadcastProgress(int32 Current, int32 Total);

	/**
	 * Gets the current progress from the stitcher.
	 * Must be implemented by derived classes.
	 * @param OutCurrent Current frame being processed
	 * @param OutTotal Total frames to process
	 * @return True if progress is available
	 */
	virtual bool GetStitcherProgress(int32& OutCurrent, int32& OutTotal) const PURE_VIRTUAL(UStitcherBPBase::GetStitcherProgress, return false;);

	/**
	 * Returns the log prefix for this stitcher (e.g., "[EquiStitcherBP]").
	 * Used for consistent logging.
	 */
	virtual FString GetLogPrefix() const PURE_VIRTUAL(UStitcherBPBase::GetLogPrefix, return TEXT("[StitcherBP]"););
};
