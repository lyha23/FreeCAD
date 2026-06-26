# 【已实现】C8-M3-S2 scope 准入与 blocker 矩阵

## 目标

把 S1 source / current coverage 复核转成 scope route、oracle plan、non-goal、blocker queue 和 implementation gate。S2 不采 oracle，不改 C++。

## live 基线

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `HEAD=6b5132ce7e`
- `git log -1 --oneline`：`6b5132ce7e docs: 完成 C8-M3 S1 源码与覆盖复核`
- S2 开始时 `git -c core.quotepath=false status --short -uall` 无输出，工作区干净。
- 队列首项是本 S2 文件；S2 完成后下一 pending 必须是 S3。

## 分类规则

| route | 使用条件 | 后续 |
| --- | --- | --- |
| `already_supported` | existing expected / focused tests 已覆盖 | S3/S6 只复核发布 |
| `oracle_candidate` | FreeCAD source 明确且可批量采集 / 比对 | S3-S5 采证据 |
| `backend_gap_candidate` | FreeCAD authority + current mismatch + code landing 清楚 | S6 实现 |
| `capability_publication_gap` | 代码/tests 已支持但 capability/docs 仍保留 active gap | S6 发布修正 |
| `non_goal` | GUI / full solver / fake TypeId / persistent cache | S4/S6 发布边界 |
| `split_required` | 调用链分叉或 oracle 风险过大 | 记录下一批范围和拆分理由 |

## 必须纳入同一轮的候选

- `PartConicCurveDTO` producer / metadata / diagnostics current coverage。
- Part consumer current coverage：Extrusion / RuledSurface；若扩展，必须覆盖同类 API 代表场景而非单 fixture。
- Sketcher ArcOfHyperbola / ArcOfParabola input / profile / external reference。
- `distance_type_default_todo` 的实现 / 发布 / non-goal 裁决。
- GUI conic edit 与 full Sketcher conic solver constraints 的 non-goal。

## 必须回写的矩阵行

- `C8M3-SCOPE-101..103`
- `C8M3-SCOPE-201..203`
- `C8M3-SCOPE-301`
- `C8M3-ORACLE-101..104`
- `C8M3-BG-101..103`
- `C8M3-BLOCKER-201`

## S2 分类结果

| 范围 | route | S2 结论 | 后续 |
| --- | --- | --- | --- |
| `C8M3-SCOPE-101` PartConicCurveDTO producer | `already_supported` | S1 已确认 Hyperbola / Parabola finite edge producer、metadata 与 invalid diagnostics 有 p8 fixture / focused test 证据。 | S3 只复核 existing batch 是否足够，不开单 fixture 扩张。 |
| `C8M3-SCOPE-102` Part conic edge consumer | `oracle_candidate` | Extrusion / RuledSurface 已有代表证据，但 S3 需裁决是否足够覆盖同类 consumer API。 | S3 若拆分，必须写 split_required reason、下一批范围和防单 fixture 闭环。 |
| `C8M3-SCOPE-103` diagnostics / metadata publication | `already_supported` | 当前 diagnostics / metadata evidence 存在；capability 发布等 S3-S5 route 完成后由 S6 同步。 | S6 publication gate。 |
| `C8M3-SCOPE-201` Sketcher conic request-local input | `oracle_candidate` | ArcOfHyperbola / ArcOfParabola input、profile 与 external reference 有 current 证据，但必须和 full solver 分开。 | S4 boundary review。 |
| `C8M3-SCOPE-202` full Sketcher solver conic constraints | `non_goal` | 只发布 solver-facing names / diagnostics，不声明完整约束求解支持。 | S4 non-goal / reopen condition。 |
| `C8M3-SCOPE-203` Sketcher external conic reference | `oracle_candidate` | external / projected conic reference 与 Sketcher conic input 同源，按代表集复核。 | S4 boundary review。 |
| `C8M3-SCOPE-301` `distance_type_default_todo` | `backend_gap_candidate` | FreeCAD Assembly DistanceType authority 与 current `default_or_todo_boundary` 让它进入 S5 gate；S2 不删除 gap，也不直接打开实现。 | S5 implementation / publication / blocker decision。 |

`gui_conic_edit` 保持 `non_goal`，`full_sketcher_solver_conic_constraints` 保持 `non_goal`。本步骤没有把它们写成 supported。

## backend gap 准入

- `C8M3-BG-101` 是当前唯一 S2 route 为 `backend_gap_candidate` 的 active gap candidate；矩阵已列出 FreeCAD authority、C++ landing 和 focused test route，并明确 S5 仍需裁决 implementation、capability publication、oracle blocker 或 non-goal。
- `C8M3-BG-102` / `C8M3-BG-103` 是 `oracle_candidate` 路由，不在 S2 发明 `backend_gap_requires_implementation`。
- `C8M3-BG-201` 保持 GUI / full solver `non_goal`；`C8M3-BG-301` 保持 capability publication gate。
- 当前没有 S2 直接落地的 `split_required` 行；若 S3-S5 后续拆分，必须记录拆分理由、下一批范围和避免长期单 fixture 推进的闭环。

## 矩阵回写

- `c8m3_conic_requestlocal_scope_review_matrix.tsv` 已把 `C8M3-SCOPE-101..103`、`201..203`、`301` 分类到 S0 状态词典。
- `c8m3_conic_requestlocal_oracle_plan.tsv` 已把 `C8M3-ORACLE-101..104` 归入 already-supported / oracle / non-goal 路线，并保留 `C8M3-ORACLE-105` 到 S5。
- `c8m3_conic_requestlocal_backend_gap_classification.tsv` 已补 FreeCAD authority、C++ landing、focused test route、split policy 和 anti-single-fixture closure path。
- `c8m3_conic_requestlocal_blocker_queue.tsv` 只关闭 `C8M3-BLOCKER-201`；`C8M3-BLOCKER-301/401/501/601` 保持后续步骤关闭。

## 验收标准

- 每个 scope 都有合法 `current_status`。
- 每个 blocker 都指向 scope 或明确 non-goal。
- 每个 `backend_gap_candidate` 都有 C++ landing、FreeCAD authority 和 focused test route。
- 每个 `split_required` 都写明为什么不能批量实现、下一批范围和避免单 fixture 推进的方法。
- 没有无证据的 `backend_gap_requires_implementation`。
- 运行：

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C8M3-SCOPE|C8M3-ORACLE|C8M3-BG|already_supported|oracle_candidate|backend_gap_candidate|capability_publication_gap|non_goal|split_required|distance_type_default_todo' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线 docs/CADCore8.0/README.md
git diff --check
```

验收通过后，将本文件重命名为 `6-27-01-03-【已实现】C8-M3-S2-scope准入与blocker矩阵.md`。

## 验收结果

已按本步骤运行以下验证；未运行 FreeCAD oracle、fixture/expected collector、C++ build、Python tests 或 Rust 验证。

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C8M3-SCOPE|C8M3-ORACLE|C8M3-BG|already_supported|oracle_candidate|backend_gap_candidate|capability_publication_gap|non_goal|split_required|distance_type_default_todo' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线 docs/CADCore8.0/README.md
git diff --check
```

本文件已按原时间前缀重命名为 `6-27-01-03-【已实现】C8-M3-S2-scope准入与blocker矩阵.md`，索引链接已更新。

## 非目标

- 不把 non-goal 写成 supported。
- 不只选单一 fixture 作为整包实现。
- 不关闭 S3-S6 blocker。
- 不采 FreeCAD oracle，不新增 fixture / expected，不改 C++ / tests / Rust。
