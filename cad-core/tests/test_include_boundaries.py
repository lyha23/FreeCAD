from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PUBLIC_INCLUDE = ROOT / "include" / ("cad_" + "core")
ALLOWED_PUBLIC_MODULES = {
    "adapters",
    "app",
    "assembly",
    "base",
    "graph",
    "mesh",
    "part",
    "part_design",
    "runtime",
    "sketcher",
    "topo",
}
LEGACY_MODULES = ("document", "features", "geometry")
SKIP_DIRS = {"build", "graphify-out", "__pycache__", ".cache", ".pytest_cache", "cache"}
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".py"}


def source_files() -> list[Path]:
    files: list[Path] = []
    for root in (ROOT / "include", ROOT / "src", ROOT / "tests"):
        for path in root.rglob("*"):
            if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
                continue
            if any(part in SKIP_DIRS for part in path.relative_to(ROOT).parts):
                continue
            files.append(path)
    return files


def line_number(text: str, index: int) -> int:
    return text.count("\n", 0, index) + 1


class IncludeBoundaryTest(unittest.TestCase):
    def test_public_include_root_contains_only_module_directories(self) -> None:
        unexpected: list[str] = []
        for path in PUBLIC_INCLUDE.iterdir():
            if path.is_dir() and path.name in ALLOWED_PUBLIC_MODULES:
                continue
            unexpected.append(path.relative_to(ROOT).as_posix())

        self.assertEqual(sorted(unexpected), [])

    def test_legacy_public_include_directories_are_absent(self) -> None:
        forbidden = [PUBLIC_INCLUDE / module for module in LEGACY_MODULES]
        forbidden.append(PUBLIC_INCLUDE / "compatibility")

        existing = [path.relative_to(ROOT).as_posix() for path in forbidden if path.exists()]

        self.assertEqual(existing, [])

    def test_readme_describes_only_current_include_modules(self) -> None:
        text = (ROOT / "README.md").read_text(encoding="utf-8")
        layout_modules = "|".join(re.escape(module) for module in (*LEGACY_MODULES, "compatibility"))
        patterns = [
            re.compile(rf"^\s*(?:{layout_modules})/\s+", re.MULTILINE),
            re.compile(r"\blegacy\s+(?:compatibility\s+)?facade\b", re.IGNORECASE),
            re.compile(r"\bcompatibility\s+facade\b", re.IGNORECASE),
        ]

        violations: list[str] = []
        for pattern in patterns:
            for match in pattern.finditer(text):
                violations.append(f"README.md:{line_number(text, match.start())}: {match.group(0)}")

        self.assertEqual(violations, [])

    def test_legacy_include_paths_are_not_used(self) -> None:
        prefix = "cad_" + "core"
        modules = "|".join(re.escape(module) for module in LEGACY_MODULES)
        pattern = re.compile(rf"^\s*#\s*include\s*[<\"]{prefix}/(?:{modules})/", re.MULTILINE)

        violations = self._scan(pattern)

        self.assertEqual(violations, [])

    def test_legacy_namespaces_are_not_used(self) -> None:
        prefix = "cad_" + "core"
        modules = "|".join(re.escape(module) for module in LEGACY_MODULES)
        patterns = [
            re.compile(rf"^\s*namespace\s+{prefix}::(?:{modules})\b", re.MULTILINE),
            re.compile(rf"\b{prefix}::(?:{modules})\b"),
            re.compile(rf"\b{prefix}::(?:{modules})::"),
            re.compile(rf"^\s*using\s+namespace\s+{prefix}::part\s*;", re.MULTILINE),
        ]

        violations: list[str] = []
        for pattern in patterns:
            violations.extend(self._scan(pattern))

        self.assertEqual(violations, [])

    def _scan(self, pattern: re.Pattern[str]) -> list[str]:
        violations: list[str] = []
        for path in source_files():
            text = path.read_text(encoding="utf-8", errors="ignore")
            for match in pattern.finditer(text):
                relative = path.relative_to(ROOT).as_posix()
                violations.append(f"{relative}:{line_number(text, match.start())}: {match.group(0)}")
        return violations


if __name__ == "__main__":
    unittest.main()
