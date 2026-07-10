#pragma once

#include "cad_core/runtime/feature_executor.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace cad_core::runtime {

enum class MissingReferenceAdmissionPolicy
{
    GraphValidated,
    ProducerValidated,
};

class FeatureRegistry {
public:
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp
    // ::makeFilledFace() resolves its helper arguments inside the producer before calling
    // TopoShape::makeElementFilledFace(). ProducerValidated keeps that admission/error envelope
    // with the producer; ordinary DocumentObject links remain runtime/graph validated.
    void registerExecutor(
        std::string typeId,
        runtime::ExecuteFn executor,
        MissingReferenceAdmissionPolicy missingReferenceAdmission
            = MissingReferenceAdmissionPolicy::GraphValidated
    );
    runtime::ExecuteFn executorFor(const std::string& typeId) const;
    const std::set<std::string>& producerMissingReferenceAdmissionTypeIds() const;
    std::vector<std::string> typeIds() const;

private:
    std::map<std::string, runtime::ExecuteFn> executors_;
    std::set<std::string> producerMissingReferenceAdmissionTypeIds_;
};

FeatureRegistry buildDefaultRegistry();

}  // namespace cad_core::runtime
