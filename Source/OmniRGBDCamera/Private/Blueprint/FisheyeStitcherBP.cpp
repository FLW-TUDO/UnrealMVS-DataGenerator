#include "Blueprint/FisheyeStitcherBP.h"
#include "Stitching/FisheyeStitcher.h"
#include "Stitching/StitchingConstants.h"
#include "HAL/FileManager.h"
#include "Async/Async.h"
#include "Interfaces/IPluginManager.h"
#include <opencv2/opencv.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --------------------------------- Initialization --------------------------------- //

bool UFisheyeStitcherBP::Initialize(const TArray<FString>& Cameras,
									const FString& NameFolderImage,
									const FString& NameFolderDepthImage)
{
	Stitcher = MakeUnique<FisheyeStitcher>(Cameras, NameFolderImage, NameFolderDepthImage);
	UE_LOG(LogTemp, Display, TEXT("[FisheyeStitcherBP] Initialized with %d cameras"), Cameras.Num());
	return Stitcher.IsValid();
}

bool UFisheyeStitcherBP::ApplySettings(const FString& InInputPath,
									   const FString& InOutputPath,
									   const FString& InOutputDirectory,
									   const FString& InFrameMin,
									   const FString& InFrameMax,
									   bool bInStitchColorImages,
									   bool bInStitchDepthImages,
									   bool bInStitchInvDepthImages)
{
	if (!Stitcher.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[FisheyeStitcherBP] ApplySettings: Stitcher not initialized. Call Initialize first."));
		return false;
	}

	Stitcher->UpdateStitchingInputPath(InInputPath, 0);
	Stitcher->UpdateStitchingOutputPath(InOutputPath);
	Stitcher->UpdateStitchingOutputDirectory(InOutputDirectory);
	Stitcher->UpdateFrames(InFrameMin, InFrameMax);

	Stitcher->bStitchColorImages    = bInStitchColorImages;
	Stitcher->bStitchDepthImages    = bInStitchDepthImages;
	Stitcher->bStitchInvDepthImages = bInStitchInvDepthImages;

	return true;
}

// --------------------------------- Resolution --------------------------------- //

void UFisheyeStitcherBP::SetOutputResolution(int32 InWidth, int32 InHeight)
{
	if (!Stitcher.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FisheyeStitcherBP] SetOutputResolution called before Initialize"));
		return;
	}

	Stitcher->ImageWidth  = FMath::Max(1, InWidth);
	Stitcher->ImageHeight = FMath::Max(1, InHeight);
}

void UFisheyeStitcherBP::SetOutputWidth(int32 InWidth)
{
	if (!Stitcher.IsValid()) return;
	Stitcher->ImageWidth = FMath::Max(1, InWidth);
}

void UFisheyeStitcherBP::SetOutputHeight(int32 InHeight)
{
	if (!Stitcher.IsValid()) return;
	Stitcher->ImageHeight = FMath::Max(1, InHeight);
}

// --------------------------------- Ideal Fisheye Intrinsics --------------------------------- //

void UFisheyeStitcherBP::SetIdealFisheyeIntrinsics(const FBasaltIdealFisheyeIntrinsics& Intrinsics)
{
	if (!Stitcher.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FisheyeStitcherBP] SetIdealFisheyeIntrinsics called before Initialize"));
		return;
	}
	Stitcher->SetIdealFisheyeIntrinsics(Intrinsics);
}

void UFisheyeStitcherBP::SetIdealFisheyeIntrinsicsArray(const TArray<FBasaltIdealFisheyeIntrinsics>& IntrinsicsArray)
{
	if (!Stitcher.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FisheyeStitcherBP] SetIdealFisheyeIntrinsicsArray called before Initialize"));
		return;
	}

	Stitcher->SetIdealFisheyeIntrinsicsArray(IntrinsicsArray);
}

// --------------------------------- KB4 Intrinsics --------------------------------- //

void UFisheyeStitcherBP::SetKB4Intrinsics(const FBasaltKB4Intrinsics& Intrinsics)
{
	if (!Stitcher.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FisheyeStitcherBP] SetKB4Intrinsics called before Initialize"));
		return;
	}
	Stitcher->SetKB4Intrinsics(Intrinsics);
}

void UFisheyeStitcherBP::SetKB4IntrinsicsArray(const TArray<FBasaltKB4Intrinsics>& IntrinsicsArray)
{
	if (!Stitcher.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FisheyeStitcherBP] SetKB4IntrinsicsArray called before Initialize"));
		return;
	}

	Stitcher->SetKB4IntrinsicsArray(IntrinsicsArray);
}

// --------------------------------- EUCM Intrinsics --------------------------------- //

