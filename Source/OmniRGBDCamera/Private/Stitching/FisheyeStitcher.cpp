#include "Stitching/FisheyeStitcher.h"
#include "Stitching/IO/RenderOutputPath.h"
#include "Utils/BasaltHelper.h"
#include <filesystem>

// Basalt camera models for projection/unprojection
THIRD_PARTY_INCLUDES_START
#include <basalt/camera/kannala_brandt_camera4.hpp>
#include <basalt/camera/extended_camera.hpp>
THIRD_PARTY_INCLUDES_END

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

DEFINE_LOG_CATEGORY_STATIC(LogFisheyeStitcher, Log, All);

// ------------------------------------------- Constructor / Destructor ------------------------------------------- //
// ---------------------------------------------------------------------------------------------------------------- //

FisheyeStitcher::FisheyeStitcher() {}

FisheyeStitcher::FisheyeStitcher(
	const TArray<FString>& Cameras,
	const FString& NameFolderImage,
	const FString& NameFolderImageDepth)
	: ::ImageStitcher(Cameras, NameFolderImage, NameFolderImageDepth) {}

FisheyeStitcher::~FisheyeStitcher()
{
	ReleaseLookupTables();
}

// ----------------------------------------- Intrinsics Setters ----------------------------------------- //
// ------------------------------------------------------------------------------------------------------ //

void FisheyeStitcher::SetFisheyeProjection(const FisheyeProjection Projection)
{
	CurrentProjection = Projection;
	switch (Projection)
	{
	case FisheyeProjection::Equidistant:
		FisheyeProjectionString = TEXT("Equidistant");
		break;
	case FisheyeProjection::Equisolid:
		FisheyeProjectionString = TEXT("Equisolid");
		break;
	case FisheyeProjection::Orthographic:
		FisheyeProjectionString = TEXT("Orthographic");
		break;
	case FisheyeProjection::Stereographic:
		FisheyeProjectionString = TEXT("Stereographic");
		break;
	case FisheyeProjection::KannalaBrandt:
		FisheyeProjectionString = TEXT("Kannala-Brandt");
		break;
	case FisheyeProjection::ExtendedUnifiedCameraModel:
		FisheyeProjectionString = TEXT("EUCM");
		break;
	default:
		FisheyeProjectionString = TEXT("Unknown");
		break;
	}
}

void FisheyeStitcher::SetIdealFisheyeIntrinsics(const FBasaltIdealFisheyeIntrinsics& Intrinsics)
{
	// Single camera mode - apply to current state
	bMultiCameraMode = false;
	FOV = Intrinsics.FOV;
	OpticalCenterX = Intrinsics.Cx;
	OpticalCenterY = Intrinsics.Cy;
	SetFisheyeProjection(GetProjectionEnumFromString(Intrinsics.ProjectionType));
	FisheyeProjectionString = GetProjectionDisplayName(Intrinsics.ProjectionType);
	ReleaseLookupTables();  // Invalidate cached LUTs when intrinsics change
}

void FisheyeStitcher::SetIdealFisheyeIntrinsicsArray(const TArray<FBasaltIdealFisheyeIntrinsics>& IntrinsicsArray)
{
	IntrinsicsMap.Empty();
	ReleaseLookupTables();  // Invalidate cached LUTs when intrinsics change

	if (IntrinsicsArray.Num() == 0)
	{
		bMultiCameraMode = false;
		return;
	}

	if (IntrinsicsArray.Num() == 1)
	{
		SetIdealFisheyeIntrinsics(IntrinsicsArray[0]);
		return;
	}

	bMultiCameraMode = true;
	for (int32 i = 0; i < IntrinsicsArray.Num(); ++i)
	{
		IntrinsicsMap.Add(i + 1, FCameraIntrinsics(TInPlaceType<FBasaltIdealFisheyeIntrinsics>(), IntrinsicsArray[i]));
	}
}

void FisheyeStitcher::SetKB4Intrinsics(const FBasaltKB4Intrinsics& Intrinsics)
{
	bMultiCameraMode = false;
	FocalLengthX = Intrinsics.Fx;
	FocalLengthY = Intrinsics.Fy;
	OpticalCenterX = static_cast<int32>(Intrinsics.Cx);
	OpticalCenterY = static_cast<int32>(Intrinsics.Cy);
	K[0] = Intrinsics.K1;
	K[1] = Intrinsics.K2;
	K[2] = Intrinsics.K3;
	K[3] = Intrinsics.K4;
	SetFisheyeProjection(FisheyeProjection::KannalaBrandt);
	ReleaseLookupTables();  // Invalidate cached LUTs when intrinsics change
}

void FisheyeStitcher::SetKB4IntrinsicsArray(const TArray<FBasaltKB4Intrinsics>& IntrinsicsArray)
{
	IntrinsicsMap.Empty();
	ReleaseLookupTables();  // Invalidate cached LUTs when intrinsics change

	if (IntrinsicsArray.Num() == 0)
	{
		bMultiCameraMode = false;
		return;
	}

	if (IntrinsicsArray.Num() == 1)
	{
		SetKB4Intrinsics(IntrinsicsArray[0]);
		return;
	}

	bMultiCameraMode = true;
	for (int32 i = 0; i < IntrinsicsArray.Num(); ++i)
	{
		IntrinsicsMap.Add(i + 1, FCameraIntrinsics(TInPlaceType<FBasaltKB4Intrinsics>(), IntrinsicsArray[i]));
	}
	SetFisheyeProjection(FisheyeProjection::KannalaBrandt);
}

