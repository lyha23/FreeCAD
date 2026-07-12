#include "cad_core/app/element_map_producer_trace.h"

#include "cad_core/part/brep_snapshot.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace cad_core::app
{

namespace
{

template<typename Map, typename Key>
bool contains(const Map& values, const Key& key)
{
    return values.find(key) != values.end();
}

nlohmann::json sidRefJson(const ElementMapProducerTrace::SidRef& ref)
{
    return {{"value", ref.value}, {"index", ref.index}};
}

std::string lowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

void appendSidRefValues(const nlohmann::json& value, std::vector<long>& result)
{
    if (value.is_number_integer()) {
        result.push_back(value.get<long>());
        return;
    }
    if (value.is_object()) {
        const auto sid = value.find("value");
        if (sid != value.end() && sid->is_number_integer()) {
            result.push_back(sid->get<long>());
        }
        for (const auto& item : value.items()) {
            if (item.value().is_object() || item.value().is_array()) {
                appendSidRefValues(item.value(), result);
            }
        }
        return;
    }
    if (value.is_array()) {
        for (const auto& item : value) {
            appendSidRefValues(item, result);
        }
    }
}

using IndexedInventory = std::map<std::string, std::set<std::string>>;

IndexedInventory indexedInventory(const nlohmann::json& shape)
{
    IndexedInventory result;
    if (!shape.is_object() || !shape.contains("indexed") || !shape.at("indexed").is_object()) {
        return result;
    }
    for (const auto& group : shape.at("indexed").items()) {
        if (!group.value().is_array()) {
            continue;
        }
        auto& names = result[lowerAscii(group.key())];
        for (const auto& item : group.value()) {
            if (item.is_object() && item.contains("indexed")
                && item.at("indexed").is_string()) {
                names.insert(lowerAscii(item.at("indexed").get<std::string>()));
            }
        }
    }
    return result;
}

std::string indexedKind(const std::string& indexed)
{
    const std::string lowered = lowerAscii(indexed);
    for (const std::string kind : {"vertex", "edge", "face"}) {
        if (lowered.rfind(kind, 0) == 0) {
            return kind;
        }
    }
    return {};
}

class RecordingGuard
{
public:
    explicit RecordingGuard(bool& recording)
        : recording_(recording)
    {
        recording_ = true;
    }

    ~RecordingGuard() { recording_ = false; }

private:
    bool& recording_;
};

}  // namespace

struct ElementMapProducerTrace::EventState
{
    std::uint64_t sequence = 0;
    std::uint64_t transaction = 0;
    std::uint64_t scope = 0;
    std::uint64_t parentScope = 0;
    std::string object;
    long objectTag = 0;
    std::string producer;
    std::string slice;
    std::string decision;
    std::string reason;
    nlohmann::json fields = nlohmann::json::object();
    std::string beforeSnapshot;
    std::string afterSnapshot;
    std::string nondeterminismClass;
    std::string stableComparisonKey;
};

struct ElementMapProducerTrace::ScopeState
{
    std::uint64_t sequence = 0;
    std::uint64_t parent = 0;
    std::uint64_t transaction = 0;
    std::string stage;
    std::string object;
    long objectTag = 0;
    std::string producer;
    nlohmann::json fields = nlohmann::json::object();
    std::uint64_t beginEvent = 0;
    std::uint64_t endEvent = 0;
    std::string outcome;
    std::string detail;
};

struct ElementMapProducerTrace::TransactionState
{
    std::uint64_t sequence = 0;
    std::vector<std::string> targets;
    nlohmann::json fields = nlohmann::json::object();
    std::uint64_t beginEvent = 0;
    std::uint64_t endEvent = 0;
    std::string outcome;
    std::string detail;
    bool closed = false;
};

struct ElementMapProducerTrace::SnapshotState
{
    std::string id;
    std::string kind;
    std::string hash;
    nlohmann::json payload = nlohmann::json::object();
    std::vector<SidRef> sidRefs;
    std::vector<long> definedSids;
    std::vector<std::string> nestedSnapshotRefs;
    std::string label;
    std::uint64_t publishedEvent = 0;
};

ElementMapProducerTrace::Scope::Scope(ElementMapProducerTrace* trace, std::uint64_t sequence)
    : trace_(trace)
    , sequence_(sequence)
    , uncaughtExceptions_(std::uncaught_exceptions())
{}

ElementMapProducerTrace::Scope::Scope(Scope&& other) noexcept
    : trace_(std::exchange(other.trace_, nullptr))
    , sequence_(std::exchange(other.sequence_, 0))
    , uncaughtExceptions_(other.uncaughtExceptions_)
    , outcome_(std::move(other.outcome_))
    , detail_(std::move(other.detail_))
{}

ElementMapProducerTrace::Scope& ElementMapProducerTrace::Scope::operator=(Scope&& other) noexcept
{
    if (this != &other) {
        close();
        trace_ = std::exchange(other.trace_, nullptr);
        sequence_ = std::exchange(other.sequence_, 0);
        uncaughtExceptions_ = other.uncaughtExceptions_;
        outcome_ = std::move(other.outcome_);
        detail_ = std::move(other.detail_);
    }
    return *this;
}

ElementMapProducerTrace::Scope::~Scope()
{
    close();
}

void ElementMapProducerTrace::Scope::success(std::string detail)
{
    outcome_ = "success";
    detail_ = std::move(detail);
}

void ElementMapProducerTrace::Scope::exception(std::string detail)
{
    outcome_ = "exception";
    detail_ = std::move(detail);
}

void ElementMapProducerTrace::Scope::cancel(std::string detail)
{
    outcome_ = "cancel";
    detail_ = std::move(detail);
}

void ElementMapProducerTrace::Scope::abort(std::string detail)
{
    outcome_ = "abort";
    detail_ = std::move(detail);
}

void ElementMapProducerTrace::Scope::close() noexcept
{
    if (trace_ == nullptr) {
        return;
    }
    if (outcome_ == "success" && std::uncaught_exceptions() > uncaughtExceptions_) {
        outcome_ = "exception";
        if (detail_.empty()) {
            detail_ = "scope_unwound_by_exception";
        }
    }
    trace_->endScope(sequence_, outcome_, detail_);
    trace_ = nullptr;
    sequence_ = 0;
}

ElementMapProducerTrace::Transaction::Transaction(ElementMapProducerTrace* trace,
                                                   std::uint64_t sequence)
    : trace_(trace)
    , sequence_(sequence)
    , uncaughtExceptions_(std::uncaught_exceptions())
{}

ElementMapProducerTrace::Transaction::Transaction(Transaction&& other) noexcept
    : trace_(std::exchange(other.trace_, nullptr))
    , sequence_(std::exchange(other.sequence_, 0))
    , uncaughtExceptions_(other.uncaughtExceptions_)
    , outcome_(std::move(other.outcome_))
    , detail_(std::move(other.detail_))
{}

ElementMapProducerTrace::Transaction& ElementMapProducerTrace::Transaction::operator=(
    Transaction&& other
) noexcept
{
    if (this != &other) {
        close();
        trace_ = std::exchange(other.trace_, nullptr);
        sequence_ = std::exchange(other.sequence_, 0);
        uncaughtExceptions_ = other.uncaughtExceptions_;
        outcome_ = std::move(other.outcome_);
        detail_ = std::move(other.detail_);
    }
    return *this;
}

ElementMapProducerTrace::Transaction::~Transaction()
{
    close();
}

void ElementMapProducerTrace::Transaction::success(std::string detail)
{
    outcome_ = "success";
    detail_ = std::move(detail);
}

void ElementMapProducerTrace::Transaction::exception(std::string detail)
{
    outcome_ = "exception";
    detail_ = std::move(detail);
}

void ElementMapProducerTrace::Transaction::cancel(std::string detail)
{
    outcome_ = "cancel";
    detail_ = std::move(detail);
}

void ElementMapProducerTrace::Transaction::abort(std::string detail)
{
    outcome_ = "abort";
    detail_ = std::move(detail);
}

void ElementMapProducerTrace::Transaction::close() noexcept
{
    if (trace_ == nullptr) {
        return;
    }
    if (outcome_ == "success" && std::uncaught_exceptions() > uncaughtExceptions_) {
        outcome_ = "exception";
        if (detail_.empty()) {
            detail_ = "transaction_unwound_by_exception";
        }
    }
    trace_->endTransaction(sequence_, outcome_, detail_);
    trace_ = nullptr;
    sequence_ = 0;
}

ElementMapProducerTrace::ElementMapProducerTrace()
{
    SnapshotState initial;
    initial.kind = "state";
    initial.payload = nlohmann::json::object();
    initial.hash = canonicalSha256(initial.payload);
    initial.id = initial.kind + ":sha256:" + initial.hash;
    initial.label = "initial";
    currentSnapshot_ = initial.id;
    snapshots_.emplace(initial.id, std::move(initial));
}

ElementMapProducerTrace::~ElementMapProducerTrace() = default;

ElementMapProducerTrace::Transaction ElementMapProducerTrace::beginTransaction(
    TransactionDescriptor descriptor
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (activeTransaction_ != 0) {
        appendEvent({"trace.transaction_rejected",
                     "rejected",
                     "transaction_already_active",
                     {{"activeTransaction", activeTransaction_}}});
        throw std::logic_error("ElementMap producer trace transaction already active");
    }

    const std::uint64_t sequence = ++nextTransaction_;
    activeTransaction_ = sequence;
    TransactionState state;
    state.sequence = sequence;
    state.targets = std::move(descriptor.targets);
    state.fields = std::move(descriptor.fields);
    transactions_.emplace(sequence, std::move(state));
    appendEvent({"document.recompute.begin",
                 "begin",
                 "request_started",
                 {{"targets", transactions_.at(sequence).targets},
                  {"descriptor", transactions_.at(sequence).fields}}});
    transactions_.at(sequence).beginEvent = events_.back().sequence;
    return Transaction(this, sequence);
}

ElementMapProducerTrace::Scope ElementMapProducerTrace::scope(ScopeDescriptor descriptor)
{
    return Scope(this, beginScope(std::move(descriptor)));
}

std::uint64_t ElementMapProducerTrace::beginScope(ScopeDescriptor descriptor)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (activeTransaction_ == 0) {
        throw std::logic_error("ElementMap producer trace scope requires an active transaction");
    }
    const std::uint64_t sequence = ++nextScope_;
    ScopeState state;
    state.sequence = sequence;
    state.parent = scopeStack_.empty() ? 0 : scopeStack_.back();
    state.transaction = activeTransaction_;
    state.stage = std::move(descriptor.stage);
    state.object = std::move(descriptor.object);
    state.objectTag = descriptor.objectTag;
    state.producer = std::move(descriptor.producer);
    state.fields = std::move(descriptor.fields);
    scopes_.emplace(sequence, std::move(state));
    scopeStack_.push_back(sequence);
    appendEvent({"scope.begin",
                 "begin",
                 "scope_started",
                 {{"stage", scopes_.at(sequence).stage},
                  {"descriptor", scopes_.at(sequence).fields}}});
    scopes_.at(sequence).beginEvent = events_.back().sequence;
    return sequence;
}

