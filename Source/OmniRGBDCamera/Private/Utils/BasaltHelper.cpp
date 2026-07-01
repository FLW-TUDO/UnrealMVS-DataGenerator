#include "Utils/BasaltHelper.h"

#include "Components/EditableTextBox.h"
#include "Misc/FileHelper.h"

// Basalt headers - only camera models, NOT the full serialization
THIRD_PARTY_INCLUDES_START
#include <basalt/camera/generic_camera.hpp>
#include <basalt/camera/kannala_brandt_camera4.hpp>
#include <basalt/camera/extended_camera.hpp>
#include <basalt/camera/double_sphere_camera.hpp>
#include <basalt/camera/pinhole_camera.hpp>
THIRD_PARTY_INCLUDES_END

// UE JSON parsing
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "OpenCVHelper.h"

#if WITH_EDITOR
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SEditableTextBox.h"
#endif

// ============================================================================================================== //
// Logging Category
// ============================================================================================================== //

DEFINE_LOG_CATEGORY_STATIC(LogBasalt, Log, All);

// ============================================================================================================== //
// Internal Helper Functions - Robust JSON parsing without full Cereal dependency
// ============================================================================================================== //

namespace BasaltInternal
{
	/**
	 * Parsed calibration data - only the fields we need
	 */
	struct ParsedCalibration
	{
		TArray<FTransform> Transforms;
		TArray<FString> CameraTypes;
		TArray<FIntPoint> Resolutions;

		// Intrinsics stored as raw parameter arrays
		TArray<TArray<double>> IntrinsicParams;
	};

	/**
	 * Parses a single SE3 transform from JSON object
	 */
	bool ParseSE3Transform(const TSharedPtr<FJsonObject>& PoseObj, FTransform& OutTransform, float Scale, bool bConvertToUnreal)
	{
		if (!PoseObj.IsValid())
		{
			return false;
		}

		double px, py, pz, qx, qy, qz, qw;
		const bool bOk =
			PoseObj->TryGetNumberField(TEXT("px"), px) &&
			PoseObj->TryGetNumberField(TEXT("py"), py) &&
			PoseObj->TryGetNumberField(TEXT("pz"), pz) &&
			PoseObj->TryGetNumberField(TEXT("qx"), qx) &&
			PoseObj->TryGetNumberField(TEXT("qy"), qy) &&
			PoseObj->TryGetNumberField(TEXT("qz"), qz) &&
			PoseObj->TryGetNumberField(TEXT("qw"), qw);

		if (!bOk)
		{
			UE_LOG(LogBasalt, Warning, TEXT("  SE3 transform missing required fields (px,py,pz,qx,qy,qz,qw)"));
			return false;
		}

		const FVector Translation(
			static_cast<float>(px) * Scale,
			static_cast<float>(py) * Scale,
			static_cast<float>(pz) * Scale
		);

		FQuat Rotation(
			static_cast<float>(qx),
			static_cast<float>(qy),
			static_cast<float>(qz),
			static_cast<float>(qw)
		);
		Rotation.Normalize();

		OutTransform = FTransform(Rotation, Translation, FVector::OneVector);

		if (bConvertToUnreal)
		{
			FOpenCVHelper::ConvertOpenCVToUnreal(OutTransform);
		}

		return true;
	}

	/**
	 * Parses camera intrinsics from JSON object
	 */
	bool ParseCameraIntrinsics(const TSharedPtr<FJsonObject>& CamObj, FString& OutType, TArray<double>& OutParams)
	{
		if (!CamObj.IsValid())
		{
			return false;
		}

		// Get camera type
		if (!CamObj->TryGetStringField(TEXT("camera_type"), OutType))
		{
			UE_LOG(LogBasalt, Warning, TEXT("  Camera missing 'camera_type' field"));
			return false;
		}

		OutType = OutType.ToLower();
		UE_LOG(LogBasalt, Log, TEXT("  Parsing camera type: %s"), *OutType);

		// Get intrinsics object
		const TSharedPtr<FJsonObject>* IntrinsicsObjPtr = nullptr;
		if (!CamObj->TryGetObjectField(TEXT("intrinsics"), IntrinsicsObjPtr) || !IntrinsicsObjPtr)
		{
			UE_LOG(LogBasalt, Warning, TEXT("  Camera missing 'intrinsics' object"));
			return false;
		}
		const TSharedPtr<FJsonObject>& IntrinsicsObj = *IntrinsicsObjPtr;

		// Parse based on camera type
		OutParams.Reset();

		if (OutType == TEXT("kb4") || OutType == TEXT("kb"))
		{
			// KB4: fx, fy, cx, cy, k1, k2, k3, k4
			double fx, fy, cx, cy, k1, k2, k3, k4;
			fx = IntrinsicsObj->GetNumberField(TEXT("fx"));
			fy = IntrinsicsObj->GetNumberField(TEXT("fy"));
			cx = IntrinsicsObj->GetNumberField(TEXT("cx"));
			cy = IntrinsicsObj->GetNumberField(TEXT("cy"));
			k1 = IntrinsicsObj->HasField(TEXT("k1")) ? IntrinsicsObj->GetNumberField(TEXT("k1")) : 0.0;
			k2 = IntrinsicsObj->HasField(TEXT("k2")) ? IntrinsicsObj->GetNumberField(TEXT("k2")) : 0.0;
			k3 = IntrinsicsObj->HasField(TEXT("k3")) ? IntrinsicsObj->GetNumberField(TEXT("k3")) : 0.0;
			k4 = IntrinsicsObj->HasField(TEXT("k4")) ? IntrinsicsObj->GetNumberField(TEXT("k4")) : 0.0;

			OutParams = {fx, fy, cx, cy, k1, k2, k3, k4};
			OutType = TEXT("kb4"); // Normalize
			UE_LOG(LogBasalt, Log, TEXT("    KB4: fx=%.2f, fy=%.2f, cx=%.2f, cy=%.2f, k1=%.4f, k2=%.4f, k3=%.4f, k4=%.4f"),
				fx, fy, cx, cy, k1, k2, k3, k4);
		}
		else if (OutType == TEXT("eucm"))
		{
			// EUCM: fx, fy, cx, cy, alpha, beta
			double fx, fy, cx, cy, alpha, beta;
			fx = IntrinsicsObj->GetNumberField(TEXT("fx"));
			fy = IntrinsicsObj->GetNumberField(TEXT("fy"));
			cx = IntrinsicsObj->GetNumberField(TEXT("cx"));
			cy = IntrinsicsObj->GetNumberField(TEXT("cy"));
			alpha = IntrinsicsObj->GetNumberField(TEXT("alpha"));
			beta = IntrinsicsObj->GetNumberField(TEXT("beta"));

			OutParams = {fx, fy, cx, cy, alpha, beta};
			UE_LOG(LogBasalt, Log, TEXT("    EUCM: fx=%.2f, fy=%.2f, cx=%.2f, cy=%.2f, alpha=%.4f, beta=%.4f"),
				fx, fy, cx, cy, alpha, beta);
		}
		else if (OutType == TEXT("ds"))
		{
			// Double Sphere: fx, fy, cx, cy, xi, alpha
			double fx, fy, cx, cy, xi, alpha;
			fx = IntrinsicsObj->GetNumberField(TEXT("fx"));
			fy = IntrinsicsObj->GetNumberField(TEXT("fy"));
			cx = IntrinsicsObj->GetNumberField(TEXT("cx"));
			cy = IntrinsicsObj->GetNumberField(TEXT("cy"));
			xi = IntrinsicsObj->GetNumberField(TEXT("xi"));
			alpha = IntrinsicsObj->GetNumberField(TEXT("alpha"));

			OutParams = {fx, fy, cx, cy, xi, alpha};
			UE_LOG(LogBasalt, Log, TEXT("    DS: fx=%.2f, fy=%.2f, cx=%.2f, cy=%.2f, xi=%.4f, alpha=%.4f"),
				fx, fy, cx, cy, xi, alpha);
		}
		else if (OutType == TEXT("pinhole"))
		{
			// Pinhole: fx, fy, cx, cy
			double fx, fy, cx, cy;
			fx = IntrinsicsObj->GetNumberField(TEXT("fx"));
			fy = IntrinsicsObj->GetNumberField(TEXT("fy"));
			cx = IntrinsicsObj->GetNumberField(TEXT("cx"));
			cy = IntrinsicsObj->GetNumberField(TEXT("cy"));

			OutParams = {fx, fy, cx, cy};
			UE_LOG(LogBasalt, Log, TEXT("    Pinhole: fx=%.2f, fy=%.2f, cx=%.2f, cy=%.2f"),
				fx, fy, cx, cy);
		}
		else if (OutType == TEXT("equidistant") || OutType == TEXT("equisolid") ||
		         OutType == TEXT("orthographic") || OutType == TEXT("stereographic"))
		{
			// Ideal fisheye: fov, cx, cy (projection_type is stored in OutType)
			double fov, cx, cy;
			fov = IntrinsicsObj->HasField(TEXT("fov")) ? IntrinsicsObj->GetNumberField(TEXT("fov")) : 120.0;
			cx = IntrinsicsObj->HasField(TEXT("cx")) ? IntrinsicsObj->GetNumberField(TEXT("cx")) : 0.0;
			cy = IntrinsicsObj->HasField(TEXT("cy")) ? IntrinsicsObj->GetNumberField(TEXT("cy")) : 0.0;

			OutParams = {fov, cx, cy};
			UE_LOG(LogBasalt, Log, TEXT("    Ideal Fisheye (%s): fov=%.1f, cx=%.1f, cy=%.1f"),
				*OutType, fov, cx, cy);
		}
		else
		{
			UE_LOG(LogBasalt, Warning, TEXT("  Unknown camera type: %s"), *OutType);
			return false;
		}

		return true;
	}

