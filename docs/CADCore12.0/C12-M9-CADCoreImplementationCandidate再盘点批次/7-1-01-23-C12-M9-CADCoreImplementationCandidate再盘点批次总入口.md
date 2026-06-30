# C12-M9 CAD Core implementation candidate 再盘点批次总入口

本文是 `docs/CADCore12.0` 下 C12-M8 之后的候选再盘点主线。

C12-M9 不直接落 C++。它重新消费 live capability、C12-M1..M8 关闭口径、current tests 与 `narrowed_gaps`，并只在 stable expected / product contract、request-local boundary、current mismatch 三项同时成立时，授权后续 implementation package。

## 主线目标

- 冻结 C12-M8 后的 live capability 和 dirty boundary。
- 防止把唯一 live remaining gap CopyOnChange 在缺 S2/S3/S4 证据时直接重开为实现包。
- 逐项区分 active remaining gap、known gap、product-contract non-parity、native-hidden、helper-blocked、current-covered 与 true implementation candidate。
- 让 S5/S6 输出明确的后续分流：implementation package、oracle / product-contract package，或 no-code backlog gate。

## 当前基线

- 创建基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- 创建基线：`HEAD=75e7a58723`（`75e7a58723 docs: 关闭 C12-M8 S6 发布闸门`）。
- 创建时 `git -c core.quotepath=false status --short -uall` 无输出，worktree clean。
- C12-M1..M8 队列均只输出表头。
- live capability 目前唯一非空 `remaining_gaps` 是 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`。
- C12-M8 已发布 `no_code_retained_diagnostic`，没有 C12-M9 implementation package，没有 C++ authorization。

## 证明链条

```text
live capability baseline
  -> C12-M1..M8 release gate inheritance
  -> remaining_gaps / known_gaps extraction
  -> narrowed_gaps and product-contract inventory
  -> stable expected / product contract / current mismatch gate
  -> implementation authorization or no-code backlog gate
