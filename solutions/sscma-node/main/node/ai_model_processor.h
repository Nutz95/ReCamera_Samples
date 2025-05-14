#pragma once
#include "label_mapper.h"
#include <forward_list>
#include <opencv2/opencv.hpp>
#include <sscma.h>
#include <string>
#include <vector>

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
    ::cv::Mat preprocessImage(::cv::Mat& image, bool forceResize = false, bool convertRgbToBgr = false);
    ma_err_t runDetection(::cv::Mat& image, bool convertRgbToBgr);

    // Résultats et visualisation
    std::vector<ma_bbox_t> getDetectionResults() const;
    void drawDetectionResults(::cv::Mat& image, bool convertBGR = false);
    ma_perf_t getPerformanceStats() const;  // Utilisation du type ma_perf_t

    // Configuration
    void setDetectionThreshold(float threshold);

    // Type de sortie du modèle
    ma_output_type_t getModelOutputType() const;  // Utilisation du type ma_output_type_t

private:
    class ColorPalette {
    public:
        static std::vector<::cv::Scalar> getPalette();
        static ::cv::Scalar getColor(int index);

    private:
        static const std::vector<::cv::Scalar> palette;
    };

    ma::engine::EngineCVI* engine_;
    ma::Model* model_;
    ma::model::Detector* detector_;
    std::forward_list<ma_bbox_t> results_;  // Modification de vector à forward_list
    bool modelLoaded_;
    float detectionThreshold_;

    LabelMapper* label_mapper_;  // Ajout d'un membre pour le LabelMapper
};

}  // namespace ma::node