	/**
	 * Loads calibration data from a JSON file using UE's JSON parser (robust, doesn't require all fields)
	 */
	bool LoadCalibrationFromFile(const FString& JsonFilePath, ParsedCalibration& OutCalib, float Scale, bool bConvertToUnreal)
	{
		UE_LOG(LogBasalt, Log, TEXT("========================================"));
		UE_LOG(LogBasalt, Log, TEXT("Loading calibration from: %s"), *JsonFilePath);
		UE_LOG(LogBasalt, Log, TEXT("  Scale: %.1f, ConvertToUnreal: %s"), Scale, bConvertToUnreal ? TEXT("true") : TEXT("false"));

		if (JsonFilePath.IsEmpty())
		{
			UE_LOG(LogBasalt, Error, TEXT("  File path is empty!"));
			return false;
		}

		// Load file
		FString JsonString;
		if (!FFileHelper::LoadFileToString(JsonString, *JsonFilePath))
		{
			UE_LOG(LogBasalt, Error, TEXT("  Failed to load file!"));
			return false;
		}
		UE_LOG(LogBasalt, Log, TEXT("  File loaded, size: %d bytes"), JsonString.Len());

		// Parse JSON
		TSharedPtr<FJsonObject> RootObj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
		{
			UE_LOG(LogBasalt, Error, TEXT("  Failed to parse JSON!"));
			return false;
		}
		UE_LOG(LogBasalt, Log, TEXT("  JSON parsed successfully"));

		// Get value0 object
		TSharedPtr<FJsonObject> Value0Obj = RootObj->GetObjectField(TEXT("value0"));
		if (!Value0Obj.IsValid())
		{
			UE_LOG(LogBasalt, Error, TEXT("  Missing 'value0' object!"));
			return false;
		}
		UE_LOG(LogBasalt, Log, TEXT("  Found 'value0' object"));

		// Log available fields in value0
		TArray<FString> FieldNames;
		Value0Obj->Values.GetKeys(FieldNames);
		UE_LOG(LogBasalt, Log, TEXT("  Available fields in value0: %s"), *FString::Join(FieldNames, TEXT(", ")));

		// ===== Parse T_imu_cam (transforms) =====
		const TArray<TSharedPtr<FJsonValue>>* TImuCamArrayPtr = nullptr;
		if (Value0Obj->TryGetArrayField(TEXT("T_imu_cam"), TImuCamArrayPtr) && TImuCamArrayPtr)
		{
			UE_LOG(LogBasalt, Log, TEXT("  Found T_imu_cam array with %d entries"), TImuCamArrayPtr->Num());

			for (int32 i = 0; i < TImuCamArrayPtr->Num(); ++i)
			{
				const TSharedPtr<FJsonObject> PoseObj = (*TImuCamArrayPtr)[i]->AsObject();
				FTransform Transform;
				if (ParseSE3Transform(PoseObj, Transform, Scale, bConvertToUnreal))
				{
					OutCalib.Transforms.Add(Transform);
					UE_LOG(LogBasalt, Log, TEXT("    Transform[%d]: Loc=%s, Rot=%s"),
						i, *Transform.GetLocation().ToString(), *Transform.GetRotation().Euler().ToString());
				}
				else
				{
					UE_LOG(LogBasalt, Warning, TEXT("    Transform[%d]: FAILED to parse!"), i);
				}
			}
		}
		else
		{
			UE_LOG(LogBasalt, Warning, TEXT("  No T_imu_cam array found!"));
		}

		// ===== Parse intrinsics =====
		const TArray<TSharedPtr<FJsonValue>>* IntrinsicsArrayPtr = nullptr;
		if (Value0Obj->TryGetArrayField(TEXT("intrinsics"), IntrinsicsArrayPtr) && IntrinsicsArrayPtr)
		{
			UE_LOG(LogBasalt, Log, TEXT("  Found intrinsics array with %d entries"), IntrinsicsArrayPtr->Num());

			for (int32 i = 0; i < IntrinsicsArrayPtr->Num(); ++i)
			{
				const TSharedPtr<FJsonObject> CamObj = (*IntrinsicsArrayPtr)[i]->AsObject();
				FString CameraType;
				TArray<double> Params;

				if (ParseCameraIntrinsics(CamObj, CameraType, Params))
				{
					OutCalib.CameraTypes.Add(CameraType);
					OutCalib.IntrinsicParams.Add(Params);
				}
				else
				{
					UE_LOG(LogBasalt, Warning, TEXT("    Intrinsics[%d]: FAILED to parse!"), i);
					OutCalib.CameraTypes.Add(TEXT("unknown"));
					OutCalib.IntrinsicParams.Add({});
				}
			}
		}
		else
		{
			UE_LOG(LogBasalt, Warning, TEXT("  No intrinsics array found!"));
		}

		// ===== Parse resolution =====
		const TArray<TSharedPtr<FJsonValue>>* ResolutionArrayPtr = nullptr;
		if (Value0Obj->TryGetArrayField(TEXT("resolution"), ResolutionArrayPtr) && ResolutionArrayPtr)
		{
			UE_LOG(LogBasalt, Log, TEXT("  Found resolution array with %d entries"), ResolutionArrayPtr->Num());

			for (int32 i = 0; i < ResolutionArrayPtr->Num(); ++i)
			{
				const TArray<TSharedPtr<FJsonValue>>* ResArray = nullptr;
				if ((*ResolutionArrayPtr)[i]->TryGetArray(ResArray) && ResArray && ResArray->Num() >= 2)
				{
					int32 Width = static_cast<int32>((*ResArray)[0]->AsNumber());
					int32 Height = static_cast<int32>((*ResArray)[1]->AsNumber());
					OutCalib.Resolutions.Add(FIntPoint(Width, Height));
					UE_LOG(LogBasalt, Log, TEXT("    Resolution[%d]: %dx%d"), i, Width, Height);
				}
			}
		}
		else
		{
			UE_LOG(LogBasalt, Warning, TEXT("  No resolution array found!"));
		}

		UE_LOG(LogBasalt, Log, TEXT("========================================"));
		UE_LOG(LogBasalt, Log, TEXT("Summary: %d transforms, %d camera types, %d resolutions"),
			OutCalib.Transforms.Num(), OutCalib.CameraTypes.Num(), OutCalib.Resolutions.Num());

		return OutCalib.Transforms.Num() > 0 || OutCalib.CameraTypes.Num() > 0;
	}

