#pragma once

#include <string>

namespace ma {

/**
 * @brief Structure contenant les paramètres de configuration du crop
 */
struct CropConfig {
    bool enabled = false;  // Activer le crop
    int xmin     = 0;      // Coordonnée X minimale
    int ymin     = 0;      // Coordonnée Y minimale
    int xmax     = 0;      // Coordonnée X maximale
    int ymax     = 0;      // Coordonnée Y maximale
};

/**
 * @brief Charge les paramètres de configuration du crop depuis un fichier JSON
 * @param filename Chemin vers le fichier de configuration (par défaut: /userdata/flow.json)
 * @return Structure CropConfig contenant les paramètres chargés ou valeurs par défaut
 */
CropConfig readCropConfigFromFile(const std::string& filename = "/userdata/flow.json");

}  // namespace ma
