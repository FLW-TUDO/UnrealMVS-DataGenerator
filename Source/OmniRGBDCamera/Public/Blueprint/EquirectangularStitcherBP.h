#pragma once

#include "CoreMinimal.h"
#include "Blueprint/StitcherBPBase.h"
#include "Stitching/EquirectangularStitcher.h"

#include "EquirectangularStitcherBP.generated.h"

class EquirectangularStitcher;

UCLASS(BlueprintType)
class OMNIRGBDCAMERA_API UEquirectangularStitcherBP : public UStitcherBPBase
{
	GENERATED_BODY()

public:
	// --------------------------------- Resolution --------------------------------- //

	UFUNCTION(BlueprintCallable, Category="Stitching|Equirectangular|Resolution")
	void SetOutputResolution(int32 InWidth, int32 InHeight);

	UFUNCTION(BlueprintCallable, Category="Stitching|Equirectangular|Resolution")
	void SetOutputWidth(int32 InWidth);

	UFUNCTION(BlueprintCallable, Category="Stitching|Equirectangular|Resolution")
	void SetOutputHeight(int32 InHeight);

	// --------------------------------- Initialization --------------------------------- //

	UFUNCTION(BlueprintCallable, Category="Stitching|Equirectangular")
	bool Initialize(const TArray<FString>& Cameras,
					const FString& NameFolderImage,
					const FString& NameFolderDepthImage);

	UFUNCTION(BlueprintCallable, Category="Stitching|Equirectangular")
	bool ApplySettings(const FString& InInputPath,
					   const FString& InOutputPath,
					   const FString& InOutputDirectory,
					   const FString& InFrameMin,
					   const FString& InFrameMax,
					   bool bInStitchColorImages,
					   bool bInStitchDepthImages,
					   bool bInStitchInvDepthImages,
					   int32 InVerticalFOV,
					   bool bInLeaveBlackBars);

	// --------------------------------- Processing --------------------------------- //
	
	/**
	 * Asynchronous processing - returns immediately, UI stays responsive.
	 * Fires OnStitchingComplete event when done.
	 * Check bIsStitching to see if currently processing.
	 */
	UFUNCTION(BlueprintCallable, Category="Stitching|Equirectangular")
	void ProcessDirectories();

protected:
	// UStitcherBPBase interface
	virtual bool GetStitcherProgress(int32& OutCurrent, int32& OutTotal) const override;
	virtual FString GetLogPrefix() const override { return TEXT("[EquiStitcherBP]"); }

private:
	TUniquePtr<EquirectangularStitcher> Stitcher;
};