	/**
	 * Fills FBasaltKB4Intrinsics from parameter array
	 */
	void FillKB4Intrinsics(const TArray<double>& Params, FBasaltKB4Intrinsics& Out)
	{
		if (Params.Num() >= 8)
		{
			Out.Fx = static_cast<float>(Params[0]);
			Out.Fy = static_cast<float>(Params[1]);
			Out.Cx = static_cast<float>(Params[2]);
			Out.Cy = static_cast<float>(Params[3]);
			Out.K1 = static_cast<float>(Params[4]);
			Out.K2 = static_cast<float>(Params[5]);
			Out.K3 = static_cast<float>(Params[6]);
			Out.K4 = static_cast<float>(Params[7]);
		}
	}

	/**
	 * Fills FBasaltEUCMIntrinsics from parameter array
	 */
	void FillEUCMIntrinsics(const TArray<double>& Params, FBasaltEUCMIntrinsics& Out)
	{
		if (Params.Num() >= 6)
		{
			Out.Fx = static_cast<float>(Params[0]);
			Out.Fy = static_cast<float>(Params[1]);
			Out.Cx = static_cast<float>(Params[2]);
			Out.Cy = static_cast<float>(Params[3]);
			Out.Alpha = static_cast<float>(Params[4]);
			Out.Beta = static_cast<float>(Params[5]);
		}
	}

	/**
	 * Fills FBasaltDoubleSphereIntrinsics from parameter array
	 */
	void FillDoubleSphereIntrinsics(const TArray<double>& Params, FBasaltDoubleSphereIntrinsics& Out)
	{
		if (Params.Num() >= 6)
		{
			Out.Fx = static_cast<float>(Params[0]);
			Out.Fy = static_cast<float>(Params[1]);
			Out.Cx = static_cast<float>(Params[2]);
			Out.Cy = static_cast<float>(Params[3]);
			Out.Xi = static_cast<float>(Params[4]);
			Out.Alpha = static_cast<float>(Params[5]);
		}
	}

	/**
	 * Fills FBasaltPinholeIntrinsics from parameter array
	 */
	void FillPinholeIntrinsics(const TArray<double>& Params, FBasaltPinholeIntrinsics& Out)
	{
		if (Params.Num() >= 4)
		{
			Out.Fx = static_cast<float>(Params[0]);
			Out.Fy = static_cast<float>(Params[1]);
			Out.Cx = static_cast<float>(Params[2]);
			Out.Cy = static_cast<float>(Params[3]);
		}
	}

	/**
	 * Fills FBasaltIdealFisheyeIntrinsics from parameter array and camera type
	 */
	void FillIdealFisheyeIntrinsics(const TArray<double>& Params, const FString& CameraType, FBasaltIdealFisheyeIntrinsics& Out)
	{
		Out.ProjectionType = CameraType;
		if (Params.Num() >= 3)
		{
			Out.FOV = static_cast<int32>(Params[0]);
			Out.Cx = static_cast<int32>(Params[1]);
			Out.Cy = static_cast<int32>(Params[2]);
		}
	}
}

// ============================================================================================================== //
// File Dialog
// ============================================================================================================== //

bool UBasaltCalibrationReader::OpenCalibrationFileDialog(
	const FString& DialogTitle,
	const FString& DefaultPath,
	UEditableTextBox* TargetTextBox,
	FString& OutFilePath)
{
	OutFilePath.Reset();

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

	const FString StartPath = DefaultPath.IsEmpty() ? FPaths::ProjectContentDir() : DefaultPath;
	const FString FileTypes = TEXT("JSON files (*.json)|*.json");

	TArray<FString> SelectedFiles;
	const bool bOpened = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		DialogTitle.IsEmpty() ? TEXT("Select Basalt Calibration File") : DialogTitle,
		StartPath,
		TEXT(""),
		FileTypes,
		EFileDialogFlags::None,
		SelectedFiles
	);

	if (!bOpened || SelectedFiles.Num() == 0)
	{
		return false;
	}

	OutFilePath = SelectedFiles[0];

	if (!OutFilePath.EndsWith(TEXT(".json"), ESearchCase::IgnoreCase))
	{
		OutFilePath.Reset();
		return false;
	}

	if (TargetTextBox)
	{
		TargetTextBox->SetText(FText::FromString(OutFilePath));
	}

	return true;
#else
	return false;
#endif
}

// ============================================================================================================== //
// Load Calibration
// ============================================================================================================== //

bool UBasaltCalibrationReader::LoadCalibration(
	const FString& JsonFilePath,
	bool bConvertToUnrealCoordinates,
	bool bMetersToCentimeters,
	TArray<FBasaltCameraCalibration>& OutCameras)
{
	OutCameras.Reset();

	const float Scale = bMetersToCentimeters ? 100.0f : 1.0f;

	BasaltInternal::ParsedCalibration Calibration;
	if (!BasaltInternal::LoadCalibrationFromFile(JsonFilePath, Calibration, Scale, bConvertToUnrealCoordinates))
	{
		return false;
	}

	const int32 NumCameras = FMath::Max3(
		Calibration.Transforms.Num(),
		Calibration.CameraTypes.Num(),
		Calibration.Resolutions.Num()
	);

	for (int32 i = 0; i < NumCameras; ++i)
	{
		FBasaltCameraCalibration CamCalib;

		// Camera type
		if (i < Calibration.CameraTypes.Num())
		{
			CamCalib.CameraType = Calibration.CameraTypes[i];
		}

		// Transform (extrinsic)
		if (i < Calibration.Transforms.Num())
		{
			CamCalib.Transform = Calibration.Transforms[i];
		}

		// Resolution
		if (i < Calibration.Resolutions.Num())
		{
			CamCalib.Resolution.Width = Calibration.Resolutions[i].X;
			CamCalib.Resolution.Height = Calibration.Resolutions[i].Y;
		}

		// Extract intrinsics based on camera type
		if (i < Calibration.IntrinsicParams.Num())
		{
			const TArray<double>& Params = Calibration.IntrinsicParams[i];

			if (CamCalib.CameraType == TEXT("kb4"))
			{
				BasaltInternal::FillKB4Intrinsics(Params, CamCalib.KB4Intrinsics);
			}
			else if (CamCalib.CameraType == TEXT("eucm"))
			{
				BasaltInternal::FillEUCMIntrinsics(Params, CamCalib.EUCMIntrinsics);
			}
			else if (CamCalib.CameraType == TEXT("ds"))
			{
				BasaltInternal::FillDoubleSphereIntrinsics(Params, CamCalib.DoubleSphereIntrinsics);
			}
			else if (CamCalib.CameraType == TEXT("pinhole"))
			{
				BasaltInternal::FillPinholeIntrinsics(Params, CamCalib.PinholeIntrinsics);
			}
			else if (CamCalib.CameraType == TEXT("equidistant") || CamCalib.CameraType == TEXT("equisolid") ||
			         CamCalib.CameraType == TEXT("orthographic") || CamCalib.CameraType == TEXT("stereographic"))
			{
				BasaltInternal::FillIdealFisheyeIntrinsics(Params, CamCalib.CameraType, CamCalib.IdealFisheyeIntrinsics);
			}
		}

		OutCameras.Add(CamCalib);
	}

	UE_LOG(LogBasalt, Log, TEXT("LoadCalibration: Returned %d cameras"), OutCameras.Num());
	return true;
}

// ============================================================================================================== //
// Camera Count
// ============================================================================================================== //

bool UBasaltCalibrationReader::GetCameraCount(
	const FString& JsonFilePath,
	int32& OutCameraCount)
{
	OutCameraCount = 0;

	BasaltInternal::ParsedCalibration Calibration;
	if (!BasaltInternal::LoadCalibrationFromFile(JsonFilePath, Calibration, 1.0f, false))
	{
		return false;
	}

	OutCameraCount = FMath::Max3(
		Calibration.Transforms.Num(),
		Calibration.CameraTypes.Num(),
		Calibration.Resolutions.Num()
	);

	UE_LOG(LogBasalt, Log, TEXT("GetCameraCount: %d"), OutCameraCount);
	return true;
}

