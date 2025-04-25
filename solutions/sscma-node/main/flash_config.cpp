#include "flash_config.h"
#include "node/led.h"
#include <fstream>

namespace ma {

static constexpr char TAG[] = "ma::flash_config";

FlashConfig readFlashConfigFromFile(const std::string& filename) {
    FlashConfig config;
    MA_LOGI(TAG, "Reading Flash Configuration: %s", filename.c_str());

    try {
        std::ifstream flowifs(filename.c_str());
        if (!flowifs.is_open()) {
            MA_LOGE(TAG, "Failed to open file: %s", filename.c_str());
            return config;  // Or throw an exception, depending on desired behavior
        }
        if (flowifs.good()) {
            try {
                nlohmann::json flow_json;
                flowifs >> flow_json;

                // Access the nested flash_config object
                if (flow_json.contains("flash_config") && flow_json["flash_config"].is_object()) {
                    const auto& flash_config = flow_json["flash_config"];

                    if (flash_config.contains("flash_enabled") && flash_config["flash_enabled"].is_boolean()) {
                        config.enabled = flash_config["flash_enabled"];
                    }

                    if (flash_config.contains("pre_capture_delay_ms") && flash_config["pre_capture_delay_ms"].is_number()) {
                        config.pre_capture_delay_ms = flash_config["pre_capture_delay_ms"];
                    }

                    if (flash_config.contains("flash_duration_ms") && flash_config["flash_duration_ms"].is_number()) {
                        config.flash_duration_ms = flash_config["flash_duration_ms"];
                    }

                    if (flash_config.contains("flash_intensity") && flash_config["flash_intensity"].is_number()) {
                        config.flash_intensity = flash_config["flash_intensity"];
                    }

                    if (flash_config.contains("disable_red_led_blinking") && flash_config["disable_red_led_blinking"].is_boolean()) {
                        config.disable_red_led_blinking = flash_config["disable_red_led_blinking"];
                    }

                    MA_LOGI(TAG,
                            "Paramètres du flash chargés: enabled:%s, pre=%d ms, duration=%d ms, intensity=%d, disable_red=%s",
                            config.enabled ? "true" : "false",
                            config.pre_capture_delay_ms,
                            config.flash_duration_ms,
                            config.flash_intensity,
                            config.disable_red_led_blinking ? "true" : "false");
                } else {
                    MA_LOGE(TAG, "flash_config object not found or is not an object");
                }
            } catch (const std::exception& e) {
                MA_LOGI(TAG, "Erreur lors de la lecture du fichier de configuration: %s", e.what());
            }
        } else {
            MA_LOGI(TAG, "Configuration file %s not found, using default settings", filename.c_str());
        }
    } catch (const std::exception& e) {
        MA_LOGE(TAG, "Error reading Flash configuration: %s", e.what());
    }

    return config;
}

void disableRedLedBlinking() {
    try {
        node::Led redLed("red");

        // Désactiver tous les triggers (notamment le timer qui fait clignoter la LED)
        redLed.write("trigger", "none");

        // Éteindre complètement la LED
        redLed.write("brightness", "0");

        MA_LOGI(TAG, "Clignotement de la LED rouge désactivé");
    } catch (const std::exception& e) {
        MA_LOGE(TAG, "Erreur lors de la désactivation du clignotement de la LED rouge: %s", e.what());
    }
}

}  // namespace ma