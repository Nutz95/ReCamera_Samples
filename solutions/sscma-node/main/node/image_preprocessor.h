#pragma once

#include "camera.h"
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
};

}  // namespace ma::node