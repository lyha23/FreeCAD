#!/usr/bin/env python3
"""Promote one staging FreeCAD fixture authority as a rollback-safe transaction."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import tempfile
from pathlib import Path
from typing import Any

from collect_freecad_expected import compare_json, compare_ledger_json


ROOT = Path(__file__).resolve().parents[1]
ROLES_PATH = Path(__file__).with_name("freecad_expected_parity") / "fixture_roles.v1.json"
REPORTS_ROOT = Path(__file__).with_name("freecad_expected_parity") / "reports"
SCHEMA = "freecad-fixture-authority-promotion/v1"
REVOCATION_SCHEMA = "freecad-fixture-authority-revocation/v1"


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def canonical_sha256(payload: Any) -> str:
    data = json.dumps(
        payload,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    return hashlib.sha256(data).hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise ValueError(f"JSON document must be an object: {path}")
    return payload


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def artifact(path: Path) -> dict[str, Any]:
    return {
        "path": str(path.resolve()),
        "sha256": file_sha256(path),
    }


def _producer_identity(report: dict[str, Any], label: str) -> None:
    candidate = report.get("candidate")
    collector = report.get("collector")
    if not isinstance(candidate, dict) or not candidate.get("path") or not candidate.get("sha256"):
        raise ValueError(f"{label} is missing FreeCADCmd path/SHA256")
    if not isinstance(collector, dict) or not collector.get("sha256"):
        raise ValueError(f"{label} is missing collector SHA256")


def _validate_collect_report(
    report: dict[str, Any],
    *,
    phase: str,
    case: str,
    fixture: Path,
    public_expected: Path,
    ledger: Path,
) -> None:
    if (
        report.get("status") != "passed"
        or report.get("processed") != 1
        or report.get("failed") != 0
        or report.get("ledgerValidationStatus") != "passed"
        or report.get("producerTraceStatus") != "not_evaluated"
    ):
        raise ValueError("staging collect report did not pass strict collection/ledger gates")
    cases = report.get("cases")
    if (
        not isinstance(cases, list)
        or len(cases) != 1
        or not isinstance(cases[0], dict)
        or cases[0].get("phase") != phase
        or cases[0].get("case") != case
    ):
        raise ValueError("staging collect report does not identify the selected case")
    case_result = cases[0]
    if (
        case_result.get("status") != "passed"
        or case_result.get("ledgerOutcome") != "accepted"
    ):
        raise ValueError("staging collect case did not produce accepted authority")
    for label, key, path in (
        ("fixture", "fixtureSha256", fixture),
        ("public expected", "publicExpectedSha256", public_expected),
        ("ledger", "ledgerSha256", ledger),
    ):
        if case_result.get(key) != file_sha256(path):
            raise ValueError(f"staging collect {label} hash does not match current artifact")
    _producer_identity(report, "staging collect report")
    runtime = report.get("runtime")
    if (
        not isinstance(runtime, dict)
        or not runtime.get("freecadVersion")
        or not runtime.get("occtVersion")
    ):
        raise ValueError("staging collect report is missing FreeCAD/OCCT versions")


def _validate_repeat_report(
    report: dict[str, Any],
    *,
    phase: str,
    case: str,
    fixture: Path,
    staging_expected: Path,
) -> tuple[Path, Path]:
    if (
        report.get("mode") != "repeated-staging-collection"
        or report.get("status") != "passed"
        or report.get("publicExpectedStatus") != "passed"
        or report.get("ledgerValidationStatus") != "passed"
        or report.get("producerTraceStatus") != "not_evaluated"
        or report.get("selectedCaseCount") != 1
        or report.get("executedCaseCount") != 1
        or report.get("runCount") != 2
        or report.get("successfulRunCount") != 2
        or report.get("candidateRunDifferences") != []
    ):
        raise ValueError("staging repeat report did not pass the independent run-A/run-B gate")
    _producer_identity(report, "staging repeat report")
    runtime_identities = report.get("runtimeIdentities")
    if not isinstance(runtime_identities, list) or not runtime_identities:
        raise ValueError("staging repeat report is missing FreeCAD/OCCT runtime identity")
    manifest = report.get("manifest")
    entries = manifest.get("entries") if isinstance(manifest, dict) else None
    if (
        not isinstance(entries, list)
        or len(entries) != 1
        or not isinstance(entries[0], dict)
        or entries[0].get("phase") != phase
        or entries[0].get("case") != case
        or entries[0].get("fixtureSha256") != file_sha256(fixture)
    ):
        raise ValueError("staging repeat manifest does not match the selected fixture")
    candidate_root_value = report.get("candidateRoot")
    if not isinstance(candidate_root_value, str) or not candidate_root_value:
        raise ValueError("staging repeat report is missing candidateRoot")
    candidate_root = Path(candidate_root_value)
    relative_expected = Path(phase) / "expected" / f"{case}.freecad.json"
    run_a_expected = candidate_root / "run-a" / relative_expected
    run_b_expected = candidate_root / "run-b" / relative_expected
    run_a_ledger = run_a_expected.with_name(f"{case}.freecad.ledger.json")
    run_b_ledger = run_b_expected.with_name(f"{case}.freecad.ledger.json")
    for required in (run_a_expected, run_b_expected, run_a_ledger, run_b_ledger):
        if not required.is_file():
            raise ValueError(f"staging repeat artifact is missing: {required}")
    if entries[0].get("publicSha256") != file_sha256(run_a_expected):
        raise ValueError("staging repeat public hash does not match run-a")
    if entries[0].get("ledgerSha256") != file_sha256(run_a_ledger):
        raise ValueError("staging repeat ledger hash does not match run-a")
    if not compare_json(run_a_expected, load_json(run_b_expected)):
        raise ValueError("staging repeat public expected differs between run-a and run-b")
    if not compare_json(staging_expected, load_json(run_a_expected)):
        raise ValueError("staging first collection differs from repeat run-a")
    # Ledger metadata may drift while both ledgers remain strict-valid; retain that
    # diagnostic split instead of turning it into a public CAD regression.
    compare_ledger_json(run_a_ledger, load_json(run_b_ledger))
    return run_a_expected, run_a_ledger


def _updated_roles(
    roles: dict[str, Any],
    *,
    phase: str,
    case: str,
) -> tuple[dict[str, Any], str]:
    entries = roles.get("roles")
    if not isinstance(entries, list):
        raise ValueError("fixture role manifest roles must be a list")
    matches = [
        (index, entry)
        for index, entry in enumerate(entries)
        if isinstance(entry, dict)
        and entry.get("phase") == phase
        and entry.get("case") == case
    ]
    if len(matches) > 1:
        raise ValueError(f"duplicate fixture roles for {phase}/{case}")
    updated = json.loads(json.dumps(roles))
    updated_entries = updated["roles"]
    if matches:
        index, entry = matches[0]
        previous_role = str(entry.get("role"))
        if previous_role == "native":
            raise ValueError(f"fixture is already native: {phase}/{case}")
        if previous_role != "unsupported":
            raise ValueError(
                f"only unsupported fixtures can be promoted, got {previous_role}: {phase}/{case}"
            )
        updated_entries[index] = {"phase": phase, "case": case, "role": "native"}
    else:
        previous_role = "absent"
        updated_entries.append({"phase": phase, "case": case, "role": "native"})
        updated_entries.sort(key=lambda item: (str(item.get("phase")), str(item.get("case"))))
    return updated, previous_role


def _transactional_replace(
    staged_targets: list[tuple[str, Path, Path]],
    *,
    transaction_root: Path,
) -> None:
    backups = transaction_root / "backups"
    backups.mkdir(parents=True, exist_ok=True)
    backup_by_target: dict[Path, Path] = {}
    for index, (_kind, _source, target) in enumerate(staged_targets):
        if target.exists():
            backup = backups / f"{index}-{target.name}"
            shutil.copy2(target, backup)
            backup_by_target[target] = backup
    committed: list[Path] = []
    try:
        for _kind, source, target in staged_targets:
            target.parent.mkdir(parents=True, exist_ok=True)
            os.replace(source, target)
            committed.append(target)
    except Exception:
        for target in reversed(committed):
            backup = backup_by_target.get(target)
            if backup is not None and backup.exists():
                os.replace(backup, target)
            elif target.exists():
                target.unlink()
        raise


def _transactional_replace_and_delete(
    staged_targets: list[tuple[str, Path, Path]],
    delete_targets: list[Path],
    *,
    transaction_root: Path,
) -> None:
    backups = transaction_root / "backups"
    backups.mkdir(parents=True, exist_ok=True)
    original_targets = [target for _kind, _source, target in staged_targets] + delete_targets
    backup_by_target: dict[Path, Path] = {}
    for index, target in enumerate(original_targets):
        if target.exists():
            backup = backups / f"{index}-{target.name}"
            shutil.copy2(target, backup)
            backup_by_target[target] = backup
    touched: list[Path] = []
    try:
        for _kind, source, target in staged_targets:
            target.parent.mkdir(parents=True, exist_ok=True)
            os.replace(source, target)
            touched.append(target)
        for target in delete_targets:
            target.unlink()
            touched.append(target)
    except Exception:
        for target in reversed(touched):
            backup = backup_by_target.get(target)
            if backup is not None and backup.exists():
                os.replace(backup, target)
            elif target.exists():
                target.unlink()
        raise


def _roles_with_revoked_authority(
    roles: dict[str, Any],
    *,
    phase: str,
    case: str,
    authority: str,
) -> dict[str, Any]:
    entries = roles.get("roles")
    if not isinstance(entries, list):
        raise ValueError("fixture role manifest roles must be a list")
    matches = [
        index
        for index, entry in enumerate(entries)
        if isinstance(entry, dict)
        and entry.get("phase") == phase
        and entry.get("case") == case
    ]
    if len(matches) != 1:
        raise ValueError(f"revocation requires exactly one fixture role: {phase}/{case}")
    index = matches[0]
    if entries[index].get("role") != "native":
        raise ValueError(f"only native authority can be revoked: {phase}/{case}")
    updated = json.loads(json.dumps(roles))
    updated["roles"][index] = {
        "phase": phase,
        "case": case,
        "role": "unsupported",
        "reason": "Post-promotion checked-in repeat produced non-reproducible public semantics.",
        "authority": authority,
        "nextAction": (
            "Keep outside native authority until controlled FreeCADCmd collection and both repeat gates pass reproducibly."
        ),
        "closeCondition": (
            "A fresh staging collect, repeat 2, promotion, and checked-in repeat 2 all pass."
        ),
    }
    return updated


def revoke_fixture_authority(
    *,
    promotion_report_path: Path,
    post_repeat_report_path: Path,
    roles_path: Path,
    reports_root: Path,
) -> dict[str, Any]:
    promotion_report_path = promotion_report_path.resolve()
    post_repeat_report_path = post_repeat_report_path.resolve()
    roles_path = roles_path.resolve()
    reports_root = reports_root.resolve()
    promotion_report = load_json(promotion_report_path)
    post_repeat_report = load_json(post_repeat_report_path)
    if (
        promotion_report.get("schema") != SCHEMA
        or promotion_report.get("status") != "passed"
    ):
        raise ValueError("revocation requires a passed promotion receipt")
    phase = promotion_report.get("phase")
    case = promotion_report.get("case")
    if not isinstance(phase, str) or not isinstance(case, str):
        raise ValueError("promotion receipt is missing phase/case")
    first_failure = post_repeat_report.get("firstFailure")
    case_result = first_failure.get("caseResult") if isinstance(first_failure, dict) else None
    if (
        post_repeat_report.get("status") != "failed"
        or post_repeat_report.get("publicExpectedStatus") != "failed"
        or not isinstance(case_result, dict)
        or case_result.get("phase") != phase
        or case_result.get("case") != case
    ):
        raise ValueError("revocation requires a matching failed checked-in public repeat")

    targets = promotion_report.get("targets")
    staging = promotion_report.get("staging")
    if not isinstance(targets, dict) or not isinstance(staging, dict):
        raise ValueError("promotion receipt is missing target/staging artifacts")
    target_input = Path(str(targets.get("input", ""))).resolve()
    target_expected = Path(str(targets.get("publicExpected", ""))).resolve()
    target_ledger = Path(str(targets.get("ledger", ""))).resolve()
    target_report = Path(str(targets.get("producerReport", ""))).resolve()
    target_roles = Path(str(targets.get("roleManifest", ""))).resolve()
    if target_report != promotion_report_path or target_roles != roles_path:
        raise ValueError("promotion receipt target paths do not match revocation inputs")
    for label, target, staging_key in (
        ("input", target_input, "input"),
        ("public expected", target_expected, "publicExpected"),
        ("ledger", target_ledger, "ledger"),
    ):
        source_receipt = staging.get(staging_key)
        if (
            not target.is_file()
            or not isinstance(source_receipt, dict)
            or source_receipt.get("sha256") != file_sha256(target)
        ):
            raise ValueError(f"promoted {label} no longer matches its promotion receipt")

    revocation_report = reports_root / "revocations" / f"{phase}-{case}.json"
    if revocation_report.exists():
        raise ValueError(f"revocation receipt already exists: {revocation_report}")
    roles = load_json(roles_path)
    updated_roles = _roles_with_revoked_authority(
        roles,
        phase=phase,
        case=case,
        authority=str(revocation_report),
    )
    receipt = {
        "schema": REVOCATION_SCHEMA,
        "status": "passed",
        "phase": phase,
        "case": case,
        "reason": "post-promotion-public-nondeterminism",
        "failure": {"kind": "candidate-determinism", "artifact": "public"},
        "evidence": {
            "promotionReport": artifact(promotion_report_path),
            "postRepeatReport": artifact(post_repeat_report_path),
        },
        "roleTransition": {"from": "native", "to": "unsupported"},
        "revokedArtifacts": [str(target_expected), str(target_ledger), str(target_report)],
        "preservedInput": artifact(target_input),
    }

    lock_path = reports_root / ".fixture-authority-promotion.lock"
    reports_root.mkdir(parents=True, exist_ok=True)
    try:
        lock_fd = os.open(lock_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o644)
    except FileExistsError as exc:
        raise ValueError(f"fixture authority transaction is already running: {lock_path}") from exc
    os.close(lock_fd)
    try:
        with tempfile.TemporaryDirectory(
            prefix=".fixture-authority-revocation-",
            dir=str(reports_root),
        ) as temp_dir:
            transaction_root = Path(temp_dir)
            staged_roles = transaction_root / "fixture-roles.json"
            staged_receipt = transaction_root / "revocation.json"
            write_json(staged_roles, updated_roles)
            write_json(staged_receipt, receipt)
            _transactional_replace_and_delete(
                [
                    ("revocationReport", staged_receipt, revocation_report),
                    ("roleManifest", staged_roles, roles_path),
                ],
                [target_expected, target_ledger, target_report],
                transaction_root=transaction_root,
            )
    finally:
        lock_path.unlink(missing_ok=True)
    return receipt


def promote_fixture_authority(
    *,
    staging_fixture: Path,
    fixtures_root: Path,
    roles_path: Path,
    reports_root: Path,
    collect_report_path: Path,
    repeat_report_path: Path,
) -> dict[str, Any]:
    staging_fixture = staging_fixture.resolve()
    fixtures_root = fixtures_root.resolve()
    roles_path = roles_path.resolve()
    reports_root = reports_root.resolve()
    phase = staging_fixture.parent.name
    case = staging_fixture.stem
    staging_expected = staging_fixture.parent / "expected" / f"{case}.freecad.json"
    staging_ledger = staging_fixture.parent / "expected" / f"{case}.freecad.ledger.json"
    for required in (
        staging_fixture,
        staging_expected,
        staging_ledger,
        roles_path,
        collect_report_path,
        repeat_report_path,
    ):
        if not required.is_file():
            raise ValueError(f"required promotion input is missing: {required}")

    collect_report = load_json(collect_report_path)
    repeat_report = load_json(repeat_report_path)
    _validate_collect_report(
        collect_report,
        phase=phase,
        case=case,
        fixture=staging_fixture,
        public_expected=staging_expected,
        ledger=staging_ledger,
    )
    _validate_repeat_report(
        repeat_report,
        phase=phase,
        case=case,
        fixture=staging_fixture,
        staging_expected=staging_expected,
    )
    roles = load_json(roles_path)
    updated_roles, previous_role = _updated_roles(roles, phase=phase, case=case)

    target_fixture = fixtures_root / phase / f"{case}.json"
    target_expected = fixtures_root / phase / "expected" / f"{case}.freecad.json"
    target_ledger = fixtures_root / phase / "expected" / f"{case}.freecad.ledger.json"
    target_report = reports_root / "promotions" / f"{phase}-{case}.json"
    if target_fixture.is_file() and file_sha256(target_fixture) != file_sha256(staging_fixture):
        raise ValueError(f"target fixture conflicts with staging input: {target_fixture}")
    for target in (target_expected, target_ledger, target_report):
        if target.exists():
            raise ValueError(f"promotion target already exists: {target}")

    role_before_sha = file_sha256(roles_path)
    role_after_sha = canonical_sha256(updated_roles)
    producer_report = {
        "schema": SCHEMA,
        "status": "passed",
        "phase": phase,
        "case": case,
        "producer": {
            "candidate": repeat_report["candidate"],
            "collector": repeat_report["collector"],
            "runtimeIdentities": repeat_report["runtimeIdentities"],
            "producerTraceStatus": repeat_report["producerTraceStatus"],
        },
        "staging": {
            "input": artifact(staging_fixture),
            "publicExpected": artifact(staging_expected),
            "ledger": artifact(staging_ledger),
            "collectReport": artifact(collect_report_path),
            "repeatReport": artifact(repeat_report_path),
        },
        "verification": {
            "stagingCollectStatus": collect_report["status"],
            "stagingRepeatStatus": repeat_report["status"],
            "publicExpectedStatus": repeat_report["publicExpectedStatus"],
            "ledgerValidationStatus": repeat_report["ledgerValidationStatus"],
            "runCount": repeat_report["runCount"],
            "successfulRunCount": repeat_report["successfulRunCount"],
        },
        "roleTransition": {
            "from": previous_role,
            "to": "native",
            "manifestBeforeSha256": role_before_sha,
            "manifestAfterSha256": role_after_sha,
        },
        "targets": {
            "input": str(target_fixture),
            "publicExpected": str(target_expected),
            "ledger": str(target_ledger),
            "producerReport": str(target_report),
            "roleManifest": str(roles_path),
        },
    }

    lock_path = reports_root / ".fixture-authority-promotion.lock"
    reports_root.mkdir(parents=True, exist_ok=True)
    try:
        lock_fd = os.open(lock_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    except FileExistsError as exc:
        raise ValueError(f"another fixture authority promotion is active: {lock_path}") from exc
    os.close(lock_fd)
    try:
        with tempfile.TemporaryDirectory(
            prefix=".fixture-authority-promotion-",
            dir=str(reports_root),
        ) as temp_dir:
            transaction_root = Path(temp_dir)
            staged_input = transaction_root / "input.json"
            staged_public = transaction_root / "public.freecad.json"
            staged_ledger = transaction_root / "ledger.freecad.ledger.json"
            staged_report = transaction_root / "producer-report.json"
            staged_roles = transaction_root / "fixture-roles.json"
            shutil.copy2(staging_fixture, staged_input)
            shutil.copy2(staging_expected, staged_public)
            shutil.copy2(staging_ledger, staged_ledger)
            write_json(staged_report, producer_report)
            write_json(staged_roles, updated_roles)
            staged_targets = [
                ("input", staged_input, target_fixture),
                ("publicExpected", staged_public, target_expected),
                ("ledger", staged_ledger, target_ledger),
                ("producerReport", staged_report, target_report),
                ("roleManifest", staged_roles, roles_path),
            ]
            _transactional_replace(staged_targets, transaction_root=transaction_root)
    finally:
        lock_path.unlink(missing_ok=True)

    promoted_artifacts = [
        {"kind": kind, **artifact(target)}
        for kind, target in (
            ("input", target_fixture),
            ("publicExpected", target_expected),
            ("ledger", target_ledger),
            ("producerReport", target_report),
            ("roleManifest", roles_path),
        )
    ]
    return {
        "schema": SCHEMA,
        "status": "passed",
        "phase": phase,
        "case": case,
        "promotedArtifacts": promoted_artifacts,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Promote one strict-valid staging fixture authority transactionally."
    )
    parser.add_argument("staging_fixture", nargs="?")
    parser.add_argument("--fixtures-root", default=str(ROOT / "fixtures"))
    parser.add_argument("--roles", default=str(ROLES_PATH))
    parser.add_argument("--reports-root", default=str(REPORTS_ROOT))
    parser.add_argument("--collect-report")
    parser.add_argument("--repeat-report")
    parser.add_argument("--promotion-report")
    parser.add_argument(
        "--revoke-post-repeat",
        help="revoke a completed promotion using a failed checked-in repeat report",
    )
    args = parser.parse_args(argv)
    try:
        if args.revoke_post_repeat:
            if args.staging_fixture or not args.promotion_report:
                parser.error(
                    "--revoke-post-repeat requires --promotion-report and no staging fixture"
                )
            receipt = revoke_fixture_authority(
                promotion_report_path=Path(args.promotion_report),
                post_repeat_report_path=Path(args.revoke_post_repeat),
                roles_path=Path(args.roles),
                reports_root=Path(args.reports_root),
            )
        else:
            if not args.staging_fixture or not args.collect_report or not args.repeat_report:
                parser.error(
                    "promotion requires staging_fixture, --collect-report and --repeat-report"
                )
            receipt = promote_fixture_authority(
                staging_fixture=Path(args.staging_fixture),
                fixtures_root=Path(args.fixtures_root),
                roles_path=Path(args.roles),
                reports_root=Path(args.reports_root),
                collect_report_path=Path(args.collect_report),
                repeat_report_path=Path(args.repeat_report),
            )
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"fixture authority promotion failed: {exc}")
        return 1
    print(json.dumps(receipt, ensure_ascii=False, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
