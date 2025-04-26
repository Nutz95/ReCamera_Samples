#include <fstream>
#include <opencv2/opencv.hpp>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>  // Include for gettimeofday

#include "image_utils.h"
#include "logger.hpp"

static constexpr char TAG[] = "ma::node::ImageUtils";

namespace cv2 = cv;

namespace ma::node {

bool ImageUtils::saveImageToJpeg(const cv2::Mat& image, const std::string& filepath, int quality, bool create_dir) {
    std::vector<int> im_params = {cv2::IMWRITE_JPEG_QUALITY, quality};
    struct timeval tv_start, tv_end;
    gettimeofday(&tv_start, NULL);
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
    cv2::Mat brg_image;
    cv2::cvtColor(image, brg_image, cv2::COLOR_RGB2BGR);  // Convertir BGR à RGB pour BMP
    MA_LOGI(TAG, "Created Mat from RGB888 frame and converted to BGR");
    bool result = cv2::imwrite(filepath, brg_image, im_params);
    gettimeofday(&tv_end, NULL);
    double elapsed_ms = (tv_end.tv_sec - tv_start.tv_sec) * 1000.0 + (tv_end.tv_usec - tv_start.tv_usec) / 1000.0;
    MA_LOGI(TAG, "saveImageToJpeg execution time: %.2f ms", elapsed_ms);
    return result;
}

// Fonction utilitaire pour appliquer la balance des blancs sur le canal bleu
cv2::Mat ImageUtils::whiteBalance(const cv2::Mat& image_rgb, float red_factor, float green_factor, float blue_factor) {
    if (blue_factor == 1.0f && red_factor == 1.0f && green_factor == 1.0f) {
        // Aucun traitement, retourne l'image d'entrée directement
        return image_rgb;
    }
    cv2::Mat balanced_image = image_rgb.clone();
    if (balanced_image.channels() == 3) {
        for (int y = 0; y < balanced_image.rows; ++y) {
            for (int x = 0; x < balanced_image.cols; ++x) {
                cv2::Vec3b& pixel = balanced_image.at<cv2::Vec3b>(y, x);
                int red           = static_cast<int>(pixel[0] * red_factor);
                int green         = static_cast<int>(pixel[1] * green_factor);
                int blue          = static_cast<int>(pixel[2] * blue_factor);
                pixel[0]          = cv2::saturate_cast<uchar>(red);
                pixel[1]          = cv2::saturate_cast<uchar>(green);
                pixel[2]          = cv2::saturate_cast<uchar>(blue);
            }
        }
    }
    MA_LOGI(TAG, "Applied white balance with blue factor: %.2f", blue_factor);
    return balanced_image;
}

// Nouvelle fonction utilitaire pour sauvegarder une image au format BMP
bool ImageUtils::saveImageToBmp(const cv2::Mat& image, const std::string& filepath, float red_factor, float green_factor, float blue_factor, bool create_dir) {
    struct timeval tv_start, tv_end;
    gettimeofday(&tv_start, NULL);
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
    cv2::Mat balanced_image = whiteBalance(image, red_factor, green_factor, blue_factor);
    cv2::Mat brg_image;
    cv2::cvtColor(balanced_image, brg_image, cv2::COLOR_RGB2BGR);  // Convertir BGR à RGB pour BMP
    MA_LOGI(TAG, "Created Mat from RGB888 frame, applied white balance, and converted to BGR");
    bool result = cv2::imwrite(filepath, brg_image);
    gettimeofday(&tv_end, NULL);
    double elapsed_ms = (tv_end.tv_sec - tv_start.tv_sec) * 1000.0 + (tv_end.tv_usec - tv_start.tv_usec) / 1000.0;
    MA_LOGI(TAG, "saveImageToBmp execution time: %.2f ms", elapsed_ms);
    return result;
}

// Fonction pour redimensionner une image à la taille cible avec des bandes noires
cv2::Mat ImageUtils::resizeImage(const cv2::Mat& input_image, int target_width, int target_height) {
    // Calculer le ratio pour le redimensionnement
    float scale_width  = static_cast<float>(target_width) / input_image.cols;
    float scale_height = static_cast<float>(target_height) / input_image.rows;
    float scale        = std::min(scale_width, scale_height);

    // Calculer les nouvelles dimensions
    int new_width  = static_cast<int>(input_image.cols * scale);
    int new_height = static_cast<int>(input_image.rows * scale);

    // Créer une image noire de taille cible
    cv2::Mat output_image(target_height, target_width, CV_8UC3, ::cv::Scalar(0, 0, 0));  // Use global namespace ::cv::Scalar

    // Redimensionner l'image originale
    cv2::Mat resized_image;
    cv2::resize(input_image, resized_image, cv2::Size(new_width, new_height));

    // Calculer les offsets pour centrer l'image redimensionnée
    int offset_x = (target_width - new_width) / 2;
    int offset_y = (target_height - new_height) / 2;

    // Créer une ROI (Region Of Interest) sur l'image de sortie
    cv2::Mat roi = output_image(cv2::Rect(offset_x, offset_y, new_width, new_height));

    // Copier l'image redimensionnée dans la ROI
    resized_image.copyTo(roi);

    return output_image;
}

// Fonction pour appliquer un filtre de débruitage à une image
cv2::Mat ImageUtils::denoiseImage(const cv2::Mat& input_image) {
    cv2::Mat denoised_image;
    try {
        MA_LOGI(TAG, "Application du filtre de débruitage gaussien...");
        // Utilisation de GaussianBlur qui est disponible dans le module core d'OpenCV
        // Paramètres: image source, image destination, taille du noyau, écart-type X, écart-type Y
        cv2::GaussianBlur(input_image, denoised_image, cv2::Size(3, 3), 1.0, 1.0);
        MA_LOGI(TAG, "Filtre de débruitage gaussien appliqué avec succès");
        return denoised_image;
    } catch (const std::exception& e) {
        MA_LOGW(TAG, "Erreur lors de l'application du filtre de débruitage: %s", e.what());
        // En cas d'erreur, retourner l'image d'origine
        return input_image.clone();
    }
}
}  // namespace ma::node