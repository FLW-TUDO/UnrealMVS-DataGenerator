#include "MoviePipeline/MoviePipelineCameraPoseOutput.h"
#include "MoviePipeline.h"
#include "MoviePipelinePrimaryConfig.h"
#include "MoviePipelineOutputSetting.h"
#include "MovieRenderPipelineDataTypes.h"
#include "Misc/FileHelper.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Misc/FrameRate.h"

DEFINE_LOG_CATEGORY_STATIC(LogCameraPoseOutput, Log, All);

UMoviePipelineCameraPoseOutput::UMoviePipelineCameraPoseOutput()
{
	ActorNamesToTrack.Add(TEXT("Equirectangular Camera"));
}

void UMoviePipelineCameraPoseOutput::SetupForPipelineImpl(UMoviePipeline* InPipeline)
{
	Super::SetupForPipelineImpl(InPipeline);
	WeakPipeline = InPipeline;
	CollectedPoses.Empty();
	RenderedFrames.Empty();
	RenderFrameRate = FFrameRate(30, 1); // fallback, overwritten in ExtractPosesFromSequence

	UE_LOG(LogCameraPoseOutput, Warning, TEXT("=== Camera Pose Output: initialized, tracking %d actor(s) ==="), ActorNamesToTrack.Num());
}

void UMoviePipelineCameraPoseOutput::OnReceiveImageDataImpl(FMoviePipelineMergerOutputFrame* InMergedOutputFrame)
{
	if (!InMergedOutputFrame)
	{
		return;
	}

	const int32 OutputFrame = InMergedOutputFrame->FrameOutputState.OutputFrameNumber;
	RenderedFrames.Add(OutputFrame);
}

