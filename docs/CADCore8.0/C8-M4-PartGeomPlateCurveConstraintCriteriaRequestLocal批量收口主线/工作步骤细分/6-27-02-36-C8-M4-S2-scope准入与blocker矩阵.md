# C8-M4-S2 scope 准入与 blocker 矩阵

## 目标

把 S1 证据转成可执行 route：哪些项已经支持，哪些项是 request-local backend gap，哪些项是 native oracle blocked，哪些项是 non-goal。S2 不写代码，只决定 S3-S6 的队列职责。

## 路由规则

- G0 / G1 / G2 三项必须一起进入同一 route；不得只让单字段进入 S6。
- FreeCAD native setter 仍 `NotImplemented` 时，route 为 `native_oracle_blocked`，但这不自动阻止 `cad-core` request-local product contract。
- 若 `cad-core` DTO 和 OCCT apply path 完整，只剩 parser / diagnostic 阻断，则 route 为 `request_local_backend_gap_candidate`。
- 若 criteria 字段已有 tests 证明可用，但 capability 仍写 active gap，则 route 为 `capability_publication_gap`。
- GUI、wrapper lifecycle、persistent geometry cache 只能进入 `non_goal`。
- 若 S1 发现 DTO / builder / tests 不足以批量覆盖三项 criteria，必须 route 为 `split_required` 并写下一批范围。

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