// ============================================================================================================== //
// Camera Type
// ============================================================================================================== //

bool UBasaltCalibrationReader::GetCameraType(
	const FString& JsonFilePath,
	int32 CameraIndex,
	FString& OutCameraType)
{
	OutCameraType = TEXT("");

	BasaltInternal::ParsedCalibration Calibration;
	if (!BasaltInternal::LoadCalibrationFromFile(JsonFilePath, Calibration, 1.0f, false))
	{
		return false;
	}

	if (CameraIndex < 0 || CameraIndex >= Calibration.CameraTypes.Num())
	{
		UE_LOG(LogBasalt, Error, TEXT("GetCameraType: Camera index %d out of range (0-%d)"),
			CameraIndex, Calibration.CameraTypes.Num() - 1);
		return false;
	}

	OutCameraType = Calibration.CameraTypes[CameraIndex];
	UE_LOG(LogBasalt, Log, TEXT("GetCameraType[%d]: %s"), CameraIndex, *OutCameraType);
	return true;
}

// ============================================================================================================== //
// Transforms
// ============================================================================================================== //

bool UBasaltCalibrationReader::GetCameraTransforms(
	const FString& JsonFilePath,
	bool bConvertToUnrealCoordinates,
	bool bMetersToCentimeters,
	bool bCenterTransforms,
	TArray<FTransform>& OutTransforms,
	FVector& OutCenter)
{
	OutTransforms.Reset();
	OutCenter = FVector::ZeroVector;

	const float Scale = bMetersToCentimeters ? 100.0f : 1.0f;

	BasaltInternal::ParsedCalibration Calibration;
	if (!BasaltInternal::LoadCalibrationFromFile(JsonFilePath, Calibration, Scale, bConvertToUnrealCoordinates))
	{
		return false;
	}

	if (Calibration.Transforms.Num() == 0)
	{
		UE_LOG(LogBasalt, Error, TEXT("GetCameraTransforms: No transforms found!"));
		return false;
	}

	OutTransforms = Calibration.Transforms;

	// Calculate center
	FVector Sum = FVector::ZeroVector;
	for (const FTransform& T : OutTransforms)
	{
		Sum += T.GetTranslation();
	}
	OutCenter = Sum / static_cast<float>(OutTransforms.Num());

	// Center transforms if requested
	if (bCenterTransforms)
	{
		for (FTransform& T : OutTransforms)
		{
			T.AddToTranslation(-OutCenter);
		}
		UE_LOG(LogBasalt, Log, TEXT("GetCameraTransforms: Centered %d transforms by: %s"),
			OutTransforms.Num(), *OutCenter.ToString());
	}

	UE_LOG(LogBasalt, Log, TEXT("GetCameraTransforms: Returned %d transforms"), OutTransforms.Num());
	return true;
}

bool UBasaltCalibrationReader::GetCameraTransform(
	const FString& JsonFilePath,
	int32 CameraIndex,
	bool bConvertToUnrealCoordinates,
	bool bMetersToCentimeters,
	FTransform& OutTransform)
{
	OutTransform = FTransform::Identity;

	const float Scale = bMetersToCentimeters ? 100.0f : 1.0f;

	BasaltInternal::ParsedCalibration Calibration;
	if (!BasaltInternal::LoadCalibrationFromFile(JsonFilePath, Calibration, Scale, bConvertToUnrealCoordinates))
	{
		return false;
	}

	if (CameraIndex < 0 || CameraIndex >= Calibration.Transforms.Num())
	{
		UE_LOG(LogBasalt, Error, TEXT("GetCameraTransform: Camera index %d out of range"), CameraIndex);
		return false;
	}

	OutTransform = Calibration.Transforms[CameraIndex];
	UE_LOG(LogBasalt, Log, TEXT("GetCameraTransform[%d]: Loc=%s"), CameraIndex, *OutTransform.GetLocation().ToString());
	return true;
}

// ============================================================================================================== //
// Resolutions
// ============================================================================================================== //

bool UBasaltCalibrationReader::GetCameraResolutions(
	const FString& JsonFilePath,
	TArray<FBasaltResolution>& OutResolutions)
{
	OutResolutions.Reset();

	BasaltInternal::ParsedCalibration Calibration;
	if (!BasaltInternal::LoadCalibrationFromFile(JsonFilePath, Calibration, 1.0f, false))
	{
		return false;
	}

	for (const FIntPoint& Res : Calibration.Resolutions)
	{
		FBasaltResolution Resolution;
		Resolution.Width = Res.X;
		Resolution.Height = Res.Y;
		OutResolutions.Add(Resolution);
	}

	UE_LOG(LogBasalt, Log, TEXT("GetCameraResolutions: Returned %d resolutions"), OutResolutions.Num());
	return true;
}

bool UBasaltCalibrationReader::GetCameraResolution(
	const FString& JsonFilePath,
	int32 CameraIndex,
	FBasaltResolution& OutResolution)
{
	OutResolution = FBasaltResolution();

	BasaltInternal::ParsedCalibration Calibration;
	if (!BasaltInternal::LoadCalibrationFromFile(JsonFilePath, Calibration, 1.0f, false))
	{
		return false;
	}

	if (CameraIndex < 0 || CameraIndex >= Calibration.Resolutions.Num())
	{
		UE_LOG(LogBasalt, Error, TEXT("GetCameraResolution: Camera index %d out of range"), CameraIndex);
		return false;
	}

	OutResolution.Width = Calibration.Resolutions[CameraIndex].X;
	OutResolution.Height = Calibration.Resolutions[CameraIndex].Y;
	UE_LOG(LogBasalt, Log, TEXT("GetCameraResolution[%d]: %dx%d"), CameraIndex, OutResolution.Width, OutResolution.Height);
	return true;
}

// ============================================================================================================== //
// KB4 Intrinsics
// ============================================================================================================== //

bool UBasaltCalibrationReader::GetKB4Intrinsics(
	const FString& JsonFilePath,
	int32 CameraIndex,
	FBasaltKB4Intrinsics& OutIntrinsics)
{
	OutIntrinsics = FBasaltKB4Intrinsics();

	BasaltInternal::ParsedCalibration Calibration;
	if (!BasaltInternal::LoadCalibrationFromFile(JsonFilePath, Calibration, 1.0f, false))
	{
		return false;
	}

	if (CameraIndex < 0 || CameraIndex >= Calibration.CameraTypes.Num())
	{
		UE_LOG(LogBasalt, Error, TEXT("GetKB4Intrinsics: Camera index %d out of range"), CameraIndex);
		return false;
	}

	if (Calibration.CameraTypes[CameraIndex] != TEXT("kb4"))
	{
		UE_LOG(LogBasalt, Warning, TEXT("GetKB4Intrinsics: Camera %d is not KB4 type (actual: %s)"),
			CameraIndex, *Calibration.CameraTypes[CameraIndex]);
		return false;
	}

	BasaltInternal::FillKB4Intrinsics(Calibration.IntrinsicParams[CameraIndex], OutIntrinsics);
	UE_LOG(LogBasalt, Log, TEXT("GetKB4Intrinsics[%d]: fx=%.2f, fy=%.2f"), CameraIndex, OutIntrinsics.Fx, OutIntrinsics.Fy);
	return true;
}

// ============================================================================================================== //
// EUCM Intrinsics
// ============================================================================================================== //

