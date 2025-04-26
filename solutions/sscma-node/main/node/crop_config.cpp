#include "crop_config.h"
#include "FlowConfigReader.h"
#include "logger.hpp"
#include <fstream>
#include <string>
#include <unistd.h>

namespace ma {

static constexpr char TAG[] = "ma::crop_config";

CropConfig readCropConfigFromFile(const std::string& filename) {
    CropConfig config;

    // Try multiple possible locations for flow.json
    std::vector<std::string> possiblePaths = {
        filename,              // Use provided path first
        "/userdata/flow.json"  // Default path in userdata
    };

    std::string usedPath = "";
    bool fileLoaded      = false;

    for (const auto& path : possiblePaths) {
        MA_LOGI(TAG, "Reading Crop Configuration: %s", path.c_str());
        try {
            FlowConfigReader reader(path);
            if (reader.reload()) {
                usedPath   = path;
                fileLoaded = true;

                // Utiliser les nouvelles méthodes getRootConfigXXX pour lire directement depuis la section crop_config
                config.enabled = reader.getRootConfigBool("crop_config", "enabled", false);
                config.xmin    = reader.getRootConfigInt("crop_config", "xmin", 0);
                config.ymin    = reader.getRootConfigInt("crop_config", "ymin", 0);
                config.xmax    = reader.getRootConfigInt("crop_config", "xmax", 0);
                config.ymax    = reader.getRootConfigInt("crop_config", "ymax", 0);

                MA_LOGI(TAG,
                        "Crop parameters loaded from %s: enabled=%s, xmin=%d, ymin=%d, xmax=%d, ymax=%d",
                        path.c_str(),
                        config.enabled ? "true" : "false",
                        config.xmin,
                        config.ymin,
                        config.xmax,
                        config.ymax);
                break;
            }
        } catch (const std::exception& e) {
            MA_LOGW(TAG, "Could not read config from %s: %s", path.c_str(), e.what());
        }
    }

    if (!fileLoaded) {
        MA_LOGE(TAG, "Failed to load configuration from any known location");
    }

    return config;
}

}  // namespace ma