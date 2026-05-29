#pragma once

#include "cad_core/features/feature_executor.h"

#include <map>
#include <string>
#include <vector>

namespace cad_core::runtime {

class FeatureRegistry {
public:
    void registerExecutor(std::string typeId, features::ExecuteFn executor);
    features::ExecuteFn executorFor(const std::string& typeId) const;
    std::vector<std::string> typeIds() const;

private:
    std::map<std::string, features::ExecuteFn> executors_;
};

FeatureRegistry buildDefaultRegistry();

}  // namespace cad_core::runtime