void UFisheyeStitcherBP::SetEUCMIntrinsics(const FBasaltEUCMIntrinsics& Intrinsics)
{
	if (!Stitcher.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FisheyeStitcherBP] SetEUCMIntrinsics called before Initialize"));
		return;
	}
	Stitcher->SetEUCMIntrinsics(Intrinsics);
}

void UFisheyeStitcherBP::SetEUCMIntrinsicsArray(const TArray<FBasaltEUCMIntrinsics>& IntrinsicsArray)
{
	if (!Stitcher.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FisheyeStitcherBP] SetEUCMIntrinsicsArray called before Initialize"));
		return;
	}

	Stitcher->SetEUCMIntrinsicsArray(IntrinsicsArray);
}

// --------------------------------- Multi-Camera Mode --------------------------------- //

void UFisheyeStitcherBP::ClearMultiCameraIntrinsics()
{
	if (!Stitcher.IsValid()) return;
	Stitcher->ClearMultiCameraIntrinsics();
}

bool UFisheyeStitcherBP::IsMultiCameraMode() const
{
	if (!Stitcher.IsValid()) return false;
	return Stitcher->IsMultiCameraMode();
}

// --------------------------------- Calibration --------------------------------- //

void UFisheyeStitcherBP::SetSourceCalibrationPath(const FString& InPath)
{
	if (!Stitcher.IsValid()) return;
	Stitcher->SetSourceCalibrationPath(InPath);
}

// --------------------------------- Vignette --------------------------------- //

void UFisheyeStitcherBP::SetApplyVignette(bool bApply)
{
	if (!Stitcher.IsValid()) return;
	Stitcher->bApplyVignette = bApply;
}

void UFisheyeStitcherBP::SetVignetteStrength(float Strength)
{
	if (!Stitcher.IsValid()) return;
	Stitcher->VignetteStrength = FMath::Max(0.0f, Strength);
}

void UFisheyeStitcherBP::ExportVignette()
{
	if (!Stitcher.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FisheyeStitcherBP] ExportVignette called before Initialize"));
		return;
	}
	Stitcher->ExportVignette();
}

// --------------------------------- Auto-Detection --------------------------------- //

bool UFisheyeStitcherBP::DetectImageParametersFromPath(
	const FString& InputPath,
	int32& OutImageWidth,
	int32& OutImageHeight,
	int32& OutCenterX,
	int32& OutCenterY,
	float& OutFocalLength)
{
	OutImageWidth = 0;
	OutImageHeight = 0;
	OutCenterX = 0;
	OutCenterY = 0;
	OutFocalLength = 0.0f;

	if (InputPath.IsEmpty())
	{
		return false;
	}

	// Find numeric frame directories (0001, 000001, 0000001, etc.)
	TArray<FString> Subdirs;
	IFileManager::Get().FindFiles(Subdirs, *FPaths::Combine(InputPath, TEXT("*")), false, true);

	FString FrameDir;
	for (const FString& Dir : Subdirs)
	{
		// Check if directory name is purely numeric (4-7 digits)
		if (Dir.Len() >= 4 && Dir.Len() <= 7 && Dir.IsNumeric())
		{
			FrameDir = FPaths::Combine(InputPath, Dir);
			break;
		}
	}

	if (FrameDir.IsEmpty())
	{
		return false;
	}

	// Search for FisheyeCamera_* folders specifically (not Equirectangular or other cameras)
	FString FisheyeCameraPath;
	TArray<FString> TypeFolders;
	IFileManager::Get().FindFiles(TypeFolders, *FPaths::Combine(FrameDir, TEXT("*")), false, true);

	for (const FString& TypeFolder : TypeFolders)
	{
		FString TypeFolderPath = FPaths::Combine(FrameDir, TypeFolder);
		TArray<FString> CameraFolders;
		IFileManager::Get().FindFiles(CameraFolders, *FPaths::Combine(TypeFolderPath, TEXT("*")), false, true);

		for (const FString& CameraFolder : CameraFolders)
		{
			if (CameraFolder.StartsWith(TEXT("FisheyeCamera_")))
			{
				FisheyeCameraPath = FPaths::Combine(TypeFolderPath, CameraFolder);
				break;
			}
		}
		if (!FisheyeCameraPath.IsEmpty())
		{
			break;
		}
	}

	if (FisheyeCameraPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FisheyeStitcherBP] DetectImageParameters: No FisheyeCamera_* folder found in %s"), *FrameDir);
		return false;
	}

	// Search for EXR in the FisheyeCamera folder
	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, *FPaths::Combine(FisheyeCameraPath, TEXT("*.exr")), true, false);

	if (FoundFiles.Num() == 0)
	{
		return false;
	}

	// Read first image from FisheyeCamera folder
	FString ImagePath = FPaths::Combine(FisheyeCameraPath, FoundFiles[0]);
	cv::Mat Image = cv::imread(TCHAR_TO_UTF8(*ImagePath), cv::IMREAD_UNCHANGED);
	if (Image.empty())
	{
		UE_LOG(LogTemp, Error, TEXT("[FisheyeStitcherBP] DetectImageParameters: Failed to read image: %s"), *ImagePath);
		return false;
	}

	// Fisheye output = 3x3 cubemap size
	OutImageWidth = Image.cols * 3;
	OutImageHeight = Image.rows * 3;
	OutCenterX = OutImageWidth / 2;
	OutCenterY = OutImageHeight / 2;
	OutFocalLength = static_cast<float>(FMath::Min(OutImageWidth, OutImageHeight)) / static_cast<float>(M_PI);

	return true;
}

