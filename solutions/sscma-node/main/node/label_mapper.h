#pragma once
#include "FlowConfigReader.h"
#include <map>
#include <string>


namespace ma::node {

class LabelMapper {
public:
    // Initialise le mapping à partir d'un fichier JSON (labels.json)
    explicit LabelMapper(const std::string& jsonPath = "labels.json");
    // Retourne le label associé à l'entier target, ou une chaîne vide si non trouvé
    std::string getLabel(int target) const;

private:
    void loadLabels(const std::string& jsonPath);
    std::map<int, std::string> labelMap_;
};

}  // namespace ma::node
