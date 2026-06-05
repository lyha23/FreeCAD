#pragma once

#include "cad_core/runtime/feature_executor.h"

#include <map>
#include <string>
#include <vector>

namespace cad_core::runtime {

class FeatureRegistry {
public:
    void registerExecutor(std::string typeId, runtime::ExecuteFn executor);
    runtime::ExecuteFn executorFor(const std::string& typeId) const;
    std::vector<std::string> typeIds() const;

private:
    std::map<std::string, runtime::ExecuteFn> executors_;
};

FeatureRegistry buildDefaultRegistry();

}  // namespace cad_core::runtime
