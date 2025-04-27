#include "label_mapper.h"
#include "logger.hpp"
#include <sstream>

namespace ma::node {

static constexpr char TAG[] = "ma::LabelMapper";

LabelMapper::LabelMapper(const std::string& jsonPath) {
    loadLabels(jsonPath);
}

void LabelMapper::loadLabels(const std::string& jsonPath) {
    MA_LOGI(TAG, "LabelMapper Loading file:%s ...", jsonPath.c_str());
    std::vector<std::string> paths = {jsonPath, "./labels.json"};
    auto tryLoad                   = [this](const std::string& path) -> bool {
        FlowConfigReader reader(path);
        MA_LOGI(TAG, "Loaded file:%s", path.c_str());
        if (!reader.reload())
            return false;
        auto& config = reader.getConfig();
        for (int i = 0; i < 1000; ++i) {
            std::string key = std::to_string(i);
            if (config.contains(key)) {
                labelMap_[i] = config[key].getString();
            }
        }
        // Log all loaded classes (key/value)
        for (const auto& kv : labelMap_) {
            MA_LOGI(TAG, "Loaded labels: key=%d, value=%s", kv.first, kv.second.c_str());
        }
        return true;
    };
    for (const auto& path : paths) {
        if (tryLoad(path))
            return;
    }
}

std::string LabelMapper::getLabel(int target) const {
    auto it = labelMap_.find(target);
    if (it != labelMap_.end()) {
        return it->second;
    }
    return "";
}

}  // namespace ma::node
