# 【已实现】C12-M11 S4 后续最小语义批次

## 完成结论

- 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=1ff42cb52a`（`1ff42cb52a 文档：关闭 C12-M11 S3 contract gap 分流裁决`），起点 dirty boundary 为 `<clean>`。
- S3 裁决继续保留：closed `p5/sketch-internal-face` backend response 为 `current_supported`，edgeSegments/subshapes alignment 为 `mismatch_absent`，request-local `stableSubname=Edge1..4` 已 passed；本步不打开 closed profile backend C++ implementation package。
- `C12M11-BLOCKER-401` 已关闭，`C12M11-VAL-401` 已记录为 passed；S4 后续最小完整语义批次定义为三个彼此独立的 follow-up，不把它们合并成一个后端大改。

## 后续最小完整语义批次

1. `my-chili3d-C12M11-SketchEdgeTokenConsumerSync批次`：落点在 `my-chili3d` response consume、selection persistence、sketch commit writeback 和 pick token storage。该批次只消费后端返回的 object-qualified `id`、`subname`、`stableSubname` 与 `indexed`；禁止前端 prefix guessing，禁止前端发明长期 topology identity。
2. `C12-M11-StableGeometryIdMappedNameLedger设计批次`：落点在 FreeCAD source authority 与 `cad-core` topo / sketch geometry id 或 mapped-name ledger 设计。必须先设计 request-local geometry id / mapped-name 账本，再决定是否升级 `stableSubname`；不能只靠当前 `EdgeN` 顺序声称 FreeCAD-grade 跨编辑稳定。
3. `C12-M11-OpenWireRawEdgeMeshProductContract裁决批次`：落点在 open sketch raw `EdgeN` 产品契约。先裁决 open wire 是否需要发布 raw `EdgeN` mesh `edgeSegments`，再决定是否进入实现；当前 `p5/sketch-open-wire-internal-empty` 的 raw `Sketch:Edge1..3` subshapes 可见且 `mesh=null`，不等同 closed profile `InternalEdgeN` 缺失。

## S5 输入

- S5 只发布 C12-M11 最终状态和后续分流：closed internal profile backend contract 为 current-supported，后续输入为前端同步、stable-id ledger 设计、open-wire 产品契约三条。
- S5 不重新裁决 closed profile backend implementation，不修改 C12-M10 口径，不创建后续包，除非用户另行授权。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M11-SketchInternalEdgeSubshapeMeshContract批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M11-SketchInternalEdgeSubshapeMeshContract批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M11-SketchInternalEdgeSubshapeMeshContract批次 docs/CADCore12.0/README.md
git diff --check
```

前端同步验证：

- 在 `my-chili3d` 侧运行或补充 response consume、selection persistence、sketch commit writeback、pick token storage 的 focused tests；若测试尚未落地，手工检查 recompute response、selection payload 和提交写回 JSON，确认引用来自后端 `subname` / `stableSubname` / `indexed`，没有 prefix guessing。

stable-id follow-up 设计验证：

- 复核 `src/Mod/Sketcher/App/SketchObject.cpp::updateGeoHistory()`、`generateId()` 与 `src/Mod/Sketcher/App/SketchObjectGeometry.cpp::getEdge()`，写出 geometry id / mapped-name ledger 设计与 `cad-core` topo/sketch 落点；未完成账本设计前，不把 `EdgeN` 顺序升级为 FreeCAD-grade stable id。

open-wire 产品契约验证：

- 独立复核 open sketch raw `EdgeN` subshapes 与 `mesh.edgeSegments` 的产品需求，明确 `mesh=null` 是否可接受；不得把 closed profile `InternalEdgeN` mesh 规则直接套到 open wire。

## 非目标

- 不在本步骤直接实现。
- 不修改 C++、tests、fixtures、expected 或 adapters。
- 不写 fixture-specific patch。
- 不新增 persistent geometry cache、persistent `NamedShape` / `ElementMap` / TopoDS / BREP cache。
- 不把 S3 current-supported closed profile backend contract 改写成 implementation package。
- 不修改 C12-M10 CopyOnChange 口径。
