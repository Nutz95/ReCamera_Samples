#pragma once
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace ma::node {

class ImageUtils {
#define JPEG_QUALITY 100
public:
    static bool saveImageToJpeg(const ::cv::Mat& image, const std::string& filepath, int quality = JPEG_QUALITY, bool create_dir = true);
    static bool saveImageToBmp(const ::cv::Mat& image, const std::string& filepath, float red_factor = 1.0f, float green_factor = 1.0f, float blue_factor = 1.0f, bool create_dir = true);
    static ::cv::Mat whiteBalance(const ::cv::Mat& image_rgb, float red_factor, float green_factor, float blue_factor);
    static ::cv::Mat resizeImage(const ::cv::Mat& input_image, int target_width, int target_height);
    static ::cv::Mat denoiseImage(const ::cv::Mat& input_image);
    static ::cv::Mat cropImage(const ::cv::Mat& input_image, int xmin, int ymin, int xmax, int ymax);
    static ::cv::Mat rotate90CCW(const ::cv::Mat& input_image);
    static std::string decodeQRCode(const ::cv::Mat& image);
    static std::vector<std::string> decodeBarcodes(const ::cv::Mat& image);
    static void printOpenCVVersion();
};

}  // namespace ma::node