float UFisheyeStitcherBP::CalculateFocalLengthFromFOV(int32 ImageWidth, float FOVDegrees, bool bEquidistant)
{
	if (ImageWidth <= 0 || FOVDegrees <= 0.0f) return 0.0f;

	if (bEquidistant)
	{
		// Equidistant: f = r / (theta/2) where theta is in radians
		float FOVRad = FMath::DegreesToRadians(FOVDegrees);
		float Radius = static_cast<float>(ImageWidth) / 2.0f;
		return Radius / (FOVRad / 2.0f);
	}
	return StitchingConstants::FocalLengthFromFOV(static_cast<float>(ImageWidth), FOVDegrees);
}

// --------------------------------- Path Helpers --------------------------------- //

FString UFisheyeStitcherBP::GetPluginDirectory()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("OmniRGBDCamera"));
	if (Plugin.IsValid())
	{
		return Plugin->GetBaseDir();
	}
	// Fallback: use project plugins directory
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir() / TEXT("UE5_Plugin"));
}

FString UFisheyeStitcherBP::GetPluginResourcesDirectory()
{
	return GetPluginDirectory() / TEXT("Resources");
}

// --------------------------------- Processing --------------------------------- //

void UFisheyeStitcherBP::ProcessDirectories(TArray<int32> CameraArray)
{
	if (!Stitcher.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[FisheyeStitcherBP] ProcessDirectoriesAsync: Stitcher not initialized."));
		OnStitchingComplete.Broadcast(false);
		return;
	}

	if (bIsStitching)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FisheyeStitcherBP] Stitching already in progress."));
		return;
	}

	bIsStitching = true;
	bCancelRequested = false;

	// Prevent garbage collection during async operation
	AddToRoot();

	// Start progress polling
	StartProgressPolling();

	// Capture stitcher pointer for the lambda
	FisheyeStitcher* StitcherPtr = Stitcher.Get();

	Async(EAsyncExecution::Thread, [this, StitcherPtr, CameraArray]()
	{
		bool bSuccess = true;

		try
		{
			StitcherPtr->ProcessDirectories(CameraArray);
		}
		catch (...)
		{
			bSuccess = false;
			UE_LOG(LogTemp, Error, TEXT("[FisheyeStitcherBP] Exception during async stitching"));
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
			OnStitchingComplete.Broadcast(bSuccess);
		});
	});
}

// --------------------------------- UStitcherBPBase Interface --------------------------------- //

bool UFisheyeStitcherBP::GetStitcherProgress(int32& OutCurrent, int32& OutTotal) const
{
	if (Stitcher.IsValid())
	{
		Stitcher->GetProgress(OutCurrent, OutTotal);
		return true;
	}
	return false;
}

// --------------------------------- Preview --------------------------------- //

UTexture2D* UFisheyeStitcherBP::GeneratePreview(const FString& SampleDirectoryPath, int32 CameraIndex)
{
	if (!Stitcher.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[FisheyeStitcherBP] GeneratePreview: Stitcher not initialized"));
		return nullptr;
	}

	// Use full output resolution
	return GeneratePreviewWithSize(SampleDirectoryPath, CameraIndex, 0, 0);
}

