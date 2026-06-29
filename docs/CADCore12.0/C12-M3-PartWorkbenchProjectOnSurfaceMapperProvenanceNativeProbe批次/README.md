# C12-M3 Part Workbench ProjectOnSurface Mapper Provenance Native Probe 批次

C12-M3 承接 C12-M2 S6 的 `no_code_oracle_blocked_gate`。本包只处理 ProjectOnSurface 的 mapper / provenance 原生可观测性问题，不打开 C++ 实现，不刷新 fixtures expected，不修改 tests、adapters 或 capability wording。

## 当前定位

C12-M2 已确认 `FeatureProjectOnSurface` 原生对象可以 build 几何，但 `getElementHistory` 等 source-backed mapper/history 证据仍隐藏；C12-M2 因此把 ProjectOnSurface 关闭为 `native_hidden`。C12-M3 的问题不是重新证明投影几何能生成，而是判断 FreeCAD 原生 API 是否能以 request-local artifact 暴露源 subelement 到目标 Edge/Wire/Face 的稳定 provenance。

只有当 S4/S5 同时证明 stable native provenance artifact、request-local 产品边界和 current cad-core mismatch 时，S6 才能建议另开 implementation 包。否则保持 no-code，并把 blocker 写清楚。

## S0 live 基线冻结

- `pwd=/Users/li/Chili3DProject/FreeCAD`
- `HEAD=7c14aa6f7a`，`git log -1 --oneline` 为 `7c14aa6f7a docs: 完成 C12-M2 S6 oracle 发布闸门`。
- S0 起点 dirty boundary：`docs/CADCore12.0/README.md` 已修改，`docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/` 为未跟踪新包；边界内没有 `cad-core/src`、`cad-core/include`、fixtures expected、tests、adapters 或 capability wording 改动。
- 队列状态：C12-M1 / C12-M2 `工作步骤细分` 均只输出表头；C12-M3 在 S0 执行前从 S0-S6 开始，S0 完成并重命名后下一步为 S1。
- 继承口径：C12-M2 S6 发布 `no_code_oracle_blocked_gate`，ProjectOnSurface geometry 可 build，但 mapper/provenance history 仍为 `native_hidden`；C12-M3 只做 ProjectOnSurface mapper / provenance native observability，不创建 implementation row。

## S1 源码与证据矩阵

- S1 live 起点已记录：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=787198e9ff`（`787198e9ff docs: 冻结 C12-M3 S0 live 基线`），起点 `git status --short -uall` 为 clean。
- `source_candidates` 已拆成 ProjectOnSurface execute/link/filter/wire/face/height/offset、TopoShapePy history API、TopoShapeExpansion `mapSubElement` / `makeShapeWithElementMap` / `MapperHistory`、PropertyPartShape ElementMap 保存恢复、C12-M2 native-hidden artifact、current cad-core ledger/tests 和 C5-M9 expected context。
- `C12M3-BLOCKER-003` 已关闭：所有 S1 row 都有 exact FreeCAD file + class/function + 支撑短句或字段名；不再存在缺 source authority 行。
- S1 结论仍是 no-code：C5-M9 expected 保持 `source_backed_known_gap` context，C12-M2 artifact 保持 `native_hidden` blocker，current cad-core provenance ledger / focused tests 只作为 S5 comparison context；本步未运行 native probe，也未做 current comparison。

## S2 范围准入与 blocker 矩阵

- S2 live 起点已记录：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=adc5b96e52`（`adc5b96e52 docs: 完成 C12-M3 S1 provenance 证据矩阵`），起点 `git status --short -uall` 为 clean。
- `edge_wire_provenance`、`face_rebuild_provenance`、`all_compound_height_offset`、`invalid_projection_diagnostic` 和 `api_observability` 均只作为 S4 probe candidate 准入；每行都要求 source endpoint、target endpoint、history API 结果、request-local judgement 和 S4 close condition。
- `C12M3-BLOCKER-006` 已关闭：GUI session / Workbench、跨请求 native document、持久 TopoDS / NamedShape / ElementMap cache、完整 BREP transport，以及 bbox / order / EdgeN / topology count / fixture-name / current-ledger guessing 均被写成 rejected 或 non-goal。
- C5-M9 source-backed expected 只能作为 current context、known-gap wording 和 delete-condition evidence；在 S4 产出 native_provenance_expected_ready artifact 前，不能当 native expected，也不能打开 S5 comparison。
- `backend_gap_classification` 在 S2 只保留 `probe_candidate` 或 `rejected`；没有 implementation candidate，本步未采 FreeCAD expected、未运行 FreeCADCmd/native probe、未做 current comparison、未改代码或 expected。

