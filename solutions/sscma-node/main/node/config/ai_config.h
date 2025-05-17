#pragma once

#include <string>

namespace ma {

struct AIConfig {
    bool enabled                      = false;
    bool enable_BGR_to_RGB_convertion = true;
    std::string model_path;
    std::string model_labels_path;
    float threshold                     = 0.5f;
    bool enable_bmp_inference_test_mode = false;
    std::string bmp_inference_test_folder;
};

AIConfig readAIConfigFromFile(const std::string& filename = "/userdata/flow.json");

}  // namespace ma
