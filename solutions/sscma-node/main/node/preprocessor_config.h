#pragma once

#include <string>

namespace ma {

/**
 * @brief Structure contenant les paramètres de configuration du préprocesseur d'image
 */
struct PreprocessorConfig {
    bool save_raw              = false;   // Indicateur pour sauvegarder l'image brute
    bool enable_resize         = true;    // Indicateur pour activer le redimensionnement
    bool enable_denoising      = false;   // Indicateur pour activer le débruitage
    bool enable_ccw_rotation   = false;   // Indicateur pour activer le débruitage
    int width                  = 640;     // Largeur de sortie par défaut
    int height                 = 640;     // Hauteur de sortie par défaut
    bool debug                 = false;   // Mode debug
    std::string attach_channel = "jpeg";  // Canal à utiliser
};

/**
 * @brief Charge les paramètres de configuration du préprocesseur depuis un fichier JSON
 * @param nodeId Identifiant du noeud préprocesseur (par exemple "preprocessor0")
 * @param filename Chemin vers le fichier de configuration (par défaut: /userdata/flow.json)
 * @return Structure PreprocessorConfig contenant les paramètres chargés ou valeurs par défaut
 */
PreprocessorConfig readPreprocessorConfigFromFile(const std::string& nodeId, const std::string& filename = "/userdata/flow.json");

}  // namespace ma