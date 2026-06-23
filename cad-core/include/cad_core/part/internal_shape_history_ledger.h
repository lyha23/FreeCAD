#pragma once

// Part-layer InternalShape history ledger aligned with FreeCAD
// /home/user/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
// ::SketchObject::buildInternals(), which publishes a request-local "InternalShape" from
// FaceMakerBuildFace and WireJoiner open-wire results.
#include "cad_core/part/face_maker.h"

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <cstddef>
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

    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMaker.cpp
    // ::FaceMaker::postBuild(), consumes "MapperHistory(myPreSplitHistory)" and
    // "MapperMaker(mySplitter)" before generated InternalShape naming.
    void addFaceMakerEvidence(const FaceMakerHistorySummary& history);
    void merge(const InternalShapeHistoryLedger& other);

    bool empty() const;
    const std::vector<InternalShapeHistoryEvent>& historyEvents() const;

    nlohmann::json internalShapeHistoryJson() const;
    nlohmann::json compatibilityObjectFields() const;
    nlohmann::json sketchInternalHistoryCompatibilityJson() const;
    std::optional<std::string> sketchInternalHistoryStatus() const;

private:
    std::unique_ptr<InternalShapeHistoryLedgerData> data_;

    friend InternalShapeHistoryLedgerData& mutableInternalShapeHistoryLedgerData(
        InternalShapeHistoryLedger& ledger
    );
    friend const InternalShapeHistoryLedgerData& internalShapeHistoryLedgerData(
        const InternalShapeHistoryLedger& ledger
    );
};

InternalShapeHistoryLedger mergeInternalShapeHistory(
    const std::optional<FaceMakerHistorySummary>& faceMaker,
    const std::optional<InternalShapeHistoryLedger>& wireJoiner
);

const char* internalShapeHistoryRelationName(InternalShapeHistoryRelation relation);
const char* internalShapeHistoryProducerName(InternalShapeHistoryProducer producer);
const char* internalShapeHistoryTargetKindName(InternalShapeHistoryTargetKind targetKind);

}  // namespace cad_core::part
