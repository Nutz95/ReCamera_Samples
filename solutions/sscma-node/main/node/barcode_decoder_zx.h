#pragma once
#include <string>
#include <vector>
#include <opencv2/core.hpp>

// Classe pour décoder les codes-barres avec ZXing-cpp
class BarcodeDecoderZX {
public:
    struct Result {
        std::string text;
        std::string format;
        bool success;
    };

    // Décode une image OpenCV (BGR ou Grayscale)
    Result decode(const cv::Mat& input);
};