void ElementMapProducerTrace::endScope(std::uint64_t sequence,
                                       const std::string& outcome,
                                       const std::string& detail) noexcept
{
    try {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        const auto found = scopes_.find(sequence);
        if (found == scopes_.end() || found->second.endEvent != 0) {
            return;
        }
        if (scopeStack_.empty() || scopeStack_.back() != sequence) {
            appendEvent({"trace.scope_mismatch",
                         "rejected",
                         "non_lifo_scope",
                         {{"scopeSequence", sequence}, {"detail", detail}}});
            scopeStack_.erase(
                std::remove(scopeStack_.begin(), scopeStack_.end(), sequence),
                scopeStack_.end()
            );
            found->second.outcome = "abort";
            found->second.detail = detail.empty() ? "non_lifo_scope" : detail;
            appendEvent({"scope.abort",
                         "abort",
                         "non_lifo_scope",
                         {{"stage", found->second.stage}, {"detail", found->second.detail}}});
            found->second.endEvent = events_.back().sequence;
            return;
        }

        if (found->second.fields.value("requiresFinalCheckpoint", false)) {
            const bool hasFinalCheckpoint = std::any_of(
                events_.begin(),
                events_.end(),
                [sequence, &found](const EventState& event) {
                    return event.sequence > found->second.beginEvent
                        && event.scope == sequence && event.slice == "maker.final_checkpoint";
                }
            );
            if (!hasFinalCheckpoint) {
                checkpoint(
                    {"state",
                     {{"stage", found->second.stage},
                      {"object", found->second.object},
                      {"outcome", outcome},
                      {"detail", detail},
                      {"partialWrite", outcome != "success"},
                      {"fallback", "scope_closed_without_value_checkpoint"}},
                     {},
                     {},
                     {},
                     "maker.final_checkpoint"}
                );
            }
        }

        found->second.outcome = outcome;
        found->second.detail = detail;
        appendEvent({outcome == "abort" ? "scope.abort" : "scope.end",
                     outcome,
                     detail.empty() ? "scope_closed" : detail,
                     {{"stage", found->second.stage}, {"detail", detail}}});
        found->second.endEvent = events_.back().sequence;
        scopeStack_.pop_back();
    }
    catch (...) {
        // RAII cleanup must never replace the producer exception. validate() will expose any
        // incomplete scope if allocating the diagnostic event itself failed.
    }
}

