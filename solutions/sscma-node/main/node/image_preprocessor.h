#pragma once

#include "ai_model_processor.h"  // Ajout de l'en-tête pour AIModelProcessor
#include "camera.h"
#include "label_mapper.h"
#include "led.h"  // Include led.h for static methods
#include "node.h"
#include "server.h"
#include <atomic>
#include <mutex>
#include <opencv2/opencv.hpp>

namespace ma::node {

class ImagePreProcessorNode : public Node {

public:
    ImagePreProcessorNode(std::string id);
    ~ImagePreProcessorNode();

    ma_err_t onCreate(const json& config) override;
    ma_err_t onStart() override;
    ma_err_t onControl(const std::string& control, const json& data) override;
    ma_err_t onStop() override;
    ma_err_t onDestroy() override;

    // Nouvelle méthode pour demander une capture d'image
    void requestCapture(std::string tubeType) {
        tube_type_ = tubeType;
        capture_requested_.store(true);
        capture_in_progress_.store(true);
        capture_done_.store(false);
    }

    bool isCaptureRequested() const {
        return capture_requested_.load();
    }

    // Nouvelle méthode pour vérifier si une capture est en cours
    bool isCapturePending() const {
        return (capture_requested_.load() || capture_in_progress_.load()) && !capture_done_.load();
    }

    bool isCaptureDone() const {
        return capture_done_.load();
    }

    void setCaptureInProgress() {
        capture_requested_.store(false);
        capture_in_progress_.store(true);
        capture_done_.store(false);
    }

    void setCaptureDone() {
        capture_requested_.store(false);
        capture_done_.store(true);
        capture_in_progress_.store(false);
    }

    std::string getCurrentTubeType() const {
        return tube_type_;
    }

    // Décode un code-barres à partir d'une image de pleine résolution selon la config barcode_config
    std::vector<std::string> decodeBarcodeFromFullResImage(const ::cv::Mat& fullres_image, bool save_roi_bmp, const std::string& tube_type);

    // Décode un datamatrix à partir d'une image de pleine résolution selon la config datamatrix_config
    std::string decodeDatamatrixFromFullResImage(const ::cv::Mat& fullres_image, bool save_roi_bmp, const std::string& tube_type, bool enable_denoising);

    // Nouvelle méthode pour la détection IA
    void performAIDetection(::cv::Mat& output_image);

protected:
    void threadEntry();
    static void threadEntryStub(void* obj);

    // Méthodes privées pour découpage logique de threadEntry
    bool fetchAndValidateFrame(videoFrame*& frame);
    void processCaptureRequest(videoFrame* frame, ma_tick_t& last_debug, std::string tubeType);
    void handleNoCaptureRequested(videoFrame* frame);

protected:
    int32_t output_width_;   // Largeur cible (640 par défaut)
    int32_t output_height_;  // Hauteur cible (640 par défaut)
    std::atomic<bool> enabled_;
    Thread* thread_;
    CameraNode* camera_;
    MessageBox input_frame_;                 // Pour recevoir les frames de la caméra
    MessageBox output_frame_;                // Pour envoyer les frames traitées
    bool debug_;                             // Mode debug pour afficher des informations supplémentaires
    int saved_image_count_;                  // Compteur pour les images sauvegardées
    std::atomic<bool> capture_requested_;    // Nouveau flag pour indiquer si la capture est demandée par l'utilisateur
    std::atomic<bool> capture_in_progress_;  // Flag pour indiquer si le traitement est en cours
    std::atomic<bool> capture_done_;         // Flag pour indiquer si le traitement est terminé
    std::string tube_type_;

    // Nouvelles variables pour gérer l'état du flash
    std::atomic<bool> flash_active_;  // Indique si le flash est actif et doit être éteint
    int flash_intensity_;             // Intensité du flash pour le flash de confirmation

    // Nouvelles variables pour les paramètres de traitement d'image
    bool enable_resize_;  // Activer/désactiver le redimensionnement
    bool ai_enable_BGR_to_RGB;
    bool enable_denoising_;              // Activer/désactiver le débruitage
    unsigned int flash_duration_ms_;     // Durée du flash
    unsigned int pre_capture_delay_ms_;  // Délai avant capture
    bool disable_red_led_blinking_;      // Désactiver le clignotement de la LED rouge


    // Nouveau membre pour stocker le canal à utiliser
    int channel_;

    // Nouveau membre pour le traitement AI
    AIModelProcessor* ai_processor_;
    std::string ai_model_path_;
    std::string ai_model_labels_path_;
    bool enable_ai_detection_;
    float ai_detection_threshold_;  // Seuil de détection pour le modèle AI

    LabelMapper* label_mapper_ = nullptr;  // Mapper pour les labels du modèle AI
};

}  // namespace ma::node