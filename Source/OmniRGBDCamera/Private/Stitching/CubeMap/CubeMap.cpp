#include "Stitching/CubeMap/CubeMap.h"

#if WITH_OPENCV
#include "PreOpenCVHeaders.h"
#include "opencv2/imgproc.hpp"
#include "PostOpenCVHeaders.h"
#endif

// ------------------------------------------ Constructors ------------------------------------------ //

CubeMap::CubeMap()
	: Layout(ECubeMapLayout::Cross4x3)
	, FaceWidth(0)
	, FaceHeight(0)
{
}

CubeMap::CubeMap(const cv::Mat& InAtlas, ECubeMapLayout InLayout)
	: Atlas(InAtlas)
	, Layout(InLayout)
{
	if (!Atlas.empty())
	{
		if (Layout == ECubeMapLayout::Cross4x3)
		{
			FaceWidth = Atlas.cols / 4;
			FaceHeight = Atlas.rows / 3;
		}
		else // Cross3x3
		{
			FaceWidth = Atlas.cols / 3;
			FaceHeight = Atlas.rows / 3;
		}
	}
}

CubeMap::~CubeMap()
{
}

// ------------------------------------------ Factory Methods ------------------------------------------ //

CubeMap CubeMap::FromImages6(const TArray<cv::Mat>& Images)
{
	std::vector<cv::Mat> StdImages;
	StdImages.reserve(Images.Num());
	for (const cv::Mat& Img : Images)
	{
		StdImages.push_back(Img);
	}
	return FromImages6(StdImages);
}

CubeMap CubeMap::FromImages6(const std::vector<cv::Mat>& Images)
{
	if (Images.size() != 6)
	{
		UE_LOG(LogTemp, Error, TEXT("[CubeMap] FromImages6 requires exactly 6 images, got %d"), static_cast<int>(Images.size()));
		return CubeMap();
	}

	// Validate all images have same size
	const int SingleWidth = Images[0].cols;
	const int SingleHeight = Images[0].rows;
	for (size_t i = 1; i < Images.size(); ++i)
	{
		if (Images[i].cols != SingleWidth || Images[i].rows != SingleHeight)
		{
			UE_LOG(LogTemp, Error, TEXT("[CubeMap] Image %d has different dimensions"), static_cast<int>(i));
			return CubeMap();
		}
	}

	// Create 4x3 atlas
	cv::Mat Atlas(SingleHeight * 3, SingleWidth * 4, Images[0].type(), cv::Scalar(0, 0, 0, 1));

	auto CopyToPosition = [&](const cv::Mat& Src, int Row, int Col) {
		cv::Mat Roi = Atlas(cv::Rect(Col * SingleWidth, Row * SingleHeight, SingleWidth, SingleHeight));
		Src.copyTo(Roi);
	};

	// Images order: Front(0), Right(1), Back(2), Left(3), Top(4), Bottom(5)
	// Grid positions match ECubeMapFace enum
	CopyToPosition(Images[0], 1, 1); // Front  -> (row=1, col=1)
	CopyToPosition(Images[1], 1, 2); // Right  -> (row=1, col=2)
	CopyToPosition(Images[2], 1, 3); // Back   -> (row=1, col=3)
	CopyToPosition(Images[3], 1, 0); // Left   -> (row=1, col=0)
	CopyToPosition(Images[4], 0, 1); // Top    -> (row=0, col=1)
	CopyToPosition(Images[5], 2, 1); // Bottom -> (row=2, col=1)

	return CubeMap(Atlas, ECubeMapLayout::Cross4x3);
}

CubeMap CubeMap::FromImages5(const TArray<cv::Mat>& Images)
{
	std::vector<cv::Mat> StdImages;
	StdImages.reserve(Images.Num());
	for (const cv::Mat& Img : Images)
	{
		StdImages.push_back(Img);
	}
	return FromImages5(StdImages);
}

