#pragma once

#include "label_mapper.h"
#include <forward_list>  // Ajout de l'en-tête pour std::forward_list
#include <opencv2/opencv.hpp>
#include <sscma.h>
#include <string>
#include <vector>

namespace cv2 = cv;

namespace ma::node {


class AIModelProcessor {
public:
    AIModelProcessor(LabelMapper* labelMapper = nullptr);  // Ajout d'un paramètre pour le LabelMapper
    ~AIModelProcessor();

    // Méthodes d'initialisation
    ma_err_t initEngine();
    ma_err_t loadModel(const std::string& modelPath);
    bool isModelLoaded() const;

    // Prétraitement et exécution
    cv2::Mat preprocessImage(cv2::Mat& image, bool forceResize = false);
    ma_err_t runDetection(cv2::Mat& image);

    // Résultats et visualisation
    std::vector<ma_bbox_t> getDetectionResults() const;
    void drawDetectionResults(cv2::Mat& image, bool convertBGR = false);
    ma_perf_t getPerformanceStats() const;  // Utilisation du type ma_perf_t

    // Configuration
    void setDetectionThreshold(float threshold);

    // Type de sortie du modèle
    ma_output_type_t getModelOutputType() const;  // Utilisation du type ma_output_type_t

private:
    class ColorPalette {
    public:
        static std::vector<cv2::Scalar> getPalette();
        static cv2::Scalar getColor(int index);

    private:
        static const std::vector<cv2::Scalar> palette;
    };

    ma::engine::EngineCVI* engine_;
    ma::Model* model_;
    ma::model::Detector* detector_;
    std::forward_list<ma_bbox_t> results_;  // Modification de vector à forward_list
    bool modelLoaded_;
    float detectionThreshold_;

    LabelMapper* label_mapper_;  // Ajout d'un membre pour le LabelMapper
};

}  // namespace ma