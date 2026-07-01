// MCAP_IMPLEMENTATION must be defined before any mcap header is included.
// It causes MCAP_PUBLIC = dllexport on Windows so that implementations are
// compiled into this translation unit (header-only pattern).
#define MCAP_IMPLEMENTATION

// UE5 requirement: module's own header must be first #include.
#include "ROS/RosbagCreator.h"
#include "ROS/CdrWriter.h"

THIRD_PARTY_INCLUDES_START
#pragma warning(push)
#pragma warning(disable: 4583 4582 4458 4456 4702)
#include <mcap/writer.hpp>
#pragma warning(pop)
THIRD_PARTY_INCLUDES_END

#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"
// FRenderOutputPath: scans numeric frame-subdirectory structure (0000/, 0001/, …)
#include "Stitching/IO/RenderOutputPath.h"
// ImageIO.h provides WITH_OPENCV guards, PreOpenCVHeaders, and opencv2/core.hpp.
// It also sets OPENCV_IO_ENABLE_OPENEXR via FImageIO::EnsureExrEnabled().
#include "Stitching/IO/ImageIO.h"

#if WITH_OPENCV
// Additional OpenCV modules needed for image processing (imgproc: cvtColor, threshold, convertTo)
#include "PreOpenCVHeaders.h"
#include "opencv2/imgproc.hpp"
#include "PostOpenCVHeaders.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogRosbagCreator, Log, All);

// ============================================================================
// ROS2 message schema definitions (ros2msg encoding for MCAP)
// Self-contained: each schema includes all nested type definitions.
// ============================================================================

static const char* kOdometrySchema = R"(std_msgs/Header header
string child_frame_id
geometry_msgs/PoseWithCovariance pose
geometry_msgs/TwistWithCovariance twist
================================================================================
MSG: std_msgs/Header
builtin_interfaces/Time stamp
string frame_id
================================================================================
MSG: builtin_interfaces/Time
int32 sec
uint32 nanosec
================================================================================
MSG: geometry_msgs/PoseWithCovariance
geometry_msgs/Pose pose
float64[36] covariance
================================================================================
MSG: geometry_msgs/Pose
geometry_msgs/Point position
geometry_msgs/Quaternion orientation
================================================================================
MSG: geometry_msgs/Point
float64 x
float64 y
float64 z
================================================================================
MSG: geometry_msgs/Quaternion
float64 x
float64 y
float64 z
float64 w
================================================================================
MSG: geometry_msgs/TwistWithCovariance
geometry_msgs/Twist twist
float64[36] covariance
================================================================================
MSG: geometry_msgs/Twist
geometry_msgs/Vector3 linear
geometry_msgs/Vector3 angular
================================================================================
MSG: geometry_msgs/Vector3
float64 x
float64 y
float64 z)";

static const char* kPointCloud2Schema = R"(std_msgs/Header header
uint32 height
uint32 width
sensor_msgs/PointField[] fields
bool is_bigendian
uint32 point_step
uint32 row_step
uint8[] data
bool is_dense
================================================================================
MSG: std_msgs/Header
builtin_interfaces/Time stamp
string frame_id
================================================================================
MSG: builtin_interfaces/Time
int32 sec
uint32 nanosec
================================================================================
MSG: sensor_msgs/PointField
uint8 INT8=1
uint8 UINT8=2
uint8 INT16=3
uint8 UINT16=4
uint8 INT32=5
uint8 UINT32=6
uint8 FLOAT32=7
uint8 FLOAT64=8
string name
uint32 offset
uint8 datatype
uint32 count)";

static const char* kPoseStampedSchema = R"(std_msgs/Header header
geometry_msgs/Pose pose
================================================================================
MSG: std_msgs/Header
builtin_interfaces/Time stamp
string frame_id
================================================================================
MSG: builtin_interfaces/Time
int32 sec
uint32 nanosec
================================================================================
MSG: geometry_msgs/Pose
geometry_msgs/Point position
geometry_msgs/Quaternion orientation
================================================================================
MSG: geometry_msgs/Point
float64 x
float64 y
float64 z
================================================================================
MSG: geometry_msgs/Quaternion
float64 x
float64 y
float64 z
float64 w)";

static const char* kTFMessageSchema = R"(geometry_msgs/TransformStamped[] transforms
================================================================================
MSG: geometry_msgs/TransformStamped
std_msgs/Header header
string child_frame_id
geometry_msgs/Transform transform
================================================================================
MSG: std_msgs/Header
builtin_interfaces/Time stamp
string frame_id
================================================================================
MSG: builtin_interfaces/Time
int32 sec
uint32 nanosec
================================================================================
MSG: geometry_msgs/Transform
geometry_msgs/Vector3 translation
geometry_msgs/Quaternion rotation
================================================================================
MSG: geometry_msgs/Vector3
float64 x
float64 y
float64 z
================================================================================
MSG: geometry_msgs/Quaternion
float64 x
float64 y
float64 z
float64 w)";

static const char* kCameraInfoSchema = R"(std_msgs/Header header
uint32 height
uint32 width
string distortion_model
float64[] D
float64[9] K
float64[9] R
float64[12] P
uint32 binning_x
uint32 binning_y
sensor_msgs/RegionOfInterest roi
================================================================================
MSG: std_msgs/Header
builtin_interfaces/Time stamp
string frame_id
================================================================================
MSG: builtin_interfaces/Time
int32 sec
uint32 nanosec
================================================================================
MSG: sensor_msgs/RegionOfInterest
uint32 x_offset
uint32 y_offset
uint32 height
uint32 width
bool do_rectify)";

static const char* kImageSchema = R"(std_msgs/Header header
uint32 height
uint32 width
string encoding
uint8 is_bigendian
uint32 step
uint8[] data
================================================================================
MSG: std_msgs/Header
builtin_interfaces/Time stamp
string frame_id
================================================================================
MSG: builtin_interfaces/Time
int32 sec
uint32 nanosec)";

// ============================================================================
// FArchive-backed IWritable — avoids fopen() / Unicode path issues on Windows
// ============================================================================

class FArchiveWritable final : public mcap::IWritable
{
public:
	explicit FArchiveWritable(FArchive* InArchive) : Archive(InArchive) {}

	void handleWrite(const std::byte* Data, uint64_t Size) override
	{
		Archive->Serialize(const_cast<std::byte*>(Data), static_cast<int64>(Size));
		BytesWritten += Size;
	}

	void end() override {}

	// Required by mcap::IWritable — returns total bytes written (used for chunk size tracking)
	uint64_t size() const override { return BytesWritten; }

private:
	FArchive* Archive;
	uint64_t BytesWritten = 0;
};

// ============================================================================
// YAML parsing helpers
// ============================================================================

/** Remove comment from a YAML line (everything after '#'). */
static FString StripYamlComment(const FString& Line)
{
	int32 Pos;
	if (Line.FindChar('#', Pos))
		return Line.Left(Pos);
	return Line;
}