void UMoviePipelineCameraPoseOutput::ExtractPosesFromSequence()
{
	UMoviePipeline* Pipeline = WeakPipeline.Get();
	if (!Pipeline)
	{
		return;
	}

	ULevelSequence* Sequence = Pipeline->GetTargetSequence();
	if (!Sequence)
	{
		UE_LOG(LogCameraPoseOutput, Warning, TEXT("No target sequence found"));
		return;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}

	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const FFrameRate DisplayRate    = MovieScene->GetDisplayRate();
	SequenceDisplayRate = DisplayRate;

	// Determine effective render frame rate (MRQ override takes priority)
	UMoviePipelineOutputSetting* OutputSettings = Pipeline->GetPipelinePrimaryConfig()->FindSetting<UMoviePipelineOutputSetting>();
	if (OutputSettings && OutputSettings->bUseCustomFrameRate)
	{
		RenderFrameRate = OutputSettings->OutputFrameRate;
		UE_LOG(LogCameraPoseOutput, Warning, TEXT("  Using MRQ custom frame rate: %d/%d"),
			RenderFrameRate.Numerator, RenderFrameRate.Denominator);
	}
	else
	{
		RenderFrameRate = DisplayRate;
		UE_LOG(LogCameraPoseOutput, Warning, TEXT("  Using sequence display rate: %d/%d"),
			RenderFrameRate.Numerator, RenderFrameRate.Denominator);
	}

	UE_LOG(LogCameraPoseOutput, Warning, TEXT("  TickResolution=%d/%d  SequenceDisplayRate=%d/%d"),
		TickResolution.Numerator, TickResolution.Denominator,
		DisplayRate.Numerator, DisplayRate.Denominator);

	TArray<int32> SortedOutputFrames = RenderedFrames.Array();
	SortedOutputFrames.Sort();

	const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();

	for (const FString& ActorName : ActorNamesToTrack)
	{
		// Find binding by actor label
		const FMovieSceneBinding* FoundBinding = nullptr;
		for (const FMovieSceneBinding& Binding : Bindings)
		{
			if (Binding.GetName() == ActorName)
			{
				FoundBinding = &Binding;
				break;
			}
		}

		if (!FoundBinding)
		{
			UE_LOG(LogCameraPoseOutput, Warning, TEXT("  '%s' not found in sequence bindings"), *ActorName);
			continue;
		}

		UMovieScene3DTransformTrack* TransformTrack = MovieScene->FindTrack<UMovieScene3DTransformTrack>(FoundBinding->GetObjectGuid());
		if (!TransformTrack || TransformTrack->GetAllSections().Num() == 0)
		{
			UE_LOG(LogCameraPoseOutput, Warning, TEXT("  No transform track for '%s'"), *ActorName);
			continue;
		}

		UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(TransformTrack->GetAllSections()[0]);
		if (!Section)
		{
			continue;
		}

		TArrayView<FMovieSceneDoubleChannel*> Ch = Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
		if (Ch.Num() < 6)
		{
			UE_LOG(LogCameraPoseOutput, Warning, TEXT("  Not enough channels (%d) for '%s'"), Ch.Num(), *ActorName);
			continue;
		}

		// Ch[0..2] = Translation XYZ, Ch[3..5] = Rotation Roll/Pitch/Yaw
		UE_LOG(LogCameraPoseOutput, Warning, TEXT("  Extracting %d frames for '%s'"), SortedOutputFrames.Num(), *ActorName);

		for (int32 OutputFrame : SortedOutputFrames)
		{
			// Convert output frame time directly to sequence ticks
			// OutputFrame N at RenderFrameRate = N / RenderFrameRate seconds
			// This is more accurate than using SourceFrameNumber which only gives the closest integer frame
			const double TimeInSeconds = static_cast<double>(OutputFrame) * RenderFrameRate.Denominator / RenderFrameRate.Numerator;
			const FFrameTime TickTime = TickResolution.AsFrameTime(TimeInSeconds);

			double PosX = 0.0, PosY = 0.0, PosZ = 0.0;
			double Roll = 0.0, Pitch = 0.0, Yaw = 0.0;

			Ch[0]->Evaluate(TickTime, PosX);
			Ch[1]->Evaluate(TickTime, PosY);
			Ch[2]->Evaluate(TickTime, PosZ);
			Ch[3]->Evaluate(TickTime, Roll);
			Ch[4]->Evaluate(TickTime, Pitch);
			Ch[5]->Evaluate(TickTime, Yaw);

			FVector Position(PosX, PosY, PosZ);
			FQuat Quat = FRotator(Pitch, Yaw, Roll).Quaternion();

			UE_LOG(LogCameraPoseOutput, Warning, TEXT("    Frame %d (t=%.4fs): (%.2f, %.2f, %.2f)"),
				OutputFrame, TimeInSeconds, PosX, PosY, PosZ);

			if (bConvertToOpenCVCoordinates)
			{
				ConvertToOpenCV(Position, Quat);
			}
			if (bConvertToMeters)
			{
				Position /= 100.0f;
			}

			const int64 TimestampNs = FMath::RoundToInt64(TimeInSeconds * 1000000000.0);
			CollectedPoses.Add(FCameraPoseEntry(TimestampNs, OutputFrame, ActorName, Position, Quat));
		}

		// Compute velocities via finite differences (central for middle frames, backward for last)
		// Poses for this actor are the last N entries in CollectedPoses
		const int32 PoseCount = SortedOutputFrames.Num();
		const int32 StartIdx  = CollectedPoses.Num() - PoseCount;

		for (int32 i = 0; i < PoseCount; i++)
		{
			FCameraPoseEntry& Curr = CollectedPoses[StartIdx + i];

			// Choose neighbors for finite differences
			const int32 PrevIdx = (i > 0)              ? StartIdx + i - 1 : -1;
			const int32 NextIdx = (i < PoseCount - 1)  ? StartIdx + i + 1 : -1;

			FVector dPos;
			double  dt = 0.0;

			if (PrevIdx >= 0 && NextIdx >= 0)
			{
				// Central difference (most accurate)
				const FCameraPoseEntry& Prev = CollectedPoses[PrevIdx];
				const FCameraPoseEntry& Next = CollectedPoses[NextIdx];
				dPos = Next.Position - Prev.Position;
				dt   = static_cast<double>(Next.TimestampNs - Prev.TimestampNs) * 1e-9;
			}
			else if (NextIdx >= 0)
			{
				// Forward difference (first frame)
				const FCameraPoseEntry& Next = CollectedPoses[NextIdx];
				dPos = Next.Position - Curr.Position;
				dt   = static_cast<double>(Next.TimestampNs - Curr.TimestampNs) * 1e-9;
			}
			else if (PrevIdx >= 0)
			{
				// Backward difference (last frame)
				const FCameraPoseEntry& Prev = CollectedPoses[PrevIdx];
				dPos = Curr.Position - Prev.Position;
				dt   = static_cast<double>(Curr.TimestampNs - Prev.TimestampNs) * 1e-9;
			}

			if (dt > 0.0)
			{
				// Linear velocity: world-frame diff rotated into body frame
				const FVector V_world = dPos / dt;
				Curr.LinearVelocity   = Curr.Orientation.Inverse().RotateVector(V_world);

				// Angular velocity in body frame from bounding quaternions
				const FQuat& Q1 = (PrevIdx >= 0) ? CollectedPoses[PrevIdx].Orientation : Curr.Orientation;
				const FQuat& Q2 = (NextIdx >= 0) ? CollectedPoses[NextIdx].Orientation  : Curr.Orientation;
				const FQuat  QRel = Q1.Inverse() * Q2;
				// Vector part [X,Y,Z] of quaternion = sin(theta/2) * axis
				// Use its magnitude for the angle, NOT GetRotationAxis().Size() which is always ~1.0
				const FVector VecPart(QRel.X, QRel.Y, QRel.Z);
				const double SinHalfAngle = VecPart.Size();
				if (SinHalfAngle > KINDA_SMALL_NUMBER)
				{
					const double Angle = 2.0 * FMath::Asin(FMath::Min(SinHalfAngle, 1.0));
					Curr.AngularVelocity = (VecPart / SinHalfAngle) * (Angle / dt);
				}
			}
		}
	}
}

