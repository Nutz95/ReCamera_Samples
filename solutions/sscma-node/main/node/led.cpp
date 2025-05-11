#include "led.h"

#include <fstream>
#include <stdexcept>
#include <unistd.h>

namespace ma::node {

static constexpr char LED_TAG[] = "ma::node::led";

Led::Led(const std::string& led_name) : path("/sys/class/leds/" + led_name + "/") {}

void Led::write(const std::string& attr, const std::string& value) {
    std::ofstream fs(path + attr);
    if (!fs.is_open()) {
        MA_LOGW(LED_TAG, "Impossible d'ouvrir %s%s", path.c_str(), attr.c_str());
        return;
    }
    fs << value;
}

int Led::getMaxBrightness() {
    std::ifstream fs(path + "max_brightness");
    if (!fs.is_open()) {
        MA_LOGW(LED_TAG, "Impossible de lire max_brightness pour %s", path.c_str());
        return 255;  // Valeur par défaut si non disponible
    }
    int max = 255;
    fs >> max;
    return max;
}

void Led::turnOn(int intensity) {
    write("trigger", "none");  // désactive tout trigger

    // Si l'intensité n'est pas spécifiée, utiliser la valeur maximale
    if (intensity < 0) {
        intensity = getMaxBrightness();
    }

    write("brightness", std::to_string(intensity));
}

void Led::turnOff() {
    write("brightness", "0");
}

void Led::flash(int intensity, unsigned int delay_ms) {
    turnOn(intensity);
    usleep(delay_ms * 1000);
    turnOff();
}

// Implémentation des nouvelles méthodes statiques

bool Led::controlLed(const std::string& led_name, bool turn_on, int intensity) {
    try {
        Led led(led_name);
        if (turn_on) {
            led.turnOn(intensity);
            // MA_LOGI(LED_TAG, "LED %s allumée (intensité: %d)", led_name.c_str(), intensity);
        } else {
            led.turnOff();
            // MA_LOGI(LED_TAG, "LED %s éteinte", led_name.c_str());
        }
        return true;
    } catch (const std::exception& e) {
        MA_LOGE(LED_TAG, "Erreur lors du contrôle de la LED %s: %s", led_name.c_str(), e.what());
        return false;
    }
}

bool Led::controlAllLeds(bool turn_on, int intensity) {
    bool success = true;
    success &= controlLed("white", turn_on, intensity);
    success &= controlLed("red", turn_on, intensity);
    success &= controlLed("blue", turn_on, intensity);
    return success;
}

bool Led::flashLed(const std::string& led_name, int intensity, unsigned int duration_ms) {
    try {
        Led led(led_name);
        led.flash(intensity, duration_ms);
        /*MA_LOGI(LED_TAG, "LED %s flashée (intensité: %d, durée: %u ms)", led_name.c_str(), intensity, duration_ms);*/
        return true;
    } catch (const std::exception& e) {
        MA_LOGE(LED_TAG, "Erreur lors du flash de la LED %s: %s", led_name.c_str(), e.what());
        return false;
    }
}

}  // namespace ma::node