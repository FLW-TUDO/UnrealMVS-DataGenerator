#pragma once

#include "CoreMinimal.h"
#include "Stitching/CubeMap/CubeMapFace.h"

#if WITH_OPENCV
#include "PreOpenCVHeaders.h"
#include "opencv2/core.hpp"
#include "PostOpenCVHeaders.h"
#endif

/**
 * @class CubeMap
 * @brief A wrapper class for cubemap operations that encapsulates a cv::Mat atlas.
 *
 * This class provides a unified interface for creating and manipulating cubemaps.
 * It supports both 4x3 (6-face) and 3x3 (5-face) atlas layouts and provides
 * consistent face indexing throughout the stitching pipeline.
 *
 * Atlas Layout (4x3 Cross):
 *        [   ][Top][   ][   ]
 *        [Left][Front][Right][Back]
 *        [   ][Bottom][   ][   ]
 *
 * Atlas Layout (3x3 Cross, no Back):
 *        [   ][Top][   ]
 *        [Left][Front][Right]
 *        [   ][Bottom][   ]
 */
class OMNIRGBDCAMERA_API CubeMap
{
public:
	// ------------------------------------------ Constructors ------------------------------------------ //

	/** Default constructor - creates an empty cubemap */
	CubeMap();

	/**
	 * Constructs a cubemap from an existing atlas cv::Mat.
	 * @param InAtlas The atlas image (must be 4x3 or 3x3 face ratio)
	 * @param InLayout The layout type of the atlas
	 */
	CubeMap(const cv::Mat& InAtlas, ECubeMapLayout InLayout);

	/** Destructor */
	~CubeMap();

	// ------------------------------------------ Factory Methods ------------------------------------------ //

	/**
	 * Creates a 4x3 cubemap from 6 individual face images.
	 * @param Images Array of 6 images in order: Front, Right, Back, Left, Top, Bottom
	 * @return A new FCubeMap instance
	 */
	static CubeMap FromImages6(const TArray<cv::Mat>& Images);
	static CubeMap FromImages6(const std::vector<cv::Mat>& Images);

	/**
	 * Creates a 3x3 cubemap from 5 individual face images (no Back face).
	 * @param Images Array of 5 images in order: Front, Right, Left, Top, Bottom
	 * @return A new FCubeMap instance
	 */
	static CubeMap FromImages5(const TArray<cv::Mat>& Images);
	static CubeMap FromImages5(const std::vector<cv::Mat>& Images);

	// ------------------------------------------ Accessors ------------------------------------------ //

	/** Returns the underlying atlas cv::Mat */
	const cv::Mat& GetAtlas() const { return Atlas; }

	/** Returns a mutable reference to the atlas */
	cv::Mat& GetAtlasMutable() { return Atlas; }

	/** Returns the layout type */
	ECubeMapLayout GetLayout() const { return Layout; }

	/** Returns the width of a single face in pixels */
	int32 GetFaceWidth() const { return FaceWidth; }

	/** Returns the height of a single face in pixels */
	int32 GetFaceHeight() const { return FaceHeight; }

	/** Returns true if the cubemap has valid data */
	bool IsValid() const { return !Atlas.empty(); }

	/** Returns the number of faces in this cubemap */
	int32 GetFaceCount() const { return ::GetFaceCount(Layout); }

	// ------------------------------------------ Face Access ------------------------------------------ //

	/**
	 * Extracts a single face from the atlas.
	 * @param Face The face to extract
	 * @return The face image as cv::Mat (copy)
	 */
	cv::Mat GetFace(ECubeMapFace Face) const;

	/**
	 * Returns a cv::Rect for the specified face within the atlas.
	 * @param Face The face to get bounds for
	 * @return Rectangle defining the face position in the atlas
	 */
	cv::Rect GetFaceRect(ECubeMapFace Face) const;

	// ------------------------------------------ Coordinate Conversion ------------------------------------------ //

	/**
	 * Converts a 3D direction to face index and UV coordinates.
	 *
	 * @param PX X component of direction
	 * @param PY Y component of direction
	 * @param PZ Z component of direction
	 * @param OutFace The dominant face for this direction
	 * @param OutU Horizontal texture coordinate [0,1]
	 * @param OutV Vertical texture coordinate [0,1]
	 */
	static void DirectionToFaceUV(double PX, double PY, double PZ,
								  ECubeMapFace& OutFace, double& OutU, double& OutV);

	/**
	 * Converts face UV coordinates to pixel coordinates in the atlas.
	 *
	 * @param Face The cubemap face
	 * @param U Horizontal texture coordinate [0,1]
	 * @param V Vertical texture coordinate [0,1]
	 * @param FaceWidth Width of a single face in pixels
	 * @param FaceHeight Height of a single face in pixels
	 * @param OutX Output X pixel coordinate in atlas
	 * @param OutY Output Y pixel coordinate in atlas
	 * @param Layout The atlas layout (4x3 or 3x3)
	 */
	static void FaceUVToPixel(ECubeMapFace Face, double U, double V,
							  int32 FaceWidth, int32 FaceHeight,
							  int32& OutX, int32& OutY,
							  ECubeMapLayout Layout = ECubeMapLayout::Cross4x3);

	/**
	 * Converts face UV to pixel coordinates using this cubemap's dimensions.
	 */
	void FaceUVToPixel(ECubeMapFace Face, double U, double V,
					   int32& OutX, int32& OutY) const;

	/**
	 * Determines which face a 3D direction maps to.
	 * @param PX X component of direction
	 * @param PY Y component of direction
	 * @param PZ Z component of direction
	 * @return The dominant cubemap face
	 */
	static ECubeMapFace DirectionToFace(double PX, double PY, double PZ);

private:
	/** The atlas image containing all faces */
	cv::Mat Atlas;

	/** The layout type of this cubemap */
	ECubeMapLayout Layout = ECubeMapLayout::Cross4x3;

	/** Width of a single face in pixels */
	int32 FaceWidth = 0;

	/** Height of a single face in pixels */
	int32 FaceHeight = 0;

	/**
	 * Returns the grid position (row, col) for a face in the atlas.
	 * @param Face The cubemap face
	 * @param Layout The atlas layout
	 * @return FIntPoint where X=column, Y=row
	 */
	static FIntPoint GetFaceGridPosition(ECubeMapFace Face, ECubeMapLayout Layout);
};
