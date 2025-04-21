#pragma once

#include "camera.h"
#include "node.h"
#include "server.h"
#include <atomic>
#include <mutex>

namespace ma::node {

// Déclaration anticipée de la classe Led
class Led;

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

    // Méthodes de contrôle des LEDs
    bool controlLight(const std::string& led_name, bool turn_on, int intensity = -1);
    bool controlAllLights(bool turn_on, int intensity = -1);
    bool flashLight(const std::string& led_name, int intensity = -1, unsigned int duration_ms = 200);

    // Nouvelle méthode pour indiquer que le flash est actif (pour l'éteindre après capture)
    void setFlashActive(bool active, int intensity = -1) {
        flash_active_.store(active);
        flash_intensity_ = intensity;
    }

protected:
    void threadEntry();
    static void threadEntryStub(void* obj);

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

    // Variables pour les LEDs
    std::atomic<bool> lights_enabled_;   // État global des LEDs
    std::atomic<bool> white_led_state_;  // État de la LED blanche
    std::atomic<bool> red_led_state_;    // État de la LED rouge
    std::atomic<bool> blue_led_state_;   // État de la LED bleue

    // Nouvelles variables pour gérer l'état du flash
    std::atomic<bool> flash_active_;  // Indique si le flash est actif et doit être éteint
    int flash_intensity_;             // Intensité du flash pour le flash de confirmation
};

}  // namespace ma::node