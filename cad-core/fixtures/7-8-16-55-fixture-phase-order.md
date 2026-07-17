# cad-core fixture 模块能力分类

基线刷新：2026-07-17。本文只说明 fixture corpus 的目录语义和权威边界；覆盖状态以 live manifest 与报告为准。

## 目录合同

fixture 输入保持一层结构：

```text
fixtures/<phase>/<case>.json
```

`<phase>` 现在统一表示“FreeCAD 模块 + 公开能力”，例如 `part-extrude`、`topology-resolve`、`spreadsheet-recompute`。不再用 `p5`、`c3m4`、`c51m3` 等实施里程碑作为目录分类。

- `expected/<case>.freecad.json`：FreeCADCmd 生成的 public expected。
- `expected/<case>.freecad.ledger.json`：与 public expected 同次生成的 authority ledger。
- `cad-core-res/<case>.cad-core.json`：CAD Core 当前对照输出，不是 FreeCAD 权威。
- `_assets/`：多个 fixture 共用的 BREP、STEP、IGES、STL 输入资源。
- `_artifacts/`：历史构建/采集 receipt，不参与 fixture 输入发现。
- `producer_trace_representatives/`：诊断 trace 样本，不是 phase。

机器可读分类由以下文件维护：

- `tools/freecad_expected_parity/fixture_capability_phases.v1.json`：允许的模块能力 phase；审计器 fail closed 校验。
- `tools/freecad_expected_parity/fixture_legacy_phase_map.v1.json`：旧目录到新目录的可追溯迁移表。
- `tools/freecad_expected_parity/fixture_roles.v1.json`：每个 case 的 `native`、`protocol_only`、`unsupported` 角色。

迁移表保留 783 条旧路径记录，当前 corpus 也保留 783 个输入：564 个 `native`、14 个 `protocol_only`、205 个 `unsupported`。三个输入相同但权威谱系不同的 Loft 镜像使用 `-legacy-c5m3` case 后缀保留，避免合并不同的 public expected/ledger mapped-name 证据。

## 当前 39 个模块能力 phase

