#pragma once

#include <string>

namespace ma {

struct AIConfig {
    bool enabled = false;
    std::string model_path;
    std::string model_labels_path;
    float threshold = 0.5f;
};

AIConfig readAIConfigFromFile(const std::string& filename = "/userdata/flow.json");

}  // namespace ma