void ElementMapProducerTrace::endTransaction(std::uint64_t sequence,
                                             const std::string& outcome,
                                             const std::string& detail) noexcept
{
    try {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        const auto found = transactions_.find(sequence);
        if (found == transactions_.end() || found->second.closed) {
            return;
        }
        while (!scopeStack_.empty()) {
            endScope(scopeStack_.back(), "abort", "transaction_closed_with_open_scope");
        }
        found->second.outcome = outcome;
        found->second.detail = detail;
        appendEvent({"document.recompute.end",
                     outcome,
                     detail.empty() ? "request_finished" : detail,
                     {{"detail", detail}, {"partialWrite", outcome != "success"}}});
        found->second.endEvent = events_.back().sequence;
        found->second.closed = true;
        if (activeTransaction_ == sequence) {
            activeTransaction_ = 0;
        }
    }
    catch (...) {
        // See endScope(): transaction destructors are noexcept cleanup.
    }
}

void ElementMapProducerTrace::record(EventValue value)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (recording_) {
        appendReentrancyDiagnostic();
        return;
    }
    RecordingGuard guard(recording_);
    appendEvent(std::move(value));
}

void ElementMapProducerTrace::appendEvent(EventValue value)
{
    EventState event;
    event.sequence = ++nextEvent_;
    event.transaction = activeTransaction_;
    event.scope = scopeStack_.empty() ? 0 : scopeStack_.back();
    event.beforeSnapshot = value.beforeSnapshot.empty() ? currentSnapshot_ : value.beforeSnapshot;
    event.afterSnapshot = value.afterSnapshot.empty() ? currentSnapshot_ : value.afterSnapshot;
    if (event.scope != 0) {
        const ScopeState& scopeState = scopes_.at(event.scope);
        event.parentScope = scopeState.parent;
        event.object = scopeState.object;
        event.objectTag = scopeState.objectTag;
        event.producer = scopeState.producer;
    }
    event.slice = std::move(value.slice);
    event.decision = std::move(value.decision);
    event.reason = std::move(value.reason);
    event.fields = std::move(value.fields);
    event.nondeterminismClass = std::move(value.nondeterminismClass);
    event.stableComparisonKey = std::move(value.stableComparisonKey);
    events_.push_back(std::move(event));
}

void ElementMapProducerTrace::appendReentrancyDiagnostic()
{
    if (emittedReentrancyDiagnostic_) {
        return;
    }
    emittedReentrancyDiagnostic_ = true;
    appendEvent({"trace.reentrancy_rejected",
                 "rejected",
                 "recorder_reentrant_write",
                 nlohmann::json::object()});
}

