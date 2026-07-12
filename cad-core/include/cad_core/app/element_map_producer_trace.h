#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <exception>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::app
{

// FreeCAD authority:
// /Users/li/Chili3DProject/FreeCAD2/src/App/ElementMapProducerTrace.h
// ::App::ElementMapProducerTrace says "The trace deliberately owns only values" and explicitly
// excludes StringIDRef, MappedNameRef, TopoDS_Shape, DocumentObject and ElementMap.
// /Users/li/Chili3DProject/FreeCAD2/src/App/ElementMapProducerTrace.cpp
// ::beginTransaction()/beginScope() provide the move-only RAII transaction/scope contract.
// CAD Core keeps the same value and drain semantics, with one recorder owner per parsed request.
class ElementMapProducerTrace
{
public:
    struct SidRef
    {
        long value = 0;
        int index = 0;
    };

    struct TransactionDescriptor
    {
        std::vector<std::string> targets;
        nlohmann::json fields = nlohmann::json::object();
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/Document.cpp
        // ::Document::recompute() executes the list returned by
        // "getDependencyList(..., DepSort | options)". Publish that complete planned list so a
        // consumer can audit execution scope without reconstructing it from producer scopes.
        std::optional<std::vector<std::string>> effectiveTargets;
    };

    struct ScopeDescriptor
    {
        std::string stage;
        std::string object;
        long objectTag = 0;
        std::string producer;
        nlohmann::json fields = nlohmann::json::object();
    };

    struct EventValue
    {
        std::string slice;
        std::string decision;
        std::string reason;
        nlohmann::json fields = nlohmann::json::object();
        std::string beforeSnapshot;
        std::string afterSnapshot;
        std::string nondeterminismClass;
        std::string stableComparisonKey;
    };

    struct SnapshotValue
    {
        std::string kind;
        nlohmann::json payload = nlohmann::json::object();
        std::vector<SidRef> sidRefs;
        std::vector<long> definedSids;
        std::vector<std::string> nestedSnapshotRefs;
        std::string label = "checkpoint";
        bool republish = false;
    };

    struct ProducerMetadata
    {
        std::string name = "CADCore";
        std::string document;
        std::string build;
        std::string inputSha256;
        std::string responseSha256;
    };

    struct ObjectInfo
    {
        long tag = 0;
        std::string object;
        std::string typeId;
    };

    struct ValidationResult
    {
        bool valid = true;
        std::vector<std::string> errors;
        explicit operator bool() const noexcept { return valid; }
    };

    class Scope
    {
    public:
        Scope() = default;
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        Scope(Scope&& other) noexcept;
        Scope& operator=(Scope&& other) noexcept;
        ~Scope();

        void success(std::string detail = {});
        void exception(std::string detail);
        void cancel(std::string detail);
        void abort(std::string detail);

    private:
        friend class ElementMapProducerTrace;
        Scope(ElementMapProducerTrace* trace, std::uint64_t sequence);
        void close() noexcept;

        ElementMapProducerTrace* trace_ = nullptr;
        std::uint64_t sequence_ = 0;
        int uncaughtExceptions_ = 0;
        std::string outcome_ = "success";
        std::string detail_;
    };

    class Transaction
    {
    public:
        Transaction() = default;
        Transaction(const Transaction&) = delete;
        Transaction& operator=(const Transaction&) = delete;
        Transaction(Transaction&& other) noexcept;
        Transaction& operator=(Transaction&& other) noexcept;
        ~Transaction();

        void success(std::string detail = {});
        void exception(std::string detail);
        void cancel(std::string detail);
        void abort(std::string detail);

    private:
        friend class ElementMapProducerTrace;
        Transaction(ElementMapProducerTrace* trace, std::uint64_t sequence);
        void close() noexcept;

        ElementMapProducerTrace* trace_ = nullptr;
        std::uint64_t sequence_ = 0;
        int uncaughtExceptions_ = 0;
        std::string outcome_ = "success";
        std::string detail_;
    };

    ElementMapProducerTrace();
    ~ElementMapProducerTrace();
    ElementMapProducerTrace(const ElementMapProducerTrace&) = delete;
    ElementMapProducerTrace& operator=(const ElementMapProducerTrace&) = delete;

    Transaction beginTransaction(TransactionDescriptor descriptor);
    Scope scope(ScopeDescriptor descriptor);
    void record(EventValue value);
    std::string checkpoint(SnapshotValue value);
    std::string firstSeenIdentity(const std::string& kind,
                                  const std::string& role,
                                  const std::string& relation = "create",
                                  const std::string& relatedIdentity = {});

    ValidationResult validate() const;
    nlohmann::json drain(const ProducerMetadata& metadata,
                         const std::vector<ObjectInfo>& objects = {});
    bool empty() const;
    const std::string& currentSnapshotId() const noexcept;
    static std::string canonicalSha256(const nlohmann::json& value);

private:
    struct EventState;
    struct ScopeState;
    struct TransactionState;
    struct SnapshotState;

    std::uint64_t beginScope(ScopeDescriptor descriptor);
    void endScope(std::uint64_t sequence,
                  const std::string& outcome,
                  const std::string& detail) noexcept;
    void endTransaction(std::uint64_t sequence,
                        const std::string& outcome,
                        const std::string& detail) noexcept;
    void appendEvent(EventValue value);
    void appendReentrancyDiagnostic();
    nlohmann::json document(const ProducerMetadata& metadata,
                            const std::vector<ObjectInfo>& objects) const;
    void reset();

    mutable std::recursive_mutex mutex_;
    std::uint64_t nextEvent_ = 0;
    std::uint64_t nextScope_ = 0;
    std::uint64_t nextTransaction_ = 0;
    std::uint64_t activeTransaction_ = 0;
    std::uint64_t nextIdentity_ = 0;
    std::vector<std::uint64_t> scopeStack_;
    std::vector<EventState> events_;
    std::map<std::uint64_t, ScopeState> scopes_;
    std::map<std::uint64_t, TransactionState> transactions_;
    std::map<std::string, SnapshotState> snapshots_;
    std::map<std::string, std::string> identities_;
    std::string currentSnapshot_;
    bool recording_ = false;
    bool emittedReentrancyDiagnostic_ = false;
};

}  // namespace cad_core::app
