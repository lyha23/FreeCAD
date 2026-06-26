# 【已实现】C8-M1-S0 live 基线与 Binder 批量边界冻结

## 目标

冻结 C8-M1 的声明口径、当前 live 基线、批量范围、禁止声明和状态词典。S0 不采 oracle，不改 C++，不把任何 Binder candidate 发布为 supported。

## 输入

- `docs/CADCore7.0/README.md`
- `docs/CADCore8.0/README.md`
- `cad-core/src/runtime/feature_registry.cpp`
- `src/Mod/PartDesign/App/ShapeBinder.cpp`
- `src/Mod/PartDesign/PartDesignTests/TestShapeBinder.py`
- `src/Mod/PartDesign/PartDesignTests/TestTopologicalNamingProblem.py`

## live 基线

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `HEAD=29da94dd13`（`29da94dd13 文档：完成 C7-M7 S6 发布闸门`）
- S0 开始时 `git -c core.quotepath=false status --short -uall` 只显示本 C8-M1 文档包与 `docs/CADCore8.0/README.md` 未跟踪文件，未发现无关 dirty 文件。
- 队列首项是本 S0 文件；S0 完成后下一 pending 必须是 S1。

## current cad-core registry

`cad-core/src/runtime/feature_registry.cpp` 当前注册了 `PartDesign::Body`、datum、`FeatureBase`、`Boolean`、`Fillet`、`Draft`、`Thickness`、`Hole`、Pattern、Pad/Pocket、Pipe/Loft、Revolution/Groove、Scaled、Chamfer 等 executor；没有 `PartDesign::ShapeBinder`、`PartDesign::SubShapeBinder` 或 `PartDesign::SubShapeBinderPython` 注册项。因此 S0 只能冻结为 `backend_gap_candidate` / `oracle_candidate`，不能声明 supported 或 `backend_gap_requires_implementation`。

## 范围冻结

| 类型 | 纳入 C8-M1 | S0 结论 |
| --- | --- | --- |
| ShapeBinder whole / subshape / multi-subshape | 是 | 同一 `ShapeBinder.cpp` 调用链 |
| ShapeBinder datum fallback (`App::Line` / `App::Plane` / `App::Point`) | 是 | `buildShapeFromReferences()` 同链路，S3 采 native 证据 |
| ShapeBinder `TraceSupport` | 是 | placement 语义必须进 S3 oracle |
| SubShapeBinder support / MakeFace / Fuse / Offset / Refine | 是 | 同一 `SubShapeBinder::update()` 调用链 |
| Relative / Context / nested `getSubObject()` | 是 | lifecycle 和 placement 高风险边界 |
| BindMode / CopyOnChange / PartialLoad | 是 | 本包审计，不能跳过；只允许 request-local 子集或显式 blocker |
| ElementMap / NamedShape / Body replay | 是 | Binder 输出不能退化成 display-only shape |
| GUI / ViewProvider / TaskPanel | 否 | non-goal |
| 跨请求 backend session / persistent BREP / shape cache | 否 | non-goal |
| C7-M7 imported ElementMap / ShowElement persistent writeback | 否 | 已是 C7-M7 oracle-blocked，不重开 |

## 状态词典

- `already_covered`：current `cad-core` 已有 expected-backed 支持。
- `supported`：S5/S6 之后才允许使用的发布状态，必须同时有 source-backed expected 与 current pass。
- `oracle_candidate`：FreeCAD source 明确，S3 需要采 native expected。
- `backend_gap_candidate`：有 FreeCAD source 与 current 缺口迹象，待 S3/S4 裁决。
- `backend_gap_requires_implementation`：S3 expected 与 current mismatch 都成立，S4 必须实现。
- `oracle_blocker`：FreeCAD 行为可触发，但 collector 或观测路径在 S3 前尚未稳定。
- `oracle_blocked`：FreeCAD native lifecycle 不可观察或不稳定，不能实现。
- `diagnostic_non_goal`：明确不属于无状态 CAD Core 后端。
- `unsupported_type`：current cad-core registry 缺失时的诊断观察值，不是发布状态；S4 前只能作为 `backend_gap_candidate` 的证据。

## 必须回写的矩阵行

- `C8M1-SCOPE-101..103`
- `C8M1-SCOPE-201..205`
- `C8M1-SCOPE-301..302`
- `C8M1-NG-001..005`
- `C8M1-BLOCKER-000`

## S0 回写结论

- `C8M1-SCOPE-101..103`、`C8M1-SCOPE-201..205`、`C8M1-SCOPE-301..302` 保持 candidate / oracle 状态，不提升为 supported。
- `C8M1-NG-001..005` 冻结 GUI、跨请求 session、persistent BREP / shape cache、完整 FreeCAD temp-document CopyOnChange、下游 Rust adapter 为 `diagnostic_non_goal` 边界。
- `C8M1-BLOCKER-000` 已由 live baseline、queue 首项和 registry 复核关闭；后续 S1 从 source authority 与 current cad-core coverage 复核继续。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
git status --short -uall
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/工作步骤细分 --format markdown
rg -n 'PartDesign::ShapeBinder|PartDesign::SubShapeBinder|backend_gap_candidate|diagnostic_non_goal|C8M1-SCOPE' docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/矩阵/*.tsv
git diff --check
```

验收通过后，将本文件按原时间前缀重命名为 `6-26-16-16-【已实现】C8-M1-S0-live基线与Binder批量边界冻结.md`，并更新索引链接。

## 非目标

- 不新增 collector / fixture / expected。
- 不修改 `cad-core/src`。
- 不声明 ShapeBinder / SubShapeBinder supported。
