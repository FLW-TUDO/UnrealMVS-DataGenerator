#include "Stitching/IO/ImageIO.h"
#include <cstdlib>
#include <filesystem>

#if WITH_OPENCV
#include "PreOpenCVHeaders.h"
#include "opencv2/imgcodecs.hpp"
#include "PostOpenCVHeaders.h"
#endif

bool FImageIO::bExrEnabled = false;

void FImageIO::EnsureExrEnabled()
{
	if (!bExrEnabled)
	{
		_putenv("OPENCV_IO_ENABLE_OPENEXR=1");
		bExrEnabled = true;
	}
}

// --------------------------------- Single Image Operations --------------------------------- //

cv::Mat FImageIO::Load(const FString& Path)
{
	EnsureExrEnabled();
	std::string PathStd = TCHAR_TO_UTF8(*Path);

	try
	{
		cv::Mat Image = cv::imread(PathStd, cv::IMREAD_UNCHANGED);
		if (Image.empty())
		{
			UE_LOG(LogTemp, Error, TEXT("[ImageIO] Failed to load: %s"), *Path);
		}
		return Image;
	}
	catch (const cv::Exception& e)
	{
		UE_LOG(LogTemp, Error, TEXT("[ImageIO] Exception loading %s: %s"), *Path, *FString(e.msg.c_str()));
		return cv::Mat();
	}
}

bool FImageIO::Save(const cv::Mat& Image, const FString& Path)
{
	EnsureExrEnabled();

	if (Image.empty())
	{
		UE_LOG(LogTemp, Error, TEXT("[ImageIO] Cannot save empty image to: %s"), *Path);
		return false;
	}

	std::string PathStd = TCHAR_TO_UTF8(*Path);

	try
	{
		return cv::imwrite(PathStd, Image);
	}
	catch (const cv::Exception& e)
	{
		UE_LOG(LogTemp, Error, TEXT("[ImageIO] Exception saving %s: %s"), *Path, *FString(e.msg.c_str()));
		return false;
	}
}

// --------------------------------- Batch Operations --------------------------------- //

int32 FImageIO::LoadBatch(const TArray<FString>& ImagePaths, std::vector<cv::Mat>& OutImages)
{
	int32 LoadedCount = 0;

	for (const FString& ImagePath : ImagePaths)
	{
		cv::Mat Img = Load(ImagePath);
		if (!Img.empty())
		{
			OutImages.push_back(Img);
			++LoadedCount;
		}
	}

	return LoadedCount;
}

bool FImageIO::SaveToDirectory(const cv::Mat& Image, const FString& OutputDir, const FString& Filename)
{
	if (Image.empty())
	{
		UE_LOG(LogTemp, Error, TEXT("[ImageIO] Cannot save empty image"));
		return false;
	}

	// Create directory if it doesn't exist
	std::filesystem::create_directories(TCHAR_TO_UTF8(*OutputDir));

	FString FullPath = FPaths::Combine(OutputDir, Filename);
	return Save(Image, FullPath);
}
