#include <fstream>
#include <opencv2/core/version.hpp>
#pragma message("OpenCV version: " CV_VERSION)
#pragma message("OpenCV include path: " CVAUX_STR(OPENCV_INCLUDE_DIRS))

#include <filesystem>
#include <opencv2/objdetect.hpp>
#include <opencv2/objdetect/barcode.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/photo.hpp>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>  // Include for gettimeofday

#include "image_utils.h"
#include "logger.hpp"
#include "profiler.h"

static constexpr char TAG[] = "ma::node::ImageUtils";

namespace ma::node {

bool ImageUtils::saveImageToJpeg(const ::cv::Mat& image, const std::string& filepath, int quality, bool create_dir) {
    Profiler p("saveImageToJpeg");
    umask(0002);
    std::vector<int> im_params = {::cv::IMWRITE_JPEG_QUALITY, quality};
    if (create_dir) {
        size_t last_slash = filepath.find_last_of('/');
        if (last_slash != std::string::npos) {
            std::string dir = filepath.substr(0, last_slash);
            struct stat st;
            if (stat(dir.c_str(), &st) != 0) {
                if (mkdir(dir.c_str(), 0777) != 0) {
                    MA_LOGE(TAG, "Failed to create directory: %s", dir.c_str());
                    return false;
                }
            }
        }
    }
    ::cv::Mat brg_image;
    ::cv::cvtColor(image, brg_image, ::cv::COLOR_RGB2BGR);
    MA_LOGI(TAG, "Created Mat from RGB888 frame and converted to BGR");
    return ::cv::imwrite(filepath, brg_image, im_params);
}

// Fonction utilitaire pour appliquer la balance des blancs sur le canal bleu
::cv::Mat ImageUtils::whiteBalance(const ::cv::Mat& image_rgb, float red_factor, float green_factor, float blue_factor) {
    Profiler p("whiteBalance");
    if (blue_factor == 1.0f && red_factor == 1.0f && green_factor == 1.0f) {
        // Aucun traitement, retourne l'image d'entrée directement
        return image_rgb;
    }
    ::cv::Mat balanced_image = image_rgb.clone();
    if (balanced_image.channels() == 3) {
        std::vector<::cv::Mat> channels;
        ::cv::split(balanced_image, channels);

        channels[0] = channels[0] * blue_factor;   // B
        channels[1] = channels[1] * green_factor;  // G
        channels[2] = channels[2] * red_factor;    // R

        ::cv::merge(channels, balanced_image);
    }
    MA_LOGI(TAG, "Applied white balance with blue factor: %.2f", blue_factor);
    return balanced_image;
}

// Nouvelle fonction utilitaire pour sauvegarder une image au format BMP
bool ImageUtils::saveImageToBmp(const ::cv::Mat& image, const std::string& filepath, float red_factor, float green_factor, float blue_factor, bool create_dir) {
    Profiler p("saveImageToBmp");
    umask(0002);
    if (create_dir) {
        size_t last_slash = filepath.find_last_of('/');
        if (last_slash != std::string::npos) {
            std::string dir = filepath.substr(0, last_slash);
            struct stat st;
            if (stat(dir.c_str(), &st) != 0) {
                if (mkdir(dir.c_str(), 0777) != 0) {
                    MA_LOGE(TAG, "Failed to create directory: %s", dir.c_str());
                    return false;
                }
            }
        }
    }
    // Appliquer la balance des blancs sur le canal bleu
    ::cv::Mat balanced_image = whiteBalance(image, red_factor, green_factor, blue_factor);
    ::cv::Mat brg_image;
    ::cv::cvtColor(balanced_image, brg_image, ::cv::COLOR_RGB2BGR);  // Convertir BGR à RGB pour BMP
    MA_LOGI(TAG, "Created Mat from RGB888 frame, applied white balance, and converted to BGR");
    return ::cv::imwrite(filepath, brg_image);
}

// Fonction pour redimensionner une image à la taille cible avec des bandes noires
::cv::Mat ImageUtils::resizeImage(const ::cv::Mat& input_image, int target_width, int target_height) {
    Profiler p("resizeImage");
    // Calculer le ratio pour le redimensionnement
    float scale_width  = static_cast<float>(target_width) / input_image.cols;
    float scale_height = static_cast<float>(target_height) / input_image.rows;
    float scale        = std::min(scale_width, scale_height);

    // Calculer les nouvelles dimensions
    int new_width  = static_cast<int>(input_image.cols * scale);
    int new_height = static_cast<int>(input_image.rows * scale);

    // Créer une image noire de taille cible
    ::cv::Mat output_image(target_height, target_width, CV_8UC3, ::cv::Scalar(0, 0, 0));  // Use global namespace ::cv::Scalar

    // Redimensionner l'image originale
    ::cv::Mat resized_image;
    ::cv::resize(input_image, resized_image, ::cv::Size(new_width, new_height));

    // Calculer les offsets pour centrer l'image redimensionnée
    int offset_x = (target_width - new_width) / 2;
    int offset_y = (target_height - new_height) / 2;

    // Créer une ROI (Region Of Interest) sur l'image de sortie
    ::cv::Mat roi = output_image(::cv::Rect(offset_x, offset_y, new_width, new_height));

    // Copier l'image redimensionnée dans la ROI
    resized_image.copyTo(roi);

    return output_image;
}

// Fonction pour appliquer un filtre de débruitage à une image
::cv::Mat ImageUtils::denoiseImage(const ::cv::Mat& input_image) {
    Profiler p("denoiseImage");
    ::cv::Mat denoised_image;
    try {
        MA_LOGI(TAG, "Application du filtre fastNlMeansDenoisingColored...");
        // Utilisation de fastNlMeansDenoisingColored pour un débruitage efficace
        ::cv::fastNlMeansDenoisingColored(input_image, denoised_image, 10, 10, 7, 21);
        MA_LOGI(TAG, "Filtre fastNlMeansDenoisingColored appliqué avec succès");
        return denoised_image;
    } catch (const std::exception& e) {
        MA_LOGW(TAG, "Erreur lors de l'application du filtre de débruitage: %s", e.what());
        // En cas d'erreur, retourner l'image d'origine
        return input_image.clone();
    }
}

// Nouvelle fonction utilitaire pour crop une région définie par xmin, ymin, xmax, ymax
::cv::Mat ImageUtils::cropImage(const ::cv::Mat& input_image, int xmin, int ymin, int xmax, int ymax) {
    Profiler p("cropImage");
    // Ensure coordinates within bounds
    int x1 = std::max(0, xmin);
    int y1 = std::max(0, ymin);
    int x2 = std::min(input_image.cols, xmax);
    int y2 = std::min(input_image.rows, ymax);
    if (x2 <= x1 || y2 <= y1) {
        MA_LOGW(TAG, "Invalid crop region [%d,%d] to [%d,%d]", x1, y1, x2, y2);
        return ::cv::Mat();
    }
    ::cv::Rect roi(x1, y1, x2 - x1, y2 - y1);
    return input_image(roi).clone();
}

// Fonction pour effectuer une rotation de 90° dans le sens inverse des aiguilles d'une montre
::cv::Mat ImageUtils::rotate90CCW(const ::cv::Mat& input_image) {
    Profiler p("rotate90CCW");
    ::cv::Mat rotated_image;
    ::cv::rotate(input_image, rotated_image, ::cv::ROTATE_90_COUNTERCLOCKWISE);
    return rotated_image;
}

// Détection et décodage de QR code datamatrix
std::string ImageUtils::decodeQRCode(const ::cv::Mat& image) {
    Profiler p("decodeQRCode");

    std::vector<std::string> decoded_info, decoded_type;
    std::vector<::cv::Point> corners;

    ::cv::Ptr<::cv::barcode::BarcodeDetector> bardet = ::cv::makePtr<cv::barcode::BarcodeDetector>();
    bardet->detectAndDecodeWithType(image, decoded_info, decoded_type, corners);

    for (size_t i = 0; i < decoded_info.size(); ++i) {
        if (!decoded_info[i].empty()) {
            MA_LOGI(TAG, "Code Detected: %s (type: %s)", decoded_info[i].c_str(), decoded_type[i].c_str());

            // Si on veut uniquement le premier DataMatrix
            if (decoded_type[i] == "DATAMATRIX") {
                return decoded_info[i];
            }
        }
    }

    // Aucun DataMatrix trouvé
    return std::string();
}

// Détection et décodage de codes-barres (tous types)
std::vector<std::string> ImageUtils::decodeBarcodes(const ::cv::Mat& image) {
    Profiler p("decodeBarcodes");
    std::vector<std::string> decoded_info, decoded_type;
    std::vector<::cv::Point> corners;
    ::cv::Ptr<::cv::barcode::BarcodeDetector> bardet = ::cv::makePtr<cv::barcode::BarcodeDetector>();
    bardet->detectAndDecodeWithType(image, decoded_info, decoded_type, corners);
    return decoded_info;
}

void ImageUtils::printOpenCVVersion() {
    MA_LOGI(TAG, "OpenCV version: %s", CV_VERSION);
}
}  // namespace ma::node