void FisheyeStitcher::SetEUCMIntrinsics(const FBasaltEUCMIntrinsics& Intrinsics)
{
	bMultiCameraMode = false;
	FocalLengthX = Intrinsics.Fx;
	FocalLengthY = Intrinsics.Fy;
	OpticalCenterX = static_cast<int32>(Intrinsics.Cx);
	OpticalCenterY = static_cast<int32>(Intrinsics.Cy);
	Alpha = Intrinsics.Alpha;
	Beta = Intrinsics.Beta;
	SetFisheyeProjection(FisheyeProjection::ExtendedUnifiedCameraModel);
	ReleaseLookupTables();  // Invalidate cached LUTs when intrinsics change
}

void FisheyeStitcher::SetEUCMIntrinsicsArray(const TArray<FBasaltEUCMIntrinsics>& IntrinsicsArray)
{
	IntrinsicsMap.Empty();
	ReleaseLookupTables();  // Invalidate cached LUTs when intrinsics change

	if (IntrinsicsArray.Num() == 0)
	{
		bMultiCameraMode = false;
		return;
	}

	if (IntrinsicsArray.Num() == 1)
	{
		SetEUCMIntrinsics(IntrinsicsArray[0]);
		return;
	}

	bMultiCameraMode = true;
	for (int32 i = 0; i < IntrinsicsArray.Num(); ++i)
	{
		IntrinsicsMap.Add(i + 1, FCameraIntrinsics(TInPlaceType<FBasaltEUCMIntrinsics>(), IntrinsicsArray[i]));
	}
	SetFisheyeProjection(FisheyeProjection::ExtendedUnifiedCameraModel);
}

void FisheyeStitcher::ClearMultiCameraIntrinsics()
{
	bMultiCameraMode = false;
	IntrinsicsMap.Empty();
	ReleaseLookupTables();
}

void FisheyeStitcher::InvalidateLookupTables()
{
	ReleaseLookupTables();
}

void FisheyeStitcher::ApplyParamsForCamera(int32 CameraIndex)
{
	if (!bMultiCameraMode)
	{
		return;
	}

	const FCameraIntrinsics* Intrinsics = IntrinsicsMap.Find(CameraIndex);
	if (!Intrinsics)
	{
		UE_LOG(LogFisheyeStitcher, Warning, TEXT("Camera %d: No intrinsics found in multi-camera mode!"), CameraIndex);
		return;
	}

	Visit([this](const auto& Intr)
	{
		using T = std::decay_t<decltype(Intr)>;

		if constexpr (std::is_same_v<T, FBasaltIdealFisheyeIntrinsics>)
		{
			FOV = Intr.FOV;
			OpticalCenterX = Intr.Cx;
			OpticalCenterY = Intr.Cy;
			CurrentProjection = GetProjectionEnumFromString(Intr.ProjectionType);
			FisheyeProjectionString = GetProjectionDisplayName(Intr.ProjectionType);
		}
		else if constexpr (std::is_same_v<T, FBasaltKB4Intrinsics>)
		{
			FocalLengthX = Intr.Fx;
			FocalLengthY = Intr.Fy;
			OpticalCenterX = static_cast<int32>(Intr.Cx);
			OpticalCenterY = static_cast<int32>(Intr.Cy);
			K[0] = Intr.K1;
			K[1] = Intr.K2;
			K[2] = Intr.K3;
			K[3] = Intr.K4;
			CurrentProjection = FisheyeProjection::KannalaBrandt;
			FisheyeProjectionString = TEXT("Kannala-Brandt");
		}
		else if constexpr (std::is_same_v<T, FBasaltEUCMIntrinsics>)
		{
			FocalLengthX = Intr.Fx;
			FocalLengthY = Intr.Fy;
			OpticalCenterX = static_cast<int32>(Intr.Cx);
			OpticalCenterY = static_cast<int32>(Intr.Cy);
			Alpha = Intr.Alpha;
			Beta = Intr.Beta;
			CurrentProjection = FisheyeProjection::ExtendedUnifiedCameraModel;
			FisheyeProjectionString = TEXT("EUCM");
		}
	}, *Intrinsics);
}

// --------------------------------------------------- Stitching -------------------------------------------------- //
// ---------------------------------------------------------------------------------------------------------------- //