bool UBasaltCalibrationReader::GetEUCMIntrinsics(
	const FString& JsonFilePath,
	int32 CameraIndex,
	FBasaltEUCMIntrinsics& OutIntrinsics)
{
	OutIntrinsics = FBasaltEUCMIntrinsics();

	BasaltInternal::ParsedCalibration Calibration;
	if (!BasaltInternal::LoadCalibrationFromFile(JsonFilePath, Calibration, 1.0f, false))
	{
		return false;
	}

	if (CameraIndex < 0 || CameraIndex >= Calibration.CameraTypes.Num())
	{
		UE_LOG(LogBasalt, Error, TEXT("GetEUCMIntrinsics: Camera index %d out of range"), CameraIndex);
		return false;
	}

	if (Calibration.CameraTypes[CameraIndex] != TEXT("eucm"))
	{
		UE_LOG(LogBasalt, Warning, TEXT("GetEUCMIntrinsics: Camera %d is not EUCM type (actual: %s)"),
			CameraIndex, *Calibration.CameraTypes[CameraIndex]);
		return false;
	}

	BasaltInternal::FillEUCMIntrinsics(Calibration.IntrinsicParams[CameraIndex], OutIntrinsics);
	UE_LOG(LogBasalt, Log, TEXT("GetEUCMIntrinsics[%d]: fx=%.2f, alpha=%.4f"), CameraIndex, OutIntrinsics.Fx, OutIntrinsics.Alpha);
	return true;
}

// ============================================================================================================== //
// Double Sphere Intrinsics
// ============================================================================================================== //

bool UBasaltCalibrationReader::GetDoubleSphereIntrinsics(
	const FString& JsonFilePath,
	int32 CameraIndex,
	FBasaltDoubleSphereIntrinsics& OutIntrinsics)
{
	OutIntrinsics = FBasaltDoubleSphereIntrinsics();

	BasaltInternal::ParsedCalibration Calibration;
	if (!BasaltInternal::LoadCalibrationFromFile(JsonFilePath, Calibration, 1.0f, false))
	{
		return false;
	}

	if (CameraIndex < 0 || CameraIndex >= Calibration.CameraTypes.Num())
	{
		UE_LOG(LogBasalt, Error, TEXT("GetDoubleSphereIntrinsics: Camera index %d out of range"), CameraIndex);
		return false;
	}

	if (Calibration.CameraTypes[CameraIndex] != TEXT("ds"))
	{
		UE_LOG(LogBasalt, Warning, TEXT("GetDoubleSphereIntrinsics: Camera %d is not DS type (actual: %s)"),
			CameraIndex, *Calibration.CameraTypes[CameraIndex]);
		return false;
	}

	BasaltInternal::FillDoubleSphereIntrinsics(Calibration.IntrinsicParams[CameraIndex], OutIntrinsics);
	UE_LOG(LogBasalt, Log, TEXT("GetDoubleSphereIntrinsics[%d]: fx=%.2f, xi=%.4f"), CameraIndex, OutIntrinsics.Fx, OutIntrinsics.Xi);
	return true;
}

// ============================================================================================================== //
// Pinhole Intrinsics
// ============================================================================================================== //

bool UBasaltCalibrationReader::GetPinholeIntrinsics(
	const FString& JsonFilePath,
	int32 CameraIndex,
	FBasaltPinholeIntrinsics& OutIntrinsics)
{
	OutIntrinsics = FBasaltPinholeIntrinsics();

	BasaltInternal::ParsedCalibration Calibration;
	if (!BasaltInternal::LoadCalibrationFromFile(JsonFilePath, Calibration, 1.0f, false))
	{
		return false;
	}

	if (CameraIndex < 0 || CameraIndex >= Calibration.CameraTypes.Num())
	{
		UE_LOG(LogBasalt, Error, TEXT("GetPinholeIntrinsics: Camera index %d out of range"), CameraIndex);
		return false;
	}

	if (Calibration.CameraTypes[CameraIndex] != TEXT("pinhole"))
	{
		UE_LOG(LogBasalt, Warning, TEXT("GetPinholeIntrinsics: Camera %d is not Pinhole type (actual: %s)"),
			CameraIndex, *Calibration.CameraTypes[CameraIndex]);
		return false;
	}

	BasaltInternal::FillPinholeIntrinsics(Calibration.IntrinsicParams[CameraIndex], OutIntrinsics);
	UE_LOG(LogBasalt, Log, TEXT("GetPinholeIntrinsics[%d]: fx=%.2f, fy=%.2f"), CameraIndex, OutIntrinsics.Fx, OutIntrinsics.Fy);
	return true;
}

// ============================================================================================================== //
// Save Calibration
// ============================================================================================================== //

