#include "Stitching/ImageStitcher.h"
#include "Stitching/IO/ImageIO.h"
#include "Stitching/IO/RenderOutputPath.h"

// ------------------------------------------- Constructor / Destructor ------------------------------------------- //
// ---------------------------------------------------------------------------------------------------------------- //

ImageStitcher::ImageStitcher(){}

ImageStitcher::ImageStitcher(
	const TArray<FString>& Cameras, const FString& NameFolderImage, const FString& NameFolderImageDepth)
	: CameraNames(Cameras), NameFolderImage(NameFolderImage), NameFolderDepthImage(NameFolderImageDepth)
{
}

ImageStitcher::~ImageStitcher(){}

// ------------------------------------------------- File Handling ------------------------------------------------ //
// ---------------------------------------------------------------------------------------------------------------- //

bool ImageStitcher::ValidateInputOutput() const
{
	if (!FPaths::DirectoryExists(this->InputPath) || this->InputPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Unknown Fisheye InputPath: %s"), *InputPath);
		return false;
	}
	return true;
}

void ImageStitcher::CollectImagePaths(const FString& DirectoryPath, TArray<FString>& ImagePaths)
{
	FRenderOutputPath::CollectImagePaths(DirectoryPath, CameraNames, ImageExtension, ImagePaths);
}

void ImageStitcher::LoadImages(const TArray<FString>& ImagePaths, std::vector<cv::Mat>& Images) const
{
	FImageIO::LoadBatch(ImagePaths, Images);
}

void ImageStitcher::UpdateStitchingInputPath(const FString& Path, const int Type)
{
	if (!FPaths::DirectoryExists(Path) || Path.IsEmpty())
	{
		bInputDirectoryIsReady = false;
		ImageHeight = 0;
		ImageWidth = 0;
		return;
	}

	InputPath = Path;
	bInputDirectoryIsReady = true;
	GetImageSize(Path, Type);
}

void ImageStitcher::UpdateStitchingOutputPath(const FString& Path)
{
	if (!FPaths::DirectoryExists(Path) || Path.IsEmpty())
	{
		bOutputDirectoryIsReady = false;
		return;
	}

	OutputPath = Path;
	bOutputDirectoryIsReady = true;
}

void ImageStitcher::UpdateStitchingOutputDirectory(const FString& Directory)
{
	this->OutputDirectory = Directory;
}

void ImageStitcher::UpdateFrames(const FString& FrameMinSet, const FString& FrameMaxSet)
{
	this->FrameMin = FrameMinSet;
	this->FrameMax = FrameMaxSet;
}

void ImageStitcher::SaveStitchedImage(const cv::Mat& Image, const FString& Index, const FString& OutputName)
{
	if (Image.empty())
	{
		UE_LOG(LogTemp, Error, TEXT("[ImageStitcher] Cannot save empty image"));
		return;
	}

	FString OutputDir = FPaths::Combine(this->OutputPath, this->OutputDirectory, Index);

	if (!FImageIO::SaveToDirectory(Image, OutputDir, OutputName))
	{
		UE_LOG(LogTemp, Error, TEXT("[ImageStitcher] Failed to save image: %s/%s"), *OutputDir, *OutputName);
	}
}

// -------------------------------------------------- Stitching ------------------------------------------------- //
// -------------------------------------------------------------------------------------------------------------- //

cv::Mat ImageStitcher::CubeMapToImage(const cv::Mat& CubeMap, const cv::Mat& LookUpX, const cv::Mat& LookUpY)
{
	cv::Mat Image;

	// Set border value based on number of channels
	// For RGBA (4 channels): use opaque black (0,0,0,1) so invalid regions are black, not transparent
	// For single channel (depth): use 0
	cv::Scalar BorderValue;
	if (CubeMap.channels() == 4)
	{
		// RGBA - opaque black (alpha = 1.0 for float, 255 for 8-bit)
		BorderValue = (CubeMap.depth() == CV_32F) ? cv::Scalar(0, 0, 0, 1.0) : cv::Scalar(0, 0, 0, 255);
	}
	else if (CubeMap.channels() == 3)
	{
		// RGB - black
		BorderValue = cv::Scalar(0, 0, 0);
	}
	else
	{
		// Single channel (depth) - zero
		BorderValue = cv::Scalar(0);
	}

	cv::remap(CubeMap, Image, LookUpX, LookUpY, cv::INTER_LINEAR, cv::BORDER_CONSTANT, BorderValue);
	return Image;
}

void ImageStitcher::CalculateCartesianCoordinates(const double Theta, const double Phi, double& PX, double& PY, double& PZ)
{
	// Physical Convention
	PX = std::sin(Theta) * std::cos(Phi);
	PY = std::sin(Theta) * std::sin(Phi);
	PZ = std::cos(Theta);
}

// ---------------------------------------------------- Getter -------------------------------------------------- //
// -------------------------------------------------------------------------------------------------------------- //

/**
Movie Renders							(Path)
├── 0000								(Frames)
│   ├── FinalImage						(TypeFolders)
│   │   ├── Equirectangular				(CameraFolders), Type 0
│   │   └── FisheyeCamera_1				(CameraFolders), Type 1
│   ├── FinalImageDepthMetricInst		(TypeFolders)
├── 0001								(Frames)
├── 0002								(Frames)
*/
void ImageStitcher::GetImageSize(const FString& Path, const int Type)
{
	// Reset to defaults
	auto ResetSize = [this]()
	{
		this->ImageWidth = 0;
		this->ImageHeight = 0;
	};

	// Find first frame directory
	TArray<FString> Frames = FRenderOutputPath::GetFrameDirectories(Path, TEXT("0000"), TEXT("99999"));
	if (Frames.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ImageStitcher] No frame directories found in: %s"), *Path);
		ResetSize();
		return;
	}

	// Determine camera folder and face multipliers based on type
	FString CameraFolder;
	int FaceWidthAmount;
	int FaceHeightAmount;

	if (Type == 0) // Equirectangular
	{
		CameraFolder = EquirectangularDirectory;
		FaceWidthAmount = 4;
		FaceHeightAmount = 2;
	}
	else // Fisheye
	{
		CameraFolder = FisheyeDirectory + TEXT("_1");
		FaceWidthAmount = 3;
		FaceHeightAmount = 3;
	}

	// Build path to camera folder in first frame
	FString CameraPath = FRenderOutputPath::BuildCameraPath(
		Path, Frames[0], NameFolderImage, CameraFolder);

	// Get first image in camera folder
	FString FirstImagePath = FRenderOutputPath::GetFirstImagePath(CameraPath, ImageExtension);
	if (FirstImagePath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ImageStitcher] No images found in: %s"), *CameraPath);
		ResetSize();
		return;
	}

	// Load image to get dimensions
	cv::Mat Image = FImageIO::Load(FirstImagePath);
	if (Image.empty())
	{
		UE_LOG(LogTemp, Error, TEXT("[ImageStitcher] Failed to read image: %s"), *FirstImagePath);
		ResetSize();
		return;
	}

	// Calculate final image size from face dimensions
	this->ImageWidth = Image.cols * FaceWidthAmount;
	this->ImageHeight = Image.rows * FaceHeightAmount;
	this->OpticalCenterX = this->ImageWidth / 2;
	this->OpticalCenterY = this->ImageHeight / 2;

	UE_LOG(LogTemp, Display, TEXT("[ImageStitcher] Image size: %d x %d"), this->ImageWidth, this->ImageHeight);
}