std::string ElementMapProducerTrace::checkpoint(SnapshotValue value)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (recording_) {
        appendReentrancyDiagnostic();
        return {};
    }
    RecordingGuard guard(recording_);
    if (value.kind.empty()) {
        throw std::invalid_argument("ElementMap producer trace snapshot kind is empty");
    }
    const std::string hash = canonicalSha256(value.payload);
    const std::string id = value.kind + ":sha256:" + hash;
    auto inserted = snapshots_.emplace(id, SnapshotState {});
    SnapshotState& snapshot = inserted.first->second;
    if (inserted.second) {
        snapshot.id = id;
        snapshot.kind = value.kind;
        snapshot.hash = hash;
        snapshot.payload = std::move(value.payload);
        snapshot.sidRefs = std::move(value.sidRefs);
        snapshot.definedSids = std::move(value.definedSids);
        snapshot.nestedSnapshotRefs = std::move(value.nestedSnapshotRefs);
        snapshot.label = value.label;
    }
    const std::string before = currentSnapshot_;
    currentSnapshot_ = id;
    appendEvent({value.label.empty() ? "checkpoint" : value.label,
                 "published",
                 "snapshot_complete",
                 {{"snapshot", id}},
                 before,
                 id});
    if (snapshot.publishedEvent == 0) {
        snapshot.publishedEvent = events_.back().sequence;
    }
    return id;
}

std::string ElementMapProducerTrace::firstSeenIdentity(const std::string& kind,
                                                       const std::string& role,
                                                       const std::string& relation,
                                                       const std::string& relatedIdentity)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const std::string key = kind + "\x1f" + role;
    const auto found = identities_.find(key);
    if (found != identities_.end()) {
        return found->second;
    }
    const std::string identity = kind + ":" + std::to_string(++nextIdentity_);
    identities_.emplace(key, identity);
    record({"trace.identity",
            relation.empty() ? "create" : relation,
            "first_seen_value_identity",
            {{"kind", kind},
             {"role", role},
             {"identity", identity},
             {"relatedIdentity", relatedIdentity}}});
    return identity;
}