void FisheyeStitcher::ProcessDirectories(TArray<int32> ActiveCameras)
{
	if (!ValidateInputOutput())
		return;

	// Scan frame directories
	TArray<FString> RelevantSubdirectories = FRenderOutputPath::GetFrameDirectories(
		this->InputPath, this->FrameMin, this->FrameMax);

	if (RelevantSubdirectories.Num() == 0)
	{
		UE_LOG(LogFisheyeStitcher, Warning, TEXT("No frame directories found in range %s-%s"),
			*this->FrameMin, *this->FrameMax);
		return;
	}

	// Pre-generate all lookup tables BEFORE parallel processing
	const FString FirstFramePath = FPaths::Combine(this->InputPath, RelevantSubdirectories[0]);
	if (!PreGenerateLookupTables(ActiveCameras, FirstFramePath))
	{
		UE_LOG(LogFisheyeStitcher, Error, TEXT("Failed to pre-generate lookup tables!"));
		return;
	}

	// Initialize progress tracking
	ProcessedFrames = 0;
	TotalFrames = RelevantSubdirectories.Num();

	auto start = std::chrono::steady_clock::now();

	ParallelFor(RelevantSubdirectories.Num(), [&](int32 Index)
	{
		const FString& Subdirectory = RelevantSubdirectories[Index];
		ProcessSubdirectory(Subdirectory, ActiveCameras);
		++ProcessedFrames;
	});

	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> duration = end - start;

	// Export masks
	this->ExportValidMasks(ActiveCameras);

	// Copy calibration file to output folder
	FString DestPath = FPaths::Combine(this->OutputPath, this->OutputDirectory, this->CalibrationFile);
	FString SourcePath = this->SourceCalibrationPath.IsEmpty()
		? UBasaltCalibrationReader::GetDefaultCalibrationPath()
		: this->SourceCalibrationPath;

	if (FPaths::FileExists(SourcePath))
	{
		UBasaltCalibrationReader::CopyCalibrationFile(SourcePath, DestPath);
	}
}

bool FisheyeStitcher::PreGenerateLookupTables(const TArray<int32>& ActiveCameras, const FString& SampleFramePath)
{
	// Clear any existing LUTs before generating new ones
	ReleaseLookupTables();

	// Auto-detect cameras if none specified
	TArray<int32> CamerasToUse = ActiveCameras;
	if (CamerasToUse.Num() == 0)
	{
		const FString ImageFolderPath = FPaths::Combine(SampleFramePath, this->NameFolderImage);
		TArray<FString> FoundCameras = FRenderOutputPath::GetCameraDirectories(
			ImageFolderPath, FisheyeDirectory + TEXT("_"));

		for (const FString& CamFolder : FoundCameras)
		{
			FString NumberStr = CamFolder.RightChop(FisheyeDirectory.Len() + 1);
			if (NumberStr.IsNumeric())
			{
				CamerasToUse.Add(FCString::Atoi(*NumberStr));
			}
		}

	}

	if (CamerasToUse.Num() == 0)
	{
		UE_LOG(LogFisheyeStitcher, Warning, TEXT("No cameras found for LUT generation"));
		return false;
	}

	// Find a sample image to determine atlas size
	const int32 FirstCamera = CamerasToUse[0];
	const FString CameraFolder = FString::Printf(TEXT("FisheyeCamera_%d"), FirstCamera);
	const FString SampleImageFolder = FPaths::Combine(SampleFramePath, this->NameFolderImage, CameraFolder);

	TArray<FString> SampleImagePaths;
	CollectImagePaths(SampleImageFolder, SampleImagePaths);

	if (SampleImagePaths.Num() < 5)
	{
		UE_LOG(LogFisheyeStitcher, Error, TEXT("Could not find 5 sample images in %s (found %d)"),
			*SampleImageFolder, SampleImagePaths.Num());
		return false;
	}

	// Load sample images to create atlas
	std::vector<cv::Mat> SampleImages;
	LoadImages(SampleImagePaths, SampleImages);

	if (SampleImages.size() < 5)
	{
		UE_LOG(LogFisheyeStitcher, Error, TEXT("Failed to load sample images"));
		return false;
	}

	cv::Mat InitialAtlas = CubeMap::FromImages5(SampleImages).GetAtlas();
	if (InitialAtlas.empty())
	{
		UE_LOG(LogFisheyeStitcher, Error, TEXT("Failed to create sample atlas"));
		return false;
	}

	// Resize to output resolution
	cv::Mat SampleAtlas;
	cv::resize(InitialAtlas, SampleAtlas, cv::Size(this->ImageWidth, this->ImageWidth), 0, 0, cv::INTER_LINEAR);

	// Generate LUTs sequentially
	if (bMultiCameraMode)
	{
		for (int32 CameraIndex : CamerasToUse)
		{
			ApplyParamsForCamera(CameraIndex);
			GenerateLookupTable(SampleAtlas, CameraIndex);
		}
	}
	else
	{
		GenerateLookupTable(SampleAtlas, 0);
	}

	return true;
}

cv::Mat FisheyeStitcher::StitchSingleFrame(const FString& DirectoryPath, int32 CameraIndex, bool bIsDepth, bool bIsInvDepth)
{
	return StitchImagesInDirectory(DirectoryPath, CameraIndex, bIsDepth, bIsInvDepth);
}

cv::Mat FisheyeStitcher::StitchImagesInDirectory(const FString& DirectoryPath, int32 CameraIndex, bool bIsDepth, bool bIsInvDepth)
{
	TArray<FString> ImagePaths;
	CollectImagePaths(DirectoryPath, ImagePaths);

	cv::Mat StitchedImage;

	if (ImagePaths.Num() == 5)
	{
		std::vector<cv::Mat> Images;
		LoadImages(ImagePaths, Images);

		// Convert depth images to single channel float to save memory
		const bool bIsDepthLike = bIsDepth || bIsInvDepth;
		if (bIsDepthLike)
		{
			for (cv::Mat& Img : Images)
			{
				Img = EnsureSingleChannelFloat(Img);
			}
		}

		cv::Mat InitialAtlas = CubeMap::FromImages5(Images).GetAtlas();
		cv::Mat CubeMapAtlas;
		// Dirty Fix, what if input of basalt has different width and height
		cv::resize(InitialAtlas, CubeMapAtlas, cv::Size(this->ImageWidth, this->ImageWidth), 0, 0, cv::INTER_LINEAR);

		// Get or create lookup table for this camera
		FCameraLookupTable& LUT = GetOrCreateLUT(CameraIndex, CubeMapAtlas);

		StitchedImage = this->CubeMapToImage(CubeMapAtlas, LUT.LookUpTableX, LUT.LookUpTableY);

		// Apply vignette only to color images
		if (this->bApplyVignette && !bIsDepthLike && !LUT.Vignette.empty())
		{
			StitchedImage = ApplyVignette(StitchedImage, LUT.Vignette);
		}
	}
	else
	{
		UE_LOG(LogFisheyeStitcher, Error, TEXT("Expected 5 images, but found %d in %s"), ImagePaths.Num(), *DirectoryPath);
	}

	return StitchedImage;
}

