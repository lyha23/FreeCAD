# C12-M3 Part Workbench ProjectOnSurface Mapper Provenance Native Probe 批次总入口

## 包目标

C12-M3 是 C12-M2 后续的窄化 native probe 包。它只回答一个问题：FreeCAD 原生 ProjectOnSurface / TopoShape history API 是否能给出稳定、request-local、source-backed 的 projection provenance，足以替换 C5-M9 当前 source-backed known-gap expected，并据此判断 current cad-core 是否真的存在 mapper/history backend gap。

S0 开包基线为 `HEAD=7c14aa6f7a`（`7c14aa6f7a docs: 完成 C12-M2 S6 oracle 发布闸门`），`pwd=/Users/li/Chili3DProject/FreeCAD`。C12-M2 S6 已发布 `no_code_oracle_blocked_gate`：ProjectOnSurface geometry 可 build，但 native provenance / mapper history 隐藏；本包不授权 C++、fixtures expected、tests、adapters、capability wording 或 full build 改动。

## S0 live 记录

- 基线命令已执行：`pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git -c core.quotepath=false status --short -uall`。
- 起点 dirty boundary：`M docs/CADCore12.0/README.md` 与未跟踪的 `docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/` 包。该边界未包含 `cad-core/src`、`cad-core/include`、fixtures expected、tests、adapters、capability wording 或构建产物。
- 队列检查：C12-M1 / C12-M2 均只输出表头；C12-M3 在 S0 执行前从 S0-S6 pending 开始，S0 完成并重命名后下一步为 S1。
- C12-M2 继承口径：`C12M2-CAT-005 project_on_surface_provenance` 为 `native_hidden` 且 `code_gate=closed`；`C12M2-CAT-006 global` 发布 `retained_no_expected` / no-code oracle gate。C12-M3 只能继续追问 native API 是否能暴露 source-backed request-local provenance。
- S0 禁止项：不运行 FreeCADCmd probe；不做 current cad-core comparison；不新增 implementation row；不修改 `cad-core/src`、`cad-core/include`、fixtures expected、tests、adapters、capability wording 或 full build 口径。

## 最小完整语义批次

本包覆盖同一条 FreeCAD 调用链和同一类 expected：`src/Mod/Part/App/FeatureProjectOnSurface.cpp` 生成 projected wire / face / compound，`src/Mod/Part/App/TopoShapePyImp.cpp` 与 `src/Mod/Part/App/TopoShapeExpansion.cpp` 暴露或维护 ElementMap / MapperHistory。C12-M3 不拆成单个 fixture，因为 source ownership、split fragments、face rebuild 和 compound result 都依赖同一 provenance 可观测性。

批次闭环如下：

- S0 冻结 C12-M2 继承口径、live baseline、dirty boundary 和 no-code 禁止项。
- S1 建立 ProjectOnSurface source authority 与 current cad-core provenance evidence 矩阵。
- S2 判断哪些场景属于 request-local mapper/provenance，哪些必须留在 product boundary 外。
- S3 固定 C12-M3 native provenance artifact schema 和 probe harness 复用方式。
- S4 对 FreeCAD 原生 ProjectOnSurface / TopoShape history API 做可观测性 probe。
- S5 只有在 S4 得到 stable native provenance expected 后，才比较 current cad-core 与 C5-M9 expected-backed path。
- S6 发布 no-code、current-covered 或另开 implementation 包的建议。

## 工作步骤

| step | title | output |
| --- | --- | --- |
| S0 | live 基线与 C12-M2 继承口径冻结 | 冻结 HEAD、C12-M2 S6 no-code gate、队列状态和禁止项。 |
| S1 | FreeCAD 源码与现有 provenance 证据矩阵 | 回填 ProjectOnSurface / TopoShape history source authority、C5-M9 expected 和 current cad-core landing。 |
| S2 | 范围准入与 blocker 矩阵 | 区分 request-local mapper provenance、native-hidden、collector bug 和 product boundary。 |
| S3 | NativeProvenanceProbe harness 与 artifact schema | 定义 C12-M3 provenance artifact 字段、probe ids、失败分类和复用 C12-M2 harness 的方式。 |
| S4 | ProjectOnSurface 原生 provenance 可观测性 probe | 运行或记录 native probe，判断 `getElementHistory` / `mapShapes` / `mapSubElement` 是否能暴露 source-backed history。 |
| S5 | Current comparison 与 implementation gate 审计 | 仅对 expected-ready 行比较 current cad-core；不能比较 native-hidden 行。 |
| S6 | 发布闸门 | 发布 `native_hidden_retained` / `current_covered` / `backend_gap_candidate`，并给出下一包授权或 no-code 结论。 |

## 发布闸门

S6 只有在同一 row 同时满足以下条件时，才允许建议另开 implementation 包：

1. FreeCAD source authority 可追溯到 ProjectOnSurface execution 和 TopoShape history / ElementMap API。
2. S4 artifact 稳定暴露 source subelement 到 result Edge/Wire/Face 的 provenance，而不是 `None`、bbox、输出顺序或 fixture 名称推断。
3. 行为属于 CAD Core request-local graph/recompute 产品边界，不依赖 GUI session、跨请求 native document 或完整 BREP 传输。
4. S5 证明 current cad-core 与 native expected 存在稳定 mismatch，并能落到 `cad-core/src/part`、`cad-core/src/topo` 或 mapper/history API 边界。

缺任一项则保持 no-code；允许记录 blocker 或建议下一轮 probe，但不得直接写代码。

## 主要交付物

- `矩阵/c12m3_project_on_surface_mapper_native_probe_source_candidates.tsv`
- `矩阵/c12m3_project_on_surface_mapper_native_probe_scope_review_matrix.tsv`
- `矩阵/c12m3_project_on_surface_mapper_native_probe_blocker_queue.tsv`
- `矩阵/c12m3_project_on_surface_mapper_native_probe_non_goal_registry.tsv`
- `矩阵/c12m3_project_on_surface_mapper_native_probe_backend_gap_classification.tsv`
- `矩阵/c12m3_project_on_surface_mapper_native_probe_probe_matrix.tsv`
- `矩阵/c12m3_project_on_surface_mapper_native_probe_validation_matrix.tsv`

## 验收命令

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次 docs/CADCore12.0/README.md
git diff --check
```
