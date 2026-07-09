#include "cad_core/runtime/topo_naming_state_probe.h"

#include "cad_core/app/property.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRepPrimAPI_MakeBox.hxx>

#include <string>
#include <utility>

namespace cad_core::runtime {

namespace {

part::MappedNameProvenance directMappedNameProvenance(const std::string& token,
                                                      const std::string& currentElement,
                                                      const std::string& sourceElement,
                                                      const std::string& elementType)
{
    part::MappedNameProvenance provenance;
    provenance.entryKey = token;
    provenance.currentElement = currentElement;
    provenance.sourceElement = sourceElement;
    provenance.elementType = elementType;
    provenance.rawMappedName = token;
    provenance.canonicalMappedName = token;
    provenance.status = part::MappedNameProvenanceStatus::SourceBacked;
    return provenance;
}

part::MapperHistoryEvent probeEvent(const std::string& objectName,
                                    const std::string& eventId,
                                    const std::string& sourceSubname,
                                    const std::string& targetSubname,
                                    const std::string& shapeKind,
                                    part::MapperHistoryRelation relation,
                                    part::MapperHistoryRecoverability recoverability,
                                    const std::string& probeCase,
                                    const std::string& diagnosticStatus = {})
{
    part::MapperHistoryEvent event;
    event.id = objectName + "." + eventId;
    event.source = {"Source", sourceSubname};
    event.target = {objectName, targetSubname};
    event.shapeKind = shapeKind;
    event.relation = relation;
    event.makerStage = "TopoNamingStateProbe";
    event.evidence = {{"probeCase", probeCase}};
    event.recoverability = recoverability;
    event.diagnosticStatus = diagnosticStatus;
    return event;
}

void addProbeDiagnostics(const app::DocumentObject& object,
                         const std::string& probeCase,
                         ComputeContext& context)
{
    addDiagnostic(context.diagnostics,
                  "info",
                  "unsupported_native_mapper_history",
                  "FreeCAD Python does not expose this fixture's producer history; "
                  "CadCore::TopoNamingStateProbe records the CAD Core mapperHistory DTO contract",
                  object.name,
                  {},
                  "topoNamingState.mapperHistory");
    addDiagnostic(context.diagnostics,
                  "warning",
                  "split_stable_subname",
                  "Stable subname Source.#e:2;MHS,E was split by mapper history and requires reselect",
                  object.name,
                  {},
                  "topoNamingState.mapperHistory");
    addDiagnostic(context.diagnostics,
                  "warning",
                  "deleted_stable_subname",
                  "Stable subname Source.#f:2;MHS,F was deleted by mapper history",
                  object.name,
                  {},
                  "topoNamingState.mapperHistory");
    addDiagnostic(context.diagnostics,
                  "warning",
                  "stable_identity_ambiguous",
                  "Stable subname Source.#f:3;MHS,F is ambiguous in mapper history and requires reselect",
                  object.name,
                  {},
                  "topoNamingState.mapperHistory");
    (void)probeCase;
}

}  // namespace

void executeTopoNamingStateProbe(const app::DocumentObject& object, ComputeContext& context)
{
    if (!rejectUnsupportedProperties(object, context, {"ProbeCase"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    const std::string probeCase = app::readString(object, "ProbeCase").value_or("");
    if (probeCase != "mapperHistory generated modified split deleted ambiguous"
        && probeCase != "mapperHistory generated modified split deleted merge ambiguous") {
        addDiagnostic(context.diagnostics,
                      "error",
                      "unsupported_probe_case",
                      "Unsupported topoNamingState probe case " + probeCase,
                      object.name,
                      "ProbeCase");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    TopoDS_Shape shape = BRepPrimAPI_MakeBox(1.0, 1.0, 1.0).Shape();
    part::NamedShape namedShape = part::indexedNamedShapeForObject(object.name, shape);
    namedShape.elementMap["Source.#f:1;MHS,F"] = "Face1";
    namedShape.elementMap["Source.#e:1;MHS,E"] = "Edge1";
    namedShape.elementMap["Source.#f:4;MHS,F"] = "Face1";
    namedShape.mappedNameProvenance["Source.#f:1;MHS,F"] =
        directMappedNameProvenance("Source.#f:1;MHS,F", "Face1", "Source.Face1", "Face");
    namedShape.mappedNameProvenance["Source.#e:1;MHS,E"] =
        directMappedNameProvenance("Source.#e:1;MHS,E", "Edge1", "Source.Edge1", "Edge");
    namedShape.mappedNameProvenance["Source.#f:4;MHS,F"] =
        directMappedNameProvenance("Source.#f:4;MHS,F", "Face1", "Source.Face4", "Face");

    namedShape.mapperHistory.push_back(probeEvent(object.name,
                                                  "mh-generated-face1",
                                                  "Face1",
                                                  "Face1",
                                                  "face",
                                                  part::MapperHistoryRelation::Generated,
                                                  part::MapperHistoryRecoverability::Resolved,
                                                  probeCase));
    namedShape.mapperHistory.push_back(probeEvent(object.name,
                                                  "mh-modified-edge1",
                                                  "Edge1",
                                                  "Edge1",
                                                  "edge",
                                                  part::MapperHistoryRelation::Modified,
                                                  part::MapperHistoryRecoverability::Resolved,
                                                  probeCase));
    namedShape.mapperHistory.push_back(probeEvent(object.name,
                                                  "mh-split-edge2-a",
                                                  "Edge2",
                                                  "Edge2",
                                                  "edge",
                                                  part::MapperHistoryRelation::Split,
                                                  part::MapperHistoryRecoverability::NeedsReselect,
                                                  probeCase,
                                                  "split_stable_subname"));
    namedShape.mapperHistory.push_back(probeEvent(object.name,
                                                  "mh-split-edge2-b",
                                                  "Edge2",
                                                  "Edge3",
                                                  "edge",
                                                  part::MapperHistoryRelation::Split,
                                                  part::MapperHistoryRecoverability::NeedsReselect,
                                                  probeCase,
                                                  "split_stable_subname"));
    namedShape.mapperHistory.push_back(probeEvent(object.name,
                                                  "mh-deleted-face2",
                                                  "Face2",
                                                  "",
                                                  "face",
                                                  part::MapperHistoryRelation::Deleted,
                                                  part::MapperHistoryRecoverability::Deleted,
                                                  probeCase,
                                                  "deleted_stable_subname"));
    namedShape.mapperHistory.push_back(probeEvent(object.name,
                                                  "mh-merge-face4",
                                                  "Face4",
                                                  "Face1",
                                                  "face",
                                                  part::MapperHistoryRelation::Merge,
                                                  part::MapperHistoryRecoverability::Resolved,
                                                  probeCase));
    namedShape.mapperHistory.push_back(probeEvent(object.name,
                                                  "mh-ambiguous-face3",
                                                  "Face3",
                                                  "",
                                                  "face",
                                                  part::MapperHistoryRelation::Ambiguous,
                                                  part::MapperHistoryRecoverability::Ambiguous,
                                                  probeCase,
                                                  "stable_identity_ambiguous"));
    namedShape.elementHistoryStatus.push_back("topo_naming_state_probe:mapper_history_contract");

    ShapeValue value {ShapeValue::Kind::Solid, shape};
    context.shapes[object.name] = value;
    context.mesh[object.name] = part::meshForShape(shape, "Face", "Edge", "Vertex", context.displayMeshDeflection);
    context.subshapes[object.name] = part::subshapeMapForShape(shape);
    context.namedShapes[object.name] = std::move(namedShape);
    context.objects[object.name] = {
        {"status", "ok"},
        {"probe", "topo_naming_state_mapper_history"},
        {"probeCase", probeCase},
        {"shape", "occt_solid"},
        {"bbox", part::objectBBoxForShape(shape)},
        {"volume", part::volumeForShape(shape)},
        {"kernel", part::kernelVersion()},
    };
    addProbeDiagnostics(object, probeCase, context);
}

}  // namespace cad_core::runtime
