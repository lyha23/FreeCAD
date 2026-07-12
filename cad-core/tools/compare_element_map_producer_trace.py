#!/usr/bin/env python3
"""Compare native and CAD Core producer traces and report the first semantic divergence."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

try:
    from element_map_producer_trace import TraceValidationError, compare_traces, validate_trace
    from element_map_producer_trace.report import (
        human_report,
        invalid_report_payload,
        report_payload,
        write_report,
    )
except ModuleNotFoundError:
    from tools.element_map_producer_trace import TraceValidationError, compare_traces, validate_trace
    from tools.element_map_producer_trace.report import (
        human_report,
        invalid_report_payload,
        report_payload,
        write_report,
    )


ROOT = Path(__file__).resolve().parents[1]


def paths_from_args(args: argparse.Namespace) -> tuple[Path, Path]:
    if args.expected is not None:
        expected = args.expected
    elif args.phase and args.case:
        expected = (
            ROOT
            / "fixtures"
            / args.phase
            / "expected"
            / f"{args.case}.freecad.producer-trace.json"
        )
    else:
        raise ValueError("--expected or both --phase/--case are required")
    if args.actual is not None:
        actual = args.actual
    elif args.phase and args.case:
        suffix = args.actual_kind
        actual = (
            ROOT
            / "fixtures"
            / args.phase
            / f"{suffix}-res"
            / f"{args.case}.{suffix}.producer-trace.json"
        )
    else:
        raise ValueError("--actual or both --phase/--case are required")
    return expected, actual


def artifact_paths_from_args(
    args: argparse.Namespace,
    expected_trace: Path,
    actual_trace: Path,
) -> tuple[Path, Path, Path]:
    case = args.case
    phase = args.phase
    suffix = ".freecad.producer-trace.json"
    if case is None and expected_trace.name.endswith(suffix):
        case = expected_trace.name[: -len(suffix)]
    if phase is None and expected_trace.parent.name == "expected":
        phase = expected_trace.parent.parent.name
    if args.input is not None:
        input_path = args.input
    elif phase and case:
        input_path = ROOT / "fixtures" / phase / f"{case}.json"
    else:
        raise ValueError("--input or an inferable fixture --phase/--case is required")
    expected_response = args.expected_response or expected_trace.with_name(
        expected_trace.name.replace(".freecad.producer-trace.json", ".freecad.json")
    )
    actual_response = args.actual_response or actual_trace.with_name(
        actual_trace.name.replace(".producer-trace.json", ".json")
    )
    return input_path, expected_response, actual_response


def _validate_bound_trace(
    trace_path: Path,
    *,
    input_path: Path,
    input_document: object,
    response_document: object,
) -> None:
    trace = validate_trace(trace_path)
    producer = trace.get("producer", {})
    if (
        not isinstance(producer, dict)
        or "inputSha256" not in producer
        or "responseSha256" not in producer
    ):
        raise TraceValidationError(
            "producer trace does not carry inputSha256/responseSha256 artifact bindings"
        )
    if (
        producer.get("name") == "CadRs"
        and isinstance(response_document, dict)
        and response_document.get("schema") == "cad-rs.ffi-result.v2"
        and "payload" in response_document
    ):
        response_bytes = json.dumps(
            response_document["payload"],
            ensure_ascii=False,
            separators=(",", ":"),
        ).encode("utf-8")
        validate_trace(
            trace_path,
            input_bytes=input_path.read_bytes(),
            response_bytes=response_bytes,
        )
        return
    validate_trace(
        trace_path,
        input_document=input_document,
        response_document=response_document,
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--phase")
    parser.add_argument("--case")
    parser.add_argument(
        "--actual-kind",
        choices=("cad-core", "cad-rs"),
        default="cad-core",
        help="Actual producer family for phase/case discovery; CAD Core is the N2 default.",
    )
    parser.add_argument("--expected", type=Path)
    parser.add_argument("--actual", type=Path)
    parser.add_argument("--input", type=Path, help="Fixture request used to verify both trace runs.")
    parser.add_argument("--expected-response", type=Path, help="Native response bound by the expected trace.")
    parser.add_argument("--actual-response", type=Path, help="CAD Core response bound by the actual trace.")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args(argv)
    expected_path: Path | None = None
    actual_path: Path | None = None
    try:
        expected_path, actual_path = paths_from_args(args)
        if not expected_path.is_file() or not actual_path.is_file():
            missing = [str(path) for path in (expected_path, actual_path) if not path.is_file()]
            raise FileNotFoundError(f"producer trace input is missing: {missing}")
        input_path, expected_response_path, actual_response_path = artifact_paths_from_args(
            args, expected_path, actual_path
        )
        companion_paths = (input_path, expected_response_path, actual_response_path)
        missing = [str(path) for path in companion_paths if not path.is_file()]
        if missing:
            raise FileNotFoundError(f"producer trace binding artifact is missing: {missing}")
        input_document = json.loads(input_path.read_text(encoding="utf-8"))
        expected_response = json.loads(expected_response_path.read_text(encoding="utf-8"))
        actual_response = json.loads(actual_response_path.read_text(encoding="utf-8"))
        try:
            _validate_bound_trace(
                expected_path,
                input_path=input_path,
                input_document=input_document,
                response_document=expected_response,
            )
        except TraceValidationError as exc:
            raise ValueError(f"invalid_expected_trace: {exc}") from exc
        try:
            _validate_bound_trace(
                actual_path,
                input_path=input_path,
                input_document=input_document,
                response_document=actual_response,
            )
        except TraceValidationError as exc:
            raise ValueError(f"invalid_actual_trace: {exc}") from exc
        result = compare_traces(
            expected_path,
            actual_path,
            document_graph=input_document,
        )
        payload = report_payload(
            result,
            phase=args.phase,
            case=args.case,
            expected_path=expected_path,
            actual_path=actual_path,
        )
        if args.report:
            write_report(args.report, payload)
        print(human_report(payload))
        return 0 if result.status == "equal" else 2 if result.status == "invalid" else 1
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        detail = str(exc)
        classification = "invalid_trace"
        if (
            isinstance(exc, FileNotFoundError)
            and expected_path is not None
            and not expected_path.is_file()
        ):
            classification = "missing_expected_trace"
        elif (
            isinstance(exc, FileNotFoundError)
            and actual_path is not None
            and not actual_path.is_file()
        ):
            classification = "missing_actual_trace"
        elif "final checkpoint" in detail or "no checkpoint" in detail:
            classification = "final_checkpoint_missing"
        elif detail.startswith("invalid_expected_trace:"):
            classification = "invalid_expected_trace"
        elif detail.startswith("invalid_actual_trace:"):
            classification = "invalid_actual_trace"
        payload = invalid_report_payload(
            classification=classification,
            detail=detail,
            phase=args.phase,
            case=args.case,
            expected_path=expected_path,
            actual_path=actual_path,
        )
        if args.report:
            write_report(args.report, payload)
        print(human_report(payload), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
