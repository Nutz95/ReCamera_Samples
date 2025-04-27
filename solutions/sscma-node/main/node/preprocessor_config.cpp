#include "preprocessor_config.h"
#include "FlowConfigReader.h"
#include "logger.hpp"
#include <fstream>
#include <string>
#include <unistd.h>

namespace ma {

static constexpr char TAG[] = "ma::preprocessor_config";

PreprocessorConfig readPreprocessorConfigFromFile(const std::string& nodeId, const std::string& filename) {
    PreprocessorConfig config;

    // Try multiple possible locations for flow.json
    std::vector<std::string> possiblePaths = {
        filename,              // Use provided path first
        "/userdata/flow.json"  // Default path in userdata
    };

    std::string usedPath = "";
    bool fileLoaded      = false;

    for (const auto& path : possiblePaths) {
        MA_LOGI(TAG, "Reading Preprocessor Configuration for node '%s': %s", nodeId.c_str(), path.c_str());
        try {
            FlowConfigReader reader(path);
            if (reader.reload()) {
                usedPath   = path;
                fileLoaded = true;

                // Utilisation des méthodes de FlowConfigReader pour accéder aux paramètres du nœud
                config.save_raw         = reader.getNodeConfigBool(nodeId, "save_raw", config.save_raw);
                config.enable_resize    = reader.getNodeConfigBool(nodeId, "enable_resize", config.enable_resize);
                config.enable_denoising = reader.getNodeConfigBool(nodeId, "enable_denoising", config.enable_denoising);
                config.width            = reader.getNodeConfigInt(nodeId, "width", config.width);
                config.height           = reader.getNodeConfigInt(nodeId, "height", config.height);
                config.debug            = reader.getNodeConfigBool(nodeId, "debug", config.debug);
                config.attach_channel   = reader.getNodeConfigString(nodeId, "attach_channel", config.attach_channel);

                MA_LOGI(TAG,
                        "Preprocessor configuration loaded for node '%s': save_raw=%s, enable_resize=%s, enable_denoising=%s",
                        nodeId.c_str(),
                        config.save_raw ? "true" : "false",
                        config.enable_resize ? "true" : "false",
                        config.enable_denoising ? "true" : "false");

                break;
            }
        } catch (const std::exception& e) {
            MA_LOGW(TAG, "Could not read config from %s: %s", path.c_str(), e.what());
        }
    }

    if (!fileLoaded) {
        MA_LOGE(TAG, "Failed to load Preprocessor configuration from any known location");
    }

    return config;
}

}  // namespace ma