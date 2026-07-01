#include "Stitching/EquirectangularStitcher.h"
#include "Stitching/IO/RenderOutputPath.h"
#include <ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/compiler.h> // M_PI
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Utils/BasaltHelper.h"

// ------------------------------------------- Constructor / Destructor ------------------------------------------- //
// ---------------------------------------------------------------------------------------------------------------- //

EquirectangularStitcher::EquirectangularStitcher(){}

EquirectangularStitcher::EquirectangularStitcher(
	const TArray<FString>& Cameras,
	const FString& NameFolderImage,
	const FString& NameFolderImageDepth)
	: ImageStitcher(Cameras, NameFolderImage, NameFolderImageDepth){}

EquirectangularStitcher::~EquirectangularStitcher(){}

// --------------------------------------------------- Stitching -------------------------------------------------- //
// ---------------------------------------------------------------------------------------------------------------- //

void EquirectangularStitcher::ProcessDirectories()
{
	UE_LOG(LogTemp, Display, TEXT("[EquiStitcher] ProcessDirectories() START"));

	// Scan frame directories
	TArray<FString> RelevantSubdirectories = FRenderOutputPath::GetFrameDirectories(
		this->InputPath, this->FrameMin, this->FrameMax);

	UE_LOG(LogTemp, Display, TEXT("[EquiStitcher] Found %d frame directories in range %s-%s"),
		RelevantSubdirectories.Num(), *this->FrameMin, *this->FrameMax);

	if (RelevantSubdirectories.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquiStitcher] No frame directories found -> nothing to do."));
		return;
	}

	// Initialize progress tracking
	ProcessedFrames = 0;
	TotalFrames = RelevantSubdirectories.Num();

	// --- Process ---
	auto start = std::chrono::steady_clock::now();
	UE_LOG(LogTemp, Display, TEXT("[EquiStitcher] Starting processing (%d dirs) ..."), RelevantSubdirectories.Num());

	ParallelFor(RelevantSubdirectories.Num(), [&](int32 Index)
	{
		const FString& Subdirectory = RelevantSubdirectories[Index];
		ProcessSubdirectory(Subdirectory);

		// Update progress (thread-safe)
		++ProcessedFrames;
	});
	
	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> duration = end - start;
	UE_LOG(LogTemp, Display, TEXT("[EquiStitcher] DONE processing. Time: %f seconds"), duration.count());

	this->LookUpTableX.release();
	this->LookUpTableY.release();
	this->dGeneratedLoopUpTable = false;
	
	// Resize mask to match output image size
	cv::Mat ResizedMask;
	cv::resize(this->ValidRegionMask, ResizedMask, cv::Size(this->ImageWidth, this->ImageHeight), 0, 0, cv::INTER_NEAREST);

	FString MaskPath = FPaths::Combine(this->OutputPath, this->OutputDirectory, TEXT("equi_mask.png"));
	std::string MaskPathStd(TCHAR_TO_UTF8(*MaskPath));
	cv::imwrite(MaskPathStd, ResizedMask);
}

void EquirectangularStitcher::ProcessSubdirectory(const FString& Subdirectory)
{
	const FString FullSubdirectoryPath = FPaths::Combine(InputPath, Subdirectory);
	const FString ImageFolderPathBase = FPaths::Combine(FullSubdirectoryPath, NameFolderImage);
	const FString DepthImageFolderPathBase = FPaths::Combine(FullSubdirectoryPath, NameFolderDepthImage);

	if (bStitchColorImages)
	{
		ProcessImageFolder(EquirectangularDirectory, ImageFolderPathBase, Subdirectory, EStitchImageType::Color);
	}

	if (bStitchDepthImages)
	{
		ProcessImageFolder(EquirectangularDirectory, DepthImageFolderPathBase, Subdirectory, EStitchImageType::Depth);
	}

	if (bStitchInvDepthImages)
	{
		ProcessImageFolder(EquirectangularDirectory, DepthImageFolderPathBase, Subdirectory, EStitchImageType::InvDepth);
	}
}

void EquirectangularStitcher::ProcessImageFolder(const FString& CameraFolder,
                                                 const FString& FolderPathBase,
                                                 const FString& Subdirectory,
                                                 EStitchImageType ImageType)
{
	const FString FullFolderPath = FPaths::Combine(FolderPathBase, CameraFolder);

	if (!FPaths::DirectoryExists(FullFolderPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquiStitcher] Subdirectory %s missing in: %s"), *CameraFolder, *FolderPathBase);
		return;
	}

	cv::Mat StitchedImage = StitchImagesInDirectory(FullFolderPath);

	if (StitchedImage.empty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquiStitcher] Stitching failed for %s"), *FullFolderPath);
		return;
	}

	if (ImageType == EStitchImageType::InvDepth)
	{
		StitchedImage = ConvertDepthToInverseDepth(StitchedImage);
	}

	const FString OutputName = CameraFolder + GetStitchOutputSuffix(ImageType);
	SaveStitchedImage(StitchedImage, Subdirectory, OutputName);
}