ElementMapProducerTrace::ValidationResult ElementMapProducerTrace::validate() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    ValidationResult result;
    const auto fail = [&result](std::string error) {
        result.valid = false;
        result.errors.push_back(std::move(error));
    };

    if (activeTransaction_ != 0) {
        fail("active_transaction");
    }
    if (!scopeStack_.empty()) {
        fail("open_scope_stack");
    }

    std::uint64_t expectedEvent = 1;
    std::uint64_t previousTransactionEnd = 0;
    std::set<long> eventKnownSids;
    for (const EventState& event : events_) {
        if (event.slice == "document.recompute.begin") {
            eventKnownSids.clear();
        }
        if (event.sequence != expectedEvent++) {
            fail("non_contiguous_event_sequence:" + std::to_string(event.sequence));
        }
        if (!contains(snapshots_, event.beforeSnapshot)
            || !contains(snapshots_, event.afterSnapshot)) {
            fail("missing_event_snapshot:" + std::to_string(event.sequence));
        }
        if (event.transaction == 0 || !contains(transactions_, event.transaction)) {
            fail("unknown_event_transaction:" + std::to_string(event.sequence));
        }
        if (event.scope != 0 && !contains(scopes_, event.scope)) {
            fail("unknown_event_scope:" + std::to_string(event.sequence));
        }
        if (event.scope != 0 && scopes_.at(event.scope).transaction != event.transaction) {
            fail("scope_transaction_mismatch:" + std::to_string(event.sequence));
        }
        if (event.slice.empty() || event.decision.empty() || event.reason.empty()) {
            fail("incomplete_event_decision:" + std::to_string(event.sequence));
        }
        if (!event.fields.is_object()) {
            fail("invalid_event_fields:" + std::to_string(event.sequence));
            continue;
        }
        for (const std::string key : {
                 "elementIdRefs",
                 "entryLocalRefs",
                 "orderedEntries",
                 "inputRelated",
                 "orderedRelated",
                 "related",
                 "tupleId"}) {
            const auto field = event.fields.find(key);
            if (field == event.fields.end()) {
                continue;
            }
            std::vector<long> refs;
            appendSidRefValues(*field, refs);
            for (const long sid : refs) {
                if (sid > 0 && eventKnownSids.count(sid) == 0U) {
                    fail("unknown_event_sid:" + std::to_string(event.sequence) + ":"
                         + std::to_string(sid));
                }
            }
        }
        const auto prefix = event.fields.find("prefixId");
        if (prefix != event.fields.end() && prefix->is_number_integer()) {
            const long sid = prefix->get<long>();
            if (sid > 0 && eventKnownSids.count(sid) == 0U) {
                fail("unknown_event_sid:" + std::to_string(event.sequence) + ":"
                     + std::to_string(sid));
            }
        }
        if (event.slice == "hasher.insert") {
            const auto sidResult = event.fields.find("result");
            if (sidResult == event.fields.end() || !sidResult->is_object()
                || !sidResult->contains("value")
                || !sidResult->at("value").is_number_integer()
                || sidResult->at("value").get<long>() <= 0) {
                fail("invalid_hasher_result:" + std::to_string(event.sequence));
            }
            else {
                const long sid = sidResult->at("value").get<long>();
                if (event.decision == "allocation") {
                    if (eventKnownSids.count(sid) != 0U) {
                        fail("reallocated_event_sid:" + std::to_string(event.sequence) + ":"
                             + std::to_string(sid));
                    }
                    eventKnownSids.insert(sid);
                }
                else if (event.decision == "hit" && eventKnownSids.count(sid) == 0U) {
                    fail("unknown_hasher_hit_sid:" + std::to_string(event.sequence) + ":"
                         + std::to_string(sid));
                }
            }
        }
    }

    std::uint64_t expectedTransaction = 1;
    for (const auto& item : transactions_) {
        const std::uint64_t sequence = item.first;
        const TransactionState& transaction = item.second;
        if (sequence != expectedTransaction++) {
            fail("non_contiguous_transaction_sequence:" + std::to_string(sequence));
        }
        if (!transaction.closed || transaction.beginEvent == 0 || transaction.endEvent == 0
            || transaction.endEvent < transaction.beginEvent) {
            fail("unclosed_transaction:" + std::to_string(sequence));
            continue;
        }
        if (transaction.beginEvent != previousTransactionEnd + 1) {
            fail("transaction_range_gap_or_overlap:" + std::to_string(sequence));
        }
        previousTransactionEnd = transaction.endEvent;
        for (std::uint64_t eventSequence = transaction.beginEvent;
             eventSequence <= transaction.endEvent && eventSequence <= events_.size();
             ++eventSequence) {
            if (events_[static_cast<std::size_t>(eventSequence - 1)].transaction != sequence) {
                fail("transaction_range_contains_foreign_event:" + std::to_string(sequence));
                break;
            }
        }
        if (events_.at(static_cast<std::size_t>(transaction.beginEvent - 1)).slice
                != "document.recompute.begin"
            || events_.at(static_cast<std::size_t>(transaction.endEvent - 1)).slice
                != "document.recompute.end") {
            fail("transaction_boundary_slice_mismatch:" + std::to_string(sequence));
        }
        const EventState* finalSemanticEvent = nullptr;
        for (std::uint64_t eventSequence = transaction.beginEvent;
             eventSequence < transaction.endEvent && eventSequence <= events_.size();
             ++eventSequence) {
            const EventState& event = events_[static_cast<std::size_t>(eventSequence - 1)];
            if (event.slice != "scope.end" && event.slice != "scope.abort") {
                finalSemanticEvent = &event;
            }
        }
        if (finalSemanticEvent == nullptr
            || (finalSemanticEvent->slice != "maker.final_checkpoint"
                && finalSemanticEvent->slice != "document.recompute.checkpoint")) {
            fail("transaction_missing_final_checkpoint:" + std::to_string(sequence));
        }
        const EventState& endEvent =
            events_.at(static_cast<std::size_t>(transaction.endEvent - 1));
        const auto partialWrite = endEvent.fields.find("partialWrite");
        if (partialWrite == endEvent.fields.end() || !partialWrite->is_boolean()) {
            fail("transaction_missing_partial_write:" + std::to_string(sequence));
        }
        else if (partialWrite->get<bool>() != (transaction.outcome != "success")) {
            fail("transaction_partial_write_outcome_mismatch:" + std::to_string(sequence));
        }
    }
    if (!events_.empty() && previousTransactionEnd != events_.back().sequence) {
        fail("event_outside_transaction_range");
    }

    std::uint64_t expectedScope = 1;
    for (const auto& item : scopes_) {
        const std::uint64_t sequence = item.first;
        const ScopeState& scopeState = item.second;
        if (sequence != expectedScope++) {
            fail("non_contiguous_scope_sequence:" + std::to_string(sequence));
        }
        if (scopeState.beginEvent == 0 || scopeState.endEvent == 0
            || scopeState.endEvent < scopeState.beginEvent || scopeState.outcome.empty()) {
            fail("unclosed_scope:" + std::to_string(sequence));
            continue;
        }
        if (!contains(transactions_, scopeState.transaction)) {
            fail("unknown_scope_transaction:" + std::to_string(sequence));
        }
        if (scopeState.parent != 0) {
            const auto parent = scopes_.find(scopeState.parent);
            if (parent == scopes_.end()) {
                fail("unknown_parent_scope:" + std::to_string(sequence));
            }
            else if (parent->second.transaction != scopeState.transaction
                     || parent->second.beginEvent >= scopeState.beginEvent
                     || parent->second.endEvent <= scopeState.endEvent) {
                fail("invalid_parent_scope_range:" + std::to_string(sequence));
            }
        }
        const EventState& begin = events_.at(static_cast<std::size_t>(scopeState.beginEvent - 1));
        const EventState& end = events_.at(static_cast<std::size_t>(scopeState.endEvent - 1));
        if (begin.slice != "scope.begin" || begin.scope != sequence
            || (end.slice != "scope.end" && end.slice != "scope.abort")
            || end.scope != sequence) {
            fail("scope_boundary_slice_mismatch:" + std::to_string(sequence));
        }
        if (scopeState.fields.value("requiresFinalCheckpoint", false)) {
            bool hasFinalCheckpoint = false;
            for (std::uint64_t eventSequence = scopeState.beginEvent + 1;
                 eventSequence < scopeState.endEvent && eventSequence <= events_.size();
                 ++eventSequence) {
                const EventState& event =
                    events_.at(static_cast<std::size_t>(eventSequence - 1));
                if (event.scope == sequence && event.slice == "maker.final_checkpoint") {
                    hasFinalCheckpoint = true;
                    break;
                }
            }
            if (!hasFinalCheckpoint) {
                fail("producer_scope_missing_final_checkpoint:" + std::to_string(sequence));
            }
        }
    }

    std::set<long> knownSids;
    std::vector<const SnapshotState*> orderedSnapshots;
    orderedSnapshots.reserve(snapshots_.size());
    for (const auto& item : snapshots_) {
        const SnapshotState& snapshot = item.second;
        if (snapshot.id != snapshot.kind + ":sha256:" + canonicalSha256(snapshot.payload)
            || snapshot.hash != canonicalSha256(snapshot.payload)) {
            fail("snapshot_content_hash_mismatch:" + snapshot.id);
        }
        for (const std::string& nested : snapshot.nestedSnapshotRefs) {
            if (!contains(snapshots_, nested)) {
                fail("missing_nested_snapshot:" + snapshot.id);
            }
        }
        if (snapshot.payload.is_object() && snapshot.payload.contains("childRanges")) {
            const auto& childRanges = snapshot.payload.at("childRanges");
            const IndexedInventory inventory = snapshot.payload.contains("shape")
                ? indexedInventory(snapshot.payload.at("shape"))
                : IndexedInventory {};
            if (!childRanges.is_array()) {
                fail("invalid_child_range_collection:" + snapshot.id);
            }
            else {
                for (const auto& child : childRanges) {
                    if (!child.is_object() || !child.contains("offset")
                        || !child.at("offset").is_number_integer()
                        || child.at("offset").get<long long>() < 0
                        || !child.contains("count") || !child.at("count").is_number_integer()
                        || child.at("count").get<long long>() < 0
                        || child.value("kind", "").empty()) {
                        fail("invalid_child_range:" + snapshot.id);
                    }
                    else {
                        const std::string kind = lowerAscii(child.at("kind").get<std::string>());
                        const long long offset = child.at("offset").get<long long>();
                        const long long count = child.at("count").get<long long>();
                        const auto group = inventory.find(kind);
                        const std::size_t inventorySize =
                            group == inventory.end() ? 0U : group->second.size();
                        if (static_cast<unsigned long long>(offset + count) > inventorySize) {
                            fail("child_range_exceeds_inventory:" + snapshot.id);
                        }
                        if (count > 0) {
                            const std::string prefix = kind == "vertex" ? "Vertex"
                                : kind == "edge"                 ? "Edge"
                                : kind == "face"                 ? "Face"
                                                                  : std::string {};
                            if (prefix.empty()
                                || child.value("targetStart", "")
                                    != prefix + std::to_string(offset + 1)
                                || child.value("targetEnd", "")
                                    != prefix + std::to_string(offset + count)) {
                                fail("child_range_target_mismatch:" + snapshot.id);
                            }
                        }
                        const std::string nestedSnapshot =
                            child.value("nestedSnapshot", "");
                        if (nestedSnapshot.empty()
                            || child.value("nestedSnapshotStatus", "") != "published"
                            || !contains(snapshots_, nestedSnapshot)
                            || std::find(
                                   snapshot.nestedSnapshotRefs.begin(),
                                   snapshot.nestedSnapshotRefs.end(),
                                   nestedSnapshot
                               ) == snapshot.nestedSnapshotRefs.end()) {
                            fail("child_range_nested_snapshot_missing:" + snapshot.id);
                        }
                        else {
                            std::set<long> nestedSids;
                            const SnapshotState& nestedLedger = snapshots_.at(nestedSnapshot);
                            for (const std::string& tableRef :
                                 nestedLedger.nestedSnapshotRefs) {
                                const auto table = snapshots_.find(tableRef);
                                if (table != snapshots_.end()) {
                                    nestedSids.insert(
                                        table->second.definedSids.begin(),
                                        table->second.definedSids.end()
                                    );
                                }
                            }
                            std::vector<long> childRefs;
                            const auto refs = child.find("sourceElementIdRefs");
                            if (refs != child.end()) {
                                appendSidRefValues(*refs, childRefs);
                            }
                            for (const long sid : childRefs) {
                                if (sid > 0 && nestedSids.count(sid) == 0U) {
                                    fail("unknown_child_range_sid:" + snapshot.id + ":"
                                         + std::to_string(sid));
                                }
                            }
                        }
                    }
                }
            }
            if (!snapshot.payload.contains("canonicalCollisions")
                || !snapshot.payload.at("canonicalCollisions").is_array()) {
                fail("ledger_collision_inventory_missing:" + snapshot.id);
            }
        }
        if (snapshot.payload.is_object() && snapshot.payload.contains("entries")
            && snapshot.payload.at("entries").is_object()) {
            const IndexedInventory inventory = snapshot.payload.contains("shape")
                ? indexedInventory(snapshot.payload.at("shape"))
                : IndexedInventory {};
            std::set<long> nestedSids;
            for (const std::string& nested : snapshot.nestedSnapshotRefs) {
                const auto nestedSnapshot = snapshots_.find(nested);
                if (nestedSnapshot == snapshots_.end()) {
                    continue;
                }
                nestedSids.insert(nestedSnapshot->second.definedSids.begin(),
                                  nestedSnapshot->second.definedSids.end());
            }
            for (const auto& entryGroup : snapshot.payload.at("entries").items()) {
                const std::string name = lowerAscii(entryGroup.key());
                const std::string kind = indexedKind(name);
                const auto inventoryGroup = inventory.find(kind);
                if (kind.empty() || inventoryGroup == inventory.end()
                    || inventoryGroup->second.count(name) == 0U) {
                    fail("ledger_entry_absent_from_inventory:" + snapshot.id + ":"
                         + entryGroup.key());
                }
                if (!entryGroup.value().is_array()) {
                    fail("invalid_ledger_entry_collection:" + snapshot.id + ":"
                         + entryGroup.key());
                    continue;
                }
                for (const auto& entry : entryGroup.value()) {
                    if (!entry.is_object()) {
                        fail("invalid_ledger_entry:" + snapshot.id + ":" + entryGroup.key());
                        continue;
                    }
                    std::vector<long> refs;
                    const auto elementRefs = entry.find("elementIdRefs");
                    if (elementRefs != entry.end()) {
                        appendSidRefValues(*elementRefs, refs);
                    }
                    for (const long sid : refs) {
                        if (sid > 0 && nestedSids.count(sid) == 0U) {
                            fail("unknown_ledger_entry_sid:" + snapshot.id + ":"
                                 + std::to_string(sid));
                        }
                    }
                }
            }
        }
        if (snapshot.payload.is_object() && snapshot.payload.contains("raw")
            && snapshot.payload.at("raw").is_object()) {
            const auto& raw = snapshot.payload.at("raw");
            const IndexedInventory output = raw.contains("output")
                ? indexedInventory(raw.at("output"))
                : IndexedInventory {};
            std::map<std::size_t, IndexedInventory> inputs;
            if (!raw.contains("inputs") || !raw.at("inputs").is_array()) {
                fail("mapper_inputs_missing:" + snapshot.id);
            }
            else {
                for (const auto& input : raw.at("inputs")) {
                    if (!input.is_object() || !input.contains("sourceOrdinal")
                        || !input.at("sourceOrdinal").is_number_unsigned()
                        || !input.contains("inventory")) {
                        fail("invalid_mapper_input:" + snapshot.id);
                        continue;
                    }
                    inputs[input.at("sourceOrdinal").get<std::size_t>()] =
                        indexedInventory(input.at("inventory"));
                }
            }
            if (!raw.contains("sources") || !raw.at("sources").is_array()) {
                fail("mapper_sources_missing:" + snapshot.id);
            }
            else {
                for (const auto& source : raw.at("sources")) {
                    if (!source.is_object() || !source.contains("sourceOrdinal")
                        || !source.at("sourceOrdinal").is_number_unsigned()) {
                        fail("invalid_mapper_source:" + snapshot.id);
                        continue;
                    }
                    const std::size_t ordinal =
                        source.at("sourceOrdinal").get<std::size_t>();
                    const std::string sourceKind = lowerAscii(source.value("sourceShapeType", ""));
                    const std::string sourceIndexed = lowerAscii(source.value("sourceIndexed", ""));
                    const auto input = inputs.find(ordinal);
                    const auto inputGroup = input == inputs.end()
                        ? IndexedInventory::const_iterator {}
                        : input->second.find(sourceKind);
                    if (input == inputs.end() || inputGroup == input->second.end()
                        || inputGroup->second.count(sourceIndexed) == 0U) {
                        fail("mapper_source_absent_from_input:" + snapshot.id + ":"
                             + sourceIndexed);
                    }
                    for (const std::string relation : {"modified", "generated"}) {
                        const auto targets = source.find(relation);
                        if (targets == source.end() || !targets->is_array()) {
                            continue;
                        }
                        for (const auto& target : *targets) {
                            const std::string targetIndexed = target.is_object()
                                ? lowerAscii(target.value("indexed", ""))
                                : std::string {};
                            const std::string targetKind = indexedKind(targetIndexed);
                            const auto outputGroup = output.find(targetKind);
                            if (!targetIndexed.empty()
                                && (outputGroup == output.end()
                                    || outputGroup->second.count(targetIndexed) == 0U)) {
                                fail("mapper_target_absent_from_output:" + snapshot.id + ":"
                                     + targetIndexed);
                            }
                        }
                    }
                }
            }
        }
        if (snapshot.publishedEvent != 0) {
            orderedSnapshots.push_back(&snapshot);
        }
    }
    std::sort(
        orderedSnapshots.begin(),
        orderedSnapshots.end(),
        [](const SnapshotState* left, const SnapshotState* right) {
            return left->publishedEvent < right->publishedEvent;
        }
    );
    std::uint64_t snapshotTransaction = 0;
    for (const SnapshotState* snapshot : orderedSnapshots) {
        const std::uint64_t transaction = snapshot->publishedEvent > 0
                && snapshot->publishedEvent <= events_.size()
            ? events_.at(static_cast<std::size_t>(snapshot->publishedEvent - 1)).transaction
            : 0;
        if (transaction != snapshotTransaction) {
            knownSids.clear();
            snapshotTransaction = transaction;
        }
        knownSids.insert(snapshot->definedSids.begin(), snapshot->definedSids.end());
        for (const SidRef& ref : snapshot->sidRefs) {
            if (ref.value > 0 && knownSids.count(ref.value) == 0U) {
                fail("unknown_snapshot_sid:" + snapshot->id + ":" + std::to_string(ref.value));
            }
        }
    }
    return result;
}