UTexture2D* UFisheyeStitcherBP::GeneratePreviewWithSize(const FString& SampleDirectoryPath, int32 CameraIndex, int32 PreviewWidth, int32 PreviewHeight)
{
	if (!Stitcher.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[FisheyeStitcherBP] GeneratePreviewWithSize: Stitcher not initialized"));
		return nullptr;
	}

	if (!FPaths::DirectoryExists(SampleDirectoryPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[FisheyeStitcherBP] GeneratePreview: Directory does not exist: %s"), *SampleDirectoryPath);
		return nullptr;
	}

	// Find the FisheyeCamera folder
	FString CameraFolder = FString::Printf(TEXT("FisheyeCamera_%d"), CameraIndex);
	FString ImageFolderPath = FPaths::Combine(SampleDirectoryPath, Stitcher->GetImageFolderName(), CameraFolder);

	if (!FPaths::DirectoryExists(ImageFolderPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[FisheyeStitcherBP] GeneratePreview: Camera folder not found: %s"), *ImageFolderPath);
		return nullptr;
	}

	// Store original dimensions
	int32 OriginalWidth = Stitcher->ImageWidth;
	int32 OriginalHeight = Stitcher->ImageHeight;

	// Use preview dimensions if specified
	if (PreviewWidth > 0 && PreviewHeight > 0)
	{
		Stitcher->ImageWidth = PreviewWidth;
		Stitcher->ImageHeight = PreviewHeight;
	}

	// Stitch the images (color only, no depth)
	cv::Mat StitchedImage = Stitcher->StitchSingleFrame(ImageFolderPath, CameraIndex, false, false);

	// Restore original dimensions
	Stitcher->ImageWidth = OriginalWidth;
	Stitcher->ImageHeight = OriginalHeight;

	if (StitchedImage.empty())
	{
		UE_LOG(LogTemp, Error, TEXT("[FisheyeStitcherBP] GeneratePreview: Stitching failed"));
		return nullptr;
	}

	// Resize if preview size was specified
	if (PreviewWidth > 0 && PreviewHeight > 0 &&
		(StitchedImage.cols != PreviewWidth || StitchedImage.rows != PreviewHeight))
	{
		cv::Mat Resized;
		cv::resize(StitchedImage, Resized, cv::Size(PreviewWidth, PreviewHeight), 0, 0, cv::INTER_LINEAR);
		StitchedImage = Resized;
	}

	// Convert to texture
	return MatToTexture2D(StitchedImage);
}

UTexture2D* UFisheyeStitcherBP::MatToTexture2D(const cv::Mat& InMat)
{
	if (InMat.empty())
	{
		return nullptr;
	}

	const int32 Width = InMat.cols;
	const int32 Height = InMat.rows;

	// Create the texture
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Texture)
	{
		UE_LOG(LogTemp, Error, TEXT("[FisheyeStitcherBP] MatToTexture2D: Failed to create texture"));
		return nullptr;
	}

	// Lock the texture for writing
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);

	if (!TextureData)
	{
		UE_LOG(LogTemp, Error, TEXT("[FisheyeStitcherBP] MatToTexture2D: Failed to lock texture"));
		return nullptr;
	}

	// Convert cv::Mat to BGRA8 format
	// EXR files are in linear color space, so we need to:
	// 1. Clamp values to 0-1 range (HDR can have values > 1)
	// 2. Apply gamma correction (linear to sRGB)
	// 3. Convert to 8-bit
	// 4. Fix channel order (OpenCV loads EXR as BGR, UE expects BGRA)
	cv::Mat ConvertedMat;
	cv::Mat WorkMat = InMat.clone();

	if (WorkMat.depth() == CV_32F)
	{
		// Clamp to 0-1 range for proper display
		cv::min(WorkMat, 1.0, WorkMat);
		cv::max(WorkMat, 0.0, WorkMat);

		// Apply gamma correction (linear to sRGB approximation: pow(x, 1/2.2))
		cv::pow(WorkMat, 1.0 / 2.2, WorkMat);
	}

	if (WorkMat.channels() == 4)
	{
		if (WorkMat.depth() == CV_32F)
		{
			// Float BGRA -> 8-bit BGRA (OpenCV EXR is BGR/BGRA)
			WorkMat.convertTo(ConvertedMat, CV_8UC4, 255.0);
		}
		else
		{
			ConvertedMat = WorkMat;
		}
	}
	else if (WorkMat.channels() == 3)
	{
		if (WorkMat.depth() == CV_32F)
		{
			cv::Mat Temp;
			WorkMat.convertTo(Temp, CV_8UC3, 255.0);
			cv::cvtColor(Temp, ConvertedMat, cv::COLOR_BGR2BGRA);
		}
		else
		{
			cv::cvtColor(WorkMat, ConvertedMat, cv::COLOR_BGR2BGRA);
		}
	}
	else if (WorkMat.channels() == 1)
	{
		// Grayscale
		if (WorkMat.depth() == CV_32F)
		{
			cv::Mat Temp;
			WorkMat.convertTo(Temp, CV_8UC1, 255.0);
			cv::cvtColor(Temp, ConvertedMat, cv::COLOR_GRAY2BGRA);
		}
		else
		{
			cv::cvtColor(WorkMat, ConvertedMat, cv::COLOR_GRAY2BGRA);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[FisheyeStitcherBP] MatToTexture2D: Unsupported channel count: %d"), WorkMat.channels());
		Mip.BulkData.Unlock();
		return nullptr;
	}

	// Copy data to texture
	const int32 DataSize = Width * Height * 4;
	FMemory::Memcpy(TextureData, ConvertedMat.data, DataSize);

	// Unlock and update
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}