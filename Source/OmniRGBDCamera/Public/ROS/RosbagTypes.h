#pragma once

#include "CoreMinimal.h"

/**
 * One evaluated camera pose with pre-computed finite-difference velocities.
 * Shared between MoviePipelineCameraPoseOutput and URosbagCreator.
 */
struct FCameraPoseEntry
{
	int64   TimestampNs;
	int32   FrameNumber;
	FString CameraName;
	FVector Position;           // meters (converted from UE cm)
	FQuat   Orientation;
	FVector LinearVelocity;     // m/s, body frame
	FVector AngularVelocity;    // rad/s, body frame

	FCameraPoseEntry()
		: TimestampNs(0), FrameNumber(0)
		, Position(FVector::ZeroVector), Orientation(FQuat::Identity)
		, LinearVelocity(FVector::ZeroVector), AngularVelocity(FVector::ZeroVector)
	{}

	FCameraPoseEntry(int64 InTimestamp, int32 InFrame, const FString& InName,
	                 const FVector& InPos, const FQuat& InQuat)
		: TimestampNs(InTimestamp), FrameNumber(InFrame), CameraName(InName)
		, Position(InPos), Orientation(InQuat)
		, LinearVelocity(FVector::ZeroVector), AngularVelocity(FVector::ZeroVector)
	{}
};
