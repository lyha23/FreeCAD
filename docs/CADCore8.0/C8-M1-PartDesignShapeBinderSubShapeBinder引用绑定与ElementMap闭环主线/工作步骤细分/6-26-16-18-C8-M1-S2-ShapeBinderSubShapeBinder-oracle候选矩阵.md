# C8-M1-S2 ShapeBinder / SubShapeBinder oracle 候选矩阵

## 目标

把 S1 source / current coverage 复核转成批量 oracle 候选、blocker route、non-goal 和 implementation gate。S2 不采 oracle，不改 C++。

## 分类规则

| route | 使用条件 | 后续 |
| --- | --- | --- |
| `already_covered` | current `cad-core` 已有 expected-backed 支持 | S3 不重复采 |
| `supported` | source-backed expected 与 current pass 都成立后的发布状态 | 只能由 S5/S6 发布 |
| `oracle_candidate` | FreeCAD source 明确，native expected 可采 | S3 批量采集 |
| `backend_gap_candidate` | source 明确且 current cad-core 缺口明显，但缺 native expected | S3 后由 S4 裁决 |
| `backend_gap_requires_implementation` | S3 expected 与 current mismatch 都成立 | S4 必须实现 |
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

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C8M1-ORACLE|oracle_candidate|backend_gap_candidate|oracle_blocker|diagnostic_non_goal|CopyOnChange|BindMode|TraceSupport|ElementMap' docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/矩阵/*.tsv
git diff --check
```

验收通过后，将本文件重命名为 `6-26-16-18-【已实现】C8-M1-S2-ShapeBinderSubShapeBinder-oracle候选矩阵.md`。

## 非目标

- 不把 `backend_gap_candidate` 当作 S4 implementation gate。
- 不因 CopyOnChange 复杂而跳过审计；只能在 S3/S4 写出 blocker 或 non-goal。