## S3 NativeProvenanceProbe schema

- S3 live 起点已记录：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=07643d5e3a`（`07643d5e3a docs: 完成 C12-M3 S2 范围准入矩阵`），起点 `git status --short -uall` 为 clean。
- C12-M2 harness 可继续作为 FreeCADCmd/runtime/process wrapper；C12-M2 schema 不足以承载 ProjectOnSurface source-to-target provenance，因此新增 `docs/temp/6-29-22-15-c12m3-native-provenance-probe-schema.md`。
- C12-M3 schema 固定 `expected_summary.c12m3_classification` 与 `provenance_observations[]`：每条观察必须有 source endpoint、target endpoint、history API name、history return summary、request-local judgement、classification 和 current comparison path。
- 分类冻结为 `native_provenance_expected_ready`、`current_covered`、`backend_gap_candidate`、`native_hidden_retained`、`collector_bug`、`product_boundary_rejected`、`sandbox_runtime_limit`；S4 只有 expected-ready 行能进入 S5 comparison。
- `C12M3-BLOCKER-002` 已关闭；S3 未运行 family expected、未做 current comparison，也未修改 `cad-core/src`、`include`、fixtures expected、tests、adapters 或 capability wording。

## S4 ProjectOnSurface native provenance probe

- S4 live 起点已记录：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=9fa00a2936`（`9fa00a2936 docs: 完成 C12-M3 S3 原生 provenance schema`），起点 `git status --short -uall` 为 clean。
- 新增并运行 `docs/temp/6-29-23-05-c12m3-s4-project-on-surface-native-provenance-probe.py`，artifact 为 `docs/temp/6-29-23-05-c12m3-s4-project-on-surface-native-provenance-probe-output.json`；FreeCADCmd 为 `/Users/li/.cargo/bin/freecadcmd`，FreeCAD baseline 为 `1.2.0 revision 20260519`，OCCT 为 `7.8.1`。
- Artifact 使用 `expected_summary.schema_version=c12m3.native-provenance-summary.v1`，共记录 12 条 observation，覆盖 `edge_wire_provenance`、`face_rebuild_provenance`、`all_compound_height_offset`、`invalid_projection_diagnostic` 和 `api_observability`。
- Object result shape 与必要 intermediate `makeParallelProjection` shape 均可在同一 FreeCADCmd request 内生成，但 `TopoShapePy.getElementHistory` 对 Edge/Wire/Face/Solid 查询均返回 `None`；`mapSubElement` / `mapShapes` 只能作为手动 API 可观测性，不能证明 `FeatureProjectOnSurface` 发布 MapperHistory 或 ElementMap。
- `PropertyPartShape` ElementMap save/load 仍是持久 native document / BREP roundtrip 语境，按 S2 product boundary 保持 `product_boundary_rejected`；invalid diagnostic 仅得到 null/Invalid 状态或 silent null shape，不形成 source-to-target provenance expected。
- S4 总结论为 `native_hidden_retained`，`s5_input=null`，`current_comparison_path=blocked: no native_provenance_expected_ready observation; S5 comparison is blocked`。`C12M3-BLOCKER-004` 关闭为 retained blocker；本步未运行 current `cad-core` comparison，未修改 C++、fixtures expected、tests、adapters 或 capability wording。

## S5 Current comparison gate audit

