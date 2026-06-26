# CADCore8.0

CADCore8.0 承接 C7-M7 之后的下一轮 CAD Core 收口工作。C7-M7 已确认 P8 Link / imported-shape stable reference 方向没有 `backend_gap_requires_implementation`：完整 imported ElementMap、ShowElement 持久写回和 cross-document hash / postfix 生命周期均保持 `oracle_blocked` 或 `oracle_blocker`，不应继续在同一包里硬扩。

C8-M1 转向 `PartDesign::ShapeBinder` / `PartDesign::SubShapeBinder` 外部引用绑定与 ElementMap 闭环。这个方向有清晰 FreeCAD 源码入口、上游测试案例和当前 `cad-core` registry 缺口，且能复用已落地的 `PropertyLink*`、Link retag、ElementMap、Body replay、CopyOnChange 和 request-local `documentObjectUpdates` 语义。

## 入口

- C8-M1 总入口：`C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/6-26-16-15-C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线总入口.md`
- C8-M1 方案：`C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/6-26-16-15-C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环方案.md`
- C8-M1 工作步骤：`C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/工作步骤细分/`
- C8-M1 矩阵：`C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/矩阵/`
- C8-M2 总入口：`C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/6-26-22-20-C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线总入口.md`
- C8-M2 方案：`C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/6-26-22-20-C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入方案.md`
- C8-M2 工作步骤：`C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/工作步骤细分/`
- C8-M2 矩阵：`C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/矩阵/`

## 当前状态

- C8-M1 S0 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=29da94dd13`（`29da94dd13 文档：完成 C7-M7 S6 发布闸门`），开始状态只包含本 C8-M1 文档包与 `docs/CADCore8.0/README.md` 未跟踪文件。
- C8-M1 为新建方案包；工作步骤总入口是索引文件，已标记 `【已实现】`。S0 已完成 live 基线冻结，S1 已完成 FreeCAD source authority 与 current cad-core coverage 复核，S2 已完成 oracle 候选矩阵分类，S3 已完成 native oracle 批量采集与 expected 固化，S4 已完成 `cad-core` executor，S5 已完成 fixtures / tests / capability 发布，S6 release gate 已完成；当前队列为空。
- C8-M1 不重开 C7-M7 的 oracle-blocked Link 持久化行，不声明完整 CopyOnChange / Frozen / Detached 持久状态已支持。
- S1 复核确认 `cad-core/src/runtime/feature_registry.cpp` 未覆盖 `PartDesign::ShapeBinder`、`PartDesign::SubShapeBinder` 或 `PartDesign::SubShapeBinderPython`；`body.cpp`、`profile_resolver.cpp`、`topo_shape_expansion.cpp`、`property_topo_shape.cpp`、`copy_on_change.cpp` 和 `reference_resolution.cpp` 仅为可复用能力，不是 Binder 支持。
- S3 已在 `cad-core/fixtures/c8m1` 固化 12 个 fixture 和 12 个 FreeCAD native expected；S4/S5 已发布 `PartDesign::ShapeBinder` / `PartDesign::SubShapeBinder` expected-backed request-local support、`topo_history.producer_matrix.shapebinder` 和 focused tests。CopyOnChange full temporary-document cache 保持 `known_gap` / `oracle_blocked` diagnostic，不作为无状态后端必须实现项。
- S6 已关闭 `C8M1-BLOCKER-601`：build、focused tests、diagnostics、capability smoke、阶段回归、TSV 和 diff 检查通过。阶段回归中仅同步 C6-M4 / c5m10 Sweep historical known-gap guard 测试断言，未修改 C8-M1 runtime scope。
- C8-M2 已创建为下一轮方案包，目标是把 C8-M1 的 `ShapeBinder` / `SubShapeBinder` 能力同步边界和 `SubShapeBinder BindCopyOnChange` request-local DTO 准入拆清楚。C8-M2 不重开 C8-M1 已关闭的 executor / ElementMap 主路径，也不把 full FreeCAD temporary-document CopyOnChange cache 写成 supported。
- C8-M2 S0 live 基线已冻结：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=dc93b0d3af`（`dc93b0d3af chore: 完成 C8-M1 S6 发布闸门`），开始状态包含既有 C8-M2 文档/矩阵未提交改动与本文件修改。S0 已确认 C8-M1 队列为空、`part_design.shape_binder.remaining_gaps=[]`、`part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- C8-M2 S1 已完成 FreeCAD source 与 C8-M1 能力复核：`HEAD=e7e07663d9`（`e7e07663d9 docs: 完成 C8-M2 S0 live 基线冻结`），开始工作区干净。S1 已确认 FreeCAD `SubShapeBinder::update()` 的 Mutated full path 依赖 temporary document / `_CopiedObjs` cache，cad-core 当前只发布 request-local diagnostic，`copy_on_change_full_temporary_document_cache` 继续保持 known_gap。
- C8-M2 S2 已完成 CopyOnChange DTO 准入与 oracle 候选矩阵：`HEAD=73a5acf8a8`（`73a5acf8a8 docs: 完成 C8-M2 S1 源码与能力复核`），开始工作区干净。S2 已把 C8-M1 capability / diagnostics / fixtures 下游同步分类为 `sync_required`，把 CopyOnChange Disabled / Enabled / Mutated property-state 和 PartialLoad allow-partial 分类为 `oracle_candidate`，把 full temporary-document copied-object cache 保持为 `known_gap_retained`，把 request-local DTO 只保留为 `backend_gap_candidate` 而非 implementation gate。
- C8-M2 S3 已完成 native CopyOnChange 生命周期探针与 blocker evidence：`HEAD=12be750a30`（`12be750a30 docs: 完成 C8-M2 S2 DTO 准入矩阵`），开始工作区干净。FreeCADCmd 采集 `freecad_version=1.2.0 revision 20260519`；`C8M2-ORACLE-101` / `102` 已采 property-state，`C8M2-ORACLE-103` 仍保留 `oracle_blocked`，因为 `_CopiedObjs` / `copyObject` / `recomputeFeature(true)` full temporary-document cache 不可导出为稳定 request-local DTO。
- C8-M2 S4 已完成下游 `opencascade-rs` / 前端同步源头合同：`HEAD=7b4eec93fe`（`7b4eec93fe docs: 完成 C8-M2 S3 native 生命周期探针`），开始工作区干净。新增 `矩阵/c8m2_downstream_sync_contract.tsv`，覆盖 TypeIds、C8-M1 fixtures / expected、C8-M2 native probe 使用边界、`/cad/capabilities`、diagnostics vocabulary、known_gap delete/reopen condition 和 ElementMap / NamedShape 输出合同；`C8M2-SYNC-101..103` 与 `C8M2-BLOCKER-401` 已关闭。
- C8-M2 S5 已完成 capability 协议与前端接入边界发布：`HEAD=d1afdb460f`（`d1afdb460f docs: 完成 C8-M2 S4 下游同步契约`），开始工作区干净。`./cad-core capabilities > /tmp/c8m2-capabilities.json` 与 `python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics` 通过；`shape_binder` 保持 supported，`sub_shape_binder` 保持 `copy_on_change_full_temporary_document_cache` known_gap / `oracle_blocked`，下游同步保持 `sync_required`，GUI/session/persistent cache/Rust 与前端持久 full BREP / TopoDS_Shape / NamedShape / ElementMap / temporary cache 均发布为 `diagnostic_non_goal`；`C8M2-BLOCKER-501` 已关闭，S6 仍为 pending。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0
git diff --check
```
