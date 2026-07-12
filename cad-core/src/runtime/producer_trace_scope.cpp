#include "cad_core/runtime/producer_trace_scope.h"

#include "cad_core/part/element_map_producer_trace_snapshot.h"
#include "cad_core/runtime/compute_context.h"

#include <utility>

namespace cad_core::runtime
{

ProducerTraceScope::ProducerTraceScope(ComputeContext& context,
                                       const app::DocumentObject& object,
                                       std::string slice,
                                       std::string stage,
                                       nlohmann::json fields)
    : context_(&context)
    , object_(&object)
    , slice_(std::move(slice))
    , stage_(std::move(stage))
    , uncaughtExceptions_(std::uncaught_exceptions())
    , scope_(context.producerTrace->scope(
          {stage_,
           object.name,
           static_cast<long>(object.id),
           object.typeId,
           {{"typeId", object.typeId},
            {"fields", fields},
            {"requiresFinalCheckpoint", true}}}
      ))
{
    context_->producerTrace->record(
        {slice_, "begin", "producer_started", std::move(fields)}
    );
}

ProducerTraceScope::~ProducerTraceScope()
{
    if (context_ == nullptr || object_ == nullptr) {
        return;
    }
    try {
        if (std::uncaught_exceptions() > uncaughtExceptions_) {
            outcome_ = "exception";
            reason_ = "producer_unwound_by_exception";
        }
        const bool failed = outcome_ == "exception" || hasFailed(*context_, object_->name);
        if (failed && outcome_ == "success") {
            outcome_ = "abort";
            reason_ = "producer_reported_failure";
        }
        const auto namedShape = context_->namedShapes.find(object_->name);
        std::string ledgerSnapshot;
        if (namedShape != context_->namedShapes.end()) {
            ledgerSnapshot = part::checkpointNamedShapeLedger(
                namedShape->second,
                object_->name + ":Shape",
                "maker.final_checkpoint"
            );
        }
        else {
            ledgerSnapshot = context_->producerTrace->checkpoint(
                {"state",
                 {{"owner", object_->name},
                  {"stage", stage_},
                  {"outcome", outcome_},
                  {"reason", reason_},
                  {"partialWrite", failed},
                  {"hasShape", context_->shapes.count(object_->name) != 0U},
                  {"hasAddSubShape", context_->addSubShapes.count(object_->name) != 0U}},
                 {},
                 {},
                 {},
                 "maker.final_checkpoint"}
            );
        }
        context_->producerTrace->record({
            "shape_slot.assign",
            failed ? "skipped" : "assigned",
            failed ? "producer_has_no_successful_shape_handoff" : "producer_shape_handoff",
            {{"owner", object_->name},
             {"property", "Shape"},
             {"sourceScope", stage_},
             {"hasShape", context_->shapes.count(object_->name) != 0U},
             {"hasAddSubShape", context_->addSubShapes.count(object_->name) != 0U},
             {"ledgerSnapshot", ledgerSnapshot}},
        });
        context_->producerTrace->record({
            slice_,
            outcome_,
            reason_,
            {{"owner", object_->name},
             {"stage", stage_},
             {"ledgerSnapshot", ledgerSnapshot},
             {"partialWrite", failed}},
        });
        if (outcome_ == "abort") {
            scope_.abort(reason_);
        }
        else if (outcome_ == "exception") {
            scope_.exception(reason_);
        }
    }
    catch (...) {
        scope_.abort("producer_trace_scope_close_failed");
    }
}

void ProducerTraceScope::event(std::string decision,
                               std::string reason,
                               nlohmann::json fields)
{
    context_->producerTrace->record(
        {slice_, std::move(decision), std::move(reason), std::move(fields)}
    );
}

void ProducerTraceScope::abort(std::string reason)
{
    outcome_ = "abort";
    reason_ = std::move(reason);
}

void ProducerTraceScope::exception(std::string reason)
{
    outcome_ = "exception";
    reason_ = std::move(reason);
}

}  // namespace cad_core::runtime