FCameraLookupTable& FisheyeStitcher::GetOrCreateLUT(int32 CameraIndex, const cv::Mat& CubeMapAtlas)
{
	if (!bMultiCameraMode)
	{
		// Single camera mode - use shared LUT
		if (!SingleCameraLUT.bGenerated)
		{
			GenerateLookupTable(CubeMapAtlas, 0);
		}
		else if (bApplyVignette && SingleCameraLUT.Vignette.empty())
		{
			SingleCameraLUT.Vignette = CreateVignetteMask(
				SingleCameraLUT.LookUpTableX.cols,
				SingleCameraLUT.LookUpTableX.rows,
				VignetteStrength, 0);
		}
		return SingleCameraLUT;
	}

	// Multi-camera mode - check if LUT exists for this camera
	FCameraLookupTable* ExistingLUT = CameraLUTs.Find(CameraIndex);
	if (ExistingLUT && ExistingLUT->bGenerated)
	{
		if (bApplyVignette && ExistingLUT->Vignette.empty())
		{
			ApplyParamsForCamera(CameraIndex);
			ExistingLUT->Vignette = CreateVignetteMask(
				ExistingLUT->LookUpTableX.cols,
				ExistingLUT->LookUpTableX.rows,
				VignetteStrength, CameraIndex);
		}
		return *ExistingLUT;
	}

	// Apply params for this camera before generating
	ApplyParamsForCamera(CameraIndex);

	// Create new LUT entry
	FCameraLookupTable& NewLUT = CameraLUTs.FindOrAdd(CameraIndex);
	GenerateLookupTable(CubeMapAtlas, CameraIndex);

	return NewLUT;
}

void FisheyeStitcher::GenerateLookupTable(const cv::Mat& CubeMapAtlas, int32 CameraIndex)
{
	const int FaceWidth = CubeMapAtlas.cols / 3;
	const int FaceHeight = CubeMapAtlas.rows / 3;

	// Output image size (can be non-square like 1920x1080)
	const int OutputWidth = this->ImageWidth;
	const int OutputHeight = this->ImageHeight;

	// Get target LUT
	FCameraLookupTable& TargetLUT = (CameraIndex == 0 || !bMultiCameraMode)
		? SingleCameraLUT
		: CameraLUTs.FindOrAdd(CameraIndex);

	// LUT matches output dimensions (non-square)
	TargetLUT.LookUpTableX.create(OutputHeight, OutputWidth, CV_32FC1);
	TargetLUT.LookUpTableY.create(OutputHeight, OutputWidth, CV_32FC1);
	TargetLUT.LookUpTableX.setTo(cv::Scalar(-1));
	TargetLUT.LookUpTableY.setTo(cv::Scalar(-1));

	// Create valid region mask (255 = inside fisheye circle, 0 = outside)
	TargetLUT.ValidMask = cv::Mat::zeros(OutputHeight, OutputWidth, CV_8UC1);

	// Circle radius based on the SMALLER dimension (circle fits within image height)
	const float CircleRadius = static_cast<float>(FMath::Min(OutputWidth, OutputHeight)) / 2.0f;

	// Scale factor to map circle-space offsets to the original calibration space
	// This makes a smaller circle produce the same FOV as a larger one
	const float OffsetScaleFactor = static_cast<float>(FMath::Max(OutputWidth, OutputHeight))
	                               / static_cast<float>(FMath::Min(OutputWidth, OutputHeight));

	// Center is directly at optical center (no scaling)
	const cv::Point2f Center(
		static_cast<float>(this->OpticalCenterX),
		static_cast<float>(this->OpticalCenterY)
	);

	const float FovRad = static_cast<float>(this->FOV) * static_cast<float>(M_PI) / 180.0f;

	// Iterate over output image dimensions
	for (int y = 0; y < OutputHeight; y++)
	{
		for (int x = 0; x < OutputWidth; x++)
		{
			ProcessPixelScaled(x, y, CircleRadius, OffsetScaleFactor, Center, FovRad, FaceWidth, FaceHeight, CubeMapAtlas,
				TargetLUT.LookUpTableX, TargetLUT.LookUpTableY, TargetLUT.ValidMask);
		}
	}

	// Create vignette if enabled
	if (bApplyVignette)
	{
		TargetLUT.Vignette = CreateVignetteMask(OutputWidth, OutputHeight, VignetteStrength, CameraIndex);
	}

	TargetLUT.bGenerated = true;
}

