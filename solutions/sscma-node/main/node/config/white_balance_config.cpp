#include "white_balance_config.h"
#include "FlowConfigReader.h"
#include "logger.hpp"
#include <fstream>
#include <string>
#include <unistd.h>

namespace ma {

static constexpr char TAG[] = "ma::white_balance_config";

WhiteBalanceConfig readWhiteBalanceConfigFromFile(const std::string& filename) {
    WhiteBalanceConfig config;

    // Try multiple possible locations for flow.json
    std::vector<std::string> possiblePaths = {
        filename,              // Use provided path first
        "/userdata/flow.json"  // Default path in userdata
    };

    std::string usedPath = "";
    bool fileLoaded      = false;

    for (const auto& path : possiblePaths) {
        // MA_LOGI(TAG, "Reading White Balance Configuration: %s", path.c_str());
        try {
            FlowConfigReader reader(path);
            if (reader.reload()) {
                usedPath   = path;
                fileLoaded = true;

                // Lecture des paramètres au niveau racine comme pour crop_config
                config.enabled              = reader.getRootConfigBool("white_balance_config", "enabled", false);
                config.red_balance_factor   = reader.getRootConfigFloat("white_balance_config", "red_balance_factor", 1.0f);
                config.green_balance_factor = reader.getRootConfigFloat("white_balance_config", "green_balance_factor", 1.0f);
                config.blue_balance_factor  = reader.getRootConfigFloat("white_balance_config", "blue_balance_factor", 1.0f);

                /*MA_LOGI(
                    TAG, "White Balance parameters loaded from %s: red=%.3f, green=%.3f, blue=%.3f", path.c_str(), config.red_balance_factor, config.green_balance_factor,
                   config.blue_balance_factor);*/
                break;
            }
        } catch (const std::exception& e) {
            MA_LOGW(TAG, "Could not read config from %s: %s", path.c_str(), e.what());
        }
    }

    if (!fileLoaded) {
        MA_LOGE(TAG, "Failed to load White Balance configuration from any known location");
    }

    return config;
}

}  // namespace ma