#pragma once
#include <opencv2/opencv.hpp>

namespace cv2 = cv;

namespace ma::node {


class ImageUtils {

#define JPEG_QUALITY 100

public:
    static bool saveImageToJpeg(const cv2::Mat& image, const std::string& filepath, int quality = JPEG_QUALITY, bool create_dir = true);

    static bool saveImageToBmp(const cv2::Mat& image, const std::string& filepath, float red_factor = 1.0f, float green_factor = 1.0f, float blue_factor = 1.0f, bool create_dir = true);

    static cv2::Mat whiteBalance(const cv2::Mat& image_rgb, float red_factor, float green_factor, float blue_factor);

    static cv2::Mat resizeImage(const cv2::Mat& input_image, int target_width, int target_height);

    static cv2::Mat denoiseImage(const cv2::Mat& input_image);

    // Nouvelle fonction pour crop une région définie par xmin, ymin, xmax, ymax
    static cv2::Mat cropImage(const cv2::Mat& input_image, int xmin, int ymin, int xmax, int ymax);
};
}  // namespace ma::node