/** Count leading spaces in a string. */
static int32 CountLeadingSpaces(const FString& Line)
{
	int32 Count = 0;
	for (TCHAR C : Line)
	{
		if (C == ' ') Count++;
		else break;
	}
	return Count;
}

/** Parse "[x, y, z]" or "[x, y, z, w]" into an array of doubles. */
static bool ParseDoubleArray(const FString& Text, TArray<double>& OutValues)
{
	FString Inner = Text.TrimStartAndEnd();
	if (!Inner.StartsWith(TEXT("[")) || !Inner.EndsWith(TEXT("]")))
		return false;

	Inner = Inner.Mid(1, Inner.Len() - 2);
	TArray<FString> Parts;
	Inner.ParseIntoArray(Parts, TEXT(","));
	for (const FString& Part : Parts)
		OutValues.Add(FCString::Atod(*Part.TrimStartAndEnd()));

	return !OutValues.IsEmpty();
}

/** Split "key: value" into key and value (value may be empty). */
static bool SplitKeyValue(const FString& Line, FString& OutKey, FString& OutValue)
{
	int32 ColonIdx;
	if (!Line.FindChar(':', ColonIdx))
		return false;

	OutKey   = Line.Left(ColonIdx).TrimStartAndEnd();
	OutValue = Line.Mid(ColonIdx + 1).TrimStartAndEnd();
	return !OutKey.IsEmpty();
}

// ============================================================================
// ParseYaml
// ============================================================================

bool URosbagCreator::ParseYaml(
	const FString&           FilePath,
	const FString&           ActorNameFilter,
	TArray<FCameraPoseEntry>& OutPoses,
	FString&                 OutCoordinateSystem) const
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		UE_LOG(LogRosbagCreator, Error, TEXT("Cannot read YAML: %s"), *FilePath);
		return false;
	}

	TArray<FString> Lines;
	FileContent.ParseIntoArrayLines(Lines, false);

	// State machine:
	//  -1 = inside "metadata:" section (extracting coordinate_system etc.)
	//   0 = scanning for top-level keys ("metadata:", "cameras:")
	//   1 = scanning for a matching camera block
	//   2 = inside a camera block, scanning for "poses:"
	//   3 = inside poses, parsing pose entries
	int32 State = 0;
	FString ActiveCameraKey;

	FCameraPoseEntry CurrentPose;
	bool bHavePose = false;

	for (int32 i = 0; i < Lines.Num(); i++)
	{
		const FString Raw      = StripYamlComment(Lines[i]).TrimEnd();
		const int32   Indent   = CountLeadingSpaces(Raw);
		const FString Trimmed  = Raw.TrimStart();

		if (Trimmed.IsEmpty())
			continue;

		switch (State)
		{
		case -1: // inside metadata section, looking for known keys at indent=2
			if (Indent <= 1 && !Trimmed.IsEmpty())
			{
				// Exited metadata — back to scanning top-level keys
				State = 0;
				// Fall through to handle the current line in State 0
				goto state0;
			}
			else if (Indent == 2)
			{
				FString Key, Value;
				if (SplitKeyValue(Trimmed, Key, Value) && Key == TEXT("coordinate_system"))
				{
					// Strip surrounding quotes if present
					OutCoordinateSystem = Value.TrimStartAndEnd().TrimChar('"').TrimChar('\'');
				}
			}
			break;

		case 0: // looking for top-level keys
		state0:
			if (Trimmed == TEXT("metadata:"))
				State = -1;
			else if (Trimmed == TEXT("cameras:"))
				State = 1;
			break;

		case 1: // looking for a camera block at indent=2
			if (Indent == 2 && Trimmed.EndsWith(TEXT(":")))
			{
				FString CamKey = Trimmed.LeftChop(1); // remove trailing ':'
				// Match if no filter or filter matches (accounting for space→underscore)
				const FString FilterNorm = ActorNameFilter.Replace(TEXT(" "), TEXT("_"));
				if (ActorNameFilter.IsEmpty() || CamKey == FilterNorm || CamKey == ActorNameFilter)
				{
					ActiveCameraKey = CamKey;
					State = 2;
					UE_LOG(LogRosbagCreator, Log, TEXT("Parsing camera: %s"), *CamKey);
				}
			}
			break;

		case 2: // inside camera block, looking for "poses:" at indent=4
			if (Indent <= 1 && !Trimmed.IsEmpty())
			{
				// Left root level without finding poses
				State = 1;
			}
			else if (Indent == 2 && Trimmed.EndsWith(TEXT(":")))
			{
				// Another camera block started — if we have a filter, skip
				// If no filter and we already collected poses, stop
				if (!OutPoses.IsEmpty())
				{
					State = 1; // done with first camera
				}
			}
			else if (Trimmed == TEXT("poses:"))
			{
				State = 3;
			}
			break;

		case 3: // inside poses
			// A pose block starts at indent=6 with "- frame: N"
			if (Indent == 6 && Trimmed.StartsWith(TEXT("- ")))
			{
				// Commit previous pose
				if (bHavePose)
					OutPoses.Add(CurrentPose);

				bHavePose = true;
				CurrentPose = FCameraPoseEntry();

				// Parse "- frame: N"
				FString AfterDash = Trimmed.Mid(2).TrimStart();
				FString Key, Value;
				if (SplitKeyValue(AfterDash, Key, Value) && Key == TEXT("frame"))
					CurrentPose.FrameNumber = FCString::Atoi(*Value);
			}
			else if (bHavePose && Indent == 8)
			{
				// Pose field: "key: value"
				FString Key, Value;
				if (!SplitKeyValue(Trimmed, Key, Value))
					continue;

				if (Key == TEXT("timestamp"))
				{
					CurrentPose.TimestampNs = FCString::Atoi64(*Value);
				}
				else if (Key == TEXT("position"))
				{
					TArray<double> Vals;
					if (ParseDoubleArray(Value, Vals) && Vals.Num() >= 3)
						CurrentPose.Position = FVector(Vals[0], Vals[1], Vals[2]);
				}
				else if (Key == TEXT("orientation"))
				{
					TArray<double> Vals;
					if (ParseDoubleArray(Value, Vals) && Vals.Num() >= 4)
						CurrentPose.Orientation = FQuat(Vals[0], Vals[1], Vals[2], Vals[3]); // x,y,z,w
				}
				else if (Key == TEXT("linear_velocity"))
				{
					TArray<double> Vals;
					if (ParseDoubleArray(Value, Vals) && Vals.Num() >= 3)
						CurrentPose.LinearVelocity = FVector(Vals[0], Vals[1], Vals[2]);
				}
				else if (Key == TEXT("angular_velocity"))
				{
					TArray<double> Vals;
					if (ParseDoubleArray(Value, Vals) && Vals.Num() >= 3)
						CurrentPose.AngularVelocity = FVector(Vals[0], Vals[1], Vals[2]);
				}
			}
			else if (Indent <= 4 && !Trimmed.IsEmpty())
			{
				// Exited poses section
				if (bHavePose)
				{
					OutPoses.Add(CurrentPose);
					bHavePose = false;
				}
				// If no filter, stop after first camera. If filter, keep looking.
				if (ActorNameFilter.IsEmpty())
					State = 99; // done
				else
					State = 1;
			}
			break;

		default:
			break;
		}
	}

	// Flush last pose if file ended while still in poses section
	if (bHavePose)
		OutPoses.Add(CurrentPose);

	UE_LOG(LogRosbagCreator, Log, TEXT("Parsed %d poses from '%s'"), OutPoses.Num(), *FilePath);
	return !OutPoses.IsEmpty();
}

