#include "Utils/Depth.h"

cv::Mat EnsureSingleChannelFloat(const cv::Mat& In)
{
	if (In.empty())
	{
		return cv::Mat();
	}

	if (In.channels() == 1)
	{
		if (In.depth() == CV_32F) return In;
		cv::Mat Out;
		In.convertTo(Out, CV_32F);
		return Out;
	}

	std::vector<cv::Mat> Ch;
	cv::split(In, Ch);

	cv::Mat Out = Ch[0];

	if (Out.depth() != CV_32F)
	{
		cv::Mat Out32;
		Out.convertTo(Out32, CV_32F);
		return Out32;
	}

	return Out;
}

cv::Mat ConvertDepthToInverseDepth(const cv::Mat& DepthImage)
{
	if (DepthImage.empty())
	{
		return cv::Mat();
	}

	cv::Mat InverseDepth = cv::Mat::zeros(DepthImage.size(), DepthImage.type());

	const float Eps = std::numeric_limits<float>::epsilon();

	// Valid pixels: strictly positive depth
	cv::Mat ValidMask = DepthImage > 0.0f;

	// Avoid division by zero
	cv::Mat SafeDepth;
	cv::max(Eps, DepthImage, SafeDepth);

	// Invert
	cv::divide(1.0f, SafeDepth, InverseDepth);

	// Mask invalid pixels
	InverseDepth.setTo(0.0f, ~ValidMask);

	return InverseDepth;
}