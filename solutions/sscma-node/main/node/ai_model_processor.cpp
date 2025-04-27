#include "ai_model_processor.h"
#include "profiler.h"
#include <iostream>
#include <sscma.h>
#define TAG "AIModelProcessor"

namespace cv2 = cv;

namespace ma {

// Définition de la palette de couleurs pour l'affichage
const std::vector<cv2::Scalar> AIModelProcessor::ColorPalette::palette = {
    cv2::Scalar(0, 255, 0),     cv2::Scalar(0, 170, 255), cv2::Scalar(0, 128, 255), cv2::Scalar(0, 64, 255),  cv2::Scalar(0, 0, 255),     cv2::Scalar(170, 0, 255),   cv2::Scalar(128, 0, 255),
    cv2::Scalar(64, 0, 255),    cv2::Scalar(0, 0, 255),   cv2::Scalar(255, 0, 170), cv2::Scalar(255, 0, 128), cv2::Scalar(255, 0, 64),    cv2::Scalar(255, 128, 0),   cv2::Scalar(255, 255, 0),
    cv2::Scalar(128, 255, 0),   cv2::Scalar(0, 255, 128), cv2::Scalar(0, 255, 255), cv2::Scalar(0, 128, 128), cv2::Scalar(128, 0, 255),   cv2::Scalar(255, 0, 255),   cv2::Scalar(128, 128, 255),
    cv2::Scalar(255, 128, 128), cv2::Scalar(255, 64, 64), cv2::Scalar(64, 255, 64), cv2::Scalar(64, 64, 255), cv2::Scalar(128, 255, 255), cv2::Scalar(255, 255, 128),
};

std::vector<cv2::Scalar> AIModelProcessor::ColorPalette::getPalette() {
    return palette;
}

cv2::Scalar AIModelProcessor::ColorPalette::getColor(int index) {
    return palette[index % palette.size()];
}

AIModelProcessor::AIModelProcessor() : engine_(nullptr), model_(nullptr), detector_(nullptr), modelLoaded_(false), detectionThreshold_(0.5f) {}

AIModelProcessor::~AIModelProcessor() {
    if (model_ != nullptr) {
        ma::ModelFactory::remove(model_);
        model_    = nullptr;
        detector_ = nullptr;
    }

    if (engine_ != nullptr) {
        delete engine_;
        engine_ = nullptr;
    }
}

ma_err_t AIModelProcessor::initEngine() {
    Profiler p("AI: initEngine");
    if (engine_ != nullptr) {
        delete engine_;
    }

    engine_      = new ma::engine::EngineCVI();
    ma_err_t ret = engine_->init();
    if (ret != MA_OK) {
        MA_LOGE(TAG, "Engine initialization failed");
    }
    return ret;
}

ma_err_t AIModelProcessor::loadModel(const std::string& modelPath) {
    Profiler p("AI: loadModel");
    if (engine_ == nullptr) {
        MA_LOGE(TAG, "Engine not initialized, call initEngine() first");
        return MA_FAILED;
    }

    ma_err_t ret = engine_->load(modelPath.c_str());
    if (ret != MA_OK) {
        MA_LOGE(TAG, "Failed to load model: %s", modelPath.c_str());
        return ret;
    }

    model_ = ma::ModelFactory::create(engine_);
    if (model_ == nullptr) {
        MA_LOGE(TAG, "Model not supported");
        return MA_FAILED;
    }

    MA_LOGI(TAG, "Model loaded successfully, model type: %d", model_->getType());

    if (model_->getInputType() != MA_INPUT_TYPE_IMAGE) {
        MA_LOGE(TAG, "Model input type not supported, expected image input");
        return MA_FAILED;
    }

    if (model_->getOutputType() == MA_OUTPUT_TYPE_BBOX) {
        detector_ = static_cast<ma::model::Detector*>(model_);
        detector_->setConfig(MA_MODEL_CFG_OPT_THRESHOLD, detectionThreshold_);
        modelLoaded_ = true;
    } else {
        MA_LOGE(TAG, "Model output type not supported for detection, got type: %d", model_->getOutputType());
        return MA_FAILED;
    }

    return MA_OK;
}

bool AIModelProcessor::isModelLoaded() const {
    return modelLoaded_;
}

cv2::Mat AIModelProcessor::preprocessImage(cv2::Mat& image, bool forceResize) {
    Profiler p("AI: preprocessImage");
    int ih = image.rows;
    int iw = image.cols;
    int oh = 0;
    int ow = 0;

    // Vérifier si le modèle est chargé
    if (model_ == nullptr || !modelLoaded_) {
        MA_LOGE(TAG, "Model not loaded, cannot preprocess image");
        return image;
    }

    // Récupérer les dimensions d'entrée requises par le modèle
    if (model_->getInputType() == MA_INPUT_TYPE_IMAGE) {
        oh = reinterpret_cast<const ma_img_t*>(model_->getInput())->height;
        ow = reinterpret_cast<const ma_img_t*>(model_->getInput())->width;
    }

    // Si l'image est déjà dans la bonne taille et n'a pas besoin d'être redimensionnée
    if (!forceResize && ih == oh && iw == ow) {
        // Vérifier si l'image est déjà au format RGB
        cv2::Mat processedImage;
        if (image.channels() == 3) {
            cv2::Mat channels[3];
            cv2::split(image, channels);

            // Si le premier canal est B et le dernier canal est R, c'est du BGR (format OpenCV par défaut)
            if (cv2::countNonZero(channels[0] - channels[2]) > 0) {
                cv2::cvtColor(image, processedImage, cv2::COLOR_BGR2RGB);
                MA_LOGI(TAG, "Image converted from BGR to RGB format");
            } else {
                // Déjà en RGB
                processedImage = image.clone();
                MA_LOGI(TAG, "Image already in RGB format, no conversion needed");
            }
        } else {
            MA_LOGW(TAG, "Image has %d channels, expected 3 channels", image.channels());
            processedImage = image.clone();
        }
        return processedImage;
    }

    // Sinon, redimensionner l'image
    cv2::Mat resizedImage;
    double resize_scale = std::min((double)oh / ih, (double)ow / iw);
    int nh              = (int)(ih * resize_scale);
    int nw              = (int)(iw * resize_scale);
    cv2::resize(image, resizedImage, cv2::Size(nw, nh));

    // Ajouter des bordures pour atteindre la taille requise
    int top    = (oh - nh) / 2;
    int bottom = (oh - nh) - top;
    int left   = (ow - nw) / 2;
    int right  = (ow - nw) - left;

    cv2::Mat paddedImage;
    cv2::copyMakeBorder(resizedImage, paddedImage, top, bottom, left, right, cv2::BORDER_CONSTANT, cv2::Scalar::all(0));

    // Convertir si nécessaire de BGR à RGB
    if (image.channels() == 3) {
        cv2::Mat channels[3];
        cv2::split(paddedImage, channels);

        // Si le premier canal est B et le dernier canal est R, c'est du BGR
        if (cv2::countNonZero(channels[0] - channels[2]) > 0) {
            cv2::cvtColor(paddedImage, paddedImage, cv2::COLOR_BGR2RGB);
            MA_LOGI(TAG, "Image resized and converted from BGR to RGB format");
        } else {
            MA_LOGI(TAG, "Image resized and already in RGB format");
        }
    }

    MA_LOGI(TAG, "Image preprocessed: %dx%d -> %dx%d", iw, ih, paddedImage.cols, paddedImage.rows);
    return paddedImage;
}


ma_err_t AIModelProcessor::runDetection(cv2::Mat& image) {

    if (!modelLoaded_ || detector_ == nullptr) {
        MA_LOGE(TAG, "Model not loaded or not a detector model");
        return MA_FAILED;
    }

    // Prétraiter l'image
    /*cv2::Mat processedImage = preprocessImage(image);

    Profiler p("AI: runDetection");
    // Préparer l'image pour le modèle
    ma_img_t img;
    img.data   = (uint8_t*)processedImage.data;
    img.size   = processedImage.rows * processedImage.cols * processedImage.channels();
    img.width  = processedImage.cols;
    img.height = processedImage.rows;
    img.format = MA_PIXEL_FORMAT_RGB888;
    img.rotate = MA_PIXEL_ROTATE_0;*/
    Profiler p("AI: runDetection");
    // Préparer l'image pour le modèle
    ma_img_t img;
    img.data   = (uint8_t*)image.data;
    img.size   = image.rows * image.cols * image.channels();
    img.width  = image.cols;
    img.height = image.rows;
    img.format = MA_PIXEL_FORMAT_RGB888;
    img.rotate = MA_PIXEL_ROTATE_0;

    // Exécuter la détection
    detector_->run(&img);
    results_ = detector_->getResults();

    // Afficher les résultats dans la console
    // Compter le nombre d'éléments dans la forward_list
    size_t count = std::distance(results_.begin(), results_.end());
    MA_LOGI(TAG, "Detection results: %zu objects found", count);

    for (const auto& result : results_) {
        MA_LOGI(TAG, "  - Object: target=%d, score=%.3f, position=[%.2f, %.2f, %.2f, %.2f]", result.target, result.score, result.x, result.y, result.w, result.h);
    }

    // Obtenir et afficher les performances
    auto perf = model_->getPerf();
    MA_LOGI(TAG, "Performance: preprocess=%ldms, inference=%ldms, postprocess=%ldms", perf.preprocess, perf.inference, perf.postprocess);

    return MA_OK;
}

std::vector<ma_bbox_t> AIModelProcessor::getDetectionResults() const {
    // Convertir la forward_list en vector pour l'interface publique
    std::vector<ma_bbox_t> resultVector;
    for (const auto& bbox : results_) {
        resultVector.push_back(bbox);
    }
    return resultVector;
}

void AIModelProcessor::drawDetectionResults(cv2::Mat& image, bool convertBGR) {
    for (const auto& result : results_) {
        Profiler p("AI: drawDetectionResults");
        // Calculer les coordonnées du rectangle (x1,y1,x2,y2)
        float x1 = (result.x - result.w / 2.0) * image.cols;
        float y1 = (result.y - result.h / 2.0) * image.rows;
        float x2 = (result.x + result.w / 2.0) * image.cols;
        float y2 = (result.y + result.h / 2.0) * image.rows;

        // Préparer le texte avec la classe et le score
        char content[100];
        MA_LOGI(TAG, "Found object target: %d, score: %f, x: %f, y: %f, w: %f, h: %f", result.target, result.score, result.x, result.y, result.w, result.h);
        sprintf(content, "%d(%.3f)", result.target, result.score);

        // Dessiner le rectangle sur l'image
        cv2::rectangle(image, cv2::Point(x1, y1), cv2::Point(x2, y2), ColorPalette::getColor(result.target), 3, 8, 0);

        // Ajouter le texte au-dessus du rectangle
        cv2::putText(image, content, cv2::Point(x1, y1 - 10), cv2::FONT_HERSHEY_SIMPLEX, 0.6, ColorPalette::getColor(result.target), 2, cv2::LINE_AA);
    }

    // Convertir l'image de RGB à BGR si demandé
    if (convertBGR && image.channels() == 3) {
        cv2::cvtColor(image, image, cv2::COLOR_RGB2BGR);
    }
}

ma_perf_t AIModelProcessor::getPerformanceStats() const {
    if (model_ == nullptr) {
        throw std::runtime_error("AIModelProcessor: model_ is nullptr, engine not initialized or model not loaded");
    }
    return model_->getPerf();
}

void AIModelProcessor::setDetectionThreshold(float threshold) {
    detectionThreshold_ = threshold;
    if (detector_ != nullptr) {
        detector_->setConfig(MA_MODEL_CFG_OPT_THRESHOLD, threshold);
    }
}

ma_output_type_t AIModelProcessor::getModelOutputType() const {
    if (model_ == nullptr) {
        throw std::runtime_error("AIModelProcessor: model_ is nullptr, engine not initialized or model not loaded");
    }
    return model_->getOutputType();
}

}  // namespace ma