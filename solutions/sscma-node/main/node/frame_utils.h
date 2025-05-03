#pragma once
#include "camera.h"  // for videoFrame definition
#include <opencv2/opencv.hpp>

namespace ma::node {


class FrameUtils {
public:
    static ::cv::Mat convertFrameToMat(videoFrame* frame);
    static bool prepareAndPublishOutputFrame(const ::cv::Mat& output_image, videoFrame* input_frame, MessageBox& output_frame, int output_width, int output_height);
};
}  // namespace ma::node