| # | phase | 模块 | 能力 | 输入 | native | protocol only | unsupported |
| ---: | --- | --- | --- | ---: | ---: | ---: | ---: |
| 1 | `app-links` | App | links | 39 | 35 | 0 | 4 |
| 2 | `app-properties` | App | properties | 3 | 3 | 0 | 0 |
| 3 | `assembly-links` | Assembly | links | 3 | 3 | 0 | 0 |
| 4 | `assembly-solve` | Assembly | solve | 74 | 74 | 0 | 0 |
| 5 | `material-properties` | Material | properties | 3 | 3 | 0 | 0 |
| 6 | `mesh-import` | Mesh | import | 3 | 3 | 0 | 0 |
| 7 | `part-boolean` | Part | boolean | 7 | 7 | 0 | 0 |
| 8 | `part-extrude` | Part | extrude | 21 | 12 | 0 | 9 |
| 9 | `part-filling` | Part | filling | 28 | 26 | 2 | 0 |
| 10 | `part-geomplate` | Part | geomplate | 17 | 17 | 0 | 0 |
| 11 | `part-import` | Part | import | 5 | 5 | 0 | 0 |
| 12 | `part-loft` | Part | loft | 12 | 8 | 0 | 4 |
| 13 | `part-offset` | Part | offset | 9 | 6 | 0 | 3 |
| 14 | `part-primitives` | Part | primitives | 28 | 27 | 0 | 1 |
| 15 | `part-project-on-surface` | Part | project-on-surface | 17 | 15 | 0 | 2 |
| 16 | `part-ruled-surface` | Part | ruled-surface | 5 | 4 | 0 | 1 |
| 17 | `part-shapefix` | Part | shapefix | 2 | 0 | 0 | 2 |
| 18 | `part-sweep` | Part | sweep | 23 | 21 | 0 | 2 |
| 19 | `part-thickness` | Part | thickness | 2 | 2 | 0 | 0 |
| 20 | `partdesign-binder` | PartDesign | binder | 14 | 6 | 0 | 8 |
| 21 | `partdesign-body` | PartDesign | body | 24 | 12 | 0 | 12 |
| 22 | `partdesign-boolean` | PartDesign | boolean | 13 | 5 | 0 | 8 |
| 23 | `partdesign-datum` | PartDesign | datum | 24 | 18 | 0 | 6 |
| 24 | `partdesign-dressup` | PartDesign | dressup | 23 | 19 | 0 | 4 |
| 25 | `partdesign-extrude` | PartDesign | extrude | 51 | 32 | 0 | 19 |
| 26 | `partdesign-hole` | PartDesign | hole | 52 | 19 | 0 | 33 |
| 27 | `partdesign-loft` | PartDesign | loft | 9 | 7 | 0 | 2 |
| 28 | `partdesign-pattern` | PartDesign | pattern | 31 | 25 | 0 | 6 |
| 29 | `partdesign-pipe` | PartDesign | pipe | 25 | 13 | 0 | 12 |
| 30 | `partdesign-revolve` | PartDesign | revolve | 27 | 15 | 0 | 12 |
| 31 | `runtime-limits` | Runtime | limits | 1 | 1 | 0 | 0 |
| 32 | `sketcher-external-geometry` | Sketcher | external-geometry | 41 | 28 | 5 | 8 |
| 33 | `sketcher-geometry` | Sketcher | geometry | 18 | 14 | 0 | 4 |
| 34 | `sketcher-internal-shape` | Sketcher | internal-shape | 27 | 26 | 0 | 1 |
| 35 | `sketcher-solve` | Sketcher | solve | 44 | 28 | 0 | 16 |
| 36 | `spreadsheet-recompute` | Spreadsheet | recompute | 3 | 3 | 0 | 0 |
| 37 | `topology-element-map` | Topology | element-map | 9 | 2 | 0 | 7 |
| 38 | `topology-resolve` | Topology | resolve | 36 | 11 | 6 | 19 |
| 39 | `topology-state` | Topology | state | 10 | 9 | 1 | 0 |
| **合计** | **39 phases** |  |  | **783** | **564** | **14** | **205** |

## 权威与覆盖结论不可混写

以下三件事必须分别报告：

1. **fixture corpus closure**：输入、角色、native expected/ledger 是否严格闭包。目前为 `passed`。
2. **模块 API coverage**：公开能力/主要分支是否都有代表性 fixture。目前仍为 `partial`；报告明确保留 thin、uncovered 和 non-native exception。
3. **CAD Core runtime parity**：CAD Core 是否与 native public expected 等价。目录迁移和权威闭包不证明这一点，目前总报告为 `not_evaluated`。

因此，retained-module coverage gate 通过不能写成“全部 FreeCAD API 已覆盖”，native expected 可复现也不能写成“CAD Core runtime 已 parity”。

## 常用命令

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core

# 审计目录角色、模块 coverage 与公开能力反向清单
python3 tools/audit_freecad_fixture_authority.py \
  --report tools/freecad_expected_parity/reports/fixture_authority_inventory.v1.json \
  --coverage-report tools/freecad_expected_parity/reports/retained_module_fixture_coverage.v1.json \
  --capability-report tools/freecad_expected_parity/reports/retained_public_capability_coverage.v1.json \
  --producer-report tools/freecad_expected_parity/reports/all-native-check.v1.json \
  --non-cad-smoke-root tools/freecad_expected_parity/reports/non_cad_smoke \
  --require-coverage-passed

# 严格验证全部 native public expected/ledger
python3 tools/validate_freecad_expected_ledger.py --all --strict \
  --report tools/freecad_expected_parity/reports/ledger-strict-validation.v1.json

# 单个模块能力 phase 的 native 可复现检查
FREECADCMD=/Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd \
python3 tools/collect_freecad_expected.py \
  --phase material-properties --check --skip-unsupported --validate-ledger
```

`*.freecad.json` 与 `*.freecad.ledger.json` 都是 collector-owned artifact，不得手改。共享资源路径变化若改变 DocumentObject graph，必须先刷新输入的 `topoNamingState.documentHash`，再由同一次 FreeCADCmd 重新生成 public expected 与 ledger。