```

## FreeCAD / CAD Core 依据

| 语义 | 依据 | 初始 C12-M9 口径 |
| --- | --- | --- |
| CopyOnChange | `src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::setupCopyOnChange()` / `update()`；C12-M8 S2-S6 | Retained blocker，不默认实现。 |
| Groove UpTo | `src/Mod/PartDesign/App/FeatureGroove.cpp`、`FeatureRevolved.cpp`、`TopoShapeExpansion.cpp`；C12-M7 | Product diagnostic contract，非 native parity success。 |
| RuledSurface wire/wire | `src/Mod/Part/App/PartFeatures.cpp::RuledSurface::execute()`；`TopoShapeExpansion.cpp::makeElementRuledSurface()`；C12-M6 | Current-supported，只有 regression / mismatch 才重开。 |
| ProjectOnSurface | `src/Mod/Part/App/FeatureProjectOnSurface.cpp`；C12-M3 / C12-M4 | Request-local ledger 是产品契约，native mapper/history oracle unavailable。 |
| Sweep / Filling / GeomPlate / Loft | `src/Mod/Part/App` 对应 feature/helper；C12-M2 与 capability `narrowed_gaps` | 多数为 helper blocked、native hidden 或 product-contract non-parity。 |
| Assembly | `src/Mod/Assembly/App/AssemblyObject.cpp`；capability assembly rows | Current-covered request-local subset，不重开 persistent solver state。 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| README | `README.md` | C12-M9 当前定位和入口。 |
| 方案 | `7-1-01-23-C12-M9-CADCoreImplementationCandidate再盘点批次方案.md` | 批次规则、步骤安排和验收分层。 |
| 工作步骤总入口 | `工作步骤细分/7-1-01-24-【已实现】C12-M9工作步骤总入口.md` | goal 队列索引，已关闭。 |
| S0 | `工作步骤细分/7-1-01-25-【已实现】C12-M9-S0-live基线与继承口径冻结.md` | 冻结 C12-M8 后 live baseline，已关闭。 |
| S1 | `工作步骤细分/7-1-01-26-C12-M9-S1-live-capability与remaining-gap抽取.md` | 抽取 remaining / known gaps。 |
| S2 | `工作步骤细分/7-1-01-27-C12-M9-S2-narrowed-gaps与产品契约归类.md` | 归类 narrowed gaps 和 non-parity。 |
| S3 | `工作步骤细分/7-1-01-28-C12-M9-S3-expected与current-mismatch准入.md` | 过滤 stable expected / mismatch。 |
| S4 | `工作步骤细分/7-1-01-29-C12-M9-S4-最高优先候选source与验证范围复核.md` | 复核 candidate source / landing。 |
| S5 | `工作步骤细分/7-1-01-30-C12-M9-S5-implementation-package-authorization裁决.md` | 授权实现包或关闭。 |
| S6 | `工作步骤细分/7-1-01-31-C12-M9-S6-发布闸门与后续分流.md` | 发布最终状态。 |
| 矩阵 | `矩阵/` | source、scope、classification、blocker、non-goal、validation。 |

## 执行规则

1. 每步开始前执行 live baseline：`pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git -c core.quotepath=false status --short -uall`。
2. 每步执行前刷新 C12-M9 队列；只处理当前第一条未完成 step。
3. S0-S4 默认只改本包 docs / matrices，不改 `cad-core/src`、`include`、fixtures、expected、tests、adapters 或 capability source。
4. 只有 S5 同时确认 stable expected / product contract、request-local boundary 和 current mismatch，才允许输出后续 implementation package。
5. 不把 C12-M8 的 retained CopyOnChange blocker、C12-M7 的 Groove product diagnostic contract、C12-M6 的 current-supported RuledSurface 或 C12-M4 的 ProjectOnSurface ledger contract误写成未实现 C++。
6. 每步完成后重命名为 `【已实现】` 并更新 README / 总入口 / 矩阵中对应状态。

## 当前执行状态

- 工作步骤总入口已标记为 `【已实现】`，仅承担 C12-M9 S0-S6 队列索引职责。
- 入口关闭执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=982ee025ce`（`982ee025ce fix: 修复 Body 替换 Tip 后 refined 继承`），起点 worktree clean。
- 关闭前队列显示入口与 S0-S6 pending；入口关闭后队列从 S0 继续。
- C12-M9 TSV 字段数检查通过；本入口未执行 S0-S6 实质盘点，未修改 `cad-core/src`、`include`、fixtures、expected、tests、adapters 或 capability source。
- S0 live 基线与继承口径已冻结：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=d7602e1bd2`（`d7602e1bd2 文档：关闭 C12-M9 工作步骤总入口`），起点 worktree clean；C12-M1..M8 队列均只输出表头，C12-M9 S0 执行前为第一项。
- S0 capability snapshot 为 `/tmp/c12m9-s0-capabilities.json`：唯一非空 `remaining_gaps` 为 `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`，known gap 继续是 `known_gap_diagnostic` / `oracle_blocked` / `copy_on_change_full_temporary_document_cache_not_supported`；`narrowed_gaps` presence 位于 `part_design.revolution_groove`、`part_workbench.filling`、`part_workbench.geomplate`、`part_workbench.loft`、`part_workbench.project_on_surface`、`part_workbench.sweep`。
- S0 只冻结 C12-M8 retained diagnostic 继承口径：S2=`native_evidence_retained_blocker`，S3=`dto_not_reviewed_due_to_native_blocker`，S4=`no_current_mismatch_retained_diagnostic`，S5=`no_code_retained_diagnostic`；未执行 S1-S6 盘点，未运行 FreeCADCmd，未修改 production code、fixtures、expected、tests、adapters 或 capability source。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M9-CADCoreImplementationCandidate再盘点批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M9-CADCoreImplementationCandidate再盘点批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M9-CADCoreImplementationCandidate再盘点批次 docs/CADCore12.0/README.md
git diff --check
```
