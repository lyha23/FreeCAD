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

## S1 live 记录

- S1 起点命令已执行：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=787198e9ff`，`git log -1 --oneline=787198e9ff docs: 冻结 C12-M3 S0 live 基线`，`git -c core.quotepath=false status --short -uall` 输出为空。
- Source candidate matrix 已回填为 exact source authority：ProjectOnSurface execute/link/filter/wire/face/height/offset、TopoShapePy `getElementHistory` / `mapShapes` / `mapSubElement`、TopoShapeExpansion `mapSubElement` / `makeShapeWithElementMap` / `MapperHistory`、PropertyPartShape ElementMap 保存恢复，以及 C12-M2 artifact / current cad-core ledger/tests / C5-M9 expected context。
- `C12M3-BLOCKER-003` 已关闭；剩余 open blocker 是 S3 schema、S4 native-hidden、S5 current comparison gate 和 S2 product boundary guard。
- 分类口径：edge/wire、face rebuild、all compound/height/offset 和 invalid diagnostic 均为 source-backed known-gap / native expected missing；C12-M2 ProjectOnSurface artifact 仍为 `native_hidden`；current cad-core ledger 与 focused tests 只作为 S5 context，不授权代码、expected、tests、adapter 或 capability wording 改动。

## S2 live 记录

- S2 起点命令已执行：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=adc5b96e52`，`git log -1 --oneline=adc5b96e52 docs: 完成 C12-M3 S1 provenance 证据矩阵`，`git -c core.quotepath=false status --short -uall` 输出为空。
- 范围准入已覆盖五个轴：`edge_wire_provenance`、`face_rebuild_provenance`、`all_compound_height_offset`、`invalid_projection_diagnostic` 和 `api_observability`。可继续 S4 的行必须给出 native source endpoint、target endpoint、history API return summary、request-local judgement 和 close condition；C5-M9 source-backed expected 只作为 current context，不是 native expected。
- Product boundary 已关闭为 rejected/non-goal：GUI session / Workbench、跨请求 native document、持久 TopoDS / NamedShape / ElementMap cache、完整 BREP transport、bbox/order/EdgeN/topology-count/fixture-name/current-ledger guessing 均不能作为 provenance 或 backend gap 依据。
- `C12M3-BLOCKER-006` 与 `C12M3-BLOCKER-007` 已关闭；backend classification 在 S2 只保留 `probe_candidate` / `rejected`，没有 implementation candidate。本步未采 FreeCAD expected、未运行 FreeCADCmd/native probe、未做 current comparison，也未修改代码、expected、tests、adapters 或 capability wording。

## S3 live 记录

- S3 起点命令已执行：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=07643d5e3a`，`git log -1 --oneline=07643d5e3a docs: 完成 C12-M3 S2 范围准入矩阵`，`git -c core.quotepath=false status --short -uall` 输出为空。
- 已复核 `docs/temp/6-29-20-12-c12m2-native-probe-schema.md`、`docs/temp/6-29-20-12-c12m2-native-probe-harness.py`、runtime baseline artifact 和 ProjectOnSurface C12-M2 S5 artifact：C12-M2 harness 可复用作 wrapper，但 C12-M2 schema 缺少 row-level source endpoint、target endpoint、history API name、history return summary、request-local judgement、classification 和 current comparison path。
- 新增 `docs/temp/6-29-22-15-c12m3-native-provenance-probe-schema.md`，要求 S4 artifact 在 `expected_summary` 写入 `c12m3.native-provenance-summary.v1` 和 `provenance_observations[]`；`native_provenance_expected_ready` 只能来自 native history API 暴露的 source-backed provenance，不能来自 bbox、顺序、数量、fixture 名称或 current ledger。
- 分类已冻结为 `native_provenance_expected_ready`、`current_covered`、`backend_gap_candidate`、`native_hidden_retained`、`collector_bug`、`product_boundary_rejected`、`sandbox_runtime_limit`。`C12M3-BLOCKER-002` 已关闭，probe/validation matrix 已记录 S4 artifact 命名和通过标准。
- S3 未运行 ProjectOnSurface family expected、未运行 current comparison、未把 C12-M2 `None` history 当最终结论，也未修改 `cad-core/src`、`include`、fixtures expected、tests、adapters 或 capability wording。

## S4 live 记录

- S4 起点命令已执行：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=9fa00a2936`，`git log -1 --oneline=9fa00a2936 docs: 完成 C12-M3 S3 原生 provenance schema`，`git -c core.quotepath=false status --short -uall` 输出为空。
- S4 新增 probe script `docs/temp/6-29-23-05-c12m3-s4-project-on-surface-native-provenance-probe.py`，并输出 artifact `docs/temp/6-29-23-05-c12m3-s4-project-on-surface-native-provenance-probe-output.json`。运行包络为 FreeCADCmd `/Users/li/.cargo/bin/freecadcmd`、FreeCAD `1.2.0 revision 20260519`、OCCT `7.8.1`。
- Artifact 使用 `c12m3.native-provenance-summary.v1`，12 条 observation 均含 source endpoint、target endpoint、history API name、history return summary、request-local judgement、classification 和 current comparison path。
- `edge_wire_provenance`、`face_rebuild_provenance`、`all_compound_height_offset` 的 object result / intermediate shape 可生成，但 `getElementHistory` 对查询元素均返回 `None`；`mapSubElement` / `mapShapes` 属于手动 API 调用，不是 `FeatureProjectOnSurface` 原生 mapper/history 输出。
- `ElementMap` save/load 需要持久 native document / BREP roundtrip，按 product boundary 保持 rejected；invalid diagnostic 只提供 null/Invalid 状态，不形成 source-to-target provenance expected。
- S4 总结论为 `native_hidden_retained`，`s5_input=null`，S5 current comparison 被阻断；`C12M3-BLOCKER-004` 关闭为 retained blocker。本步未做 current cad-core comparison，未改 C++、expected、tests、adapters 或 capability wording。

## S5 live 记录

- S5 起点命令已执行：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=c7a60c98bd`，`git log -1 --oneline=c7a60c98bd docs: 完成 C12-M3 S4 原生 provenance probe`，`git -c core.quotepath=false status --short -uall` 输出为空。
- 已审计 S4 artifact `docs/temp/6-29-23-05-c12m3-s4-project-on-surface-native-provenance-probe-output.json`：`expected_summary.c12m3_classification=native_hidden_retained`，`s5_input=null`，12 条 observation 只有 `native_hidden_retained` / `product_boundary_rejected`，无 `native_provenance_expected_ready` row。
- S5 按 no-comparison 关闭，证据为 `docs/temp/6-29-22-40-c12m3-s5-project-on-surface-no-comparison-evidence.json`；未运行 current mismatch，未创建 current comparison artifact、`current_covered` 或 `backend_gap_candidate`。
- C5-M9 source-backed known-gap expected 保持 context/delete-condition evidence，不能替代 native expected；native-hidden row 未被比较。
- Implementation gate 未满足，C12-M3 仍不授权 C++、fixtures expected、tests、adapters 或 capability wording 改动。

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
| S4 | ProjectOnSurface 原生 provenance 可观测性 probe | 已运行 native probe；原生 history 仍隐藏，S5 comparison 被阻断。 |
| S5 | Current comparison 与 implementation gate 审计 | 已审计 S4 artifact；无 expected-ready row，S5 no-comparison 关闭，不创建 implementation gate。 |
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
