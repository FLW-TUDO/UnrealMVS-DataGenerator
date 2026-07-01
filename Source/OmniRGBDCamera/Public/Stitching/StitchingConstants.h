#pragma once

#include <cmath>
#include "Math/UnrealMathUtility.h"

/**
 * @file StitchingConstants.h
 * @brief Central location for all stitching-related default values and constants.
 */
namespace StitchingConstants
{
	// --------------------------------- Helper Functions --------------------------------- //

	/** Calculate focal length from image width and field of view */
	inline float FocalLengthFromFOV(float ImageWidth, float FOVDegrees)
	{
		return (ImageWidth / 2.0f) / std::tan(FMath::DegreesToRadians(FOVDegrees) / 2.0f);
	}
	// --------------------------------- Fisheye Defaults --------------------------------- //

	/** Default field of view for fisheye projection (degrees) */
	constexpr int32 DefaultFisheyeFOV = 120;

	/** Default Kannala-Brandt distortion coefficients [k1, k2, k3, k4] */
	constexpr double DefaultKB4Coefficients[4] = {0.03, 0.03, 0.002, 0.0};

	/** Default EUCM alpha parameter */
	constexpr float DefaultEUCMAlpha = 0.0f;

	/** Default EUCM beta parameter */
	constexpr float DefaultEUCMBeta = 0.0f;

	// --------------------------------- Equirectangular Defaults --------------------------------- //

	/** Default vertical field of view for equirectangular projection (degrees) */
	constexpr int32 DefaultVerticalFOV = 180;

	// --------------------------------- Camera Defaults --------------------------------- //

	/** Default focal length in pixels */
	constexpr float DefaultFocalLength = 350.0f;
}