void UMoviePipelineCameraPoseOutput::BeginFinalizeImpl()
{
	Super::BeginFinalizeImpl();

	UE_LOG(LogCameraPoseOutput, Warning, TEXT("=== Camera Pose Output: finalizing (%d frames rendered) ==="), RenderedFrames.Num());

	if (ActorNamesToTrack.Num() == 0 || RenderedFrames.Num() == 0)
	{
		UE_LOG(LogCameraPoseOutput, Warning, TEXT("Nothing to export"));
		return;
	}

	ExtractPosesFromSequence();

	if (CollectedPoses.Num() == 0)
	{
		UE_LOG(LogCameraPoseOutput, Warning, TEXT("No poses collected - check actor names match sequence bindings"));
		return;
	}

	// Resolve output directory
	FString OutputDirectory;
	UMoviePipeline* Pipeline = WeakPipeline.Get();
	if (Pipeline)
	{
		UMoviePipelineOutputSetting* OutputSettings = Pipeline->GetPipelinePrimaryConfig()->FindSetting<UMoviePipelineOutputSetting>();
		if (OutputSettings)
		{
			OutputDirectory = OutputSettings->OutputDirectory.Path;
			OutputDirectory = OutputDirectory.Replace(TEXT("{project_dir}"), *FPaths::ProjectDir());
			OutputDirectory = OutputDirectory.Replace(TEXT("{project_saved_dir}"), *FPaths::ProjectSavedDir());
			OutputDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
		}
	}

	if (OutputDirectory.IsEmpty())
	{
		OutputDirectory = FPaths::ProjectSavedDir() / TEXT("MovieRenders");
	}

	IFileManager::Get().MakeDirectory(*OutputDirectory, true);
	const FString FullPath = FPaths::Combine(OutputDirectory, OutputFilename + TEXT(".yaml"));

	WritePosesToYAML(FullPath);

	UE_LOG(LogCameraPoseOutput, Warning, TEXT("=== Exported %d poses to: %s ==="), CollectedPoses.Num(), *FullPath);
}