bool UBasaltCalibrationReader::SaveCalibration(
	const FString& JsonFilePath,
	const TArray<FBasaltCameraCalibration>& Cameras,
	bool bConvertFromUnrealCoordinates,
	bool bCentimetersToMeters)
{
	UE_LOG(LogBasalt, Log, TEXT("========================================"));
	UE_LOG(LogBasalt, Log, TEXT("Saving calibration to: %s"), *JsonFilePath);
	UE_LOG(LogBasalt, Log, TEXT("  Cameras: %d"), Cameras.Num());
	UE_LOG(LogBasalt, Log, TEXT("  ConvertFromUnreal: %s, CmToM: %s"),
		bConvertFromUnrealCoordinates ? TEXT("true") : TEXT("false"),
		bCentimetersToMeters ? TEXT("true") : TEXT("false"));

	if (Cameras.Num() == 0)
	{
		UE_LOG(LogBasalt, Warning, TEXT("  No cameras to save!"));
		return false;
	}

	const float Scale = bCentimetersToMeters ? 0.01f : 1.0f;

	// Build JSON structure
	TSharedPtr<FJsonObject> RootObj = MakeShareable(new FJsonObject);
	TSharedPtr<FJsonObject> Value0Obj = MakeShareable(new FJsonObject);

	// T_imu_cam array (transforms)
	TArray<TSharedPtr<FJsonValue>> TImuCamArray;
	for (const FBasaltCameraCalibration& Cam : Cameras)
	{
		FTransform Transform = Cam.Transform;

		// Convert from Unreal to OpenCV coordinate system if needed
		if (bConvertFromUnrealCoordinates)
		{
			FOpenCVHelper::ConvertUnrealToOpenCV(Transform);
		}

		// Apply scale
		FVector Location = Transform.GetLocation() * Scale;
		FQuat Rotation = Transform.GetRotation();

		TSharedPtr<FJsonObject> PoseObj = MakeShareable(new FJsonObject);
		PoseObj->SetNumberField(TEXT("px"), Location.X);
		PoseObj->SetNumberField(TEXT("py"), Location.Y);
		PoseObj->SetNumberField(TEXT("pz"), Location.Z);
		PoseObj->SetNumberField(TEXT("qx"), Rotation.X);
		PoseObj->SetNumberField(TEXT("qy"), Rotation.Y);
		PoseObj->SetNumberField(TEXT("qz"), Rotation.Z);
		PoseObj->SetNumberField(TEXT("qw"), Rotation.W);

		TImuCamArray.Add(MakeShareable(new FJsonValueObject(PoseObj)));
	}
	Value0Obj->SetArrayField(TEXT("T_imu_cam"), TImuCamArray);

	// Intrinsics array
	TArray<TSharedPtr<FJsonValue>> IntrinsicsArray;
	for (const FBasaltCameraCalibration& Cam : Cameras)
	{
		TSharedPtr<FJsonObject> CamObj = MakeShareable(new FJsonObject);
		CamObj->SetStringField(TEXT("camera_type"), Cam.CameraType);

		TSharedPtr<FJsonObject> IntrObj = MakeShareable(new FJsonObject);

		if (Cam.CameraType == TEXT("kb4"))
		{
			IntrObj->SetNumberField(TEXT("fx"), Cam.KB4Intrinsics.Fx);
			IntrObj->SetNumberField(TEXT("fy"), Cam.KB4Intrinsics.Fy);
			IntrObj->SetNumberField(TEXT("cx"), Cam.KB4Intrinsics.Cx);
			IntrObj->SetNumberField(TEXT("cy"), Cam.KB4Intrinsics.Cy);
			IntrObj->SetNumberField(TEXT("k1"), Cam.KB4Intrinsics.K1);
			IntrObj->SetNumberField(TEXT("k2"), Cam.KB4Intrinsics.K2);
			IntrObj->SetNumberField(TEXT("k3"), Cam.KB4Intrinsics.K3);
			IntrObj->SetNumberField(TEXT("k4"), Cam.KB4Intrinsics.K4);
		}
		else if (Cam.CameraType == TEXT("eucm"))
		{
			IntrObj->SetNumberField(TEXT("fx"), Cam.EUCMIntrinsics.Fx);
			IntrObj->SetNumberField(TEXT("fy"), Cam.EUCMIntrinsics.Fy);
			IntrObj->SetNumberField(TEXT("cx"), Cam.EUCMIntrinsics.Cx);
			IntrObj->SetNumberField(TEXT("cy"), Cam.EUCMIntrinsics.Cy);
			IntrObj->SetNumberField(TEXT("alpha"), Cam.EUCMIntrinsics.Alpha);
			IntrObj->SetNumberField(TEXT("beta"), Cam.EUCMIntrinsics.Beta);
		}
		else if (Cam.CameraType == TEXT("ds"))
		{
			IntrObj->SetNumberField(TEXT("fx"), Cam.DoubleSphereIntrinsics.Fx);
			IntrObj->SetNumberField(TEXT("fy"), Cam.DoubleSphereIntrinsics.Fy);
			IntrObj->SetNumberField(TEXT("cx"), Cam.DoubleSphereIntrinsics.Cx);
			IntrObj->SetNumberField(TEXT("cy"), Cam.DoubleSphereIntrinsics.Cy);
			IntrObj->SetNumberField(TEXT("xi"), Cam.DoubleSphereIntrinsics.Xi);
			IntrObj->SetNumberField(TEXT("alpha"), Cam.DoubleSphereIntrinsics.Alpha);
		}
		else if (Cam.CameraType == TEXT("pinhole"))
		{
			IntrObj->SetNumberField(TEXT("fx"), Cam.PinholeIntrinsics.Fx);
			IntrObj->SetNumberField(TEXT("fy"), Cam.PinholeIntrinsics.Fy);
			IntrObj->SetNumberField(TEXT("cx"), Cam.PinholeIntrinsics.Cx);
			IntrObj->SetNumberField(TEXT("cy"), Cam.PinholeIntrinsics.Cy);
		}
		else if (Cam.CameraType == TEXT("equidistant") || Cam.CameraType == TEXT("equisolid") ||
		         Cam.CameraType == TEXT("orthographic") || Cam.CameraType == TEXT("stereographic"))
		{
			// Ideal fisheye models
			IntrObj->SetStringField(TEXT("projection_type"), Cam.IdealFisheyeIntrinsics.ProjectionType);
			IntrObj->SetNumberField(TEXT("fov"), Cam.IdealFisheyeIntrinsics.FOV);
			IntrObj->SetNumberField(TEXT("cx"), Cam.IdealFisheyeIntrinsics.Cx);
			IntrObj->SetNumberField(TEXT("cy"), Cam.IdealFisheyeIntrinsics.Cy);
		}

		CamObj->SetObjectField(TEXT("intrinsics"), IntrObj);
		IntrinsicsArray.Add(MakeShareable(new FJsonValueObject(CamObj)));
	}
	Value0Obj->SetArrayField(TEXT("intrinsics"), IntrinsicsArray);

	// Resolution array
	TArray<TSharedPtr<FJsonValue>> ResolutionArray;
	for (const FBasaltCameraCalibration& Cam : Cameras)
	{
		TArray<TSharedPtr<FJsonValue>> ResArray;
		ResArray.Add(MakeShareable(new FJsonValueNumber(Cam.Resolution.Width)));
		ResArray.Add(MakeShareable(new FJsonValueNumber(Cam.Resolution.Height)));
		ResolutionArray.Add(MakeShareable(new FJsonValueArray(ResArray)));
	}
	Value0Obj->SetArrayField(TEXT("resolution"), ResolutionArray);

	// Set value0 in root
	RootObj->SetObjectField(TEXT("value0"), Value0Obj);

	// Serialize to string
	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);

	if (!FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer))
	{
		UE_LOG(LogBasalt, Error, TEXT("  Failed to serialize JSON!"));
		return false;
	}

	// Save to file
	if (!FFileHelper::SaveStringToFile(OutputString, *JsonFilePath))
	{
		UE_LOG(LogBasalt, Error, TEXT("  Failed to save file!"));
		return false;
	}

	UE_LOG(LogBasalt, Log, TEXT("  Successfully saved calibration"));
	UE_LOG(LogBasalt, Log, TEXT("========================================"));
	return true;
}

// ============================================================================================================== //
// Get Default Calibration Path
// ============================================================================================================== //

FString UBasaltCalibrationReader::GetDefaultCalibrationPath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MovieRenders"), TEXT("calibration.json"));
}

FString UBasaltCalibrationReader::GetDefaultMovieRenderPath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MovieRenders"));
}

// ============================================================================================================== //
// Save Extrinsics Only
// ============================================================================================================== //

bool UBasaltCalibrationReader::SaveExtrinsics(
	const FString& JsonFilePath,
	const TArray<FTransform>& Transforms,
	bool bConvertFromUnrealCoordinates,
	bool bCentimetersToMeters)
{
	// Use default path if none provided
	FString ActualPath = JsonFilePath.IsEmpty() ? GetDefaultCalibrationPath() : JsonFilePath;

	UE_LOG(LogBasalt, Log, TEXT("========================================"));
	UE_LOG(LogBasalt, Log, TEXT("Saving extrinsics to: %s"), *ActualPath);
	UE_LOG(LogBasalt, Log, TEXT("  Cameras: %d"), Transforms.Num());

	if (Transforms.Num() == 0)
	{
		UE_LOG(LogBasalt, Warning, TEXT("  No transforms to save!"));
		return false;
	}

	// Ensure directory exists
	FString Directory = FPaths::GetPath(ActualPath);
	if (!FPaths::DirectoryExists(Directory))
	{
		IFileManager::Get().MakeDirectory(*Directory, true);
	}

	const float Scale = bCentimetersToMeters ? 0.01f : 1.0f;

	// Build JSON structure
	TSharedPtr<FJsonObject> RootObj = MakeShareable(new FJsonObject);
	TSharedPtr<FJsonObject> Value0Obj = MakeShareable(new FJsonObject);

	// T_imu_cam array (transforms)
	TArray<TSharedPtr<FJsonValue>> TImuCamArray;
	for (int32 i = 0; i < Transforms.Num(); ++i)
	{
		FTransform Transform = Transforms[i];

		if (bConvertFromUnrealCoordinates)
		{
			FOpenCVHelper::ConvertUnrealToOpenCV(Transform);
		}

		FVector Location = Transform.GetLocation() * Scale;
		FQuat Rotation = Transform.GetRotation();

		TSharedPtr<FJsonObject> PoseObj = MakeShareable(new FJsonObject);
		PoseObj->SetNumberField(TEXT("px"), Location.X);
		PoseObj->SetNumberField(TEXT("py"), Location.Y);
		PoseObj->SetNumberField(TEXT("pz"), Location.Z);
		PoseObj->SetNumberField(TEXT("qx"), Rotation.X);
		PoseObj->SetNumberField(TEXT("qy"), Rotation.Y);
		PoseObj->SetNumberField(TEXT("qz"), Rotation.Z);
		PoseObj->SetNumberField(TEXT("qw"), Rotation.W);

		TImuCamArray.Add(MakeShareable(new FJsonValueObject(PoseObj)));
	}
	Value0Obj->SetArrayField(TEXT("T_imu_cam"), TImuCamArray);

	// Empty intrinsics and resolution arrays - will be filled by UpdateCalibrationWithIntrinsics
	Value0Obj->SetArrayField(TEXT("intrinsics"), TArray<TSharedPtr<FJsonValue>>());
	Value0Obj->SetArrayField(TEXT("resolution"), TArray<TSharedPtr<FJsonValue>>());

	RootObj->SetObjectField(TEXT("value0"), Value0Obj);

	// Serialize and save
	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);

	if (!FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer))
	{
		UE_LOG(LogBasalt, Error, TEXT("  Failed to serialize JSON!"));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(OutputString, *ActualPath))
	{
		UE_LOG(LogBasalt, Error, TEXT("  Failed to save file!"));
		return false;
	}

	UE_LOG(LogBasalt, Log, TEXT("  Successfully saved extrinsics"));
	UE_LOG(LogBasalt, Log, TEXT("========================================"));
	return true;
}

