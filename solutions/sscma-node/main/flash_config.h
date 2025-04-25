#pragma once

#include "node/server.h"
#include <string>

namespace ma {

/**
 * @brief Structure contenant les paramètres de configuration du flash
 */
struct FlashConfig {
    bool enabled                = true;  // Activer le flash
    int pre_capture_delay_ms      = 300;   // Délai en ms entre l'allumage du flash et la capture
    int flash_duration_ms         = 100;   // Durée en ms du flash de confirmation (LED bleue)
    int flash_intensity           = 255;   // Intensité du flash (0-255, -1 = maximum)
    bool disable_red_led_blinking = true;  // Désactiver le clignotement de la LED rouge
};

/**
 * @brief Charge les paramètres de configuration du flash depuis un fichier JSON
 * @param filename Chemin vers le fichier de configuration (par défaut: /userdata/flow.json)
 * @return Structure FlashConfig contenant les paramètres chargés ou les valeurs par défaut
 */
FlashConfig readFlashConfigFromFile(const std::string& filename = "/userdata/flow.json");

/**
 * @brief Désactive le clignotement de la LED rouge
 */
void disableRedLedBlinking();

}  // namespace ma