"""Thin generation adapter for the FreeCAD expected parity module."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

try:
    from freecad_expected_parity import MaterializeRequest, materialize_current
    from freecad_expected_parity.catalog import FixtureCase, load_catalog
except ImportError:
    from tools.freecad_expected_parity import MaterializeRequest, materialize_current
    from tools.freecad_expected_parity.catalog import FixtureCase, load_catalog


ROOT = Path(__file__).resolve().parents[1]
ExpectedCase = FixtureCase


def discover_expected_cases(
    root: Path = ROOT,
    phase: str | None = None,
    case: str | None = None,
) -> list[FixtureCase]:
    return load_catalog(Path(root), phase=phase, case=case).cases


def regenerate_cases(
    cases: object | None = None,
    root: Path = ROOT,
    bin_path: Path | None = None,
    *,
    phase: str | None = None,
    case: str | None = None,
    roles_path: Path | None = None,
) -> dict:
    """Compatibility entrypoint; selection now belongs to ``MaterializeRequest``.

    A legacy caller may hand a homogeneous list of cases.  It is converted to
    its only safe supported shape (one phase/case) rather than retaining a
    second discovery and generation implementation.
    """

    if cases is not None:
        items = list(cases)  # type: ignore[arg-type]
        if len(items) == 1:
            phase = getattr(items[0], "phase", phase)
            case = getattr(items[0], "case", case)
        elif items:
            return {
                "schemaVersion": "cad-core.freecad-expected-generation.v2",
                "status": "invalid",
                "summary": {"cases": len(items), "written": 0, "failed": len(items)},
                "cases": [],
                "preflight": {"valid": False, "errors": ["pass phase/case; arbitrary case lists are unsupported"]},
            }
    return materialize_current(
        MaterializeRequest(root=root, phase=phase, case=case, binary=bin_path, roles_path=roles_path)
    ).to_dict()


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Atomically regenerate CAD Core current outputs.")
    parser.add_argument("--phase", help="Only regenerate one fixture phase.")
    parser.add_argument("--case", help="Only regenerate one fixture case name.")
    parser.add_argument("--bin", type=Path, help="cad-core binary path.")
    parser.add_argument("--roles", type=Path, help="Fixture-role manifest path.")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    report = regenerate_cases(
        root=ROOT,
        bin_path=args.bin,
        phase=args.phase,
        case=args.case,
        roles_path=args.roles,
    )
    print(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True))
    return 0 if report["status"] == "ok" else 1


if __name__ == "__main__":
    raise SystemExit(main())
