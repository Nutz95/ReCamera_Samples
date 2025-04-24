#pragma once

#include "camera.h"
#include "led.h"  // Include led.h for static methods
#include "node.h"
#include "server.h"
#include <atomic>
#include <mutex>

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
    void requestCapture() {
        capture_requested_.store(true);
    }

    // Nouvelle méthode pour vérifier si une capture est en cours
    bool isCapturePending() const {
        return capture_requested_.load();
    }

    // Nouvelle méthode pour indiquer que le flash est actif (pour l'éteindre après capture)
    void setFlashActive(bool active, int intensity = -1) {
        flash_active_.store(active);
        flash_intensity_ = intensity;
    }

protected:
    void threadEntry();
    static void threadEntryStub(void* obj);

    // Méthodes privées pour découpage logique de threadEntry
    bool fetchAndValidateFrame(videoFrame*& frame);
    void processCaptureRequest(videoFrame* frame, ma_tick_t& last_debug);
    void handleNoCaptureRequested(videoFrame* frame);

protected:
    int32_t output_width_;   // Largeur cible (640 par défaut)
    int32_t output_height_;  // Hauteur cible (640 par défaut)
    std::atomic<bool> enabled_;
    Thread* thread_;
    CameraNode* camera_;
    MessageBox input_frame_;               // Pour recevoir les frames de la caméra
    MessageBox output_frame_;              // Pour envoyer les frames traitées
    bool debug_;                           // Mode debug pour afficher des informations supplémentaires
    int saved_image_count_;                // Compteur pour les images sauvegardées
    std::atomic<bool> capture_requested_;  // Nouveau flag pour indiquer si la capture est demandée par l'utilisateur

    // Nouvelles variables pour gérer l'état du flash
    std::atomic<bool> flash_active_;  // Indique si le flash est actif et doit être éteint
    int flash_intensity_;             // Intensité du flash pour le flash de confirmation

    // Nouvelles variables pour les paramètres de traitement d'image
    bool enable_resize_;                 // Activer/désactiver le redimensionnement
    bool enable_denoising_;              // Activer/désactiver le débruitage
    unsigned int flash_duration_ms_;     // Durée du flash
    unsigned int pre_capture_delay_ms_;  // Délai avant capture
    bool disable_red_led_blinking_;      // Désactiver le clignotement de la LED rouge

    // Nouveau membre pour stocker le canal à utiliser
    int channel_;
};

}  // namespace ma::node