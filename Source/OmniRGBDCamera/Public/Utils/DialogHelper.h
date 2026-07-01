#pragma once

#include "DialogHelper.generated.h"

class UEditableTextBox;

/**
 * @class UFileIO
 * @brief Blueprint function library for file dialogs and basic file operations.
 *
 * Note: Class name kept as UFileIO for Blueprint compatibility.
 * Use FRenderOutputPath for directory scanning operations.
 */
UCLASS()
class UFileIO : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * @brief Opens a directory picker dialog.
	 * @param DialogTitle Title for the dialog window.
	 * @param DefaultPath Initial path to show in the dialog.
	 * @param TargetTextBox Optional text box to update with the selected path.
	 * @param OutDirectoryPath Output: the selected directory path.
	 * @return True if a directory was selected.
	 */
	UFUNCTION(BlueprintCallable, Category="FileIO")
	static bool OpenFileDialog(
		const FString& DialogTitle,
		const FString& DefaultPath,
		UEditableTextBox* TargetTextBox,
		FString& OutDirectoryPath
	);

	/**
	 * @brief Copies a file to a destination directory.
	 * @param SourcePath Full path to the source file.
	 * @param DestinationPath Destination directory path.
	 * @param bOverwrite Whether to overwrite existing files.
	 * @return True if the copy succeeded.
	 */
	UFUNCTION(BlueprintCallable, Category="FileIO")
	static bool CopyFile(
		const FString& SourcePath,
		const FString& DestinationPath,
		bool bOverwrite = true
	);

	/**
	 * @brief Gets the minimum and maximum frame numbers in a render output directory.
	 * @param BaseDirectory Directory containing numeric frame folders (e.g., 0000, 0001, ...).
	 * @param OutMinFrame Output: minimum frame number found.
	 * @param OutMaxFrame Output: maximum frame number found.
	 * @return True if at least one frame folder was found.
	 */
	UFUNCTION(BlueprintCallable, Category="FileIO")
	static bool GetFrameRange(
		const FString& BaseDirectory,
		int32& OutMinFrame,
		int32& OutMaxFrame
	);
};
