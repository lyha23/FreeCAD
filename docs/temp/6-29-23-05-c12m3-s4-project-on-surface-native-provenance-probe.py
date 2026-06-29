#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import traceback
from pathlib import Path
from typing import Any


PAYLOAD_PREFIX = "C12M3_PROBE_PAYLOAD="
WRAPPER_SCHEMA_VERSION = "c12m2.native-probe-artifact.v1"
SUMMARY_SCHEMA_VERSION = "c12m3.native-provenance-summary.v1"


SOURCE_AUTHORITY = (
    "src/Mod/Part/App/FeatureProjectOnSurface.cpp::Part::ProjectOnSurface::tryExecute(),"
    "getProjectionShapes(),createProjectedWire(),projectWire(),projectFace(),"
    "createSolidIfHeight(),createCompound(),getOffsetPlacement(); "
    "src/Mod/Part/App/TopoShapePyImp.cpp::Part::TopoShapePy::getElementHistory(),"
    "mapShapes(),mapSubElement(); "
    "src/Mod/Part/App/TopoShapeExpansion.cpp::Part::TopoShape::mapSubElement(),"
    "mapCompoundSubElements(),makeShapeWithElementMap(),Part::MapperHistory; "
    "src/Mod/Part/App/PropertyTopoShape.cpp::Part::PropertyPartShape::setValue(),"
    "beforeSave(),Save(),RestoreDocFile()"
)


def discover_freecadcmd() -> str:
    for name in ("freecadcmd", "FreeCADCmd", "freecadcmd-daily"):
        path = shutil.which(name)
        if path:
            return path
    return ""


def freecad_command(freecadcmd: str, script: Path) -> list[str]:
    script_path = str(script.resolve())
    argv = json.dumps([script_path, "--inside-freecad"], ensure_ascii=False)
    code = (
        "import sys; "
        f"sys.argv = {argv}; "
        f"exec(compile(open({script_path!r}, encoding='utf-8').read(), {script.name!r}, 'exec'))"
    )
    return [freecadcmd, "-c", code]


def parse_payload(stdout: str) -> dict[str, Any] | None:
    index = stdout.rfind(PAYLOAD_PREFIX)
    if index < 0:
        return None
    value = stdout[index + len(PAYLOAD_PREFIX) :]
    try:
        parsed, _ = json.JSONDecoder().raw_decode(value)
    except json.JSONDecodeError:
        return None
    if isinstance(parsed, dict):
        return parsed
    return None


def process_classification(returncode: int, stdout: str, stderr: str, payload: dict[str, Any] | None) -> str:
    combined = f"{stdout}\n{stderr}"
    if returncode != 0:
        if "Incompatible processor" in combined or "Application unexpectedly terminated" in combined:
            return "sandbox_runtime_limit"
        if "Timeout after" in combined:
            return "sandbox_runtime_limit"
        return "collector_bug"
    if payload is None:
        return "collector_bug"
    return payload.get("c12m3_classification") or "collector_bug"


def default_runtime_summary(payload: dict[str, Any] | None) -> dict[str, Any]:
    payload = payload or {}
    return {
        "freecad_version": payload.get("freecad_version_string"),
        "freecad_version_tuple": payload.get("freecad_version"),
        "occt_version": payload.get("occt_version") or payload.get("config_occt_version"),
        "libpack": payload.get("libpack"),
        "libpack_version": payload.get("libpack_version"),
        "freecad_libs": payload.get("freecad_libs"),
        "app_home_path": payload.get("app_home_path"),
        "run_mode": payload.get("run_mode"),
    }


