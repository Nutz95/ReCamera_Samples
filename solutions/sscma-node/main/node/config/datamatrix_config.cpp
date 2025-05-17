#include "datamatrix_config.h"
#include "FlowConfigReader.h"
#include "logger.hpp"
#include <string>
#include <unistd.h>

namespace ma {

static constexpr char TAG[] = "ma::datamatrix_config";

DatamatrixConfig readDatamatrixConfigFromFile(const std::string& filename) {
    DatamatrixConfig config;
    std::vector<std::string> possiblePaths = {filename, "/userdata/flow.json"};
    std::string usedPath                   = "";
    bool fileLoaded                        = false;
    for (const auto& path : possiblePaths) {
        // MA_LOGI(TAG, "Reading Datamatrix Configuration: %s", path.c_str());
        try {
            FlowConfigReader reader(path);
            if (reader.reload()) {
                usedPath        = path;
                fileLoaded      = true;
                config.enabled  = reader.getRootConfigBool("datamatrix_config", "enabled", false);
                config.roi_xmin = reader.getRootConfigInt("datamatrix_config", "roi_xmin", 0);
                config.roi_ymin = reader.getRootConfigInt("datamatrix_config", "roi_ymin", 0);
                config.roi_xmax = reader.getRootConfigInt("datamatrix_config", "roi_xmax", 0);
                config.roi_ymax = reader.getRootConfigInt("datamatrix_config", "roi_ymax", 0);
                /*MA_LOGI(TAG,
                        "Datamatrix parameters loaded from %s: enabled=%s, roi_xmin=%d, roi_ymin=%d, roi_xmax=%d, roi_ymax=%d",
                        path.c_str(),
                        config.enabled ? "true" : "false",
                        config.roi_xmin,
                        config.roi_ymin,
                        config.roi_xmax,
                        config.roi_ymax);*/
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
