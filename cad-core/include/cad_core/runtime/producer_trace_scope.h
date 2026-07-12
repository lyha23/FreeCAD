#pragma once

#include "cad_core/app/document_object.h"
#include "cad_core/app/element_map_producer_trace.h"

#include <nlohmann/json.hpp>

#include <exception>
#include <string>

namespace cad_core::runtime
{

struct ComputeContext;

// FreeCAD authority:
// /Users/li/Chili3DProject/FreeCAD2/src/App/DocumentObject.cpp
// ::DocumentObject::recompute() opens "document.object.recompute", records
// "document.object.execute.begin", and marks a non-StdReturn as an exception. This narrow CAD
// Core wrapper establishes the corresponding Feature/Sketch/Body parent and final property
// handoff; low-level Part producers still publish their own value events.
class ProducerTraceScope
{
public:
    ProducerTraceScope(ComputeContext& context,
                       const app::DocumentObject& object,
                       std::string slice,
                       std::string stage,
                       nlohmann::json fields = nlohmann::json::object());
    ProducerTraceScope(const ProducerTraceScope&) = delete;
    ProducerTraceScope& operator=(const ProducerTraceScope&) = delete;
    ~ProducerTraceScope();

    void event(std::string decision,
               std::string reason,
               nlohmann::json fields = nlohmann::json::object());
    void abort(std::string reason);
    void exception(std::string reason);

private:
    ComputeContext* context_ = nullptr;
    const app::DocumentObject* object_ = nullptr;
    std::string slice_;
    std::string stage_;
    std::string outcome_ = "success";
    std::string reason_ = "producer_finished";
    int uncaughtExceptions_ = 0;
    app::ElementMapProducerTrace::Scope scope_;
};

}  // namespace cad_core::runtime
