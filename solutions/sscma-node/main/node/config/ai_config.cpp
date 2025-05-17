#include "ai_config.h"
#include "FlowConfigReader.h"
#include "logger.hpp"
#include <string>
#include <vector>

namespace ma {

static constexpr char TAG[] = "ma::ai_config";

AIConfig readAIConfigFromFile(const std::string& filename) {
    AIConfig config;
    std::vector<std::string> possiblePaths = {filename, "/userdata/flow.json"};

    for (const auto& path : possiblePaths) {
        // MA_LOGI(TAG, "Reading AI Configuration: %s", path.c_str());
        try {
            FlowConfigReader reader(path);
            if (reader.reload()) {
                config.enabled                        = reader.getRootConfigBool("ai_config", "enabled", false);
                config.model_path                     = reader.getRootConfigString("ai_config", "model_path", "");
                config.model_labels_path              = reader.getRootConfigString("ai_config", "model_labels_path", "");
                config.threshold                      = reader.getRootConfigFloat("ai_config", "threshold", 0.5f);
                config.enable_BGR_to_RGB_convertion   = reader.getRootConfigBool("ai_config", "enable_BGR_to_RGB_convertion", true);
                config.enable_bmp_inference_test_mode = reader.getRootConfigBool("ai_config", "enable_bmp_inference_test_mode", true);
                config.bmp_inference_test_folder      = reader.getRootConfigString("ai_config", "bmp_inference_test_folder", "");

                /*MA_LOGI(TAG, "AI config loaded: enabled=%s, model_path=%s, threshold=%.3f", config.enabled ? "true" : "false", config.model_path.c_str(), config.threshold);*/
                break;
            }
        } catch (const std::exception& e) {
            MA_LOGW(TAG, "Could not read AI config from %s: %s", path.c_str(), e.what());
        }
    }
    return config;
}

}  // namespace ma