void UMoviePipelineCameraPoseOutput::WritePosesToYAML(const FString& OutputPath)
{
	TMap<FString, TArray<FCameraPoseEntry>> PosesByCamera;
	for (const FCameraPoseEntry& Pose : CollectedPoses)
	{
		PosesByCamera.FindOrAdd(Pose.CameraName).Add(Pose);
	}

	TArray<int32> AllOutputFrames = RenderedFrames.Array();
	AllOutputFrames.Sort();
	const int32 FirstFrame = AllOutputFrames.Num() > 0 ? AllOutputFrames[0]    : -1;
	const int32 LastFrame  = AllOutputFrames.Num() > 0 ? AllOutputFrames.Last() : -1;

	FString Out;
	Out += TEXT("# Camera trajectory - OmniRGBDCamera Plugin\n");
	Out += TEXT("\n");
	Out += TEXT("metadata:\n");
	Out += FString::Printf(TEXT("  render_frame_rate: \"%d/%d\"  # %s fps\n"),
		RenderFrameRate.Numerator,
		RenderFrameRate.Denominator,
		*FString::SanitizeFloat(RenderFrameRate.AsDecimal(), 4));
	Out += FString::Printf(TEXT("  sequence_frame_rate: \"%d/%d\"  # %s fps\n"),
		SequenceDisplayRate.Numerator,
		SequenceDisplayRate.Denominator,
		*FString::SanitizeFloat(SequenceDisplayRate.AsDecimal(), 4));
	Out += FString::Printf(TEXT("  frame_range: [%d, %d]\n"), FirstFrame, LastFrame);
	Out += FString::Printf(TEXT("  frame_count: %d\n"), AllOutputFrames.Num());
	Out += FString::Printf(TEXT("  coordinate_system: \"%s\"\n"),
		bConvertToOpenCVCoordinates ? TEXT("OpenCV/Basalt (X-right, Y-down, Z-forward)") : TEXT("Unreal Engine (X-forward, Y-right, Z-up)"));
	Out += FString::Printf(TEXT("  units: \"%s\"\n"),
		bConvertToMeters ? TEXT("meters") : TEXT("centimeters"));
	Out += FString::Printf(TEXT("  timestamp_unit: \"nanoseconds\"\n"));
	Out += TEXT("\ncameras:\n");

	for (auto& Pair : PosesByCamera)
	{
		FString SafeName = Pair.Key.Replace(TEXT("/"), TEXT("_")).Replace(TEXT(" "), TEXT("_"));
		Out += FString::Printf(TEXT("  %s:\n"), *SafeName);
		Out += FString::Printf(TEXT("    frame_count: %d\n"), Pair.Value.Num());

		// Covariance matrices once per camera (same for all frames)
		auto WriteCov = [&Out](const TCHAR* Label, float Diag)
		{
			Out += FString::Printf(TEXT("    %s:  # 6x6 diagonal, row-major\n"), Label);
			for (int Row = 0; Row < 6; Row++)
			{
				Out += TEXT("      - [");
				for (int Col = 0; Col < 6; Col++)
				{
					Out += FString::Printf(TEXT("%.2e"), (Row == Col) ? Diag : 0.0f);
					if (Col < 5) Out += TEXT(", ");
				}
				Out += TEXT("]\n");
			}
		};
		WriteCov(TEXT("pose_covariance"),  PoseCovarianceDiagonal);   // [x, y, z, roll, pitch, yaw]
		WriteCov(TEXT("twist_covariance"), TwistCovarianceDiagonal);  // [vx, vy, vz, wx, wy, wz]

		Out += TEXT("    poses:\n");
		for (const FCameraPoseEntry& Pose : Pair.Value)
		{
			Out += FString::Printf(TEXT("      - frame: %d\n"),       Pose.FrameNumber);
			Out += FString::Printf(TEXT("        timestamp: %lld\n"),  Pose.TimestampNs);
			Out += FString::Printf(TEXT("        position: [%.6f, %.6f, %.6f]\n"),
				Pose.Position.X, Pose.Position.Y, Pose.Position.Z);
			Out += FString::Printf(TEXT("        orientation: [%.6f, %.6f, %.6f, %.6f]  # [qx, qy, qz, qw]\n"),
				Pose.Orientation.X, Pose.Orientation.Y, Pose.Orientation.Z, Pose.Orientation.W);
			Out += FString::Printf(TEXT("        linear_velocity: [%.6f, %.6f, %.6f]  # m/s body frame\n"),
				Pose.LinearVelocity.X, Pose.LinearVelocity.Y, Pose.LinearVelocity.Z);
			Out += FString::Printf(TEXT("        angular_velocity: [%.6f, %.6f, %.6f]  # rad/s body frame\n"),
				Pose.AngularVelocity.X, Pose.AngularVelocity.Y, Pose.AngularVelocity.Z);
		}
	}

	FFileHelper::SaveStringToFile(Out, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void UMoviePipelineCameraPoseOutput::ConvertToOpenCV(FVector& Position, FQuat& Orientation) const
{
	const FVector P = Position;
	Position.X =  P.Y;
	Position.Y = -P.Z;
	Position.Z =  P.X;

	const FQuat Q = Orientation;
	Orientation.X =  Q.Y;
	Orientation.Y = -Q.Z;
	Orientation.Z =  Q.X;
	Orientation.W =  Q.W;
}
