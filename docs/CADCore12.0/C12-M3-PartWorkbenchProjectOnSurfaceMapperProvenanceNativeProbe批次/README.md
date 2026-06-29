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

## 禁止项

- 不修改 `cad-core/src`、`cad-core/include`、fixtures、expected、tests、adapters 或 capability wording。
- 不用 bbox、输出顺序、EdgeN 数量或 fixture 名称倒推 source ownership。
- 不把 FreeCAD GUI session、Workbench 状态、跨请求 Document / TopoDS / NamedShape / ElementMap cache 纳入 CAD Core 产品边界。
- 不引入完整 BREP 传输；`ReferenceShadow.brep` 例外不适用于本包新增建模输入。
- 不把 C12-M2 的 `native_hidden` 直接改写成 implementation row。
- 不运行 full build；S0 也不运行 FreeCADCmd probe 或 current cad-core comparison。