void FisheyeStitcher::ProcessPixelScaled(int X, int Y, float CircleRadius, float OffsetScaleFactor, const cv::Point2f& Center, float FovRad,
	int FaceWidth, int FaceHeight, const cv::Mat& CubeMapAtlas, cv::Mat& OutLUTX, cv::Mat& OutLUTY, cv::Mat& OutValidMask)
{
	// Calculate pixel offset from center
	const float OffsetX = X - Center.x;
	const float OffsetY = Y - Center.y;

	// Normalize for R <= 1.0 check (based on circle radius)
	const float DX = OffsetX / CircleRadius;
	const float DY = OffsetY / CircleRadius;
	const float R = std::sqrt(DX * DX + DY * DY);

	if (R <= 1.0f)
	{
		// Mark pixel as valid (inside fisheye circle)
		OutValidMask.at<uint8_t>(Y, X) = 255;

		// Scale offsets for camera models (to achieve correct FOV in smaller circle)
		const float ScaledOffsetX = OffsetX * OffsetScaleFactor;
		const float ScaledOffsetY = OffsetY * OffsetScaleFactor;

		double PX, PY, PZ;

		switch (this->CurrentProjection)
		{
		case FisheyeProjection::KannalaBrandt:
			// KB4 uses scaled pixel offsets
			CalculateCartesianCoordinatesKB(ScaledOffsetX, ScaledOffsetY, PX, PY, PZ);
			break;
		case FisheyeProjection::ExtendedUnifiedCameraModel:
			// EUCM uses scaled pixel offsets
			CalculateCartesianCoordinatesEucm(ScaledOffsetX, ScaledOffsetY, PX, PY, PZ);
			break;
		default:
			// Ideal fisheye models use normalized coordinates (no scaling needed, R already normalized)
			{
				const double Phi = std::atan2(DY, DX);
				const double Theta = CalculateTheta(R, FovRad);
				CalculateCartesianCoordinates(Theta, Phi, PX, PY, PZ);
			}
		}

		// Transform from fisheye coordinate system to CubeMap coordinate system
		double CM_X, CM_Y, CM_Z;
		TransformToCubeMapCoords(PX, PY, PZ, CM_X, CM_Y, CM_Z);

		// Use CubeMap class for face/UV determination
		ECubeMapFace Face;
		double U, V;
		CubeMap::DirectionToFaceUV(CM_X, CM_Y, CM_Z, Face, U, V);

		// Skip Back face (not present in 5-camera fisheye setup)
		if (Face == ECubeMapFace::Back)
		{
			return;
		}

		// Convert UV to pixel coordinates using CubeMap class
		int32 SrcX, SrcY;
		CubeMap::FaceUVToPixel(Face, U, V, FaceWidth, FaceHeight, SrcX, SrcY, ECubeMapLayout::Cross3x3);

		if (SrcX >= 0 && SrcX < CubeMapAtlas.cols && SrcY >= 0 && SrcY < CubeMapAtlas.rows)
		{
			OutLUTX.at<float>(Y, X) = static_cast<float>(SrcX);
			OutLUTY.at<float>(Y, X) = static_cast<float>(SrcY);
		}
	}
}

// ---------------------------------------- Coordinate Transformation ---------------------------------------- //

void FisheyeStitcher::TransformToCubeMapCoords(double InX, double InY, double InZ,
											   double& OutX, double& OutY, double& OutZ)
{
	// Transform from fisheye coordinate system to CubeMap coordinate system
	// Fisheye: Z+ = Forward, Y- = Up, X+ = Right
	// CubeMap: Y+ = Forward, Z+ = Up, X- = Right
	OutX = -InX;
	OutY = InZ;
	OutZ = -InY;
}

// ---------------------------------- Ideal Fisheye ---------------------------------- //

double FisheyeStitcher::CalculateTheta(float R, float FovRad)
{
	switch (this->CurrentProjection)
	{
	case FisheyeProjection::Equidistant:
		// r = theta
		return R * (FovRad / 2);
	case FisheyeProjection::Equisolid:
		// r = 2*sin(theta/2)
		return 2 * std::asin(R * std::sin(FovRad / 4));
	case FisheyeProjection::Orthographic:
		// r = sin(theta)
		return std::asin(R * std::sin(FovRad / 2));
	case FisheyeProjection::Stereographic:
		// r = 2 * tan(theta / 2)
		return 2 * std::atan(R * std::tan(FovRad / 4));
	default:
		return 0;
	}
}

// ---------------------------------- Kannala Brandt ---------------------------------- //

void FisheyeStitcher::CalculateCartesianCoordinatesKB(const float DX, const float DY, double& PX, double& PY, double& PZ)
{
	using KB4 = basalt::KannalaBrandtCamera4<double>;

	// Build parameter vector [fx, fy, cx, cy, k1, k2, k3, k4]
	// Note: We set cx=cy=0 since DX/DY are already offsets from optical center
	Eigen::Matrix<double, 8, 1> params;
	params << static_cast<double>(FocalLengthX),
	          static_cast<double>(FocalLengthY),
	          0.0, 0.0,  // cx, cy = 0 (DX/DY are already relative to center)
	          K[0], K[1], K[2], K[3];

	KB4 camera(params);

	// Unproject pixel offset to 3D ray
	Eigen::Vector2d pixel(static_cast<double>(DX), static_cast<double>(DY));
	Eigen::Vector3d ray;

	if (camera.unproject(pixel, ray))
	{
		PX = ray.x();
		PY = ray.y();
		PZ = ray.z();
	}
	else
	{
		PX = PY = PZ = 0.0;
	}
}

