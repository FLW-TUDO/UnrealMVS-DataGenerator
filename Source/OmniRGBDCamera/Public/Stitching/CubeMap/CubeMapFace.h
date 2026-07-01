#pragma once

#include "CoreMinimal.h"

/**
 * @brief Enumeration representing the six faces of a cubemap.
 *
 * This enum provides a consistent face indexing scheme used throughout
 * the stitching pipeline. All cubemap operations should use this enum
 * to avoid indexing inconsistencies.
 *
 * The faces are ordered to match the standard cubemap convention:
 * - Front (0): +X direction
 * - Right (1): +Y direction
 * - Back (2): -X direction
 * - Left (3): -Y direction
 * - Top (4): +Z direction
 * - Bottom (5): -Z direction
 */
UENUM(BlueprintType)
enum class ECubeMapFace : uint8
{
	Front = 0   UMETA(DisplayName = "Front (+X)"),
	Right = 1   UMETA(DisplayName = "Right (+Y)"),
	Back = 2    UMETA(DisplayName = "Back (-X)"),
	Left = 3    UMETA(DisplayName = "Left (-Y)"),
	Top = 4     UMETA(DisplayName = "Top (+Z)"),
	Bottom = 5  UMETA(DisplayName = "Bottom (-Z)"),

	MAX = 6     UMETA(Hidden)
};

/**
 * @brief Layout type for cubemap atlas arrangement.
 */
UENUM(BlueprintType)
enum class ECubeMapLayout : uint8
{
	/** 4x3 cross layout with all 6 faces (used by Equirectangular) */
	Cross4x3 = 0  UMETA(DisplayName = "4x3 Cross (6 faces)"),

	/** 3x3 cross layout with 5 faces, no back face (used by Fisheye) */
	Cross3x3 = 1  UMETA(DisplayName = "3x3 Cross (5 faces)")
};

/**
 * @brief Returns the number of faces for a given layout.
 */
inline int32 GetFaceCount(ECubeMapLayout Layout)
{
	switch (Layout)
	{
	case ECubeMapLayout::Cross4x3: return 6;
	case ECubeMapLayout::Cross3x3: return 5;
	default: return 6;
	}
}

/**
 * @brief Checks if a face is present in the given layout.
 */
inline bool HasFace(ECubeMapLayout Layout, ECubeMapFace Face)
{
	if (Layout == ECubeMapLayout::Cross3x3 && Face == ECubeMapFace::Back)
	{
		return false;
	}
	return static_cast<uint8>(Face) < static_cast<uint8>(ECubeMapFace::MAX);
}