// ============================================================================
// CDR helpers
// ============================================================================

static void NsToSecNanosec(int64 Ns, int32& OutSec, uint32& OutNanosec)
{
	OutSec     = static_cast<int32>(Ns / 1000000000LL);
	OutNanosec = static_cast<uint32>(Ns % 1000000000LL);
}

static void WriteMsgHeader(FCdrWriter& W, int64 TimestampNs, const FString& FrameId)
{
	int32 Sec; uint32 NSec;
	NsToSecNanosec(TimestampNs, Sec, NSec);
	W.WriteInt32(Sec);
	W.WriteUInt32(NSec);
	W.WriteString(FrameId);
}

// ============================================================================
// ConvertPoseOpenCVToROS
//
// OpenCV/Basalt convention  →  ROS world convention
//   X-right, Y-down, Z-fwd     X-fwd,  Y-left, Z-up
//
// Position:
//   ros.X =  cv.Z   (forward)
//   ros.Y = -cv.X   (left = -right)
//   ros.Z = -cv.Y   (up   = -down)
//
// Quaternion (derived from basis-change analysis for the LHS→RHS handedness flip):
//   The OpenCV rotation axis (qx_cv, qy_cv, qz_cv) maps to ROS axes as:
//   q_ros = (-qz_cv, -qx_cv, qy_cv, qw_cv)
//
// Velocities use the same axis relabeling (both are body-frame vectors):
//   v_ros.X =  v_cv.Z
//   v_ros.Y = -v_cv.X
//   v_ros.Z = -v_cv.Y
// ============================================================================

void URosbagCreator::ConvertPoseOpenCVToROS(FCameraPoseEntry& Pose)
{
	// Position
	const FVector P = Pose.Position;
	Pose.Position = FVector(P.Z, -P.X, -P.Y);

	// Orientation
	const FQuat Q = Pose.Orientation;
	Pose.Orientation = FQuat(-Q.Z, -Q.X, Q.Y, Q.W);
	Pose.Orientation.Normalize();

	// Linear velocity (body frame — same axis relabeling)
	const FVector LV = Pose.LinearVelocity;
	Pose.LinearVelocity = FVector(LV.Z, -LV.X, -LV.Y);

	// Angular velocity (body frame — same axis relabeling)
	const FVector AV = Pose.AngularVelocity;
	Pose.AngularVelocity = FVector(AV.Z, -AV.X, -AV.Y);
}

// ============================================================================
// URosbagCreator::CreateRosbag  (async shell — public Blueprint entry point)
// ============================================================================

void URosbagCreator::CreateRosbag(
	const FString&         YamlPath,
	const FString&         RenderDirectory,
	const FString&         OutputPath,
	const FRosbagSettings& Settings)
{
	if (bIsRunning)
	{
		UE_LOG(LogRosbagCreator, Warning, TEXT("CreateRosbag: already running, ignoring call."));
		return;
	}

	bIsRunning = true;
	ProcessedFrames.store(0);
	TotalFramesCount.store(0);

	// Prevent GC while the worker thread holds a reference to this UObject
	AddToRoot();
	StartProgressPolling();

	// Capture by value so the lambda owns all inputs
	Async(EAsyncExecution::Thread,
		[this,
		 YamlPath        = YamlPath,
		 RenderDirectory = RenderDirectory,
		 OutputPath      = OutputPath,
		 Settings        = Settings]()
	{
		bool bSuccess = false;
		try
		{
			bSuccess = CreateRosbagInternal(YamlPath, RenderDirectory, OutputPath, Settings);
		}
		catch (...)
		{
			UE_LOG(LogRosbagCreator, Error, TEXT("CreateRosbag: unhandled exception on worker thread"));
		}

		AsyncTask(ENamedThreads::GameThread, [this, bSuccess]()
		{
			StopProgressPolling();

			// Fire a final 100 % progress event
			const int32 Total = TotalFramesCount.load();
			if (Total > 0)
				OnRosbagProgress.Broadcast(Total, Total);

			bIsRunning = false;
			RemoveFromRoot();

			UE_LOG(LogRosbagCreator, Log, TEXT("CreateRosbag done. Success: %s"),
				bSuccess ? TEXT("true") : TEXT("false"));

			OnRosbagComplete.Broadcast(bSuccess);
		});
	});
}

// ============================================================================
// URosbagCreator::CreateRosbagInternal  (synchronous, runs on worker thread)
// ============================================================================

bool URosbagCreator::CreateRosbagInternal(
	const FString&         YamlPath,
	const FString&         RenderDirectory,
	const FString&         OutputPath,
	const FRosbagSettings& Settings)
{
	UE_LOG(LogRosbagCreator, Log, TEXT("=== CreateRosbag start ==="));

	if (!FPaths::FileExists(YamlPath))
	{
		UE_LOG(LogRosbagCreator, Error, TEXT("YAML not found: %s"), *YamlPath);
		return false;
	}

	if (!IFileManager::Get().DirectoryExists(*RenderDirectory))
	{
		UE_LOG(LogRosbagCreator, Error, TEXT("RenderDirectory does not exist: %s"), *RenderDirectory);
		return false;
	}

	// Step 1: parse poses and coordinate system from YAML
	TArray<FCameraPoseEntry> Poses;
	FString CoordinateSystem;
	if (!ParseYaml(YamlPath, Settings.ActorName, Poses, CoordinateSystem))
	{
		UE_LOG(LogRosbagCreator, Error, TEXT("No poses found in YAML (actor filter: '%s')"), *Settings.ActorName);
		return false;
	}

	// Step 1b: convert coordinate system to ROS world (X-fwd, Y-left, Z-up).
	// Prefer the coordinate_system field from the YAML metadata; fall back to
	// Settings.bInputIsOpenCVCoordinates for YAMLs that predate the metadata section.
	const bool bIsOpenCV = CoordinateSystem.IsEmpty()
		? Settings.bInputIsOpenCVCoordinates
		: CoordinateSystem.Contains(TEXT("OpenCV"), ESearchCase::IgnoreCase);

	if (bIsOpenCV)
	{
		UE_LOG(LogRosbagCreator, Log, TEXT("Converting %d poses from OpenCV to ROS world convention (source: %s)"),
			Poses.Num(), CoordinateSystem.IsEmpty() ? TEXT("Settings") : TEXT("YAML metadata"));
		for (FCameraPoseEntry& Pose : Poses)
			ConvertPoseOpenCVToROS(Pose);
	}

	// Step 2: resolve output path
	FString ResolvedOutput = OutputPath;
	if (ResolvedOutput.IsEmpty())
		ResolvedOutput = RenderDirectory / TEXT("camera_trajectory.mcap");

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ResolvedOutput), true);

	// Step 3: write MCAP
	const bool bOk = WriteMcap(ResolvedOutput, Poses, RenderDirectory, Settings);
	if (bOk)
		UE_LOG(LogRosbagCreator, Log, TEXT("=== Rosbag written: %s ==="), *ResolvedOutput);

	return bOk;
}