// --------------------------- Extended unified camera Model -------------------------- //

void FisheyeStitcher::CalculateCartesianCoordinatesEucm(const float DX, const float DY, double& PX, double& PY, double& PZ)
{
	using EUCM = basalt::ExtendedUnifiedCamera<double>;

	// Build parameter vector [fx, fy, cx, cy, alpha, beta]
	// Note: We set cx=cy=0 since DX/DY are already offsets from optical center
	Eigen::Matrix<double, 6, 1> params;
	params << static_cast<double>(FocalLengthX),
	          static_cast<double>(FocalLengthY),
	          0.0, 0.0,  // cx, cy = 0 (DX/DY are already relative to center)
	          static_cast<double>(Alpha),
	          static_cast<double>(Beta);

	EUCM camera(params);

	// Unproject pixel offset to 3D ray
	Eigen::Vector2d pixel(static_cast<double>(DX), static_cast<double>(DY));
	Eigen::Vector3d ray;

	if (camera.unproject(pixel, ray))
	{
		PX = ray.x();
		PY = ray.y();
		PZ = ray.z();
	}
	else
	{
		PX = PY = PZ = 0.0;
	}
}

// -------------------------------------------------- Vignette -------------------------------------------------- //
// -------------------------------------------------------------------------------------------------------------- //

cv::Mat FisheyeStitcher::CreateVignetteMask(int Width, int Height, double Strength, int32 CameraIndex)
{
	// Get center for this camera
	float CenterX = static_cast<float>(OpticalCenterX);
	float CenterY = static_cast<float>(OpticalCenterY);

	if (bMultiCameraMode)
	{
		if (const FCameraIntrinsics* Intrinsics = IntrinsicsMap.Find(CameraIndex))
		{
			Visit([&CenterX, &CenterY](const auto& Intr)
			{
				CenterX = static_cast<float>(Intr.Cx);
				CenterY = static_cast<float>(Intr.Cy);
			}, *Intrinsics);
		}
	}

	const float radius = std::min(Width, Height) / 2.0f;

	// Create coordinate grids (vectorized, much faster than pixel-by-pixel)
	cv::Mat XCoords(Height, Width, CV_32FC1);
	cv::Mat YCoords(Height, Width, CV_32FC1);

	for (int y = 0; y < Height; ++y)
	{
		float* xRow = XCoords.ptr<float>(y);
		float* yRow = YCoords.ptr<float>(y);
		const float dy = (y - CenterY) / radius;
		for (int x = 0; x < Width; ++x)
		{
			xRow[x] = (x - CenterX) / radius;
			yRow[x] = dy;
		}
	}

	// Calculate distance: sqrt(dx^2 + dy^2)
	cv::Mat Distance;
	cv::magnitude(XCoords, YCoords, Distance);

	// Create mask for valid pixels (inside circle)
	cv::Mat ValidMask = Distance <= 1.0f;

	// Calculate vignette: pow(1 - pow(distance, 10), strength)
	cv::Mat AdjustedDistance;
	cv::pow(Distance, 10.0, AdjustedDistance);

	cv::Mat OneMinusAdj = 1.0f - AdjustedDistance;
	cv::max(OneMinusAdj, 0.0f, OneMinusAdj);  // Clamp to >= 0

	cv::Mat VignetteMask;
	cv::pow(OneMinusAdj, Strength, VignetteMask);

	// Apply circle mask (0 outside, vignette inside)
	cv::Mat Result = cv::Mat::zeros(Height, Width, CV_32FC1);
	VignetteMask.copyTo(Result, ValidMask);

	return Result;
}

cv::Mat FisheyeStitcher::ApplyVignette(const cv::Mat& StitchedImage, const cv::Mat& VignetteMask)
{
	// Size mismatch check
	if (StitchedImage.cols != VignetteMask.cols || StitchedImage.rows != VignetteMask.rows)
	{
		UE_LOG(LogFisheyeStitcher, Error, TEXT("ApplyVignette: SIZE MISMATCH! Image=%dx%d, Mask=%dx%d"),
			StitchedImage.cols, StitchedImage.rows, VignetteMask.cols, VignetteMask.rows);
		return StitchedImage;
	}

	cv::Mat result;
	StitchedImage.convertTo(result, CV_32FC4);

	std::vector<cv::Mat> channels(4);
	cv::split(result, channels);

	// Multiply RGB channels with vignette mask (alpha unchanged)
	for (int i = 0; i < 3; ++i)
	{
		channels[i] = channels[i].mul(VignetteMask);
	}

	cv::merge(channels, result);
	result.convertTo(result, StitchedImage.type());

	return result;
}

void FisheyeStitcher::ExportVignette()
{
	cv::Mat Mask = this->CreateVignetteMask(this->ImageWidth, this->ImageHeight, this->VignetteStrength, 0);

	if (Mask.empty())
	{
		UE_LOG(LogFisheyeStitcher, Error, TEXT("Cannot save empty mask"));
		return;
	}

	FString OutputDir = FPaths::Combine(this->OutputPath, this->OutputDirectory);
	std::filesystem::create_directories(*OutputDir);

	FString FullOutputPath = FPaths::Combine(OutputDir, "mask.png");
	std::string OutputPathStr = TCHAR_TO_UTF8(*FullOutputPath);

	try
	{
		cv::imwrite(OutputPathStr, Mask * 255);
	}
	catch (cv::Exception& e)
	{
		UE_LOG(LogFisheyeStitcher, Warning, TEXT("Save: %s"), *FString(e.msg.c_str()));
	}
}

