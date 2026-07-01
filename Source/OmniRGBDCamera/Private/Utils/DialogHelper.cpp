#include "Utils/DialogHelper.h"
#include "Stitching/IO/RenderOutputPath.h"

#include "Components/EditableTextBox.h"

#if WITH_EDITOR
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SEditableTextBox.h"
#endif

bool UFileIO::OpenFileDialog(
	const FString& DialogTitle,
	const FString& DefaultPath,
	UEditableTextBox* TargetTextBox,
	FString& OutDirectoryPath
)
{
	OutDirectoryPath.Reset();

#if WITH_EDITOR
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return false;
	}

	const void* ParentWindowHandle = nullptr;
	if (FSlateApplication::IsInitialized())
	{
		ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	}

	FString StartPath = !DefaultPath.IsEmpty()
		? DefaultPath
		: (FPaths::ProjectSavedDir() / TEXT("MovieRenders"));

	bool bSuccess = DesktopPlatform->OpenDirectoryDialog(
		ParentWindowHandle,
		DialogTitle,
		StartPath,
		OutDirectoryPath
	);

	if (bSuccess && TargetTextBox)
	{
		TargetTextBox->SetText(FText::FromString(OutDirectoryPath));
	}

	return bSuccess;
#else
	return false;
#endif
}

bool UFileIO::CopyFile(const FString& SourcePath, const FString& DestinationPath, bool bOverwrite)
{
	const FString NormalizedSource = FPaths::ConvertRelativePathToFull(SourcePath);
	const FString FileName = FPaths::GetCleanFilename(SourcePath);
	const FString NormalizedDest = FPaths::ConvertRelativePathToFull(FPaths::Combine(DestinationPath, FileName));

	IFileManager& FileManager = IFileManager::Get();

	if (!FileManager.FileExists(*NormalizedSource))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FileIO] CopyFile failed: Source does not exist: %s"), *NormalizedSource);
		return false;
	}

	const FString DestDir = FPaths::GetPath(NormalizedDest);
	if (!DestDir.IsEmpty() && !FileManager.DirectoryExists(*DestDir))
	{
		FileManager.MakeDirectory(*DestDir, true);
	}

	const uint32 Result = FileManager.Copy(
		*NormalizedDest,
		*NormalizedSource,
		bOverwrite,
		true,
		false
	);

	const bool bSuccess = (Result == COPY_OK);
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FileIO] CopyFile failed (%u): %s -> %s"), Result, *NormalizedSource, *NormalizedDest);
	}
	return bSuccess;
}

bool UFileIO::GetFrameRange(
	const FString& BaseDirectory,
	int32& OutMinFrame,
	int32& OutMaxFrame)
{
	return FRenderOutputPath::GetFrameRange(BaseDirectory, OutMinFrame, OutMaxFrame);
}