// ============================================================================
// Progress polling helpers
// ============================================================================

void URosbagCreator::StartProgressPolling()
{
	ProgressTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([this](float) -> bool
		{
			if (bIsRunning)
			{
				const int32 Current = ProcessedFrames.load();
				const int32 Total   = TotalFramesCount.load();
				if (Total > 0)
					OnRosbagProgress.Broadcast(Current, Total);
			}
			return true; // keep ticking
		}),
		0.1f);  // 100 ms interval
}

void URosbagCreator::StopProgressPolling()
{
	if (ProgressTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ProgressTickerHandle);
		ProgressTickerHandle.Reset();
	}
}

// ============================================================================
// ScanFramesInDirectory
// ============================================================================

TArray<int32> URosbagCreator::ScanFramesInDirectory(
	const FString& Dir,
	const FString& Pattern) const
{
	// The render output uses numeric sub-directories (0000/, 0001/, …).
	// Scan all direct children of Dir, keep the ones whose names are all-digits,
	// and verify that the resolved file actually exists inside that sub-directory.

	TArray<FString> SubDirNames;
	IFileManager::Get().FindFiles(SubDirNames, *(Dir / TEXT("*")), false, true);

	TArray<int32> Frames;
	for (const FString& DirName : SubDirNames)
	{
		// Reject non-numeric directory names (e.g. "Saved", "Logs")
		bool bAllDigits = !DirName.IsEmpty();
		for (TCHAR C : DirName)
		{
			if (!FChar::IsDigit(C)) { bAllDigits = false; break; }
		}
		if (!bAllDigits)
			continue;

		const int32 Frame = FCString::Atoi(*DirName);
		const FString FilePath = Dir / ResolveFrameFilename(Pattern, Frame);
		if (FPaths::FileExists(FilePath))
			Frames.AddUnique(Frame);
	}
	Frames.Sort();

	UE_LOG(LogRosbagCreator, Log, TEXT("ScanFramesInDirectory: found %d frames in '%s'"),
		Frames.Num(), *Dir);
	return Frames;
}

// ============================================================================
// WriteMcap
// ============================================================================

bool URosbagCreator::WriteMcap(
	const FString&                  FilePath,
	const TArray<FCameraPoseEntry>& Poses,
	const FString&                  RenderDirectory,
	const FRosbagSettings&          Settings) const
{
	TUniquePtr<FArchive> FileAr(IFileManager::Get().CreateFileWriter(*FilePath));
	if (!FileAr)
	{
		UE_LOG(LogRosbagCreator, Error, TEXT("Cannot open output file: %s"), *FilePath);
		return false;
	}

	FArchiveWritable Writable(FileAr.Get());

	mcap::McapWriterOptions Options("ros2");
	Options.compression = mcap::Compression::None;
	Options.noChunkCRC  = true;

	mcap::McapWriter Writer;
	Writer.open(Writable, Options);

	// ------------------------------------------------------------------
	// Register schemas

	const bool bWriteOdom    = !Settings.OdometryTopic.IsEmpty();
	const bool bWritePose    = !Settings.PoseStampedTopic.IsEmpty();
	const bool bWritePC2     = !Settings.PointCloudTopic.IsEmpty()
	                           && !Settings.DepthFilePattern.IsEmpty()
	                           && !Settings.ColorFilePattern.IsEmpty();
	const bool bWriteTf      = !Settings.TfTopic.IsEmpty();
	const bool bWriteDepth   = !Settings.DepthTopic.IsEmpty() && !Settings.DepthFilePattern.IsEmpty();
	const bool bWriteColor   = !Settings.ColorTopic.IsEmpty() && !Settings.ColorFilePattern.IsEmpty();
	const bool bWriteCamInfo = !Settings.CameraInfoTopic.IsEmpty();

	// Parse Basalt calibration JSON for /tf_static (optional)
	TArray<TPair<FVector, FQuat>> RigCameras;
	bool bWriteStaticTf = !Settings.CalibrationFilePath.IsEmpty()
	                      && !Settings.TfStaticTopic.IsEmpty()
	                      && !Settings.CamFramePrefix.IsEmpty();
	if (bWriteStaticTf)
		bWriteStaticTf = ParseCalibrationJson(Settings.CalibrationFilePath, RigCameras);

	mcap::Schema OdomSchema  ("nav_msgs/Odometry",           "ros2msg", kOdometrySchema);
	mcap::Schema PoseSchema  ("geometry_msgs/PoseStamped",   "ros2msg", kPoseStampedSchema);
	mcap::Schema TFSchema    ("tf2_msgs/TFMessage",          "ros2msg", kTFMessageSchema);
	mcap::Schema CISchema    ("sensor_msgs/CameraInfo",      "ros2msg", kCameraInfoSchema);
	mcap::Schema ImgSchema   ("sensor_msgs/Image",           "ros2msg", kImageSchema);
	mcap::Schema PC2Schema   ("sensor_msgs/PointCloud2",     "ros2msg", kPointCloud2Schema);

	if (bWriteOdom)                        Writer.addSchema(OdomSchema);
	if (bWritePose)                        Writer.addSchema(PoseSchema);
	if (bWriteTf || bWriteStaticTf)        Writer.addSchema(TFSchema);
	if (bWriteCamInfo)                     Writer.addSchema(CISchema);
	if (bWriteDepth || bWriteColor)        Writer.addSchema(ImgSchema);
	if (bWritePC2)                         Writer.addSchema(PC2Schema);

	// ------------------------------------------------------------------
	// Register channels

	auto MakeCh = [](const FString& Topic, mcap::SchemaId SchId) -> mcap::Channel
	{
		std::string TopicStd(TCHAR_TO_UTF8(*Topic));
		return mcap::Channel(TopicStd, "cdr", SchId);
	};

	mcap::Channel OdomCh, PoseCh, TfCh, StaticTfCh, CamInfoCh, DepthCh, ColorCh, PC2Ch;

	if (bWriteOdom)      { OdomCh      = MakeCh(Settings.OdometryTopic,    OdomSchema.id);  Writer.addChannel(OdomCh); }
	if (bWritePose)      { PoseCh      = MakeCh(Settings.PoseStampedTopic, PoseSchema.id);  Writer.addChannel(PoseCh); }
	if (bWriteTf)        { TfCh        = MakeCh(Settings.TfTopic,          TFSchema.id);    Writer.addChannel(TfCh); }
	if (bWriteStaticTf)  { StaticTfCh  = MakeCh(Settings.TfStaticTopic,    TFSchema.id);    Writer.addChannel(StaticTfCh); }
	if (bWriteCamInfo)   { CamInfoCh   = MakeCh(Settings.CameraInfoTopic,  CISchema.id);    Writer.addChannel(CamInfoCh); }
	if (bWriteDepth)     { DepthCh     = MakeCh(Settings.DepthTopic,       ImgSchema.id);   Writer.addChannel(DepthCh); }
	if (bWriteColor)     { ColorCh     = MakeCh(Settings.ColorTopic,       ImgSchema.id);   Writer.addChannel(ColorCh); }
	if (bWritePC2)       { PC2Ch       = MakeCh(Settings.PointCloudTopic,  PC2Schema.id);   Writer.addChannel(PC2Ch); }

	// ------------------------------------------------------------------
	// Write messages

	uint32 MsgSequence = 0;

	auto WriteMsg = [&](mcap::ChannelId ChId, int64 TimestampNs, const TArray<uint8>& Data)
	{
		if (Data.IsEmpty()) return;
		mcap::Message Msg;
		Msg.channelId   = ChId;
		Msg.sequence    = MsgSequence++;
		Msg.logTime     = static_cast<uint64_t>(TimestampNs);
		Msg.publishTime = Msg.logTime;
		Msg.dataSize    = static_cast<uint64_t>(Data.Num());
		Msg.data        = reinterpret_cast<const std::byte*>(Data.GetData());
		const mcap::Status WriteStatus = Writer.write(Msg);
		if (!WriteStatus.ok())
		{
			UE_LOG(LogRosbagCreator, Warning, TEXT("MCAP write failed: %hs"), WriteStatus.message.c_str());
		}
	};

	// Write /tf_static once at the timestamp of the first pose
	if (bWriteStaticTf && !Poses.IsEmpty())
	{
		WriteMsg(StaticTfCh.id, Poses[0].TimestampNs,
			SerializeStaticTfMessage(RigCameras, Poses[0].TimestampNs, Settings));
		UE_LOG(LogRosbagCreator, Log, TEXT("Written /tf_static with %d camera transforms"), RigCameras.Num());
	}

	// Expose total frame count so the async progress ticker can read it
	TotalFramesCount.store(Poses.Num());
	ProcessedFrames.store(0);

	for (const FCameraPoseEntry& Pose : Poses)
	{
		if (bWriteOdom)
			WriteMsg(OdomCh.id, Pose.TimestampNs, SerializeOdometry(Pose, Settings));

		if (bWritePose)
			WriteMsg(PoseCh.id, Pose.TimestampNs, SerializePoseStamped(Pose, Settings));

		if (bWriteTf)
			WriteMsg(TfCh.id, Pose.TimestampNs, SerializeTfMessage(Pose, Settings));

		if (bWriteCamInfo)
			WriteMsg(CamInfoCh.id, Pose.TimestampNs, SerializeCameraInfo(Pose.TimestampNs, Settings));

		if (bWriteDepth)
		{
			const FString File = RenderDirectory / ResolveFrameFilename(Settings.DepthFilePattern, Pose.FrameNumber);
			if (FPaths::FileExists(File))
				WriteMsg(DepthCh.id, Pose.TimestampNs, SerializeDepthImage(File, Pose.TimestampNs, Settings));
			else
				UE_LOG(LogRosbagCreator, Warning, TEXT("Depth file not found: %s"), *File);
		}

		if (bWriteColor)
		{
			const FString File = RenderDirectory / ResolveFrameFilename(Settings.ColorFilePattern, Pose.FrameNumber);
			if (FPaths::FileExists(File))
				WriteMsg(ColorCh.id, Pose.TimestampNs, SerializeColorImage(File, Pose.TimestampNs, Settings));
			else
				UE_LOG(LogRosbagCreator, Warning, TEXT("Color file not found: %s"), *File);
		}

		if (bWritePC2)
		{
			const FString DepthFile = RenderDirectory / ResolveFrameFilename(Settings.DepthFilePattern, Pose.FrameNumber);
			const FString ColorFile = RenderDirectory / ResolveFrameFilename(Settings.ColorFilePattern, Pose.FrameNumber);
			if (FPaths::FileExists(DepthFile) && FPaths::FileExists(ColorFile))
				WriteMsg(PC2Ch.id, Pose.TimestampNs, SerializePointCloud2(DepthFile, ColorFile, Pose, Settings));
			else
				UE_LOG(LogRosbagCreator, Warning, TEXT("Point cloud files not found for frame %d"), Pose.FrameNumber);
		}

		++ProcessedFrames;
	}

	Writer.close();
	return true;
}

