# 【已实现】C8-M1-S3 native oracle 批量采集与 expected 固化

## 目标

按 S2 oracle plan 批量采集 FreeCAD native expected，或记录 source-backed native blocker。S3 可以新增 collector、fixtures、expected 和 known_gap evidence；不得改 runtime C++ 主路径。

## live 基线

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `HEAD=d1aa2d0f19`（`d1aa2d0f19 docs: 完成 C8-M1 S2 oracle 候选矩阵`）
- S3 开始时 `git -c core.quotepath=false status --short -uall` 为空，未发现无关 dirty 文件。
- 队列首项是本 S3 文件；S3 完成后下一 pending 必须是 S4。

## 采集批次

| 批次 | 对应 oracle | S3 结果 |
| --- | --- | --- |
| ShapeBinder core | `C8M1-ORACLE-101..104` | collected |
| SubShapeBinder geometry | `C8M1-ORACLE-201..204` | collected |
| Downstream consumer | `C8M1-ORACLE-205` | collected |
| ElementMap | `C8M1-ORACLE-206` | collected |
| lifecycle | `C8M1-ORACLE-301..302` | `301` collected；`302` property-state collected + CopyOnChange full temp-document `known_gap` |

## S3 回写结论

- 新增 12 个 `cad-core/fixtures/c8m1/*.json`，一一覆盖 `C8M1-ORACLE-101..104`、`201..206`、`301..302`。
- 新增 `cad-core/tools/collect_c8m1_shapebinder_expected.py`。普通 `python3` 不能 import FreeCAD；collector 通过 `freecadcmd` wrapper 进入 native FreeCAD。采集版本为 `FreeCAD 1.2.0 revision 20260519`。
- 新增 12 个 `cad-core/fixtures/c8m1/expected/*.freecad.json`。`C8M1-ORACLE-101..104`、`201..206`、`301` 均为 `route=native_oracle_collected`；每个 expected 记录 `freecad_version`、`source_fixture`、`freecad_authority`、shape summary、`topology_counts`、bbox、area / volume / length、`ElementMap` 和 `childShapes`。
- `C8M1-ORACLE-302` 采到 CopyOnChange / PartialLoad 的 Python-visible property state，同时写入 `known_gap.kind=c8m1_copy_on_change_full_temporary_document_native_lifecycle_blocked`。完整 FreeCAD temporary-document copied-object cache、mutation trigger 和 `Support.setAllowPartial` 内部标志仍不可作为无状态 cad-core expected；S4 只能实现 request-local diagnostics 或产品批准的 DTO 子集。
- 当前 `cad-core/cad-core recompute cad-core/fixtures/c8m1/*.json` 均可执行完成，但 Binder 对象返回 `unsupported_type`；profile consumer 还因 Binder shape 缺失出现 downstream `missing_link_target`。因此 `C8M1-BG-101`、`201`、`301` 打开 S4 implementation gate，`C8M1-BG-401` 只对 BindMode / request-local lifecycle 子集打开，full CopyOnChange cache 保持 blocker / diagnostic。
- `C8M1-BLOCKER-301..303` 已关闭到 checked-in expected evidence；S4 后续 blocker 保持打开。

## expected 清单

| oracle | fixture | expected | S3 route |
| --- | --- | --- | --- |
| `C8M1-ORACLE-101` | `shape-binder-whole-box-cross-body.json` | `expected/shape-binder-whole-box-cross-body.freecad.json` | collected |
| `C8M1-ORACLE-102` | `shape-binder-face-edge-vertex-multi-subshape.json` | `expected/shape-binder-face-edge-vertex-multi-subshape.freecad.json` | collected |
| `C8M1-ORACLE-103` | `shape-binder-trace-support-placement.json` | `expected/shape-binder-trace-support-placement.freecad.json` | collected |
| `C8M1-ORACLE-104` | `shape-binder-datum-fallback-line-plane-point.json` | `expected/shape-binder-datum-fallback-line-plane-point.freecad.json` | collected |
| `C8M1-ORACLE-201` | `subshape-binder-basic-support-whole-face-edge-list.json` | `expected/subshape-binder-basic-support-whole-face-edge-list.freecad.json` | collected |
| `C8M1-ORACLE-202` | `subshape-binder-makeface-offset-fuse-refine.json` | `expected/subshape-binder-makeface-offset-fuse-refine.freecad.json` | collected |
| `C8M1-ORACLE-203` | `subshape-binder-setlinks-normalization-diagnostics.json` | `expected/subshape-binder-setlinks-normalization-diagnostics.freecad.json` | collected |
| `C8M1-ORACLE-204` | `subshape-binder-relative-context-nested-route.json` | `expected/subshape-binder-relative-context-nested-route.freecad.json` | collected |
| `C8M1-ORACLE-205` | `subshape-binder-profile-consumer-before-after-pad.json` | `expected/subshape-binder-profile-consumer-before-after-pad.freecad.json` | collected |
| `C8M1-ORACLE-206` | `shape-binder-subshape-binder-element-map-namedshape-body-replay.json` | `expected/shape-binder-subshape-binder-element-map-namedshape-body-replay.freecad.json` | collected |
| `C8M1-ORACLE-301` | `subshape-binder-bindmode-synchronized-frozen-detached.json` | `expected/subshape-binder-bindmode-synchronized-frozen-detached.freecad.json` | collected |
| `C8M1-ORACLE-302` | `subshape-binder-copy-on-change-disabled-enabled-mutated-partialload.json` | `expected/subshape-binder-copy-on-change-disabled-enabled-mutated-partialload.freecad.json` | diagnostic known_gap |

## 下一步

进入 S4：基于 S3 expected 实现 `PartDesign::ShapeBinder` / `PartDesign::SubShapeBinder` executor、DTO、registry、CMake、ElementMap / NamedShape propagation 和 lifecycle diagnostics。不得把 `C8M1-ORACLE-302` 的 full temporary-document CopyOnChange cache 当作必须实现的无状态后端语义。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
find cad-core/fixtures/c8m1 -maxdepth 2 -type f | sort
rg -n 'freecad_version|PartDesign::ShapeBinder|PartDesign::SubShapeBinder|ElementMap|known_gap|native_oracle_blocked|C8M1-ORACLE' cad-core/fixtures/c8m1 docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0 cad-core/fixtures/c8m1 cad-core/tools 2>/dev/null || true
git diff --check
git status --short -uall
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/工作步骤细分 --format markdown
```

验收通过后，将本文件重命名为 `6-26-16-19-【已实现】C8-M1-S3-native-oracle批量采集与expected固化.md`。

## 非目标

- 不实现 executor。
- 不修改 capability supported status。
- 不放宽 expected comparator 来容纳环境差异。