void FisheyeStitcher::ExportValidMasks(const TArray<int32>& ActiveCameras)
{
	FString OutputDir = FPaths::Combine(this->OutputPath, this->OutputDirectory);
	std::filesystem::create_directories(*OutputDir);

	// Helper lambda to save a mask
	auto SaveMask = [&](const cv::Mat& Mask, const FString& Filename)
	{
		if (Mask.empty())
		{
			UE_LOG(LogFisheyeStitcher, Warning, TEXT("Cannot save empty mask: %s"), *Filename);
			return;
		}

		FString FullPath = FPaths::Combine(OutputDir, Filename);
		std::string PathStr = TCHAR_TO_UTF8(*FullPath);

		try
		{
			cv::imwrite(PathStr, Mask);
		}
		catch (cv::Exception& e)
		{
			UE_LOG(LogFisheyeStitcher, Warning, TEXT("Failed to save mask %s: %s"), *Filename, *FString(e.msg.c_str()));
		}
	};

	// Helper lambda to save vignette (float 0-1 -> 8-bit 0-255)
	auto SaveVignette = [&](const cv::Mat& Vignette, const FString& Filename)
	{
		if (Vignette.empty())
		{
			return;
		}

		FString FullPath = FPaths::Combine(OutputDir, Filename);
		std::string PathStr = TCHAR_TO_UTF8(*FullPath);

		try
		{
			cv::Mat Vignette8U;
			Vignette.convertTo(Vignette8U, CV_8UC1, 255.0);
			cv::imwrite(PathStr, Vignette8U);
		}
		catch (cv::Exception& e)
		{
			UE_LOG(LogFisheyeStitcher, Warning, TEXT("Failed to save vignette %s: %s"), *Filename, *FString(e.msg.c_str()));
		}
	};

	if (!bMultiCameraMode)
	{
		// Single camera mode - save one mask and vignette
		if (SingleCameraLUT.bGenerated)
		{
			if (!SingleCameraLUT.ValidMask.empty())
			{
				SaveMask(SingleCameraLUT.ValidMask, TEXT("fisheye_mask.png"));
			}
			if (bApplyVignette && !SingleCameraLUT.Vignette.empty())
			{
				SaveVignette(SingleCameraLUT.Vignette, TEXT("fisheye_vignette.png"));
			}
		}
	}
	else
	{
		// Multi-camera mode - save mask and vignette for each camera
		for (int32 CameraIndex : ActiveCameras)
		{
			if (FCameraLookupTable* LUT = CameraLUTs.Find(CameraIndex))
			{
				if (LUT->bGenerated)
				{
					if (!LUT->ValidMask.empty())
					{
						FString MaskFilename = FString::Printf(TEXT("fisheye_mask_%d.png"), CameraIndex);
						SaveMask(LUT->ValidMask, MaskFilename);
					}
					if (bApplyVignette && !LUT->Vignette.empty())
					{
						FString VignetteFilename = FString::Printf(TEXT("fisheye_vignette_%d.png"), CameraIndex);
						SaveVignette(LUT->Vignette, VignetteFilename);
					}
				}
			}
		}
	}
}

// ----------------------------------------------- File Handling ------------------------------------------------ //
// -------------------------------------------------------------------------------------------------------------- //

void FisheyeStitcher::ProcessSubdirectory(const FString& Subdirectory, TArray<int32> ActiveCameras)
{
	const FString FullSubdirectoryPath = FPaths::Combine(InputPath, Subdirectory);
	const FString ImageFolderPathBase = FPaths::Combine(FullSubdirectoryPath, NameFolderImage);
	const FString DepthImageFolderPathBase = FPaths::Combine(FullSubdirectoryPath, NameFolderDepthImage);

	// If no cameras specified, auto-detect from directory
	TArray<int32> CamerasToProcess = ActiveCameras;
	if (CamerasToProcess.Num() == 0)
	{
		TArray<FString> FoundCameras = FRenderOutputPath::GetCameraDirectories(
			ImageFolderPathBase, FisheyeDirectory + TEXT("_"));

		for (const FString& CameraFolder : FoundCameras)
		{
			// Extract camera number from "FisheyeCamera_N"
			FString NumberStr = CameraFolder.RightChop(FisheyeDirectory.Len() + 1);
			if (NumberStr.IsNumeric())
			{
				CamerasToProcess.Add(FCString::Atoi(*NumberStr));
			}
		}
	}

	// Process each camera
	for (int32 CameraNumber : CamerasToProcess)
	{
		const FString CameraFolder = FString::Printf(TEXT("%s_%d"), *FisheyeDirectory, CameraNumber);

		if (bStitchColorImages)
		{
			ProcessImageFolder(CameraFolder, ImageFolderPathBase, Subdirectory, CameraNumber, EStitchImageType::Color);
		}

		if (bStitchDepthImages)
		{
			ProcessImageFolder(CameraFolder, DepthImageFolderPathBase, Subdirectory, CameraNumber, EStitchImageType::Depth);
		}

		if (bStitchInvDepthImages)
		{
			ProcessImageFolder(CameraFolder, DepthImageFolderPathBase, Subdirectory, CameraNumber, EStitchImageType::InvDepth);
		}
	}
}