// ============================================================================
// ParseCalibrationJson
// ============================================================================

bool URosbagCreator::ParseCalibrationJson(
	const FString&                FilePath,
	TArray<TPair<FVector, FQuat>>& OutCameras) const
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogRosbagCreator, Error, TEXT("Cannot read calibration JSON: %s"), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogRosbagCreator, Error, TEXT("Failed to parse calibration JSON: %s"), *FilePath);
		return false;
	}

	const TSharedPtr<FJsonObject>* Value0Ptr;
	if (!Root->TryGetObjectField(TEXT("value0"), Value0Ptr))
	{
		UE_LOG(LogRosbagCreator, Error, TEXT("Calibration JSON missing 'value0'"));
		return false;
	}
	const TSharedPtr<FJsonObject>& Value0 = *Value0Ptr;

	const TArray<TSharedPtr<FJsonValue>>* TImuCamArray;
	if (!Value0->TryGetArrayField(TEXT("T_imu_cam"), TImuCamArray))
	{
		UE_LOG(LogRosbagCreator, Error, TEXT("Calibration JSON missing 'T_imu_cam'"));
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Entry : *TImuCamArray)
	{
		const TSharedPtr<FJsonObject>* ObjPtr;
		if (!Entry->TryGetObject(ObjPtr))
			continue;
		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

		const FVector Position(
			Obj->GetNumberField(TEXT("px")),
			Obj->GetNumberField(TEXT("py")),
			Obj->GetNumberField(TEXT("pz")));

		const FQuat Orientation(
			Obj->GetNumberField(TEXT("qx")),
			Obj->GetNumberField(TEXT("qy")),
			Obj->GetNumberField(TEXT("qz")),
			Obj->GetNumberField(TEXT("qw")));

		OutCameras.Add(TPair<FVector, FQuat>(Position, Orientation));
	}

	UE_LOG(LogRosbagCreator, Log, TEXT("ParseCalibrationJson: %d cameras from '%s'"),
		OutCameras.Num(), *FilePath);
	return OutCameras.Num() > 0;
}

// ============================================================================
// SerializeStaticTfMessage
// ============================================================================

