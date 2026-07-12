#include <nlohmann/json.hpp>

#include <cstdint>
#include <exception>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

// Keep the intentional corruption seam entirely in this focused probe. Production headers do
// not expose a friend or control that can force the recorder into its internal reentrancy state.
#define private public
#include "cad_core/app/element_map_producer_trace.h"
#undef private
#include "cad_core/app/string_hasher.h"
#include "cad_core/part/element_map_producer_trace_snapshot.h"
#include "cad_core/part/topo_shape.h"
#include "topo_shape_producer_trace_internal.h"

#include <BRepPrimAPI_MakeBox.hxx>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace cad_core::app
{

// Test-only access compiled after the private/public substitution above.
struct ElementMapProducerTraceProbeAccess
{
    static void forceReentrantRecord(ElementMapProducerTrace& trace)
    {
        trace.recording_ = true;
        trace.record({"probe.reentrant", "attempt", "forced_by_focused_probe"});
        trace.record({"probe.reentrant", "attempt", "forced_by_focused_probe_again"});
        trace.recording_ = false;
    }
};

}  // namespace cad_core::app

namespace
{

using Trace = cad_core::app::ElementMapProducerTrace;

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void recordExceptionalTransaction(Trace& trace)
{
    auto transaction = trace.beginTransaction({{"ExceptionTarget"}, {{"kind", "probe"}}});
    auto scope = trace.scope({"exception-stage", "ExceptionTarget", 202, "Probe::Exception"});
    trace.checkpoint({"ledger", {{"owner", "ExceptionTarget"}}, {}, {}, {}, "maker.final_checkpoint"});
    throw std::runtime_error("probe exception");
}

void verifyClosedTrace()
{
    Trace trace;
    {
        auto transaction = trace.beginTransaction({{"Body"}, {{"kind", "normal"}}});
        auto outer = trace.scope({"body", "Body", 101, "PartDesign::Body"});
        const std::string table = trace.checkpoint(
            {"stringTable",
             {{"lastId", 2},
              {"entries",
               {{{"value", 1}, {"data", "Pad"}, {"related", nlohmann::json::array()}},
                {{"value", 2},
                 {"data", "Edge"},
                 {"related", {{{"value", 1}, {"index", 0}}}}}}}},
             {{1, 0}},
             {1, 2},
             {},
             "table_checkpoint"}
        );
        require(!table.empty(), "string table checkpoint missing");
        {
            auto inner = trace.scope({"tip-inheritance", "Body", 101, "PartDesign::Body"});
            const std::string identity = trace.firstSeenIdentity("ledger", "Body.Shape");
            require(identity == "ledger:1", "first-seen identity is not monotonic");
            require(
                trace.firstSeenIdentity("ledger", "Body.Shape") == identity,
                "first-seen identity was not stable"
            );
            trace.checkpoint(
                {"ledger",
                 {{"owner", "Body"}, {"identity", identity}, {"stringTableSnapshot", table}},
                 {{2, 0}},
                 {},
                 {table},
                 "maker.final_checkpoint"}
            );
            trace.record({"partdesign.body_tip",
                          "inherit",
                          "tip_ledger_handoff",
                          {{"tip", "Pad"}, {"replayedProducer", false}}});
        }
        trace.checkpoint(
            {"state", {{"stage", "body-final"}}, {}, {}, {}, "maker.final_checkpoint"}
        );
    }

    {
        auto transaction = trace.beginTransaction({{}, {{"kind", "empty_recompute"}}});
        trace.checkpoint(
            {"state", {{"empty", true}}, {}, {}, {}, "maker.final_checkpoint"}
        );
    }

    try {
        recordExceptionalTransaction(trace);
    }
    catch (const std::runtime_error&) {
    }

    {
        auto transaction = trace.beginTransaction({{}, {{"kind", "cancel"}}});
        auto scope = trace.scope({"cancel-stage", "", 0, "Probe::Cancel"});
        trace.checkpoint(
            {"state",
             {{"stage", "cancel-stage"}, {"partialWrite", false}},
             {},
             {},
             {},
             "maker.final_checkpoint"}
        );
        scope.cancel("real_cancel_source");
        transaction.cancel("real_cancel_source");
    }

    const Trace::ValidationResult validation = trace.validate();
    require(validation.valid, validation.errors.empty() ? "trace validation failed"
                                                        : validation.errors.front());
    nlohmann::json document = trace.drain(
        {"CADCore", "probe", "focused", "input", "response"},
        {{101, "Body", "PartDesign::Body"}, {202, "ExceptionTarget", "Probe::Exception"}}
    );
    require(document.at("transactions").size() == 4, "multi-transaction drain lost a transaction");
    require(
        document.at("transactions").at(1).at("fields").at("kind") == "empty_recompute",
        "second empty recompute was erased"
    );
    require(document.at("transactions").at(2).at("outcome") == "exception", "exception was not closed");
    require(document.at("transactions").at(3).at("outcome") == "cancel", "cancel was not closed");
    require(document.at("objectTagIndex").contains("101"), "objectTag index is incomplete");
    require(!document.at("events").empty(), "drained trace has no events");
    std::uint64_t sequence = 1;
    for (const auto& event : document.at("events")) {
        require(event.at("sequence") == sequence++, "event sequence is not contiguous");
        require(!event.at("beforeSnapshot").get<std::string>().empty(), "missing before snapshot");
        require(!event.at("afterSnapshot").get<std::string>().empty(), "missing after snapshot");
    }
    require(trace.empty(), "drain did not release recorder state");
    const nlohmann::json second = trace.drain({"CADCore", "probe", "focused", "input", "response"});
    require(second.at("transactions").empty(), "second drain retained transactions");
    require(second.at("events").empty(), "second drain retained events");
}

void verifyScopeMismatchHardFails()
{
    Trace trace;
    auto transaction = trace.beginTransaction({{"Broken"}, {}});
    auto parent = trace.scope({"parent", "Broken", 303, "Probe::Broken"});
    auto child = trace.scope({"child", "Broken", 303, "Probe::Broken"});
    parent = Trace::Scope {};
    child = Trace::Scope {};
    transaction.abort("intentional_scope_mismatch");
    transaction = Trace::Transaction {};
    const Trace::ValidationResult validation = trace.validate();
    require(!validation.valid, "non-LIFO scope was silently accepted");
}

void verifyReentrancyIsRejectedOnce()
{
    Trace trace;
    {
        auto transaction = trace.beginTransaction({{"Reentrant"}, {}});
        cad_core::app::ElementMapProducerTraceProbeAccess::forceReentrantRecord(trace);
        trace.checkpoint({"state", {{"closed", true}}, {}, {}, {}, "maker.final_checkpoint"});
    }
    const Trace::ValidationResult validation = trace.validate();
    require(validation.valid, "reentrancy diagnostic did not leave a closed transaction");
    const auto document = trace.drain({"CADCore", "probe", "focused", "input", "response"});
    std::size_t diagnostics = 0;
    for (const auto& event : document.at("events")) {
        diagnostics += event.at("slice") == "trace.reentrancy_rejected" ? 1U : 0U;
    }
    require(diagnostics == 1U, "reentrancy guard must emit exactly one diagnostic");
}

void verifyElementMapDropProducerPath()
{
    Trace trace;
    {
        auto transaction = trace.beginTransaction({{"DropResult"}, {}});
        const TopoDS_Shape shape = BRepPrimAPI_MakeBox(1.0, 2.0, 3.0).Shape();
        cad_core::part::NamedShape source =
            cad_core::part::indexedNamedShapeForObject("DropSource", shape);
        source.stringHasher = std::make_shared<cad_core::app::StringHasher>();
        source.stringHasher->attachProducerTrace(&trace);
        cad_core::part::NamedShape dropped = cad_core::part::namedShapeForElementMapPolicyDrop(
            "DropResult",
            shape,
            {{"DropSource", shape, &source}}
        );
        cad_core::part::checkpointNamedShapeLedger(
            dropped,
            "DropResult:Shape",
            "maker.final_checkpoint"
        );
    }
    const Trace::ValidationResult validation = trace.validate();
    require(validation.valid, "ElementMapPolicy::Drop trace did not close");
    const auto document = trace.drain({"CADCore", "probe", "focused", "input", "response"});
    const bool found = std::any_of(
        document.at("events").begin(),
        document.at("events").end(),
        [](const auto& event) { return event.at("slice") == "element_map.drop"; }
    );
    require(found, "ElementMapPolicy::Drop did not publish its producer slice");
    const bool foundDropIdentity = std::any_of(
        document.at("events").begin(),
        document.at("events").end(),
        [](const auto& event) {
            return event.at("slice") == "trace.identity"
                && event.at("decision") == "drop";
        }
    );
    require(foundDropIdentity, "ElementMapPolicy::Drop did not publish identity drop");
}

void verifyOrderedEntryLocalRefs()
{
    Trace trace;
    long secondValue = 0;
    {
        auto transaction = trace.beginTransaction({{"EntryLocalRefs"}, {}});
        auto hasher = std::make_shared<cad_core::app::StringHasher>();
        hasher->attachProducerTrace(&trace);
        const auto first = hasher->getId("first-source");
        const auto second = hasher->getId("second-source");
        secondValue = second.value;
        cad_core::part::NamedShape ledger = cad_core::part::indexedNamedShapeForObject(
            "EntryLocalRefs",
            BRepPrimAPI_MakeBox(1.0, 1.0, 1.0).Shape()
        );
        ledger.stringHasher = hasher;
        ledger.elementMapEntries["Face1"] = {
            {"#1;A", {first}},
            {"#2;B", {second, first}},
        };
        cad_core::part::checkpointNamedShapeLedger(
            ledger,
            "EntryLocalRefs:Shape",
            "maker.final_checkpoint"
        );
    }
    const auto document = trace.drain({"CADCore", "probe", "focused", "input", "response"});
    bool found = false;
    for (const auto& [snapshotId, snapshot] : document.at("ledgerSnapshots").items()) {
        (void)snapshotId;
        const auto& payload = snapshot.at("payload");
        if (!payload.contains("entries") || !payload.at("entries").contains("Face1")) {
            continue;
        }
        const auto entries = payload.at("entries").at("Face1");
        if (entries.size() != 2U) {
            continue;
        }
        require(entries.at(0).at("elementIdRefs").size() == 1U, "first entry refs drifted");
        require(entries.at(1).at("elementIdRefs").size() == 2U, "second entry refs drifted");
        require(
            entries.at(1).at("elementIdRefs").at(0).at("value") == secondValue,
            "entry-local ref order was not preserved"
        );
        found = true;
    }
    require(found, "ordered duplicate ElementMap entries were not snapshotted");
}

void verifyIdentityLifecycle()
{
    Trace trace;
    {
        auto transaction = trace.beginTransaction({{"IdentityLifecycle"}, {}});
        const std::string source =
            trace.firstSeenIdentity("ledger", "IdentityLifecycle.Source", "create");
        trace.firstSeenIdentity("ledger", "IdentityLifecycle.Copy", "copy", source);
        trace.firstSeenIdentity("ledger", "IdentityLifecycle.Share", "share", source);
        trace.firstSeenIdentity("ledger", "IdentityLifecycle.Reset", "reset", source);
        trace.firstSeenIdentity("ledger", "IdentityLifecycle.Drop", "drop", source);
        trace.checkpoint(
            {"state", {{"closed", true}}, {}, {}, {}, "maker.final_checkpoint"}
        );
    }
    const auto document = trace.drain({"CADCore", "probe", "focused", "input", "response"});
    std::set<std::string> decisions;
    for (const auto& event : document.at("events")) {
        if (event.at("slice") == "trace.identity") {
            decisions.insert(event.at("decision").get<std::string>());
        }
    }
    require(
        decisions == std::set<std::string>({"create", "copy", "share", "reset", "drop"}),
        "identity lifecycle is incomplete"
    );
}

void verifyStringHasherTraceHasNoSideEffects()
{
    const auto exercise = [](cad_core::app::StringHasher& hasher) {
        std::vector<cad_core::app::StringId> ids;
        ids.push_back(hasher.getId("source"));
        ids.push_back(hasher.getId(";:M;PROBE"));
        ids.push_back(hasher.getMappedNameId("#1", 2, "#2", {ids.front()}));
        ids.push_back(hasher.getMappedNameId("#1", 3, "#2", {ids.front()}));
        return ids;
    };

    cad_core::app::StringHasher baseline;
    const auto baselineIds = exercise(baseline);
    const nlohmann::json baselineState = baseline.inspectProducerTraceState();

    Trace trace;
    cad_core::app::StringHasher observed;
    std::vector<cad_core::app::StringId> observedIds;
    {
        auto transaction = trace.beginTransaction({{"HasherIsolation"}, {}});
        observed.attachProducerTrace(&trace);
        observedIds = exercise(observed);
        trace.checkpoint(
            {"stringTable",
             observed.inspectProducerTraceState(),
             {},
             {1, 2, 3},
             {},
             "maker.final_checkpoint"}
        );
    }
    require(baselineIds == observedIds, "trace attachment changed SID allocation results");
    require(
        baselineState == observed.inspectProducerTraceState(),
        "trace attachment changed StringHasher allocation table or relation order"
    );
    require(trace.validate().valid, "StringHasher isolation trace did not close");
}

void verifyTransactionLocalSidNamespace()
{
    Trace trace;
    for (int transactionIndex = 0; transactionIndex < 2; ++transactionIndex) {
        auto transaction = trace.beginTransaction(
            {{"HasherTransaction" + std::to_string(transactionIndex + 1)}, {}}
        );
        cad_core::app::StringHasher hasher;
        hasher.attachProducerTrace(&trace);
        const auto first = hasher.getId("transaction-local-source");
        require(first.value == 1L, "fresh transaction StringHasher did not restart at SID 1");
        trace.checkpoint(
            {"stringTable",
             hasher.inspectProducerTraceState(),
             {},
             {1},
             {},
             "maker.final_checkpoint"}
        );
    }
    const auto validation = trace.validate();
    require(
        validation.valid,
        validation.errors.empty() ? "transaction-local SID validation failed"
                                  : validation.errors.front()
    );
}

void verifyUpperLowerDuplicateSuppression()
{
    Trace trace;
    {
        auto transaction = trace.beginTransaction({{"DuplicateSuppression"}, {}});
        cad_core::app::StringHasher hasher;
        hasher.attachProducerTrace(&trace);
        const auto upperKeptRef = hasher.getId("upper-kept-source");
        const auto upperSuppressedRef = hasher.getId("upper-suppressed-source");
        const auto lowerKeptRef = hasher.getId("lower-kept-source");
        const auto lowerSuppressedRef = hasher.getId("lower-suppressed-source");
        cad_core::part::producer_trace_detail::publishDuplicateCandidateSuppressed(
            trace,
            "maker.upper",
            "upper_duplicate_candidate_suppressed",
            {"#1;:M;SRC",
             {"Face1", "Edge1", 1U, {upperKeptRef}},
             {"Face2", "Edge1", 2U, {upperSuppressedRef}}}
        );
        cad_core::part::producer_trace_detail::publishDuplicateCandidateSuppressed(
            trace,
            "maker.lower",
            "lower_duplicate_candidate_suppressed",
            {"#2;:M;SRC",
             {"Face1", "Edge1", 1U, {lowerKeptRef}},
             {"Face1", "Edge2", 2U, {lowerSuppressedRef}}}
        );
        trace.checkpoint(
            {"stringTable",
             hasher.inspectProducerTraceState(),
             {},
             {upperKeptRef.value,
              upperSuppressedRef.value,
              lowerKeptRef.value,
              lowerSuppressedRef.value},
             {},
             "maker.final_checkpoint"}
        );
    }
    const auto document = trace.drain({"CADCore", "probe", "focused", "input", "response"});
    std::set<std::string> duplicateReasons;
    for (const auto& event : document.at("events")) {
        const std::string reason = event.at("reason").get<std::string>();
        if (reason == "upper_duplicate_candidate_suppressed"
            || reason == "lower_duplicate_candidate_suppressed") {
            duplicateReasons.insert(reason);
            const auto& kept = event.at("fields").at("keptCandidate");
            const auto& suppressed = event.at("fields").at("suppressedCandidate");
            require(
                kept.at("parent") != suppressed.at("parent")
                    || kept.at("child") != suppressed.at("child"),
                "duplicate evidence does not distinguish kept and suppressed candidates"
            );
            require(
                kept.at("entryLocalRefs") != suppressed.at("entryLocalRefs"),
                "duplicate evidence does not preserve both entry-local ref sets"
            );
        }
    }
    require(
        duplicateReasons.count("upper_duplicate_candidate_suppressed") == 1U,
        "upper duplicate suppression was not published"
    );
    require(
        duplicateReasons.count("lower_duplicate_candidate_suppressed") == 1U,
        "lower seam duplicate suppression was not published"
    );
}

}  // namespace

int main()
{
    try {
        verifyClosedTrace();
        verifyScopeMismatchHardFails();
        verifyReentrancyIsRejectedOnce();
        verifyElementMapDropProducerPath();
        verifyOrderedEntryLocalRefs();
        verifyIdentityLifecycle();
        verifyStringHasherTraceHasNoSideEffects();
        verifyTransactionLocalSidNamespace();
        verifyUpperLowerDuplicateSuppression();
        std::cout << "element-map producer trace probe: ok\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error) {
        std::cerr << "element-map producer trace probe: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
