#include "Stitching/IO/RenderOutputPath.h"

// --------------------------------- Frame Directories --------------------------------- //

TArray<FString> FRenderOutputPath::GetFrameDirectories(
	const FString& BasePath,
	const FString& FrameMin,
	const FString& FrameMax)
{
	TArray<FString> Result;

	if (!FPaths::DirectoryExists(BasePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[RenderOutputPath] Base path does not exist: %s"), *BasePath);
		return Result;
	}

	const int32 FrameMinInt = FCString::Atoi(*FrameMin);
	const int32 FrameMaxInt = FCString::Atoi(*FrameMax);

	// Find all subdirectories
	TArray<FString> AllDirectories;
	FString SearchPath = FPaths::Combine(BasePath, TEXT("*"));
	IFileManager::Get().FindFiles(AllDirectories, *SearchPath, false, true);

	// Filter by numeric name and frame range
	for (const FString& DirName : AllDirectories)
	{
		if (!IsNumericDirectory(DirName))
		{
			continue;
		}

		const int32 FrameNumber = FCString::Atoi(*DirName);
		if (FrameNumber >= FrameMinInt && FrameNumber <= FrameMaxInt)
		{
			Result.Add(DirName);
		}
	}

	// Sort numerically
	Result.Sort([](const FString& A, const FString& B)
	{
		return FCString::Atoi(*A) < FCString::Atoi(*B);
	});

	return Result;
}

bool FRenderOutputPath::GetFrameRange(
	const FString& BasePath,
	int32& OutMin,
	int32& OutMax)
{
	OutMin = 0;
	OutMax = 0;

	if (!IFileManager::Get().DirectoryExists(*BasePath))
	{
		return false;
	}

	TArray<FString> SubDirs;
	IFileManager::Get().FindFiles(
		SubDirs,
		*(BasePath / TEXT("*")),
		false,
		true
	);

	bool bFoundAny = false;

	for (const FString& DirName : SubDirs)
	{
		if (!IsNumericDirectory(DirName))
		{
			continue;
		}

		int32 Value = FCString::Atoi(*DirName);

		if (!bFoundAny)
		{
			OutMin = Value;
			OutMax = Value;
			bFoundAny = true;
		}
		else
		{
			OutMin = FMath::Min(OutMin, Value);
			OutMax = FMath::Max(OutMax, Value);
		}
	}

	return bFoundAny;
}

// --------------------------------- Camera Directories --------------------------------- //

TArray<FString> FRenderOutputPath::GetCameraDirectories(
	const FString& TypeFolderPath,
	const FString& CameraPrefix)
{
	TArray<FString> Result;

	if (!FPaths::DirectoryExists(TypeFolderPath))
	{
		return Result;
	}

	FString SearchPattern = FPaths::Combine(TypeFolderPath, CameraPrefix + TEXT("*"));
	IFileManager::Get().FindFiles(Result, *SearchPattern, false, true);

	Result.Sort();
	return Result;
}

// --------------------------------- Image Paths --------------------------------- //

void FRenderOutputPath::CollectImagePaths(
	const FString& DirectoryPath,
	const TArray<FString>& CameraNames,
	const FString& Extension,
	TArray<FString>& OutImagePaths)
{
	for (const FString& CameraName : CameraNames)
	{
		FString ImagePath = FPaths::Combine(DirectoryPath, CameraName + TEXT(".") + Extension);

		if (FPaths::FileExists(ImagePath))
		{
			OutImagePaths.Add(ImagePath);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[RenderOutputPath] Image missing: %s"), *ImagePath);
		}
	}
}

FString FRenderOutputPath::GetFirstImagePath(
	const FString& DirectoryPath,
	const FString& Extension)
{
	if (!FPaths::DirectoryExists(DirectoryPath))
	{
		return FString();
	}

	TArray<FString> ImageFiles;
	FString SearchPattern = FPaths::Combine(DirectoryPath, TEXT("*.") + Extension);
	IFileManager::Get().FindFiles(ImageFiles, *SearchPattern, true, false);

	if (ImageFiles.Num() > 0)
	{
		return FPaths::Combine(DirectoryPath, ImageFiles[0]);
	}

	return FString();
}

// --------------------------------- Path Building --------------------------------- //

FString FRenderOutputPath::BuildCameraPath(
	const FString& BasePath,
	const FString& FrameDir,
	const FString& TypeFolder,
	const FString& CameraFolder)
{
	return FPaths::Combine(BasePath, FrameDir, TypeFolder, CameraFolder);
}

// --------------------------------- Validation --------------------------------- //

bool FRenderOutputPath::HasFilesWithExtension(
	const FString& DirectoryPath,
	const FString& Extension)
{
	if (!FPaths::DirectoryExists(DirectoryPath))
	{
		return false;
	}

	TArray<FString> Files;
	FString SearchPattern = FPaths::Combine(DirectoryPath, TEXT("*.") + Extension);
	IFileManager::Get().FindFiles(Files, *SearchPattern, true, false);

	return Files.Num() > 0;
}

bool FRenderOutputPath::IsNumericDirectory(const FString& DirectoryName)
{
	if (DirectoryName.IsEmpty())
	{
		return false;
	}

	for (int32 i = 0; i < DirectoryName.Len(); ++i)
	{
		if (!FChar::IsDigit(DirectoryName[i]))
		{
			return false;
		}
	}

	return true;
}