CubeMap CubeMap::FromImages5(const std::vector<cv::Mat>& Images)
{
	if (Images.size() != 5)
	{
		UE_LOG(LogTemp, Error, TEXT("[FCubeMap] FromImages5 requires exactly 5 images, got %d"), static_cast<int>(Images.size()));
		return CubeMap();
	}

	// Validate all images have same size
	const int SingleWidth = Images[0].cols;
	const int SingleHeight = Images[0].rows;
	for (size_t i = 1; i < Images.size(); ++i)
	{
		if (Images[i].cols != SingleWidth || Images[i].rows != SingleHeight)
		{
			UE_LOG(LogTemp, Error, TEXT("[FCubeMap] Image %d has different dimensions"), static_cast<int>(i));
			return CubeMap();
		}
	}

	// Create 3x3 atlas
	cv::Mat Atlas(SingleHeight * 3, SingleWidth * 3, Images[0].type(), cv::Scalar(0, 0, 0, 1));

	auto CopyToPosition = [&](const cv::Mat& Src, int Row, int Col) {
		cv::Mat Roi = Atlas(cv::Rect(Col * SingleWidth, Row * SingleHeight, SingleWidth, SingleHeight));
		Src.copyTo(Roi);
	};

	// Images order: Front(0), Right(1), Left(2), Top(3), Bottom(4)
	// Note: No Back face in 5-camera setup
	CopyToPosition(Images[0], 1, 1); // Front  -> (row=1, col=1)
	CopyToPosition(Images[1], 1, 2); // Right  -> (row=1, col=2)
	CopyToPosition(Images[2], 1, 0); // Left   -> (row=1, col=0)
	CopyToPosition(Images[3], 0, 1); // Top    -> (row=0, col=1)
	CopyToPosition(Images[4], 2, 1); // Bottom -> (row=2, col=1)

	return CubeMap(Atlas, ECubeMapLayout::Cross3x3);
}

// ------------------------------------------ Face Access ------------------------------------------ //

cv::Mat CubeMap::GetFace(ECubeMapFace Face) const
{
	if (!IsValid())
	{
		return cv::Mat();
	}

	if (!HasFace(Layout, Face))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FCubeMap] Face %d not present in layout"), static_cast<int>(Face));
		return cv::Mat();
	}

	cv::Rect Rect = GetFaceRect(Face);
	return Atlas(Rect).clone();
}

cv::Rect CubeMap::GetFaceRect(ECubeMapFace Face) const
{
	FIntPoint GridPos = GetFaceGridPosition(Face, Layout);
	return cv::Rect(GridPos.X * FaceWidth, GridPos.Y * FaceHeight, FaceWidth, FaceHeight);
}

FIntPoint CubeMap::GetFaceGridPosition(ECubeMapFace Face, ECubeMapLayout Layout)
{
	// Returns (Column, Row) for the face in the atlas grid
	if (Layout == ECubeMapLayout::Cross4x3)
	{
		switch (Face)
		{
		case ECubeMapFace::Front:  return FIntPoint(1, 1);
		case ECubeMapFace::Right:  return FIntPoint(2, 1);
		case ECubeMapFace::Back:   return FIntPoint(3, 1);
		case ECubeMapFace::Left:   return FIntPoint(0, 1);
		case ECubeMapFace::Top:    return FIntPoint(1, 0);
		case ECubeMapFace::Bottom: return FIntPoint(1, 2);
		default: return FIntPoint(0, 0);
		}
	}
	else // Cross3x3
	{
		switch (Face)
		{
		case ECubeMapFace::Front:  return FIntPoint(1, 1);
		case ECubeMapFace::Right:  return FIntPoint(2, 1);
		case ECubeMapFace::Left:   return FIntPoint(0, 1);
		case ECubeMapFace::Top:    return FIntPoint(1, 0);
		case ECubeMapFace::Bottom: return FIntPoint(1, 2);
		case ECubeMapFace::Back:   return FIntPoint(0, 0); // Not present, return invalid
		default: return FIntPoint(0, 0);
		}
	}
}