TArray<uint8> URosbagCreator::SerializeStaticTfMessage(
	const TArray<TPair<FVector, FQuat>>& Cameras,
	int64                               TimestampNs,
	const FRosbagSettings&              Settings) const
{
	FCdrWriter W;

	// geometry_msgs/TransformStamped[] transforms — one entry per camera
	W.WriteUInt32(static_cast<uint32>(Cameras.Num()));

	// Basalt IMU convention: Y-up, XZ = horizontal plane
	// ROS camera_link convention: Z-up, XY = horizontal plane
	// Basis change: +90° rotation around X axis
	//   ros.X =  basalt.X   (X stays)
	//   ros.Y = -basalt.Z   (horizontal: Z → -Y)
	//   ros.Z =  basalt.Y   (up: Y → Z)
	// The quaternion frame change must use the same +90° to stay consistent.
	const FQuat QFrameChange(FVector::XAxisVector, PI / 2.0f);

	for (int32 i = 0; i < Cameras.Num(); ++i)
	{
		const FVector& Pos = Cameras[i].Key;
		const FQuat&   Rot = Cameras[i].Value;

		// Apply basis change to position
		const FVector PosRos(Pos.X, -Pos.Z, Pos.Y);

		// Apply same basis change to orientation (passive coordinate transform)
		const FQuat RotRos = (QFrameChange * Rot * QFrameChange.Inverse()).GetNormalized();

		// Header: parent frame = camera_link (= IMU frame in Basalt)
		WriteMsgHeader(W, TimestampNs, Settings.CameraFrameId);
		// child_frame_id: cam0, cam1, ...
		W.WriteString(FString::Printf(TEXT("%s%d"), *Settings.CamFramePrefix, i));
		// transform.translation
		W.WriteFloat64(PosRos.X);
		W.WriteFloat64(PosRos.Y);
		W.WriteFloat64(PosRos.Z);
		// transform.rotation (x, y, z, w)
		W.WriteFloat64(RotRos.X);
		W.WriteFloat64(RotRos.Y);
		W.WriteFloat64(RotRos.Z);
		W.WriteFloat64(RotRos.W);
	}

	return W.Buffer;
}

// ============================================================================
// ResolveFrameFilename
// ============================================================================

FString URosbagCreator::ResolveFrameFilename(const FString& Pattern, int32 Frame) const
{
	FString Result = Pattern;
	if (Result.Contains(TEXT("{frame:05d}")))
		Result = Result.Replace(TEXT("{frame:05d}"), *FString::Printf(TEXT("%05d"), Frame));
	else if (Result.Contains(TEXT("{frame:04d}")))
		Result = Result.Replace(TEXT("{frame:04d}"), *FString::Printf(TEXT("%04d"), Frame));
	else
		Result = Result.Replace(TEXT("{frame}"), *FString::Printf(TEXT("%d"), Frame));
	return Result;
}

// ============================================================================
// CDR serializers
// ============================================================================

TArray<uint8> URosbagCreator::SerializeOdometry(
	const FCameraPoseEntry& Pose,
	const FRosbagSettings&  Settings) const
{
	FCdrWriter W;

	// std_msgs/Header
	WriteMsgHeader(W, Pose.TimestampNs, Settings.MapFrameId);

	// child_frame_id
	W.WriteString(Settings.CameraFrameId);

	// geometry_msgs/PoseWithCovariance
	//   geometry_msgs/Pose.position  (float64 x, y, z — Pad(8) auto-aligns after strings)
	W.WriteFloat64(Pose.Position.X);
	W.WriteFloat64(Pose.Position.Y);
	W.WriteFloat64(Pose.Position.Z);
	//   geometry_msgs/Pose.orientation  (x, y, z, w)
	W.WriteFloat64(Pose.Orientation.X);
	W.WriteFloat64(Pose.Orientation.Y);
	W.WriteFloat64(Pose.Orientation.Z);
	W.WriteFloat64(Pose.Orientation.W);
	//   float64[36] covariance  (diagonal)
	{
		double Cov[36] = {};
		const double D = Settings.PoseCovarianceDiagonal;
		for (int i = 0; i < 6; i++) Cov[i * 6 + i] = D;
		W.WriteFloat64FixedArray(Cov, 36);
	}

	// geometry_msgs/TwistWithCovariance
	//   geometry_msgs/Twist.linear
	W.WriteFloat64(Pose.LinearVelocity.X);
	W.WriteFloat64(Pose.LinearVelocity.Y);
	W.WriteFloat64(Pose.LinearVelocity.Z);
	//   geometry_msgs/Twist.angular
	W.WriteFloat64(Pose.AngularVelocity.X);
	W.WriteFloat64(Pose.AngularVelocity.Y);
	W.WriteFloat64(Pose.AngularVelocity.Z);
	//   float64[36] covariance
	{
		double Cov[36] = {};
		const double D = Settings.TwistCovarianceDiagonal;
		for (int i = 0; i < 6; i++) Cov[i * 6 + i] = D;
		W.WriteFloat64FixedArray(Cov, 36);
	}

	return W.Buffer;
}

TArray<uint8> URosbagCreator::SerializePoseStamped(
	const FCameraPoseEntry& Pose,
	const FRosbagSettings&  Settings) const
{
	FCdrWriter W;

	// std_msgs/Header
	WriteMsgHeader(W, Pose.TimestampNs, Settings.MapFrameId);

	// geometry_msgs/Pose.position  (WriteFloat64 auto-aligns to 8 after the string)
	W.WriteFloat64(Pose.Position.X);
	W.WriteFloat64(Pose.Position.Y);
	W.WriteFloat64(Pose.Position.Z);

	// geometry_msgs/Pose.orientation  (x, y, z, w)
	W.WriteFloat64(Pose.Orientation.X);
	W.WriteFloat64(Pose.Orientation.Y);
	W.WriteFloat64(Pose.Orientation.Z);
	W.WriteFloat64(Pose.Orientation.W);

	return W.Buffer;
}

TArray<uint8> URosbagCreator::SerializeTfMessage(
	const FCameraPoseEntry& Pose,
	const FRosbagSettings&  Settings) const
{
	FCdrWriter W;

	// geometry_msgs/TransformStamped[] transforms — variable array, one element
	W.WriteUInt32(1);

	// TransformStamped[0].header
	WriteMsgHeader(W, Pose.TimestampNs, Settings.MapFrameId);
	// TransformStamped[0].child_frame_id
	W.WriteString(Settings.CameraFrameId);
	// TransformStamped[0].transform.translation
	W.WriteFloat64(Pose.Position.X);
	W.WriteFloat64(Pose.Position.Y);
	W.WriteFloat64(Pose.Position.Z);
	// TransformStamped[0].transform.rotation  (x, y, z, w)
	W.WriteFloat64(Pose.Orientation.X);
	W.WriteFloat64(Pose.Orientation.Y);
	W.WriteFloat64(Pose.Orientation.Z);
	W.WriteFloat64(Pose.Orientation.W);

	return W.Buffer;
}

