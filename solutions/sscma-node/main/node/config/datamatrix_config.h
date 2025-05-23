#pragma once

#include <string>

namespace ma {

/**
 * @brief Structure contenant les paramètres de configuration du décodage de datamatrix
 */
struct DatamatrixConfig {
    bool enabled = false;  // Activer le décodage de datamatrix
    int roi_xmin = 0;      // Coordonnée X minimale de la région d'intérêt (ROI)
    int roi_ymin = 0;      // Coordonnée Y minimale de la région d'intérêt (ROI)
    int roi_xmax = 0;      // Coordonnée X maximale de la région d'intérêt (ROI)
    int roi_ymax = 0;      // Coordonnée Y maximale de la région d'intérêt (ROI)
};

/**
 * @brief Charge les paramètres de configuration du décodage de datamatrix depuis un fichier JSON
 * @param filename Chemin vers le fichier de configuration (par défaut: /userdata/flow.json)
 * @return Structure DatamatrixConfig contenant les paramètres chargés ou valeurs par défaut
 */
DatamatrixConfig readDatamatrixConfigFromFile(const std::string& filename = "/userdata/flow.json");

}  // namespace ma