// ============================================================================================================== //
// Update Calibration With Intrinsics
// ============================================================================================================== //

bool UBasaltCalibrationReader::UpdateCalibrationWithIntrinsics(
	const FString& JsonFilePath,
	const TArray<FBasaltCameraCalibration>& Cameras)
{
	UE_LOG(LogBasalt, Log, TEXT("========================================"));
	UE_LOG(LogBasalt, Log, TEXT("Updating intrinsics in: %s"), *JsonFilePath);
	UE_LOG(LogBasalt, Log, TEXT("  Cameras: %d"), Cameras.Num());

	if (Cameras.Num() == 0)
	{
		UE_LOG(LogBasalt, Warning, TEXT("  No cameras to update!"));
		return false;
	}

	// Load existing file
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *JsonFilePath))
	{
		UE_LOG(LogBasalt, Error, TEXT("  Failed to load existing file! Creating new file."));
		// If file doesn't exist, create new with SaveCalibration
		return SaveCalibration(JsonFilePath, Cameras, false, false);
	}

	TSharedPtr<FJsonObject> RootObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		UE_LOG(LogBasalt, Error, TEXT("  Failed to parse existing JSON!"));
		return false;
	}

	TSharedPtr<FJsonObject> Value0Obj = RootObj->GetObjectField(TEXT("value0"));
	if (!Value0Obj.IsValid())
	{
		Value0Obj = MakeShareable(new FJsonObject);
		RootObj->SetObjectField(TEXT("value0"), Value0Obj);
	}

	// Update intrinsics array
	TArray<TSharedPtr<FJsonValue>> IntrinsicsArray;
	for (const FBasaltCameraCalibration& Cam : Cameras)
	{
		TSharedPtr<FJsonObject> CamObj = MakeShareable(new FJsonObject);
		CamObj->SetStringField(TEXT("camera_type"), Cam.CameraType);

		TSharedPtr<FJsonObject> IntrObj = MakeShareable(new FJsonObject);

		if (Cam.CameraType == TEXT("kb4"))
		{
			IntrObj->SetNumberField(TEXT("fx"), Cam.KB4Intrinsics.Fx);
			IntrObj->SetNumberField(TEXT("fy"), Cam.KB4Intrinsics.Fy);
			IntrObj->SetNumberField(TEXT("cx"), Cam.KB4Intrinsics.Cx);
			IntrObj->SetNumberField(TEXT("cy"), Cam.KB4Intrinsics.Cy);
			IntrObj->SetNumberField(TEXT("k1"), Cam.KB4Intrinsics.K1);
			IntrObj->SetNumberField(TEXT("k2"), Cam.KB4Intrinsics.K2);
			IntrObj->SetNumberField(TEXT("k3"), Cam.KB4Intrinsics.K3);
			IntrObj->SetNumberField(TEXT("k4"), Cam.KB4Intrinsics.K4);
		}
		else if (Cam.CameraType == TEXT("eucm"))
		{
			IntrObj->SetNumberField(TEXT("fx"), Cam.EUCMIntrinsics.Fx);
			IntrObj->SetNumberField(TEXT("fy"), Cam.EUCMIntrinsics.Fy);
			IntrObj->SetNumberField(TEXT("cx"), Cam.EUCMIntrinsics.Cx);
			IntrObj->SetNumberField(TEXT("cy"), Cam.EUCMIntrinsics.Cy);
			IntrObj->SetNumberField(TEXT("alpha"), Cam.EUCMIntrinsics.Alpha);
			IntrObj->SetNumberField(TEXT("beta"), Cam.EUCMIntrinsics.Beta);
		}
		else if (Cam.CameraType == TEXT("ds"))
		{
			IntrObj->SetNumberField(TEXT("fx"), Cam.DoubleSphereIntrinsics.Fx);
			IntrObj->SetNumberField(TEXT("fy"), Cam.DoubleSphereIntrinsics.Fy);
			IntrObj->SetNumberField(TEXT("cx"), Cam.DoubleSphereIntrinsics.Cx);
			IntrObj->SetNumberField(TEXT("cy"), Cam.DoubleSphereIntrinsics.Cy);
			IntrObj->SetNumberField(TEXT("xi"), Cam.DoubleSphereIntrinsics.Xi);
			IntrObj->SetNumberField(TEXT("alpha"), Cam.DoubleSphereIntrinsics.Alpha);
		}
		else if (Cam.CameraType == TEXT("pinhole"))
		{
			IntrObj->SetNumberField(TEXT("fx"), Cam.PinholeIntrinsics.Fx);
			IntrObj->SetNumberField(TEXT("fy"), Cam.PinholeIntrinsics.Fy);
			IntrObj->SetNumberField(TEXT("cx"), Cam.PinholeIntrinsics.Cx);
			IntrObj->SetNumberField(TEXT("cy"), Cam.PinholeIntrinsics.Cy);
		}
		else if (Cam.CameraType == TEXT("equidistant") || Cam.CameraType == TEXT("equisolid") ||
		         Cam.CameraType == TEXT("orthographic") || Cam.CameraType == TEXT("stereographic"))
		{
			// Ideal fisheye models
			IntrObj->SetStringField(TEXT("projection_type"), Cam.IdealFisheyeIntrinsics.ProjectionType);
			IntrObj->SetNumberField(TEXT("fov"), Cam.IdealFisheyeIntrinsics.FOV);
			IntrObj->SetNumberField(TEXT("cx"), Cam.IdealFisheyeIntrinsics.Cx);
			IntrObj->SetNumberField(TEXT("cy"), Cam.IdealFisheyeIntrinsics.Cy);
		}

		CamObj->SetObjectField(TEXT("intrinsics"), IntrObj);
		IntrinsicsArray.Add(MakeShareable(new FJsonValueObject(CamObj)));
	}
	Value0Obj->SetArrayField(TEXT("intrinsics"), IntrinsicsArray);

	// Update resolution if provided
	TArray<TSharedPtr<FJsonValue>> ResolutionArray;
	for (const FBasaltCameraCalibration& Cam : Cameras)
	{
		TArray<TSharedPtr<FJsonValue>> ResArray;
		ResArray.Add(MakeShareable(new FJsonValueNumber(Cam.Resolution.Width)));
		ResArray.Add(MakeShareable(new FJsonValueNumber(Cam.Resolution.Height)));
		ResolutionArray.Add(MakeShareable(new FJsonValueArray(ResArray)));
	}
	Value0Obj->SetArrayField(TEXT("resolution"), ResolutionArray);

	// Serialize and save
	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);

	if (!FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer))
	{
		UE_LOG(LogBasalt, Error, TEXT("  Failed to serialize JSON!"));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(OutputString, *JsonFilePath))
	{
		UE_LOG(LogBasalt, Error, TEXT("  Failed to save file!"));
		return false;
	}

	UE_LOG(LogBasalt, Log, TEXT("  Successfully updated intrinsics"));
	UE_LOG(LogBasalt, Log, TEXT("========================================"));
	return true;
}

// ============================================================================================================== //
// Save Intrinsics (called on every parameter change)
// ============================================================================================================== //

