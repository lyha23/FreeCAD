# C8-M3-S2 scope 准入与 blocker 矩阵

## 目标

把 S1 source / current coverage 复核转成 scope route、oracle plan、non-goal、blocker queue 和 implementation gate。S2 不采 oracle，不改 C++。

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
git diff --check
```

验收通过后，将本文件重命名为 `6-27-01-03-【已实现】C8-M3-S2-scope准入与blocker矩阵.md`。

## 非目标

- 不把 non-goal 写成 supported。
- 不只选单一 fixture 作为整包实现。
- 不关闭 S3-S6 blocker。