cv::Mat EquirectangularStitcher::StitchImagesInDirectory(const FString& DirectoryPath)
{
	TArray<FString> ImagePaths;
	CollectImagePaths(DirectoryPath, ImagePaths);

	cv::Mat ResizedStitchedImage;
	
	if (ImagePaths.Num() != 6)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[EquiStitcher] Expected 6 images, but found %d in %s"),
			ImagePaths.Num(), *DirectoryPath);
		return ResizedStitchedImage;
	}
	
	const bool bIsDepthLike =
		DirectoryPath.Contains(TEXT("Depth"), ESearchCase::IgnoreCase);
	
	std::vector<cv::Mat> Images;
	LoadImages(ImagePaths, Images);

	if (bIsDepthLike)
	{
		for (cv::Mat& Img : Images)
		{
			Img = EnsureSingleChannelFloat(Img);
		}
	}
	
	for (int32 i = 0; i < Images.size(); ++i)
	{
		if (Images[i].empty())
		{
			UE_LOG(LogTemp, Error,
				TEXT("[EquiStitcher] Empty image %d in %s"),
				i, *DirectoryPath);
			return cv::Mat();
		}
	}
	
	const cv::Mat CubeMapAtlas = CubeMap::FromImages6(Images).GetAtlas();

	static FCriticalSection LutMutex;
	if (!dGeneratedLoopUpTable)
	{
		FScopeLock Lock(&LutMutex);
		if (!dGeneratedLoopUpTable)
		{
			GenerateLookupTable(CubeMapAtlas);
			dGeneratedLoopUpTable = true;
		}
	}

	cv::Mat StitchedImage =
		this->CubeMapToImage(CubeMapAtlas, this->LookUpTableX, this->LookUpTableY);
	cv::resize(
		StitchedImage,
		ResizedStitchedImage,
		cv::Size(this->ImageWidth, this->ImageHeight),
		0, 0,
		cv::INTER_LINEAR);
	UE_LOG(LogTemp, Display,
		TEXT("[EquiStitcher] Resize target size: Width=%d Height=%d"),
		this->ImageWidth,
		this->ImageHeight
	);
	
	return ResizedStitchedImage;
}

// --------------------------------------------------- Stitching -------------------------------------------------- //
// ---------------------------------------------------------------------------------------------------------------- //

void EquirectangularStitcher::GenerateLookupTable(const cv::Mat& CubeMapAtlas)
{
	const int Width = CubeMapAtlas.cols;
	const int Height = Width / 2;

	const int FaceWidth = CubeMapAtlas.cols / 4;
	const int FaceHeight = CubeMapAtlas.rows / 3;

	const float NormHeight = Height / 180.0;
	const int HalfHeight = Height / 2;
	
	const int VerticalOffset = NormHeight * this->VerticalFOV / 2;
	const int RelevantMinY = HalfHeight - VerticalOffset;
	const int RelevantMaxY = HalfHeight + VerticalOffset;
	
	int outputHeight = bLeaveBlackBars ? Height : (RelevantMaxY - RelevantMinY);

	this->LookUpTableX.create(outputHeight, Width, CV_32FC1);
	this->LookUpTableY.create(outputHeight, Width, CV_32FC1);

	this->ValidRegionMask = cv::Mat::zeros(outputHeight, Width, CV_8UC1);
	
	for (int Y = 0; Y < outputHeight; Y++)
    {
        for (int X = 0; X < Width; X++)
        {
            int originalY = bLeaveBlackBars ? Y : (Y + RelevantMinY);
        	
        	// Check if inside the wanted Vertical FOV
        	if (bLeaveBlackBars && (originalY < RelevantMinY || originalY >= RelevantMaxY))
        	{
        		// Use -1 to mark as outside, so cv::remap with BORDER_CONSTANT returns black
        		this->LookUpTableX.at<float>(Y, X) = -1.0f;
        		this->LookUpTableY.at<float>(Y, X) = -1.0f;
        		continue;
        	}
        	
			this->ValidRegionMask.at<uint8_t>(Y, X) = 255;
        	
        	// Calculate spherical coordinates phi (latitude) and theta (longitude) for the current pixel.
        	// equirectangular image represents a spherical projection, where the horizontal axis (x)
        	// corresponds to the longitude (theta) and the vertical axis (y) corresponds to the latitude (phi)
        	const double Theta = M_PI * static_cast<double>(originalY) / Height;                // phi from 0° to 180°
        	const double Phi = 2 * M_PI * static_cast<double>(X) / Width;               // theta from 0° to 360°

        	// Convert spherical coordinates to Cartesian 3D coordinates.
			double PX, PY, PZ;
        	CalculateCartesianCoordinates(Theta, Phi, PX, PY, PZ);

        	ECubeMapFace Face;
        	double U, V;
        	CubeMap::DirectionToFaceUV(PX, PY, PZ, Face, U, V);

            int32 SrcX, SrcY; // Coordinates of the source pixel in the CubeMap
			CubeMap::FaceUVToPixel(Face, U, V, FaceWidth, FaceHeight, SrcX, SrcY, ECubeMapLayout::Cross4x3);
        	
	        SrcX = FMath::Clamp(SrcX, 0, CubeMapAtlas.cols - 1);
        	SrcY = FMath::Clamp(SrcY, 0, CubeMapAtlas.rows - 1);

	        if (SrcX >= 0 && SrcX < CubeMapAtlas.cols && SrcY >= 0 && SrcY < CubeMapAtlas.rows)
	        {
        		this->LookUpTableX.at<float>(Y, X) = static_cast<float>(SrcX);
	        	this->LookUpTableY.at<float>(Y, X) = static_cast<float>(SrcY);
	        }
        }
    }
}
