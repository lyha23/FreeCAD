# 【已实现】C8-M1-S2 ShapeBinder / SubShapeBinder oracle 候选矩阵

## 目标

把 S1 source / current coverage 复核转成批量 oracle 候选、blocker route、non-goal 和 implementation gate。S2 不采 oracle，不改 C++。

## 分类规则

S2 只写候选 / 边界词，不发布 expected-backed 支持状态，也不打开 S4 implementation gate。

| route | 使用条件 | 后续 |
| --- | --- | --- |
| `already_covered` | current `cad-core` 已有 expected-backed 支持 | S3 不重复采 |
| `oracle_candidate` | FreeCAD source 明确，native expected 可采 | S3 批量采集 |
| `backend_gap_candidate` | source 明确且 current cad-core 缺口明显，但缺 native expected | S3 后由 S4 裁决 |
| `oracle_blocker` | FreeCAD 行为可触发但 collector 暂不可稳定观测 | S3 记录 blocker |
| `oracle_blocked` | FreeCAD native lifecycle 不可观察或不稳定 | S4/S5 只能发布 blocker / diagnostic |
| `diagnostic_non_goal` | GUI/session/cross-request state/Rust 下游 | S5 发布边界 |
| `unsupported_type` | current cad-core registry 缺失诊断观察值 | 只能作为 candidate 证据 |

## 必须纳入同一轮的 oracle 候选

- ShapeBinder whole object support。
- ShapeBinder selected Face / Edge / Vertex。
- ShapeBinder multi-subshape compound。
- ShapeBinder `TraceSupport=true` placement。
- SubShapeBinder whole / subshape / edge-list support。
- SubShapeBinder `MakeFace` / `Offset` / `Fuse` / `Refine`。
- SubShapeBinder as downstream profile consumer。
- ShapeBinder / SubShapeBinder ElementMap preservation。
- BindMode / CopyOnChange lifecycle probe。

## 必须回写的矩阵行

- `C8M1-ORACLE-101..104`
- `C8M1-ORACLE-201..206`
- `C8M1-ORACLE-301..302`
- `C8M1-BG-101..401`
- `C8M1-BLOCKER-201`

## live 基线

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `HEAD=6e8d3c781a`（`6e8d3c781a docs: 完成 C8-M1 S1 源码覆盖复核`）
- S2 开始时 `git -c core.quotepath=false status --short -uall` 为空，未发现无关 dirty 文件。
- 队列首项是本 S2 文件；S2 完成后下一 pending 必须是 S3。

## S2 回写结论

- `c8m1_shapebinder_oracle_plan.tsv`：`C8M1-ORACLE-101..104`、`201..206`、`301..302` 已全部写成 S3 批量 `oracle_candidate`。候选覆盖 ShapeBinder whole、Face / Edge / Vertex、多 subshape compound、`TraceSupport` placement、datum fallback，SubShapeBinder whole / subshape / edge-list、`MakeFace` / `Offset` / `Fuse` / `Refine`、`Relative` / `Context` / nested `getSubObject()`、downstream profile consumer、`ElementMap` / `NamedShape` / Body replay，以及 `BindMode` / `CopyOnChange` / `PartialLoad` lifecycle probe。
- `C8M1-ORACLE-104`、`C8M1-ORACLE-204`、`C8M1-ORACLE-302` 只预留 S3 转 `oracle_blocker` 或 `oracle_blocked` 的通道；S2 不因 datum、nested route 或 CopyOnChange 复杂度跳过采集计划。
- `c8m1_shapebinder_backend_gap_classification.tsv`：`C8M1-BG-101..401` 保持 `backend_gap_candidate`。当前 registry 缺失、executor 缺失、ElementMap 差异和 lifecycle 差异都只是 candidate evidence；只有 S3 expected 与 current mismatch 同时成立后，S4 才能打开 implementation gate。
- `c8m1_shapebinder_blocker_queue.tsv`：`C8M1-BLOCKER-201` 已关闭到 S2 矩阵完整性，关闭条件是全部 `C8M1-ORACLE` 行按批量候选存在；真正 native oracle / collector blocker 留给 S3 的 `C8M1-BLOCKER-301..303`。
- `c8m1_shapebinder_non_goal_registry.tsv`：GUI / ViewProvider / TaskPanel、跨请求 backend session、persistent BREP / shape cache、完整 FreeCAD temporary-document CopyOnChange、下游 Rust adapter、adapter/output patch 和 C7-M7 imported ElementMap / ShowElement persistent writeback 均为 `diagnostic_non_goal`。
- `c8m1_shapebinder_validation_matrix.tsv`：`C8M1-VAL-201` 已同步 S2 grep，覆盖 `C8M1-ORACLE`、`oracle_candidate`、`backend_gap_candidate`、`oracle_blocker`、`diagnostic_non_goal`、`CopyOnChange`、`BindMode`、`TraceSupport` 和 `ElementMap`。

## 下一步

进入 S3 批量 native oracle 采集。S3 可以新增 collector / fixture / expected / known-gap evidence，但不能把 S2 的 source-only 或 registry 缺失证据直接当作 C++ implementation gate。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C8M1-ORACLE|oracle_candidate|backend_gap_candidate|oracle_blocker|diagnostic_non_goal|CopyOnChange|BindMode|TraceSupport|ElementMap' docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0
git diff --check
git status --short -uall
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/工作步骤细分 --format markdown
```

验收通过后，将本文件重命名为 `6-26-16-18-【已实现】C8-M1-S2-ShapeBinderSubShapeBinder-oracle候选矩阵.md`。

## 非目标

- 不把 `backend_gap_candidate` 当作 S4 implementation gate。
- 不因 CopyOnChange 复杂而跳过审计；只能在 S3/S4 写出 blocker 或 non-goal。
