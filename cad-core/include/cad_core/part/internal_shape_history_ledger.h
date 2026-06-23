#pragma once

// Part-layer InternalShape history ledger aligned with FreeCAD
// /home/user/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
// ::SketchObject::buildInternals(), which publishes a request-local "InternalShape" from
// FaceMakerBuildFace and WireJoiner open-wire results.
#include <TopoDS_Shape.hxx>
#include "cad_core/part/topo_shape_mapper.h"
#include <nlohmann/json.hpp>

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part
{

enum class InternalShapeHistoryRelation
{
    Preserved,
    Generated,
    Modified,
    Deleted,
    Split,
    DiagnosticOnly,
};

enum class InternalShapeHistoryProducer
{
    FaceMakerBuildFace,
    WireJoinerOpenWires,
};

enum class InternalShapeHistoryTargetKind
{
    Shape,
    Face,
    Edge,
    Vertex,
    Wire,
};

struct InternalShapeHistoryEvent
{
    InternalShapeHistoryRelation relation = InternalShapeHistoryRelation::DiagnosticOnly;
    InternalShapeHistoryProducer producer = InternalShapeHistoryProducer::FaceMakerBuildFace;
    InternalShapeHistoryTargetKind targetKind = InternalShapeHistoryTargetKind::Shape;
    std::string stage;
    std::string diagnosticCode;
    std::vector<std::size_t> sourceEdgeIndices;
    TopoDS_Shape targetShape;
};

struct InternalShapePublishedElementHistory
{
    InternalShapeHistoryRelation relation = InternalShapeHistoryRelation::DiagnosticOnly;
    std::string element;
    std::vector<std::string> sources;
};

struct InternalShapeHistoryPublishInput
{
    std::string owner;
    TopoDS_Shape rawShape;
    TopoDS_Shape internalShape;
    nlohmann::json internalElementMap = nlohmann::json::object();
};

struct InternalShapeHistoryPublication
{
    std::map<std::string, std::string> elementMapAliases;
    std::vector<InternalShapePublishedElementHistory> elementHistory;
    std::vector<MapperHistoryEvent> mapperHistory;
    std::vector<std::string> elementHistoryStatus;
    nlohmann::json diagnostics = nlohmann::json::object();
};

struct InternalShapeHistoryLedgerData;

class InternalShapeHistoryLedger
{
public:
    InternalShapeHistoryLedger();
    ~InternalShapeHistoryLedger();
    InternalShapeHistoryLedger(const InternalShapeHistoryLedger& other);
    InternalShapeHistoryLedger& operator=(const InternalShapeHistoryLedger& other);
    InternalShapeHistoryLedger(InternalShapeHistoryLedger&&) noexcept;
    InternalShapeHistoryLedger& operator=(InternalShapeHistoryLedger&&) noexcept;

    void merge(const InternalShapeHistoryLedger& other);

    bool empty() const;

    InternalShapeHistoryPublication publishForInternalShape(
        const InternalShapeHistoryPublishInput& input
    ) const;
    nlohmann::json diagnosticsJson() const;

private:
    std::unique_ptr<InternalShapeHistoryLedgerData> data_;

    friend InternalShapeHistoryLedgerData& mutableInternalShapeHistoryLedgerData(
        InternalShapeHistoryLedger& ledger
    );
    friend const InternalShapeHistoryLedgerData& internalShapeHistoryLedgerData(
        const InternalShapeHistoryLedger& ledger
    );
};

const char* internalShapeHistoryRelationName(InternalShapeHistoryRelation relation);
const char* internalShapeHistoryProducerName(InternalShapeHistoryProducer producer);
const char* internalShapeHistoryTargetKindName(InternalShapeHistoryTargetKind targetKind);

}  // namespace cad_core::part