TArray<uint8> URosbagCreator::SerializeCameraInfo(
	int64                  TimestampNs,
	const FRosbagSettings& Settings) const
{
	FCdrWriter W;

	WriteMsgHeader(W, TimestampNs, Settings.CameraFrameId);

	W.WriteUInt32(static_cast<uint32>(Settings.ImageHeight));
	W.WriteUInt32(static_cast<uint32>(Settings.ImageWidth));

	W.WriteString("plumb_bob");

	// float64[] D — empty for equirectangular (no distortion model)
	W.WriteFloat64Sequence(nullptr, 0);

	// float64[9] K — camera matrix
	const double fx = Settings.FocalLengthX;
	const double fy = Settings.FocalLengthY;
	const double cx = Settings.PrincipalPointX;
	const double cy = Settings.PrincipalPointY;
	const double K[9] = { fx, 0.0, cx,  0.0, fy, cy,  0.0, 0.0, 1.0 };
	W.WriteFloat64FixedArray(K, 9);

	// float64[9] R — identity
	const double R[9] = { 1.0, 0.0, 0.0,  0.0, 1.0, 0.0,  0.0, 0.0, 1.0 };
	W.WriteFloat64FixedArray(R, 9);

	// float64[12] P — projection matrix
	const double P[12] = { fx, 0.0, cx, 0.0,  0.0, fy, cy, 0.0,  0.0, 0.0, 1.0, 0.0 };
	W.WriteFloat64FixedArray(P, 12);

	// binning_x, binning_y (0 = no binning)
	W.WriteUInt32(0);
	W.WriteUInt32(0);

	// sensor_msgs/RegionOfInterest — all zeros = full image
	W.WriteUInt32(0); // x_offset
	W.WriteUInt32(0); // y_offset
	W.WriteUInt32(0); // height (0 = full)
	W.WriteUInt32(0); // width  (0 = full)
	W.WriteBool(false); // do_rectify

	return W.Buffer;
}

TArray<uint8> URosbagCreator::SerializePointCloud2(
	const FString&          DepthFile,
	const FString&          ColorFile,
	const FCameraPoseEntry& Pose,
	const FRosbagSettings&  Settings) const
{
#if WITH_OPENCV
	// --- Load images -------------------------------------------------------

	cv::Mat DepthRaw = FImageIO::Load(DepthFile);
	cv::Mat ColorRaw = FImageIO::Load(ColorFile);

	if (DepthRaw.empty() || ColorRaw.empty())
	{
		UE_LOG(LogRosbagCreator, Warning, TEXT("Point cloud: failed to load depth or color EXR"));
		return {};
	}

	// Depth → float32 single-channel, convert cm → m
	cv::Mat Depth;
	if (DepthRaw.channels() > 1)
		cv::cvtColor(DepthRaw, Depth, cv::COLOR_BGR2GRAY);
	else
		Depth = DepthRaw;
	if (Depth.type() != CV_32F)
		Depth.convertTo(Depth, CV_32F);
	//Depth /= 100.0f; // UE cm → m

	// Color → uint8 BGR
	cv::Mat Color;
	if (ColorRaw.depth() != CV_8U)
	{
		cv::Mat F32;
		ColorRaw.convertTo(F32, CV_32F);
		cv::threshold(F32, F32, 1.0f, 1.0f, cv::THRESH_TRUNC);
		cv::threshold(F32, F32, 0.0f, 0.0f, cv::THRESH_TOZERO);
		F32.convertTo(Color, CV_8U, 255.0);
	}
	else
	{
		Color = ColorRaw;
	}

	if (Color.channels() == 1)      cv::cvtColor(Color, Color, cv::COLOR_GRAY2BGR);
	else if (Color.channels() == 4) cv::cvtColor(Color, Color, cv::COLOR_BGRA2BGR);

	// Match resolution (color may differ from depth)
	if (Color.size() != Depth.size())
		cv::resize(Color, Color, Depth.size(), 0, 0, cv::INTER_LINEAR);

	if (!Depth.isContinuous()) Depth = Depth.clone();
	if (!Color.isContinuous()) Color = Color.clone();

	// --- Unproject equirectangular pixels and transform to world (map) frame ---
	//
	// Each pixel (u, v) in a WxH equirectangular image maps to a direction in
	// the camera_link frame (X-fwd, Y-left, Z-up / ROS robot convention):
	//   θ (azimuth)  = (u / W) * 2π - π        → -π … +π
	//   φ (elevation)= π/2 - (v / H) * π        → +π/2 … -π/2
	//
	//   cam.x =  d · cos(φ) · cos(θ)   (forward when θ=0)
	//   cam.y = -d · cos(φ) · sin(θ)   (left when θ<0)
	//   cam.z =  d · sin(φ)             (up when φ>0)
	//
	// Points are then rotated and translated into the map frame using the
	// camera pose (already in ROS world convention from ConvertPoseOpenCVToROS):
	//   p_world = Pose.Position + Pose.Orientation * p_cam
	//
	// This eliminates the need for TF lookup in Foxglove and guarantees
	// the point cloud is static in the map frame.
	//
	// Stride = 4 → sample every 4th pixel → ~1/16 of all pixels.
	// Max depth = 500 m → skip sky/background pixels.

	const int    H        = Depth.rows;
	const int    W_       = Depth.cols;
	const int    Stride   = 4;
	const float  MaxDepth = 500.0f;
	const double TwoPi    = 2.0 * 3.14159265358979323846;
	const double Pi       =       3.14159265358979323846;

	// Pre-allocate worst case (all pixels valid)
	TArray<uint8> PointData;
	PointData.Reserve(((H / Stride) + 1) * ((W_ / Stride) + 1) * 16);

	uint32 NumPoints = 0;

	for (int v = 0; v < H; v += Stride)
	{
		const float* DepthRow = Depth.ptr<float>(v);
		const uint8* ColorRow = Color.ptr<uint8>(v);

		// Zenith angle: 0 at top (up), π at bottom (down) — matches EquirectangularStitcher
		const double Theta    = (static_cast<double>(v) / H) * Pi;
		const double SinTheta = FMath::Sin(Theta);
		const double CosTheta = FMath::Cos(Theta);

		for (int u = 0; u < W_; u += Stride)
		{
			const float d = DepthRow[u];
			if (d <= 0.0f || !FMath::IsFinite(d) || d > MaxDepth)
				continue;

			// Azimuth: 0 at left edge (u=0), 2π at right edge — matches EquirectangularStitcher
			// Derived from CubeMap::DirectionToFaceUV coordinate system:
			//   Phi=π/2  (u=W/4)  → Front camera = camera_link +X (forward)
			//   Phi=0    (u=0)    → Left  camera = camera_link +Y (left)
			//   Phi=π    (u=W/2)  → Right camera = camera_link -Y (right)
			//   Phi=3π/2 (u=3W/4) → Back  camera = camera_link -X (backward)
			const double Phi    = (static_cast<double>(u) / W_) * TwoPi;
			const double SinPhi = FMath::Sin(Phi);
			const double CosPhi = FMath::Cos(Phi);

			// Point in camera_link frame (X-fwd, Y-left, Z-up)
			const FVector PCam(
				 d * SinTheta * SinPhi,   // X: forward
				 d * SinTheta * CosPhi,   // Y: left
				 d * CosTheta);           // Z: up

			const float wx = static_cast<float>(PCam.X);
			const float wy = static_cast<float>(PCam.Y);
			const float wz = static_cast<float>(PCam.Z);

			// OpenCV stores BGR; pack as 0x00RRGGBB for ROS rgb field
			const int    ColorIdx = u * 3;
			const uint32 B        = ColorRow[ColorIdx + 0];
			const uint32 G        = ColorRow[ColorIdx + 1];
			const uint32 R        = ColorRow[ColorIdx + 2];
			const uint32 Packed   = (R << 16) | (G << 8) | B;

			// Bit-cast packed uint32 to float (PCL convention for rgb field)
			float PackedFloat;
			FMemory::Memcpy(&PackedFloat, &Packed, sizeof(float));

			PointData.Append(reinterpret_cast<const uint8*>(&wx),          4);
			PointData.Append(reinterpret_cast<const uint8*>(&wy),          4);
			PointData.Append(reinterpret_cast<const uint8*>(&wz),          4);
			PointData.Append(reinterpret_cast<const uint8*>(&PackedFloat), 4);

			NumPoints++;
		}
	}

	UE_LOG(LogRosbagCreator, Log, TEXT("PointCloud2: %u valid points (stride=%d, src=%dx%d)"),
		NumPoints, Stride, W_, H);

	// --- CDR serialization -------------------------------------------------

	FCdrWriter W;

	// std_msgs/Header — frame_id = camera_link (points in sensor frame, TF applied by viewer)
	WriteMsgHeader(W, Pose.TimestampNs, Settings.CameraFrameId);

	// height=1 (unordered cloud), width=number of points
	W.WriteUInt32(1);
	W.WriteUInt32(NumPoints);

	// sensor_msgs/PointField[] fields — 4 entries
	W.WriteUInt32(4);

	auto WriteField = [&W](const char* Name, uint32 Offset, uint8 Datatype)
	{
		W.WriteString(Name);
		W.WriteUInt32(Offset);
		W.WriteUInt8(Datatype);
		W.WriteUInt32(1); // count
	};

	// FLOAT32 = 7; layout: x@0, y@4, z@8, rgb@12 (all float32 = 4 bytes each)
	WriteField("x",   0,  7);
	WriteField("y",   4,  7);
	WriteField("z",   8,  7);
	WriteField("rgb", 12, 7);

	W.WriteUInt8(0);              // is_bigendian = false
	W.WriteUInt32(16);            // point_step = 4 × float32
	W.WriteUInt32(NumPoints * 16); // row_step

	// uint8[] data
	W.WriteUInt8Sequence(PointData.GetData(), static_cast<int32>(PointData.Num()));

	W.WriteBool(false); // is_dense = false (invalid points were skipped)

	return W.Buffer;
#else
	UE_LOG(LogRosbagCreator, Warning, TEXT("OpenCV not available — point cloud skipped"));
	return {};
#endif
}