bool UBasaltCalibrationReader::SaveIntrinsics(
	const FString& JsonFilePath,
	const FString& CameraType,
	const FBasaltKB4Intrinsics& KB4Params,
	const FBasaltEUCMIntrinsics& EUCMParams,
	const FBasaltIdealFisheyeIntrinsics& IdealParams,
	const FBasaltResolution& Resolution)
{
	FString ActualPath = JsonFilePath.IsEmpty() ? GetDefaultCalibrationPath() : JsonFilePath;

	UE_LOG(LogBasalt, Log, TEXT("========================================"));
	UE_LOG(LogBasalt, Log, TEXT("Saving intrinsics to: %s"), *ActualPath);
	UE_LOG(LogBasalt, Log, TEXT("  CameraType: %s"), *CameraType);

	// Ensure directory exists
	FString Directory = FPaths::GetPath(ActualPath);
	if (!Directory.IsEmpty() && !FPaths::DirectoryExists(Directory))
	{
		IFileManager::Get().MakeDirectory(*Directory, true);
	}

	// Try to load existing file to preserve extrinsics
	TSharedPtr<FJsonObject> RootObj;
	TSharedPtr<FJsonObject> Value0Obj;
	int32 NumCameras = 1;

	FString JsonString;
	if (FFileHelper::LoadFileToString(JsonString, *ActualPath))
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
		{
			Value0Obj = RootObj->GetObjectField(TEXT("value0"));
			if (Value0Obj.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* TImuCamArray;
				if (Value0Obj->TryGetArrayField(TEXT("T_imu_cam"), TImuCamArray))
				{
					NumCameras = TImuCamArray->Num();
				}
			}
		}
	}

	// Create new structure if file didn't exist or was invalid
	if (!RootObj.IsValid())
	{
		RootObj = MakeShareable(new FJsonObject);
	}
	if (!Value0Obj.IsValid())
	{
		Value0Obj = MakeShareable(new FJsonObject);

		// Create identity transforms for cameras
		TArray<TSharedPtr<FJsonValue>> TImuCamArray;
		TSharedPtr<FJsonObject> IdentityPose = MakeShareable(new FJsonObject);
		IdentityPose->SetNumberField(TEXT("px"), 0.0);
		IdentityPose->SetNumberField(TEXT("py"), 0.0);
		IdentityPose->SetNumberField(TEXT("pz"), 0.0);
		IdentityPose->SetNumberField(TEXT("qx"), 0.0);
		IdentityPose->SetNumberField(TEXT("qy"), 0.0);
		IdentityPose->SetNumberField(TEXT("qz"), 0.0);
		IdentityPose->SetNumberField(TEXT("qw"), 1.0);
		TImuCamArray.Add(MakeShareable(new FJsonValueObject(IdentityPose)));
		Value0Obj->SetArrayField(TEXT("T_imu_cam"), TImuCamArray);
		NumCameras = 1;
	}

	UE_LOG(LogBasalt, Log, TEXT("  NumCameras: %d"), NumCameras);

	// Build intrinsics array (same params for all cameras)
	TArray<TSharedPtr<FJsonValue>> IntrinsicsArray;
	TArray<TSharedPtr<FJsonValue>> ResolutionArray;

	for (int32 i = 0; i < NumCameras; ++i)
	{
		TSharedPtr<FJsonObject> CamObj = MakeShareable(new FJsonObject);
		CamObj->SetStringField(TEXT("camera_type"), CameraType);

		TSharedPtr<FJsonObject> IntrObj = MakeShareable(new FJsonObject);

		if (CameraType == TEXT("kb4") || CameraType == TEXT("Kannala Brandt"))
		{
			IntrObj->SetNumberField(TEXT("fx"), KB4Params.Fx);
			IntrObj->SetNumberField(TEXT("fy"), KB4Params.Fy);
			IntrObj->SetNumberField(TEXT("cx"), KB4Params.Cx);
			IntrObj->SetNumberField(TEXT("cy"), KB4Params.Cy);
			IntrObj->SetNumberField(TEXT("k1"), KB4Params.K1);
			IntrObj->SetNumberField(TEXT("k2"), KB4Params.K2);
			IntrObj->SetNumberField(TEXT("k3"), KB4Params.K3);
			IntrObj->SetNumberField(TEXT("k4"), KB4Params.K4);
		}
		else if (CameraType == TEXT("eucm") || CameraType == TEXT("Extended Unified Camera Model"))
		{
			IntrObj->SetNumberField(TEXT("fx"), EUCMParams.Fx);
			IntrObj->SetNumberField(TEXT("fy"), EUCMParams.Fy);
			IntrObj->SetNumberField(TEXT("cx"), EUCMParams.Cx);
			IntrObj->SetNumberField(TEXT("cy"), EUCMParams.Cy);
			IntrObj->SetNumberField(TEXT("alpha"), EUCMParams.Alpha);
			IntrObj->SetNumberField(TEXT("beta"), EUCMParams.Beta);
		}
		else if (CameraType == TEXT("pinhole"))
		{
			IntrObj->SetNumberField(TEXT("fx"), KB4Params.Fx);
			IntrObj->SetNumberField(TEXT("fy"), KB4Params.Fy);
			IntrObj->SetNumberField(TEXT("cx"), KB4Params.Cx);
			IntrObj->SetNumberField(TEXT("cy"), KB4Params.Cy);
		}
		else if (CameraType == TEXT("equidistant") || CameraType == TEXT("equisolid") ||
		         CameraType == TEXT("orthographic") || CameraType == TEXT("stereographic"))
		{
			// Ideal fisheye models
			IntrObj->SetStringField(TEXT("projection_type"), IdealParams.ProjectionType);
			IntrObj->SetNumberField(TEXT("fov"), IdealParams.FOV);
			IntrObj->SetNumberField(TEXT("cx"), IdealParams.Cx);
			IntrObj->SetNumberField(TEXT("cy"), IdealParams.Cy);
		}

		CamObj->SetObjectField(TEXT("intrinsics"), IntrObj);
		IntrinsicsArray.Add(MakeShareable(new FJsonValueObject(CamObj)));

		// Resolution
		TArray<TSharedPtr<FJsonValue>> ResArray;
		ResArray.Add(MakeShareable(new FJsonValueNumber(Resolution.Width)));
		ResArray.Add(MakeShareable(new FJsonValueNumber(Resolution.Height)));
		ResolutionArray.Add(MakeShareable(new FJsonValueArray(ResArray)));
	}

	Value0Obj->SetArrayField(TEXT("intrinsics"), IntrinsicsArray);
	Value0Obj->SetArrayField(TEXT("resolution"), ResolutionArray);
	RootObj->SetObjectField(TEXT("value0"), Value0Obj);

	// Serialize and save
	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);

	if (!FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer))
	{
		UE_LOG(LogBasalt, Error, TEXT("  Failed to serialize JSON!"));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(OutputString, *ActualPath))
	{
		UE_LOG(LogBasalt, Error, TEXT("  Failed to save file!"));
		return false;
	}

	UE_LOG(LogBasalt, Log, TEXT("  Successfully saved intrinsics"));
	UE_LOG(LogBasalt, Log, TEXT("========================================"));
	return true;
}

// ============================================================================================================== //
// Copy Calibration File
// ============================================================================================================== //

bool UBasaltCalibrationReader::CopyCalibrationFile(
	const FString& SourcePath,
	const FString& DestinationPath)
{
	UE_LOG(LogBasalt, Log, TEXT("========================================"));
	UE_LOG(LogBasalt, Log, TEXT("Copying calibration file"));
	UE_LOG(LogBasalt, Log, TEXT("  From: %s"), *SourcePath);
	UE_LOG(LogBasalt, Log, TEXT("  To: %s"), *DestinationPath);

	if (!FPaths::FileExists(SourcePath))
	{
		UE_LOG(LogBasalt, Error, TEXT("  Source file does not exist!"));
		return false;
	}

	// Ensure destination directory exists
	FString DestDir = FPaths::GetPath(DestinationPath);
	if (!DestDir.IsEmpty() && !FPaths::DirectoryExists(DestDir))
	{
		IFileManager::Get().MakeDirectory(*DestDir, true);
	}

	// Copy file
	if (IFileManager::Get().Copy(*DestinationPath, *SourcePath) != COPY_OK)
	{
		UE_LOG(LogBasalt, Error, TEXT("  Failed to copy file!"));
		return false;
	}

	UE_LOG(LogBasalt, Log, TEXT("  Successfully copied calibration file"));
	UE_LOG(LogBasalt, Log, TEXT("========================================"));
	return true;
}