// ------------------------------------------ Coordinate Conversion ------------------------------------------ //

void CubeMap::DirectionToFaceUV(double PX, double PY, double PZ,
								 ECubeMapFace& OutFace, double& OutU, double& OutV)
{
	int FaceIndex;

	if (fabs(PX) > fabs(PY) && fabs(PX) > fabs(PZ))
	{
		// X-axis dominates
		FaceIndex = PX > 0 ? 3 : 1; // Left : Right
		OutU = PY / PX;
		OutV = PZ / PX;
	}
	else if (fabs(PY) > fabs(PZ))
	{
		// Y-axis dominates
		FaceIndex = PY > 0 ? 0 : 2; // Front : Back
		OutU = PX / PY;
		OutV = PZ / PY;
	}
	else
	{
		// Z-axis dominates
		FaceIndex = PZ > 0 ? 4 : 5; // Top : Bottom
		OutU = PX / PZ;
		OutV = -PY / PZ;
	}

	// Normalize texture coordinates from [-1, 1] to [0, 1]
	OutU = (OutU + 1.0) * 0.5;
	OutV = (OutV + 1.0) * 0.5;

	OutFace = static_cast<ECubeMapFace>(FaceIndex);
}

ECubeMapFace CubeMap::DirectionToFace(double PX, double PY, double PZ)
{
	ECubeMapFace Face;
	double U, V;
	DirectionToFaceUV(PX, PY, PZ, Face, U, V);
	return Face;
}

void CubeMap::FaceUVToPixel(ECubeMapFace Face, double U, double V,
							 int32 FaceWidth, int32 FaceHeight,
							 int32& OutX, int32& OutY,
							 ECubeMapLayout Layout)
{
	FIntPoint GridPos = GetFaceGridPosition(Face, Layout);
	int32 Col = GridPos.X;
	int32 Row = GridPos.Y;

	// Apply UV transformations based on face orientation
	// These transformations ensure correct texture mapping for each face
	// Use (FaceWidth - 1) to map U=[0,1] to pixels [0, FaceWidth-1] correctly
	const int32 MaxPixelX = FaceWidth - 1;
	const int32 MaxPixelY = FaceHeight - 1;

	switch (Face)
	{
	case ECubeMapFace::Front:
		OutX = Col * FaceWidth + static_cast<int32>((1.0 - U) * MaxPixelX);
		OutY = Row * FaceHeight + static_cast<int32>((1.0 - V) * MaxPixelY);
		break;
	case ECubeMapFace::Right:
		OutX = Col * FaceWidth + static_cast<int32>(U * MaxPixelX);
		OutY = Row * FaceHeight + static_cast<int32>(V * MaxPixelY);
		break;
	case ECubeMapFace::Back:
		OutX = Col * FaceWidth + static_cast<int32>((1.0 - U) * MaxPixelX);
		OutY = Row * FaceHeight + static_cast<int32>(V * MaxPixelY);
		break;
	case ECubeMapFace::Left:
		OutX = Col * FaceWidth + static_cast<int32>(U * MaxPixelX);
		OutY = Row * FaceHeight + static_cast<int32>((1.0 - V) * MaxPixelY);
		break;
	case ECubeMapFace::Top:
		OutX = Col * FaceWidth + static_cast<int32>((1.0 - U) * MaxPixelX);
		OutY = Row * FaceHeight + static_cast<int32>((1.0 - V) * MaxPixelY);
		break;
	case ECubeMapFace::Bottom:
		OutX = Col * FaceWidth + static_cast<int32>(U * MaxPixelX);
		OutY = Row * FaceHeight + static_cast<int32>((1.0 - V) * MaxPixelY);
		break;
	default:
		OutX = 0;
		OutY = 0;
		break;
	}
}

void CubeMap::FaceUVToPixel(ECubeMapFace Face, double U, double V,
							 int32& OutX, int32& OutY) const
{
	FaceUVToPixel(Face, U, V, FaceWidth, FaceHeight, OutX, OutY, Layout);
}