nlohmann::json ElementMapProducerTrace::document(
    const ProducerMetadata& metadata,
    const std::vector<ObjectInfo>& suppliedObjects
) const
{
    nlohmann::json trace = {
        {"schemaVersion", "freecad.element-map-producer-trace.v1"},
        {"producer",
         {{"name", metadata.name},
          {"document", metadata.document},
          {"build", metadata.build},
          {"inputSha256", metadata.inputSha256},
          {"responseSha256", metadata.responseSha256}}},
        {"transactions", nlohmann::json::array()},
        {"objectTagIndex", nlohmann::json::object()},
        {"objects", nlohmann::json::object()},
        {"events", nlohmann::json::array()},
        {"stringTableSnapshots", nlohmann::json::object()},
        {"ledgerSnapshots", nlohmann::json::object()},
        {"mapperSnapshots", nlohmann::json::object()},
    };

    for (const auto& item : transactions_) {
        const TransactionState& transaction = item.second;
        trace["transactions"].push_back({
            {"sequence", transaction.sequence},
            {"targets", transaction.targets},
            {"outcome", transaction.outcome},
            {"detail", transaction.detail},
            {"eventRange", {transaction.beginEvent, transaction.endEvent}},
            {"fields", transaction.fields},
        });
    }

    std::map<long, ObjectInfo> objectByTag;
    for (const ObjectInfo& object : suppliedObjects) {
        if (object.tag == 0) {
            continue;
        }
        const auto inserted = objectByTag.emplace(object.tag, object);
        if (!inserted.second
            && std::tie(inserted.first->second.object, inserted.first->second.typeId)
                != std::tie(object.object, object.typeId)) {
            throw std::runtime_error("ElementMap producer trace objectTag is not unique");
        }
    }
    for (const EventState& event : events_) {
        if (event.objectTag == 0) {
            continue;
        }
        ObjectInfo observed {event.objectTag,
                             event.object.empty()
                                 ? "@transient:" + std::to_string(event.objectTag)
                                 : event.object,
                             event.producer.empty() ? "transient" : event.producer};
        const auto inserted = objectByTag.emplace(event.objectTag, observed);
        if (!inserted.second && !event.object.empty()
            && inserted.first->second.object != event.object) {
            throw std::runtime_error("ElementMap producer trace event objectTag owner mismatch");
        }
    }
    for (const auto& item : objectByTag) {
        const ObjectInfo& object = item.second;
        trace["objectTagIndex"][std::to_string(object.tag)] = {
            {"object", object.object},
            {"typeId", object.typeId},
        };
        trace["objects"][object.object] = {
            {"tag", object.tag},
            {"typeId", object.typeId},
            {"slices", nlohmann::json::array()},
        };
    }

    for (const EventState& event : events_) {
        nlohmann::json value = {
            {"sequence", event.sequence},
            {"transactionSequence", event.transaction},
            {"scopeSequence", event.scope},
            {"parentScopeSequence", event.parentScope},
            {"object", event.object},
            {"objectTag", event.objectTag},
            {"producer", event.producer},
            {"slice", event.slice},
            {"decision", event.decision},
            {"reason", event.reason},
            {"beforeSnapshot", event.beforeSnapshot},
            {"afterSnapshot", event.afterSnapshot},
            {"fields", event.fields},
        };
        if (!event.nondeterminismClass.empty()) {
            value["nondeterminismClass"] = event.nondeterminismClass;
            value["stableComparisonKey"] = event.stableComparisonKey;
        }
        trace["events"].push_back(std::move(value));
        if (event.objectTag != 0) {
            const std::string owner = objectByTag.at(event.objectTag).object;
            trace["objects"][owner]["slices"].push_back(event.sequence);
        }
    }

    for (const auto& item : snapshots_) {
        const SnapshotState& snapshot = item.second;
        nlohmann::json value = {
            {"kind", snapshot.kind},
            {"sha256", snapshot.hash},
            {"payload", snapshot.payload},
            {"sidRefs", nlohmann::json::array()},
            {"definedSids", snapshot.definedSids},
            {"nestedSnapshotRefs", snapshot.nestedSnapshotRefs},
            {"label", snapshot.label},
        };
        for (const SidRef& ref : snapshot.sidRefs) {
            value["sidRefs"].push_back(sidRefJson(ref));
        }
        nlohmann::json* destination = &trace["ledgerSnapshots"];
        if (snapshot.kind == "stringTable") {
            destination = &trace["stringTableSnapshots"];
        }
        else if (snapshot.kind == "mapper") {
            destination = &trace["mapperSnapshots"];
        }
        (*destination)[snapshot.id] = std::move(value);
    }
    return trace;
}