- S5 live 起点已记录：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=c7a60c98bd`（`c7a60c98bd docs: 完成 C12-M3 S4 原生 provenance probe`），起点 `git status --short -uall` 为 clean。
- S5 复核 S4 artifact 后确认：12 条 observation 中没有 `native_provenance_expected_ready`，`s5_input=null`，`current_comparison_path` 仍为 blocked。
- S5 因此关闭为 no comparison：未运行 current mismatch，未创建 current comparison artifact，未创建 `current_covered` / `backend_gap_candidate`，也未把 C5-M9 source-backed known-gap expected 当 native expected。
- No-comparison 证据为 `docs/temp/6-29-22-40-c12m3-s5-project-on-surface-no-comparison-evidence.json`；`C12M3-BLOCKER-005` 关闭为 `closed_s5_no_comparison`。
- Implementation gate 未满足，C12-M3 仍不授权 C++、fixtures expected、tests、adapters 或 capability wording 改动。

## S6 发布闸门

- S6 live 起点已记录：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=0a32e7cb36`（`0a32e7cb36 docs: 完成 C12-M3 S5 no-comparison 审计`），起点 `git status --short -uall` 输出为空。
- 已逐行复核 `source_candidates`、`scope_review`、`blocker_queue`、`backend_gap_classification`、`probe_matrix`、`validation_matrix`、S4 artifact 和 S5 no-comparison evidence：没有 row 同时满足 stable native provenance expected、request-local product boundary 和 current mismatch。
- C12-M3 最终发布为 `native_hidden_retained` / `no_code_retained_gate`：ProjectOnSurface geometry 和必要 intermediate shape 可生成，但 FreeCAD 原生 history API 仍未暴露 source subelement 到 projected Edge/Wire/Face/Solid 的稳定 provenance。
- 剩余 retained blocker 是 `C12M3-BLOCKER-008`：只能在未来 FreeCAD 原生 API 或 collector 能输出 `native_provenance_expected_ready`、`s5_input` 非空并证明 current mismatch 时重开。C12-M3 不建议 implementation 包，不授权 C++、fixtures expected、tests、adapters 或 capability wording 改动。

## 入口

- 总入口：`6-29-21-29-C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次总入口.md`
- 方案：`6-29-21-29-C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 范围

| axis | purpose |
| --- | --- |
| `edge_wire_provenance` | 复核 C5-M9 edge / wire split provenance 的 native history 可观测性。 |
| `face_rebuild_provenance` | 复核 projected face rebuild 后 Face/Wire/Edge ownership 是否能从 native history 得到。 |
| `all_compound_height_offset` | 复核 all-compound、height solid 和 offset placement 场景是否有稳定 source-backed trace。 |
| `invalid_projection_diagnostic` | 只在 native 能给出可追溯 property / subname failure 时收为 diagnostic expected；不能从失败栈推导业务语义。 |
| `api_observability` | 复核 `TopoShapePyImp` / `TopoShapeExpansion` / `PropertyTopoShape` 中 ElementMap、MapperHistory、`getElementHistory`、`mapShapes`、`mapSubElement` 的可调用边界。 |

## 出口分类

- `native_provenance_expected_ready`：原生 artifact 稳定暴露 source-backed provenance，可进入 S5 current comparison。
- `current_covered`：存在 expected-ready artifact，但当前 cad-core 已覆盖。
- `backend_gap_candidate`：存在 expected-ready artifact，且 current cad-core 有稳定 mismatch，可由 S6 建议另开 implementation 包。
- `native_hidden_retained`：FreeCAD native API 仍不暴露 mapper/provenance，保持 no-code。
- `product_boundary_rejected`：依赖 GUI session、跨请求 native document、完整 BREP 或持久 TopoDS/ElementMap cache。
- `collector_bug`：probe 脚本或调用方式错误，需要先修 collector，不能升级为 backend gap。
- `sandbox_runtime_limit`：当前运行环境无法启动或完成 FreeCADCmd，例如 Qt / processor / startup / timeout 限制。

## 禁止项

- 不修改 `cad-core/src`、`cad-core/include`、fixtures、expected、tests、adapters 或 capability wording。
- 不用 bbox、输出顺序、EdgeN 数量或 fixture 名称倒推 source ownership。
- 不把 FreeCAD GUI session、Workbench 状态、跨请求 Document / TopoDS / NamedShape / ElementMap cache 纳入 CAD Core 产品边界。
- 不引入完整 BREP 传输；`ReferenceShadow.brep` 例外不适用于本包新增建模输入。
- 不把 C12-M2 的 `native_hidden` 直接改写成 implementation row。
- 不运行 full build；S0 也不运行 FreeCADCmd probe 或 current cad-core comparison。
