#include "led.h"

#include <fstream>
#include <stdexcept>
#include <unistd.h>

namespace ma::node {

static constexpr char LED_TAG[] = "ma::node::led";

Led::Led(const std::string& led_name)
    : path("/sys/class/leds/" + led_name + "/") {}

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
        return 255; // Valeur par défaut si non disponible
    }
    int max = 255;
    fs >> max;
    return max;
}

void Led::turnOn(int intensity) {
    write("trigger", "none"); // désactive tout trigger
    
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

} // namespace ma::node