nlohmann::json ElementMapProducerTrace::drain(const ProducerMetadata& metadata,
                                              const std::vector<ObjectInfo>& objects)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const ValidationResult validation = validate();
    if (!validation) {
        std::string message = "ElementMap producer trace closure validation failed";
        for (const std::string& error : validation.errors) {
            message += ":" + error;
        }
        throw std::runtime_error(message);
    }
    nlohmann::json result = document(metadata, objects);
    reset();
    return result;
}

bool ElementMapProducerTrace::empty() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return events_.empty() && transactions_.empty() && scopes_.empty();
}

std::string ElementMapProducerTrace::canonicalSha256(const nlohmann::json& value)
{
    return part::sha256Hex(value.dump());
}

void ElementMapProducerTrace::reset()
{
    nextEvent_ = 0;
    nextScope_ = 0;
    nextTransaction_ = 0;
    activeTransaction_ = 0;
    nextIdentity_ = 0;
    scopeStack_.clear();
    events_.clear();
    scopes_.clear();
    transactions_.clear();
    snapshots_.clear();
    identities_.clear();
    recording_ = false;
    emittedReentrancyDiagnostic_ = false;

    SnapshotState initial;
    initial.kind = "state";
    initial.payload = nlohmann::json::object();
    initial.hash = canonicalSha256(initial.payload);
    initial.id = initial.kind + ":sha256:" + initial.hash;
    initial.label = "initial";
    currentSnapshot_ = initial.id;
    snapshots_.emplace(initial.id, std::move(initial));
}

}  // namespace cad_core::app
