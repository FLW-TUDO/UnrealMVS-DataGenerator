#pragma once

#if WITH_OPENCV
#include "PreOpenCVHeaders.h"
#include "opencv2/core.hpp"
#include "opencv2/opencv.hpp"
#include "PostOpenCVHeaders.h"
#endif

/**
 * @brief Ensures that an image is single-channel and stored as 32-bit float.
 *
 * This function converts the input image into a single-channel `CV_32F` image.
 * It is primarily intended for depth-like images (e.g. depth or inverse depth),
 * where the depth information may be stored in one or multiple channels.
 *
 * Conversion rules:
 * - If the input is empty, an empty `cv::Mat` is returned.
 * - If the input has exactly one channel:
 *   - If its depth is already `CV_32F`, it is returned unchanged.
 *   - Otherwise, it is converted to `CV_32F`.
 * - If the input has multiple channels:
 *   - Only the first channel (channel 0) is used.
 *   - That channel is converted to `CV_32F` if necessary.
 *
 * @param In Input image (any number of channels, any depth).
 * @return Single-channel image of type `CV_32F`.
 *
 * @note For multi-channel images, all channels except the first one are discarded.
 * @note The returned image may share data with the input if no conversion is required.
 * @note This function performs no semantic validation of depth values.
 */
cv::Mat EnsureSingleChannelFloat(const cv::Mat& In);

/**
 * @brief Converts a depth image into an inverse depth image.
 *
 * All positive depth values are inverted (1 / depth).
 * Zero or negative values are treated as invalid and remain zero in the output.
 *
 * @param DepthImage Input depth image (single-channel float).
 * @return Inverse depth image with the same size and type as the input.
 *
 * @note The input image is not modified.
 * @note Depth values <= 0 are considered invalid.
 */
cv::Mat ConvertDepthToInverseDepth(const cv::Mat& DepthImage);