def blocked_summary(
    classification: str,
    runtime_summary: dict[str, Any],
    diagnostics: list[str],
) -> dict[str, Any]:
    return {
        "schema_version": SUMMARY_SCHEMA_VERSION,
        "artifact_kind": "project_on_surface_native_provenance_probe",
        "c12m3_classification": classification,
        "source_authority": SOURCE_AUTHORITY,
        "input_fixture_or_probe": {
            "probe_script": "docs/temp/6-29-23-05-c12m3-s4-project-on-surface-native-provenance-probe.py",
            "case_id": "c12m3_s4_project_on_surface_native_provenance",
        },
        "runtime_summary": runtime_summary,
        "result_shape_summary": {},
        "provenance_observations": [
            {
                "observation_id": "C12M3-S4-RUNTIME-001",
                "axis": "runtime",
                "source_endpoint": None,
                "target_endpoint": None,
                "history_api_name": "FreeCADCmd",
                "history_return_summary": "FreeCADCmd did not produce a usable C12-M3 payload.",
                "request_local_judgement": "not_evaluated",
                "classification": classification,
                "current_comparison_path": f"blocked: {classification}; no S5 comparison input",
            }
        ],
        "diagnostics": diagnostics,
        "non_evidence": [],
    }


def build_artifact(
    args: argparse.Namespace,
    command: list[str],
    result: subprocess.CompletedProcess[str],
    payload: dict[str, Any] | None,
) -> dict[str, Any]:
    classification = process_classification(result.returncode, result.stdout, result.stderr, payload)
    runtime_summary = default_runtime_summary(payload)
    expected_summary = (
        payload.get("expected_summary")
        if payload and isinstance(payload.get("expected_summary"), dict)
        else blocked_summary(
            classification,
            runtime_summary,
            [
                "S4 could not interpret the native ProjectOnSurface probe payload.",
                "This is collector/runtime status only and must not be treated as backend gap.",
            ],
        )
    )
    return {
        "schema_version": WRAPPER_SCHEMA_VERSION,
        "probe": {
            "id": "C12M3-S4-PROJECT-ON-SURFACE-NATIVE-PROVENANCE",
            "family": "project_on_surface_native_provenance",
            "case_id": "c12m3_s4_project_on_surface_native_provenance",
        },
        "source_authority": SOURCE_AUTHORITY,
        "input_artifact": "docs/temp/6-29-22-15-c12m3-native-provenance-probe-schema.md",
        "freecadcmd": {
            "path": args.freecadcmd or None,
            "version": runtime_summary.get("freecad_version"),
            "version_tuple": runtime_summary.get("freecad_version_tuple"),
            "occt_version": runtime_summary.get("occt_version"),
            "libpack": runtime_summary.get("libpack"),
            "libpack_version": runtime_summary.get("libpack_version"),
            "freecad_libs": runtime_summary.get("freecad_libs"),
            "app_home_path": runtime_summary.get("app_home_path"),
            "raw_probe_payload": payload,
        },
        "command": command,
        "process": {
            "exit_code": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr,
            "timeout_seconds": args.timeout,
        },
        "exception_classification": classification,
        "expected_summary": expected_summary,
        "request_local": {
            "judgement": "request_local_probe_completed"
            if result.returncode == 0 and payload is not None
            else "not_evaluated",
            "notes": "One FreeCADCmd request creates source/support ProjectOnSurface objects and inspects result/intermediate TopoShape history APIs.",
        },
        "current_comparison_path": expected_summary.get(
            "current_comparison_path",
            "blocked: no native_provenance_expected_ready row; S5 comparison blocked",
        ),
        "conclusion": classification,
        "notes": [
            "Geometry summaries in this artifact are non-provenance evidence.",
            "Only native history API source-to-target rows can feed S5.",
        ],
    }


def run_wrapper() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--out",
        default="docs/temp/6-29-23-05-c12m3-s4-project-on-surface-native-provenance-probe-output.json",
    )
    parser.add_argument("--freecadcmd", default=os.environ.get("FREECADCMD") or discover_freecadcmd())
    parser.add_argument("--timeout", type=int, default=60)
    args = parser.parse_args()

    command = freecad_command(args.freecadcmd, Path(__file__)) if args.freecadcmd else []
    if command:
        try:
            result = subprocess.run(
                command,
                cwd=Path.cwd(),
                text=True,
                capture_output=True,
                timeout=args.timeout,
                check=False,
            )
        except subprocess.TimeoutExpired as exc:
            result = subprocess.CompletedProcess(
                command,
                124,
                exc.stdout or "",
                (exc.stderr or "") + f"\nTimeout after {args.timeout}s",
            )
    else:
        result = subprocess.CompletedProcess([], 127, "", "FreeCADCmd not found")

    payload = parse_payload(result.stdout)
    artifact = build_artifact(args, command, result, payload)
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(artifact, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(artifact, ensure_ascii=False, indent=2, sort_keys=True))
    return 0 if artifact["conclusion"] != "collector_bug" else 1


