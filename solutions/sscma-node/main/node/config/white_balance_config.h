#pragma once

#include <string>

namespace ma {

/**
 * @brief Structure contenant les paramètres de configuration de la balance des blancs
 */
struct WhiteBalanceConfig {
    bool enabled               = false;  // Indique si la balance des blancs est activée
    float red_balance_factor   = 1.0f;   // Facteur de balance pour le canal rouge
    float green_balance_factor = 1.0f;   // Facteur de balance pour le canal vert
    float blue_balance_factor  = 1.0f;   // Facteur de balance pour le canal bleu
};

/**
 * @brief Charge les paramètres de configuration de balance des blancs depuis un fichier JSON
 * @param filename Chemin vers le fichier de configuration (par défaut: /userdata/flow.json)
 * @return Structure WhiteBalanceConfig contenant les paramètres chargés ou valeurs par défaut
 */
WhiteBalanceConfig readWhiteBalanceConfigFromFile(const std::string& filename = "/userdata/flow.json");

}  // namespace ma