# 【已实现】C8-M4-S2 scope 准入与 blocker 矩阵

## 目标

把 S1 证据转成可执行 route：哪些项已经支持，哪些项是 request-local backend gap，哪些项是 native oracle blocked，哪些项是 non-goal。S2 不写代码，只决定 S3-S6 的队列职责。

## live 基线

本步骤已记录：

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`37497319c6`
- `git log -1 --oneline`：`37497319c6 docs: 完成 C8-M4 S1 源码覆盖复核`
- `git status --short -uall`：无输出，开始工作区干净。
- S2 执行前 C8-M4 队列首项为 `6-27-02-36-C8-M4-S2-scope准入与blocker矩阵.md`。

## S2 分类结论

- `C8M4-SCOPE-101` / `C8M4-SCOPE-102` / `C8M4-SCOPE-103` 三项同批进入 `request_local_backend_gap_candidate`。S1 live 证据确认当前 Curve DTO 缺 G0/G1/G2 字段，`readCurveConstraints()` 遇到 criteria 字段即以 `unsupported_curve_criteria` 阻断，Curve `addCurveConstraint()` path 尚未调用 `SetG0Criterion()` / `SetG1Criterion()` / `SetG2Criterion()`；因此不能写成 `already_supported`，也不能写成单纯 stale diagnostic。
- FreeCAD native CurveConstraint setter 三项仍为 `NotImplementedError`，route 为 `native_oracle_blocked`。该 native boundary 不阻止后续 cad-core request-local product contract 在 S4/S6 独立打开。
- `PointConstraint` criteria setter 只作为 analog evidence，不是 CurveConstraint setter parity，也不是当前 Curve DTO / parser / apply path 已支持的证据。
- capability / diagnostics 是 publication pending：S5 才决定保留 diagnostic 口径或在 S4/S6 支持后发布 supported。
- GUI / TaskPanel / ViewProvider、`PlateSurface.Curves` wrapper lifecycle、persistent geometry cache 和下游持久化均为 `non_goal`。

## S3-S6 职责

- S3 只复核并记录 native `CurveConstraintPy` setter boundary，可用源码和可选 probe；不打开 cad-core implementation gate。
- S4 裁决 request-local fixture / DTO / parser / apply gate，只有 S4 明确打开后，S6 才能进入 C++ / fixture / focused test 实现。
- S5 裁决 capability / diagnostics publication 和 non-goal 口径，避免把 native blocker、request-local gap 和 GUI / wrapper / cache non-goal 混在一起。
- S6 只在 S4 打开 implementation gate 时落代码和测试；若 S4 不打开，则 S6 必须走 no-code release gate。

## 路由规则

- G0 / G1 / G2 三项必须一起进入同一 route；不得只让单字段进入 S6。
- FreeCAD native setter 仍 `NotImplemented` 时，route 为 `native_oracle_blocked`，但这不自动阻止 `cad-core` request-local product contract。
- 若 `cad-core` Curve DTO、parser 或 OCCT apply path 缺失，但三项 criteria 仍属于同一 request-local DTO / builder 边界，则 route 为 `request_local_backend_gap_candidate`；S4 再决定是否打开 implementation gate。
- 若 criteria 字段已有 tests 证明可用，但 capability 仍写 active gap，则 route 为 `capability_publication_gap`。
- GUI、wrapper lifecycle、persistent geometry cache 只能进入 `non_goal`。
- 只有 S1/S2 发现三项 criteria 不再共享同一 FreeCAD 调用链、DTO 边界或 fixture 验收口径时，才 route 为 `split_required` 并写下一批范围；当前 G0/G1/G2 不拆分。

## 必须分类的 scope row

- `C8M4-SCOPE-101`：CurveConstraint `G0Criterion`。
- `C8M4-SCOPE-102`：CurveConstraint `G1Criterion`。
- `C8M4-SCOPE-103`：CurveConstraint `G2Criterion`。
- `C8M4-SCOPE-201`：FreeCAD native `CurveConstraintPy` criteria setter parity。
- `C8M4-SCOPE-202`：PointConstraint criteria analog evidence。
- `C8M4-SCOPE-301`：capability / diagnostics publication。
- `C8M4-SCOPE-401`：`PlateSurface.Curves` wrapper lifecycle。

## blocker 队列

- `C8M4-BLOCKER-000`：S0 live baseline 与批量范围。
- `C8M4-BLOCKER-101`：S1 source/current coverage。
- `C8M4-BLOCKER-201`：S2 route 分类。
- `C8M4-BLOCKER-301`：S3 native setter boundary。
- `C8M4-BLOCKER-401`：S4 request-local fixture / DTO / parser gate。
- `C8M4-BLOCKER-501`：S5 capability / non-goal publication。
- `C8M4-BLOCKER-601`：S6 implementation / release gate。

S2 已关闭 `C8M4-BLOCKER-201`；下一 pending 必须是 S3。

## 验收标准

- `c8m4_geomplate_criteria_scope_review_matrix.tsv` 每个 scope row 都有 route、evidence、owner_step 和 next_action。
- `c8m4_geomplate_criteria_backend_gap_classification.tsv` 明确是否打开 S6 implementation gate。
- `c8m4_geomplate_criteria_non_goal_registry.tsv` 至少包含 GUI、wrapper lifecycle、persistent cache 和 native setter blocked 口径。
- `C8M4-BLOCKER-201` 关闭，下一 pending 必须是 S3。

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线/工作步骤细分 --format markdown
rg -n 'C8M4-SCOPE-101|C8M4-SCOPE-102|C8M4-SCOPE-103|request_local_backend_gap_candidate|native_oracle_blocked|non_goal|split_required' docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线/矩阵/*.tsv
git diff --check
```

## 非目标

- 不把 route 分类直接当作已实现。
- 不新增 fixture / expected / tests。
- 不修改 `cad-core` capability。
- 不修改 C++、parser、测试逻辑、collector 或 FreeCAD `src/`。