def run_inside_freecad() -> int:
    import FreeCAD  # type: ignore
    import Part  # type: ignore

    def version_string() -> str:
        items = FreeCAD.Version()
        if len(items) >= 4:
            return f"{items[0]}.{items[1]}.{items[2]} revision {str(items[3]).split()[0]}"
        return " ".join(str(item) for item in items)

    def metadata() -> dict[str, Any]:
        return {
            "freecad_version": list(FreeCAD.Version()),
            "freecad_version_string": version_string(),
            "occt_version": getattr(Part, "OCC_VERSION", None),
            "config_occt_version": FreeCAD.ConfigGet("OCC_VERSION"),
            "libpack": FreeCAD.ConfigGet("LibPack"),
            "libpack_version": FreeCAD.ConfigGet("LibPackVersion"),
            "freecad_libs": FreeCAD.ConfigGet("FreeCADLibs"),
            "app_home_path": FreeCAD.getHomePath(),
            "run_mode": "Cmd",
        }

    def counts(shape: Any) -> dict[str, int]:
        return {
            "solids": len(getattr(shape, "Solids", [])),
            "shells": len(getattr(shape, "Shells", [])),
            "faces": len(getattr(shape, "Faces", [])),
            "wires": len(getattr(shape, "Wires", [])),
            "edges": len(getattr(shape, "Edges", [])),
            "vertices": len(getattr(shape, "Vertexes", [])),
        }

    def shape_summary(shape: Any) -> dict[str, Any]:
        if shape is None:
            return {"is_null": True, "reason": "shape is None"}
        try:
            if shape.isNull():
                return {"is_null": True}
        except Exception:
            pass
        bbox = shape.BoundBox
        return {
            "is_null": False,
            "shape_type": str(getattr(shape, "ShapeType", "")),
            "topology_counts": counts(shape),
            "bbox": {
                "xmin": round(float(bbox.XMin), 6),
                "ymin": round(float(bbox.YMin), 6),
                "zmin": round(float(bbox.ZMin), 6),
                "xmax": round(float(bbox.XMax), 6),
                "ymax": round(float(bbox.YMax), 6),
                "zmax": round(float(bbox.ZMax), 6),
            },
            "volume": round(float(getattr(shape, "Volume", 0.0)), 9),
            "area": round(float(getattr(shape, "Area", 0.0)), 9),
            "length": round(float(getattr(shape, "Length", 0.0)), 9),
        }

    def short_repr(value: Any) -> str:
        text = repr(value)
        return text if len(text) <= 600 else text[:597] + "..."

    def endpoint(obj: str, prop: str | None, subname: str | None, role: str) -> dict[str, Any]:
        return {
            "object": obj,
            "property": prop,
            "subname": subname,
            "shape_role": role,
        }

    def null_endpoint(reason: str) -> dict[str, Any]:
        return {"endpoint": None, "blocker_reason": reason}

    observations: list[dict[str, Any]] = []
    result_shapes: dict[str, Any] = {}
    diagnostics: list[str] = []
    non_evidence: list[str] = [
        "Object result shape topology counts, bbox, area, volume and length are geometry smoke evidence only.",
        "Projected EdgeN/WireN order and compound child order were not used as ownership evidence.",
        "Manual mapShapes mapper injection is rejected as ProjectOnSurface-native provenance evidence.",
        "PropertyPartShape ElementMap save/restore requires native document persistence and is product-boundary context only.",
    ]

    def add_observation(
        observation_id: str,
        axis: str,
        source_endpoint: Any,
        target_endpoint: Any,
        api: str,
        return_summary: Any,
        judgement: str,
        classification: str,
        comparison_path: str | None = None,
    ) -> None:
        observations.append(
            {
                "observation_id": observation_id,
                "axis": axis,
                "source_endpoint": source_endpoint,
                "target_endpoint": target_endpoint,
                "history_api_name": api,
                "history_return_summary": return_summary,
                "request_local_judgement": judgement,
                "classification": classification,
                "current_comparison_path": comparison_path
                or f"blocked: {classification}; no native_provenance_expected_ready row for S5",
            }
        )

    def summarize_get_element_history(shape: Any, names: list[str]) -> dict[str, Any]:
        rows: dict[str, Any] = {}
        for name in names:
            try:
                value = shape.getElementHistory(name)
                rows[name] = {
                    "return_type": type(value).__name__,
                    "repr": short_repr(value),
                    "is_none": value is None,
                }
            except Exception as exc:
                rows[name] = {
                    "error_class": type(exc).__name__,
                    "message": str(exc),
                }
        return rows

    def any_visible_history(summary: dict[str, Any]) -> bool:
        for value in summary.values():
            if value.get("error_class"):
                continue
            if not value.get("is_none") and value.get("repr") not in ("", "None"):
                return True
        return False

    def safe_doc(name: str):
        return FreeCAD.newDocument(name)

    def close_doc(doc: Any) -> None:
        try:
            FreeCAD.closeDocument(doc.Name)
        except Exception:
            pass

    def add_support_and_source(doc: Any, source_name: str = "ProjectionWireFace") -> tuple[Any, Any]:
        support = doc.addObject("Part::Plane", "SupportPlane")
        support.Length = 8
        support.Width = 5
        source = doc.addObject("Part::Plane", source_name)
        source.Length = 3
        source.Width = 2
        source.Placement = FreeCAD.Placement(FreeCAD.Vector(1, 1, -2), FreeCAD.Rotation())
        doc.recompute()
        return support, source

    def assign_project_on_surface(
        doc: Any,
        obj_name: str,
        support: Any,
        projection_source: Any,
        projection_subname: str,
        mode: str,
        height: float = 0.0,
        offset: float = 0.0,
    ) -> Any:
        projection = doc.addObject("Part::ProjectOnSurface", obj_name)
        projection.SupportFace = (support, ["Face1"])
        projection.Projection = [(projection_source, [projection_subname])]
        projection.Mode = mode
        projection.Height = height
        projection.Offset = offset
        projection.Direction = FreeCAD.Vector(0, 0, 1)
        doc.recompute()
        return projection

    def projected_intermediate(support: Any, source: Any, subshape_attr: str) -> dict[str, Any]:
        payload: dict[str, Any] = {"source_attr": subshape_attr}
        try:
            source_shape = getattr(source.Shape, subshape_attr)
        except Exception as exc:
            payload["source_attr_error"] = f"{type(exc).__name__}: {exc}"
            return payload
        try:
            projected = support.Shape.Face1.makeParallelProjection(source_shape, FreeCAD.Vector(0, 0, 1))
            payload["shape_summary"] = shape_summary(projected)
            payload["history"] = summarize_get_element_history(projected, ["Edge1", "Wire1", "Face1"])
        except Exception as exc:
            payload["projection_error"] = f"{type(exc).__name__}: {exc}"
        return payload

    def run_edge_wire_case() -> None:
        doc = safe_doc("C12M3_S4_EdgeWire")
        try:
            support, source = add_support_and_source(doc)
            projection = assign_project_on_surface(
                doc,
                "ProjectedEdgeWire",
                support,
                source,
                "Wire1",
                "Edges",
            )
            result_shapes["edge_wire_result"] = shape_summary(projection.Shape)
            history = summarize_get_element_history(
                projection.Shape,
                ["Edge1", "Edge2", "Edge3", "Edge4", "Wire1", "Face1"],
            )
            add_observation(
                "C12M3-S4-EDGEWIRE-001",
                "edge_wire_provenance",
                endpoint("ProjectionWireFace", "Projection", "Wire1", "source wire selected by LinkSubList"),
                endpoint("ProjectedEdgeWire", "Shape", "Edge1..Edge4/Wire1", "object result compound edges"),
                "TopoShapePy.getElementHistory",
                history,
                "request-local object result in one FreeCADCmd document",
                "native_hidden_retained",
            )
            if any_visible_history(history):
                diagnostics.append("edge_wire getElementHistory unexpectedly returned non-None data; manual review required before S5.")

            intermediate = projected_intermediate(support, source, "Wire1")
            add_observation(
                "C12M3-S4-EDGEWIRE-002",
                "edge_wire_provenance",
                endpoint("ProjectionWireFace", "Projection", "Wire1", "source wire projected through makeParallelProjection"),
                endpoint("SupportPlane.Face1", "intermediate", "projected Edge/Wire", "intermediate projected wire/edges"),
                "TopoShapePy.makeParallelProjection + getElementHistory",
                intermediate,
                "request-local intermediate shape; no persisted document cache",
                "native_hidden_retained",
            )

            source_history = summarize_get_element_history(source.Shape, ["Edge1", "Wire1", "Face1"])
            add_observation(
                "C12M3-S4-EDGEWIRE-003",
                "api_observability",
                endpoint("ProjectionWireFace", "Shape", "Edge1/Wire1/Face1", "source object shape"),
                endpoint("ProjectionWireFace", "Shape", "Edge1/Wire1/Face1", "source object shape history self-check"),
                "TopoShapePy.getElementHistory",
                source_history,
                "request-local source object shape self-check",
                "native_hidden_retained",
            )
        except Exception as exc:
            add_observation(
                "C12M3-S4-EDGEWIRE-ERROR",
                "edge_wire_provenance",
                endpoint("ProjectionWireFace", "Projection", "Wire1", "source wire selected by LinkSubList"),
                null_endpoint("edge/wire ProjectOnSurface case failed before stable result"),
                "Part::ProjectOnSurface",
                {"error_class": type(exc).__name__, "message": str(exc), "traceback_tail": traceback.format_exc().splitlines()[-5:]},
                "collector reached FreeCADCmd but not stable edge/wire result",
                "collector_bug",
            )
        finally:
            close_doc(doc)

    def run_face_case() -> None:
        doc = safe_doc("C12M3_S4_FaceRebuild")
        try:
            support, source = add_support_and_source(doc, "ProjectionFace")
            projection = assign_project_on_surface(
                doc,
                "ProjectedFaceRebuild",
                support,
                source,
                "Face1",
                "Faces",
            )
            result_shapes["face_rebuild_result"] = shape_summary(projection.Shape)
            history = summarize_get_element_history(
                projection.Shape,
                ["Face1", "Wire1", "Edge1", "Edge2", "Edge3", "Edge4"],
            )
            add_observation(
                "C12M3-S4-FACE-001",
                "face_rebuild_provenance",
                endpoint("ProjectionFace", "Projection", "Face1", "source face selected by LinkSubList"),
                endpoint("ProjectedFaceRebuild", "Shape", "Face1/Wire1/Edge1..Edge4", "rebuilt projected face result"),
                "TopoShapePy.getElementHistory",
                history,
                "request-local object result in one FreeCADCmd document",
                "native_hidden_retained",
            )
            intermediate = projected_intermediate(support, source, "Wire1")
            add_observation(
                "C12M3-S4-FACE-002",
                "face_rebuild_provenance",
                endpoint("ProjectionFace", "Projection", "Face1.Wire1", "source face outer wire projected before rebuild"),
                endpoint("SupportPlane.Face1", "intermediate", "projected wire then face rebuild", "intermediate projected wire/face context"),
                "TopoShapePy.makeParallelProjection + getElementHistory",
                intermediate,
                "request-local intermediate shape; no persisted document cache",
                "native_hidden_retained",
            )
        except Exception as exc:
            add_observation(
                "C12M3-S4-FACE-ERROR",
                "face_rebuild_provenance",
                endpoint("ProjectionFace", "Projection", "Face1", "source face selected by LinkSubList"),
                null_endpoint("face rebuild ProjectOnSurface case failed before stable result"),
                "Part::ProjectOnSurface",
                {"error_class": type(exc).__name__, "message": str(exc), "traceback_tail": traceback.format_exc().splitlines()[-5:]},
                "collector reached FreeCADCmd but not stable face result",
                "collector_bug",
            )
        finally:
            close_doc(doc)

    def run_all_compound_case() -> None:
        doc = safe_doc("C12M3_S4_AllCompound")
        try:
            support, source = add_support_and_source(doc, "ProjectionFaceForSolid")
            projection = assign_project_on_surface(
                doc,
                "ProjectedAllCompoundHeightOffset",
                support,
                source,
                "Face1",
                "All",
                height=1.25,
                offset=0.35,
            )
            result_shapes["all_compound_height_offset_result"] = shape_summary(projection.Shape)
            history = summarize_get_element_history(
                projection.Shape,
                ["Solid1", "Shell1", "Face1", "Face2", "Edge1", "Wire1"],
            )
            add_observation(
                "C12M3-S4-COMPOUND-001",
                "all_compound_height_offset",
                endpoint("ProjectionFaceForSolid", "Projection", "Face1", "source face selected by LinkSubList"),
                endpoint("ProjectedAllCompoundHeightOffset", "Shape", "Solid1/Face1/Edge1", "height solid and offset compound result"),
                "TopoShapePy.getElementHistory",
                history,
                "request-local object result; offset/height geometry observed but not used as provenance",
                "native_hidden_retained",
            )
            add_observation(
                "C12M3-S4-COMPOUND-002",
                "all_compound_height_offset",
                endpoint("ProjectionFaceForSolid", "Projection", "Face1", "source face before height extrusion and offset"),
                endpoint("ProjectedAllCompoundHeightOffset", "Shape", None, "compound child pre/post offset identity"),
                "TopoShapeExpansion.mapCompoundSubElements / PropertyPartShape::setValue",
                {
                    "object_result_shape": result_shapes["all_compound_height_offset_result"],
                    "visible_history": any_visible_history(history),
                    "summary": "Only output geometry and compound topology are visible; no ProjectOnSurface-native child ElementMap rows are exposed.",
                },
                "request-local object result; persistent ElementMap cache not used",
                "native_hidden_retained",
            )
        except Exception as exc:
            add_observation(
                "C12M3-S4-COMPOUND-ERROR",
                "all_compound_height_offset",
                endpoint("ProjectionFaceForSolid", "Projection", "Face1", "source face selected by LinkSubList"),
                null_endpoint("all/height/offset ProjectOnSurface case failed before stable result"),
                "Part::ProjectOnSurface",
                {"error_class": type(exc).__name__, "message": str(exc), "traceback_tail": traceback.format_exc().splitlines()[-5:]},
                "collector reached FreeCADCmd but not stable all/height/offset result",
                "collector_bug",
            )
        finally:
            close_doc(doc)

    def run_invalid_case() -> None:
        doc = safe_doc("C12M3_S4_InvalidDiagnostic")
        try:
            support, source = add_support_and_source(doc, "InvalidProjectionSource")
            cases = []

            def run_invalid_projection(name: str, setup: Any) -> dict[str, Any]:
                projection = doc.addObject("Part::ProjectOnSurface", name)
                projection.Mode = "Edges"
                projection.Direction = FreeCAD.Vector(0, 0, 1)
                setup(projection)
                try:
                    doc.recompute()
                    return {
                        "case": name,
                        "recompute_returned": True,
                        "state": getattr(projection, "State", None),
                        "shape": shape_summary(projection.Shape),
                    }
                except Exception as exc:
                    return {
                        "case": name,
                        "recompute_returned": False,
                        "error_class": type(exc).__name__,
                        "message": str(exc),
                        "traceback_tail": traceback.format_exc().splitlines()[-5:],
                    }

            cases.append(
                run_invalid_projection(
                    "InvalidNoSupportFace",
                    lambda projection: setattr(projection, "Projection", [(source, ["Wire1"])]),
                )
            )
            cases.append(
                run_invalid_projection(
                    "InvalidMultipleSupportFaceSubnames",
                    lambda projection: (
                        setattr(projection, "SupportFace", (support, ["Face1", "Face1"])),
                        setattr(projection, "Projection", [(source, ["Wire1"])]),
                    ),
                )
            )
            cases.append(
                run_invalid_projection(
                    "InvalidProjectionSubname",
                    lambda projection: (
                        setattr(projection, "SupportFace", (support, ["Face1"])),
                        setattr(projection, "Projection", [(source, ["NotASubshape"])]),
                    ),
                )
            )
            add_observation(
                "C12M3-S4-DIAG-001",
                "invalid_projection_diagnostic",
                endpoint("InvalidProjectionSource", "Projection/SupportFace", "missing/multiple/NotASubshape", "invalid native property or subname inputs"),
                endpoint("InvalidNoSupportFace/InvalidMultipleSupportFaceSubnames/InvalidProjectionSubname", "Shape", None, "diagnostic-only targets"),
                "Part::ProjectOnSurface::getProjectionShapes / Feature::getTopoShape",
                {"cases": cases},
                "request-local native diagnostic probe; no crash/timeout/current cad-core comparison",
                "native_hidden_retained",
                "blocked: diagnostic is not source-to-target provenance; S5 comparison blocked",
            )
        except Exception as exc:
            add_observation(
                "C12M3-S4-DIAG-ERROR",
                "invalid_projection_diagnostic",
                endpoint("InvalidProjectionSource", "Projection", "NotASubshape", "invalid source subname"),
                null_endpoint("invalid diagnostic probe setup failed"),
                "Part::ProjectOnSurface",
                {"error_class": type(exc).__name__, "message": str(exc), "traceback_tail": traceback.format_exc().splitlines()[-5:]},
                "collector reached FreeCADCmd but invalid diagnostic setup failed",
                "collector_bug",
            )
        finally:
            close_doc(doc)

    def run_api_observability_case() -> None:
        doc = safe_doc("C12M3_S4_ApiObservability")
        try:
            support, source = add_support_and_source(doc, "ApiProjectionSource")
            projection = assign_project_on_surface(
                doc,
                "ApiProjectedObject",
                support,
                source,
                "Wire1",
                "Edges",
            )
            result_copy = projection.Shape.copy()
            map_sub_summary: dict[str, Any] = {}
            try:
                returned = result_copy.mapSubElement([source.Shape], "C12M3Probe")
                map_sub_summary["return_type"] = type(returned).__name__
                map_sub_summary["post_history"] = summarize_get_element_history(result_copy, ["Edge1", "Wire1", "Face1"])
            except Exception as exc:
                map_sub_summary["error_class"] = type(exc).__name__
                map_sub_summary["message"] = str(exc)
            add_observation(
                "C12M3-S4-API-001",
                "api_observability",
                endpoint("ApiProjectionSource", "Shape", "Wire1", "source shape supplied manually to mapSubElement"),
                endpoint("ApiProjectedObject", "Shape.copy()", "Edge1/Wire1/Face1", "copied ProjectOnSurface result shape"),
                "TopoShapePy.mapSubElement",
                map_sub_summary,
                "request-local manual API call; not automatically published by ProjectOnSurface",
                "native_hidden_retained",
            )

            map_shapes_summary: dict[str, Any] = {}
            try:
                target_edge = result_copy.Edges[0] if getattr(result_copy, "Edges", []) else result_copy
                source_edge = source.Shape.Edges[0] if getattr(source.Shape, "Edges", []) else source.Shape
                returned = result_copy.mapShapes([(source_edge, target_edge)], [], "C12M3Probe")
                map_shapes_summary["return_type"] = type(returned).__name__
                map_shapes_summary["post_history"] = summarize_get_element_history(result_copy, ["Edge1", "Wire1", "Face1"])
                map_shapes_summary["manual_mapper_input"] = "generated=[(source.Shape.Edges[0], result.Shape.copy().Edges[0])]"
            except Exception as exc:
                map_shapes_summary["error_class"] = type(exc).__name__
                map_shapes_summary["message"] = str(exc)
            add_observation(
                "C12M3-S4-API-002",
                "api_observability",
                endpoint("ApiProjectionSource", "Shape", "Edge1", "manual source edge supplied to mapShapes"),
                endpoint("ApiProjectedObject", "Shape.copy()", "Edge1", "manual target edge supplied to mapShapes"),
                "TopoShapePy.mapShapes",
                map_shapes_summary,
                "request-local manual mapper injection; rejected as ProjectOnSurface-native provenance",
                "product_boundary_rejected",
                "blocked: manual mapper injection is not native ProjectOnSurface history evidence",
            )

            add_observation(
                "C12M3-S4-API-003",
                "api_observability",
                endpoint("ApiProjectedObject", "Shape", None, "ProjectOnSurface PropertyPartShape result"),
                null_endpoint("ElementMap save/load requires native document persistence or FCStd/BREP roundtrip"),
                "PropertyPartShape::beforeSave/Save/RestoreDocFile and ElementMap persistence",
                {
                    "executed": False,
                    "reason": "S2 product boundary rejects cross-request native document, persistent TopoDS/NamedShape/ElementMap cache and full BREP transport.",
                },
                "not request-local product evidence; source review context only",
                "product_boundary_rejected",
                "blocked: persistent ElementMap cache is outside CAD Core request-local product boundary",
            )

            add_observation(
                "C12M3-S4-API-004",
                "api_observability",
                endpoint("ApiProjectionSource", "Projection", "Wire1", "source wire selected by LinkSubList"),
                endpoint("ApiProjectedObject", "Shape", "Edge1..Edge4", "ProjectOnSurface result"),
                "TopoShapeExpansion::MapperHistory",
                {
                    "executed": False,
                    "reason": "MapperHistory has no direct Python constructor on ProjectOnSurface result, and FeatureProjectOnSurface.cpp does not publish a BRepTools_History/BRepTools_ReShape mapper for BRepProj_Projection.",
                },
                "request-local source review plus Python API observability",
                "native_hidden_retained",
            )
        except Exception as exc:
            add_observation(
                "C12M3-S4-API-ERROR",
                "api_observability",
                null_endpoint("api observability setup failed"),
                null_endpoint("api observability setup failed"),
                "TopoShapePy history APIs",
                {"error_class": type(exc).__name__, "message": str(exc), "traceback_tail": traceback.format_exc().splitlines()[-5:]},
                "collector reached FreeCADCmd but api observability setup failed",
                "collector_bug",
            )
        finally:
            close_doc(doc)

    run_edge_wire_case()
    run_face_case()
    run_all_compound_case()
    run_invalid_case()
    run_api_observability_case()

    if any(row["classification"] == "collector_bug" for row in observations):
        overall = "collector_bug"
    elif any(row["classification"] == "native_provenance_expected_ready" for row in observations):
        overall = "native_provenance_expected_ready"
    else:
        overall = "native_hidden_retained"

    if overall == "native_hidden_retained":
        diagnostics.extend(
            [
                "ProjectOnSurface object result shapes and intermediate makeParallelProjection shapes were observable, but getElementHistory returned None/error-only data rather than source-backed source-to-target provenance.",
                "mapSubElement/mapShapes are callable only as manual request-local API operations; they do not prove FeatureProjectOnSurface publishes MapperHistory or ElementMap rows.",
                "S5 current comparison remains blocked because no observation is native_provenance_expected_ready.",
            ]
        )

    expected_summary = {
        "schema_version": SUMMARY_SCHEMA_VERSION,
        "artifact_kind": "project_on_surface_native_provenance_probe",
        "c12m3_classification": overall,
        "source_authority": SOURCE_AUTHORITY,
        "input_fixture_or_probe": {
            "probe_script": "docs/temp/6-29-23-05-c12m3-s4-project-on-surface-native-provenance-probe.py",
            "case_id": "edge_wire+face_rebuild+all_compound_height_offset+invalid_diagnostic+api_observability",
            "c12m2_context": "docs/temp/6-29-20-40-c12m2-s5-project-on-surface-native-probe-output.json",
        },
        "runtime_summary": default_runtime_summary(metadata()),
        "result_shape_summary": result_shapes,
        "provenance_observations": observations,
        "diagnostics": diagnostics,
        "non_evidence": non_evidence,
        "current_comparison_path": "blocked: no native_provenance_expected_ready observation; S5 comparison is blocked",
        "s5_input": None,
    }

    payload = metadata()
    payload.update(
        {
            "c12m3_classification": overall,
            "expected_summary": expected_summary,
        }
    )
    print(PAYLOAD_PREFIX + json.dumps(payload, ensure_ascii=False, sort_keys=True))
    return 0 if overall != "collector_bug" else 1


def main() -> int:
    if "--inside-freecad" in sys.argv:
        return run_inside_freecad()
    return run_wrapper()


if __name__ == "__main__":
    raise SystemExit(main())