TArray<uint8> URosbagCreator::SerializeDepthImage(
	const FString&         FilePath,
	int64                  TimestampNs,
	const FRosbagSettings& Settings) const
{
#if WITH_OPENCV
	// FImageIO::Load() calls EnsureExrEnabled() which sets OPENCV_IO_ENABLE_OPENEXR=1
	cv::Mat Raw = FImageIO::Load(FilePath);
	if (Raw.empty())
	{
		UE_LOG(LogRosbagCreator, Warning, TEXT("Failed to load depth EXR: %s"), *FilePath);
		return {};
	}

	// Ensure float32 single-channel
	cv::Mat Float32;
	if (Raw.channels() > 1)
		cv::cvtColor(Raw, Float32, cv::COLOR_BGR2GRAY);
	else
		Float32 = Raw;

	if (Float32.type() != CV_32F)
		Float32.convertTo(Float32, CV_32F);

	// Depth EXR is already in meters (ground truth from the stitching pipeline)

	if (!Float32.isContinuous())
		Float32 = Float32.clone();

	const int32  H        = Float32.rows;
	const int32  W_       = Float32.cols;
	const uint32 Step     = static_cast<uint32>(Float32.step[0]);
	const uint32 DataSize = static_cast<uint32>(H) * Step;

	FCdrWriter W;
	WriteMsgHeader(W, TimestampNs, Settings.CameraFrameId);
	W.WriteUInt32(static_cast<uint32>(H));
	W.WriteUInt32(static_cast<uint32>(W_));
	W.WriteString("32FC1");
	W.WriteUInt8(0);           // is_bigendian = false
	W.WriteUInt32(Step);
	W.WriteUInt8Sequence(Float32.data, static_cast<int32>(DataSize));

	return W.Buffer;
#else
	UE_LOG(LogRosbagCreator, Warning, TEXT("OpenCV not available — depth image skipped: %s"), *FilePath);
	return {};
#endif
}

TArray<uint8> URosbagCreator::SerializeColorImage(
	const FString&         FilePath,
	int64                  TimestampNs,
	const FRosbagSettings& Settings) const
{
#if WITH_OPENCV
	// FImageIO::Load() calls EnsureExrEnabled() which sets OPENCV_IO_ENABLE_OPENEXR=1
	cv::Mat Raw = FImageIO::Load(FilePath);
	if (Raw.empty())
	{
		UE_LOG(LogRosbagCreator, Warning, TEXT("Failed to load color EXR: %s"), *FilePath);
		return {};
	}

	// Convert HDR float → uint8 BGR (clamp [0,1] → [0,255])
	cv::Mat Bgr8;
	if (Raw.depth() != CV_8U)
	{
		cv::Mat Float32;
		Raw.convertTo(Float32, CV_32F);
		cv::threshold(Float32, Float32, 1.0f, 1.0f, cv::THRESH_TRUNC);
		cv::threshold(Float32, Float32, 0.0f, 0.0f, cv::THRESH_TOZERO);
		Float32.convertTo(Bgr8, CV_8U, 255.0);
	}
	else
	{
		Bgr8 = Raw;
	}

	// Ensure 3-channel BGR
	if (Bgr8.channels() == 1)
		cv::cvtColor(Bgr8, Bgr8, cv::COLOR_GRAY2BGR);
	else if (Bgr8.channels() == 4)
		cv::cvtColor(Bgr8, Bgr8, cv::COLOR_BGRA2BGR);

	if (!Bgr8.isContinuous())
		Bgr8 = Bgr8.clone();

	const int32  H        = Bgr8.rows;
	const int32  W_       = Bgr8.cols;
	const uint32 Step     = static_cast<uint32>(Bgr8.step[0]);
	const uint32 DataSize = static_cast<uint32>(H) * Step;

	FCdrWriter W;
	WriteMsgHeader(W, TimestampNs, Settings.CameraFrameId);
	W.WriteUInt32(static_cast<uint32>(H));
	W.WriteUInt32(static_cast<uint32>(W_));
	W.WriteString("bgr8");
	W.WriteUInt8(0);           // is_bigendian = false
	W.WriteUInt32(Step);
	W.WriteUInt8Sequence(Bgr8.data, static_cast<int32>(DataSize));

	return W.Buffer;
#else
	UE_LOG(LogRosbagCreator, Warning, TEXT("OpenCV not available — color image skipped: %s"), *FilePath);
	return {};
#endif
}
