#pragma once

#include <string>
#include <atomic>
#include "node.h"

namespace ma::node {

/**
 * @brief Classe pour contrôler les LEDs de la ReCamera
 * 
 * Cette classe permet d'allumer, éteindre et faire clignoter les LEDs
 * en accédant aux fichiers du système dans /sys/class/leds/
 */
class Led {
public:
    /**
     * @brief Constructeur
     * @param led_name Nom de la LED (ex: "white", "red", "blue")
     */
    Led(const std::string& led_name);
    
    /**
     * @brief Écrit une valeur dans un attribut de la LED
     * @param attr Nom de l'attribut ("brightness", "trigger", etc.)
     * @param value Valeur à écrire
     */
    void write(const std::string& attr, const std::string& value);
    
    /**
     * @brief Récupère la luminosité maximale de la LED
     * @return Valeur de luminosité maximale (généralement 255)
     */
    int getMaxBrightness();
    
    /**
     * @brief Allume la LED
     * @param intensity Intensité de la lumière (-1 pour utiliser la valeur maximale)
     */
    void turnOn(int intensity = -1);
    
    /**
     * @brief Éteint la LED
     */
    void turnOff();
    
    /**
     * @brief Fait clignoter la LED
     * @param intensity Intensité de la lumière (-1 pour utiliser la valeur maximale)
     * @param delay_ms Durée en millisecondes
     */
    void flash(int intensity, unsigned int delay_ms);

    /**
     * @brief Contrôle une LED spécifique (allumer ou éteindre)
     * @param led_name Nom de la LED (ex: "white", "red", "blue")
     * @param turn_on True pour allumer, False pour éteindre
     * @param intensity Intensité de la lumière (-1 pour utiliser la valeur maximale)
     * @return True si l'opération a réussi, False sinon
     */
    static bool controlLed(const std::string& led_name, bool turn_on, int intensity = -1);
    
    /**
     * @brief Contrôle toutes les LEDs (allumer ou éteindre)
     * @param turn_on True pour allumer, False pour éteindre
     * @param intensity Intensité de la lumière (-1 pour utiliser la valeur maximale)
     * @return True si toutes les opérations ont réussi, False sinon
     */
    static bool controlAllLeds(bool turn_on, int intensity = -1);
    
    /**
     * @brief Fait clignoter une LED spécifique
     * @param led_name Nom de la LED (ex: "white", "red", "blue")
     * @param intensity Intensité de la lumière (-1 pour utiliser la valeur maximale)
     * @param duration_ms Durée en millisecondes
     * @return True si l'opération a réussi, False sinon
     */
    static bool flashLed(const std::string& led_name, int intensity = -1, unsigned int duration_ms = 200);

private:
    std::string path; // Chemin vers les fichiers de contrôle de la LED
};

} // namespace ma::node