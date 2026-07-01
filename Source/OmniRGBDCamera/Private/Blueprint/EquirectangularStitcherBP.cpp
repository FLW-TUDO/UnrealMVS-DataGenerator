#include "Blueprint/EquirectangularStitcherBP.h"
#include "Stitching/EquirectangularStitcher.h"
#include "Async/Async.h"

// --------------------------------- Resolution --------------------------------- //

void UEquirectangularStitcherBP::SetOutputResolution(int32 InWidth, int32 InHeight)
{
	if (!Stitcher.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquiStitcherBP] SetOutputResolution called before Initialize"));
		return;
	}

	Stitcher->ImageWidth  = FMath::Max(1, InWidth);
	Stitcher->ImageHeight = FMath::Max(1, InHeight);

	Stitcher->LookUpTableX.release();
	Stitcher->LookUpTableY.release();
	Stitcher->dGeneratedLoopUpTable = false;

	UE_LOG(LogTemp, Display, TEXT("[EquiStitcherBP] Output resolution set to %dx%d"),
		Stitcher->ImageWidth, Stitcher->ImageHeight);
}

void UEquirectangularStitcherBP::SetOutputWidth(int32 InWidth)
{
	if (!Stitcher.IsValid()) return;
	Stitcher->ImageWidth = FMath::Max(1, InWidth);
	Stitcher->dGeneratedLoopUpTable = false;
}

void UEquirectangularStitcherBP::SetOutputHeight(int32 InHeight)
{
	if (!Stitcher.IsValid()) return;
	Stitcher->ImageHeight = FMath::Max(1, InHeight);
	Stitcher->dGeneratedLoopUpTable = false;
}

// --------------------------------- Initialization --------------------------------- //

bool UEquirectangularStitcherBP::Initialize(const TArray<FString>& Cameras,
											const FString& NameFolderImage,
											const FString& NameFolderDepthImage)
{
	Stitcher = MakeUnique<EquirectangularStitcher>(Cameras, NameFolderImage, NameFolderDepthImage);
	UE_LOG(LogTemp, Display, TEXT("[EquiStitcherBP] Initialized with %d cameras"), Cameras.Num());
	return Stitcher.IsValid();
}

bool UEquirectangularStitcherBP::ApplySettings(const FString& InInputPath,
											   const FString& InOutputPath,
											   const FString& InOutputDirectory,
											   const FString& InFrameMin,
											   const FString& InFrameMax,
											   bool bInStitchColorImages,
											   bool bInStitchDepthImages,
											   bool bInStitchInvDepthImages,
											   int32 InVerticalFOV,
											   bool bInLeaveBlackBars)
{
	if (!Stitcher.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[EquiStitcherBP] ApplySettings: Stitcher not initialized."));
		return false;
	}

	Stitcher->UpdateStitchingInputPath(InInputPath, 0);
	Stitcher->UpdateStitchingOutputPath(InOutputPath);
	Stitcher->UpdateStitchingOutputDirectory(InOutputDirectory);
	Stitcher->UpdateFrames(InFrameMin, InFrameMax);

	Stitcher->bStitchColorImages    = bInStitchColorImages;
	Stitcher->bStitchDepthImages    = bInStitchDepthImages;
	Stitcher->bStitchInvDepthImages = bInStitchInvDepthImages;
	Stitcher->VerticalFOV           = InVerticalFOV;
	Stitcher->bLeaveBlackBars       = bInLeaveBlackBars;

	UE_LOG(LogTemp, Display, TEXT("[EquiStitcherBP] Settings applied - Input: %s, Frames: %s-%s"),
		*InInputPath, *InFrameMin, *InFrameMax);

	return true;
}

// --------------------------------- Processing --------------------------------- //

void UEquirectangularStitcherBP::ProcessDirectories()
{
	if (!Stitcher.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[EquiStitcherBP] ProcessDirectoriesAsync: Stitcher not initialized."));
		OnStitchingComplete.Broadcast(false);
		return;
	}

	if (bIsStitching)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquiStitcherBP] Stitching already in progress."));
		return;
	}

	bIsStitching = true;
	bCancelRequested = false;

	UE_LOG(LogTemp, Display, TEXT("[EquiStitcherBP] Starting async stitching..."));

	// Prevent garbage collection during async operation
	AddToRoot();

	// Start progress polling
	StartProgressPolling();

	// Capture stitcher pointer for the lambda
	EquirectangularStitcher* StitcherPtr = Stitcher.Get();

	Async(EAsyncExecution::Thread, [this, StitcherPtr]()
	{
		bool bSuccess = true;

		try
		{
			StitcherPtr->ProcessDirectories();
		}
		catch (...)
		{
			bSuccess = false;
			UE_LOG(LogTemp, Error, TEXT("[EquiStitcherBP] Exception during async stitching"));
		}

		// Back to Game Thread to fire delegate
		AsyncTask(ENamedThreads::GameThread, [this, bSuccess]()
		{
			StopProgressPolling();

			// Send final progress update (100%)
			if (Stitcher.IsValid())
			{
				int32 Total = Stitcher->TotalFrames;
				OnStitchingProgress.Broadcast(Total, Total);
			}

			bIsStitching = false;
			RemoveFromRoot();

			UE_LOG(LogTemp, Display, TEXT("[EquiStitcherBP] Async stitching complete. Success: %s"),
				bSuccess ? TEXT("true") : TEXT("false"));

			OnStitchingComplete.Broadcast(bSuccess);
		});
	});
}

// --------------------------------- UStitcherBPBase Interface --------------------------------- //

bool UEquirectangularStitcherBP::GetStitcherProgress(int32& OutCurrent, int32& OutTotal) const
{
	if (Stitcher.IsValid())
	{
		Stitcher->GetProgress(OutCurrent, OutTotal);
		return true;
	}
	return false;
}