void FisheyeStitcher::ProcessImageFolder(const FString& CameraFolder,
                                         const FString& FolderPathBase,
                                         const FString& Subdirectory,
                                         int32 CameraNumber,
                                         EStitchImageType ImageType)
{
	const FString FullFolderPath = FPaths::Combine(FolderPathBase, CameraFolder);

	if (!FPaths::DirectoryExists(FullFolderPath))
	{
		UE_LOG(LogFisheyeStitcher, Warning, TEXT("Subdirectory %s missing in: %s"), *CameraFolder, *FolderPathBase);
		return;
	}

	const bool bIsDepth = (ImageType == EStitchImageType::Depth);
	const bool bIsInvDepth = (ImageType == EStitchImageType::InvDepth);

	cv::Mat StitchedImage = StitchImagesInDirectory(FullFolderPath, CameraNumber, bIsDepth, bIsInvDepth);

	if (StitchedImage.empty())
	{
		UE_LOG(LogFisheyeStitcher, Warning, TEXT("Stitching failed for %s"), *FullFolderPath);
		return;
	}
	
	if (ImageType == EStitchImageType::InvDepth)
	{
		StitchedImage = ConvertDepthToInverseDepth(StitchedImage);
	}

	const FString OutputName = CameraFolder + GetStitchOutputSuffix(ImageType);
	SaveStitchedImage(StitchedImage, Subdirectory, OutputName);
}

// ------------------------------------------------ Calibration ------------------------------------------------- //
// -------------------------------------------------------------------------------------------------------------- //

void FisheyeStitcher::ExportCameraIntrinsicToJson(TArray<int32> ActiveCameras)
{
	FString Path = FPaths::Combine(this->OutputPath, this->OutputDirectory, this->CalibrationFile);

	// Build array of camera calibrations for Basalt export
	TArray<FBasaltCameraCalibration> Cameras;

	for (int32 CameraNumber : ActiveCameras)
	{
		// Get params for this camera
		if (bMultiCameraMode)
		{
			ApplyParamsForCamera(CameraNumber);
		}

		FBasaltCameraCalibration Cam;
		Cam.Resolution.Width = this->ImageWidth;
		Cam.Resolution.Height = this->ImageHeight;

		if (this->CurrentProjection == FisheyeProjection::ExtendedUnifiedCameraModel)
		{
			Cam.CameraType = TEXT("eucm");
			Cam.EUCMIntrinsics.Fx = this->FocalLengthX;
			Cam.EUCMIntrinsics.Fy = this->FocalLengthY;
			Cam.EUCMIntrinsics.Cx = static_cast<float>(this->OpticalCenterX);
			Cam.EUCMIntrinsics.Cy = static_cast<float>(this->OpticalCenterY);
			Cam.EUCMIntrinsics.Alpha = this->Alpha;
			Cam.EUCMIntrinsics.Beta = this->Beta;
		}
		else if (this->CurrentProjection == FisheyeProjection::KannalaBrandt)
		{
			Cam.CameraType = TEXT("kb4");
			Cam.KB4Intrinsics.Fx = this->FocalLengthX;
			Cam.KB4Intrinsics.Fy = this->FocalLengthY;
			Cam.KB4Intrinsics.Cx = static_cast<float>(this->OpticalCenterX);
			Cam.KB4Intrinsics.Cy = static_cast<float>(this->OpticalCenterY);
			Cam.KB4Intrinsics.K1 = static_cast<float>(this->K[0]);
			Cam.KB4Intrinsics.K2 = static_cast<float>(this->K[1]);
			Cam.KB4Intrinsics.K3 = static_cast<float>(this->K[2]);
			Cam.KB4Intrinsics.K4 = static_cast<float>(this->K[3]);
		}
		else
		{
			// For ideal fisheye models (equidistant, equisolid, etc.)
			// Store as pinhole with FOV info (Basalt doesn't have these models natively)
			Cam.CameraType = TEXT("pinhole");
			float ApproxFocal = StitchingConstants::FocalLengthFromFOV(
				static_cast<float>(this->ImageWidth), static_cast<float>(this->FOV));
			Cam.PinholeIntrinsics.Fx = ApproxFocal;
			Cam.PinholeIntrinsics.Fy = ApproxFocal;
			Cam.PinholeIntrinsics.Cx = static_cast<float>(this->OpticalCenterX);
			Cam.PinholeIntrinsics.Cy = static_cast<float>(this->OpticalCenterY);
		}

		Cameras.Add(Cam);
	}

	// Use Basalt helper to update the calibration file
	UBasaltCalibrationReader::UpdateCalibrationWithIntrinsics(Path, Cameras);
}

// --------------------------------------------------- Cleanup -------------------------------------------------- //
// -------------------------------------------------------------------------------------------------------------- //

void FisheyeStitcher::ReleaseLookupTables()
{
	SingleCameraLUT.Release();

	for (auto& Pair : CameraLUTs)
	{
		Pair.Value.Release();
	}
	CameraLUTs.Empty();

	UE_LOG(LogFisheyeStitcher, Verbose, TEXT("Released all lookup tables"));
}
