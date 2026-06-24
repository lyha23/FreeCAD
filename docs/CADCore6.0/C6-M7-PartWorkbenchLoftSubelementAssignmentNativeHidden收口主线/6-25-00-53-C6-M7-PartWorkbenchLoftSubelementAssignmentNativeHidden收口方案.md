# C6-M7 Part Workbench Loft Subelement Assignment Native Hidden 收口方案

## 背景

C6-M6 关闭后，`docs/CADCore6.0` 下现有 C6-M1 到 C6-M6 工作步骤队列均为空。当前能力合同中 `part_workbench.loft` 仍保留唯一 active `remaining_gaps`：`part_loft_subelement_assignment_native_hidden`。C5-M12 已证明普通 Loft、linearize、complex profile family 和 whole sketch object / vertex representatives 有 expected-backed 或 focused-test 证据；剩余问题集中在 `Part::Loft.Sections` 是 `App::PropertyLinkList`，原生 FreeCAD 不暴露 subelement storage path，导致 Sketch subelement assignment 只能作为 native-hidden diagnostic-only 证据保留。

## 本轮做什么

- S0：复核 live baseline、C6-M1 到 C6-M6 queue、`part_workbench.loft` capability 和 C5-M12 Loft expected evidence，冻结唯一 active gap。
- S1：复核 FreeCAD authority：`PartFeatures.cpp::Loft::execute()`、`PropertyLinkList`、`TopoShapeExpansion.cpp::makeElementLoft()`、`MapperThruSections` 和 cad-core `part_loft.cpp` / `topo_shape_expansion.cpp` 落点。
- S2：判定 `part_loft_subelement_assignment_native_hidden` 的准入路径：继续 nativeHidden / diagnosticOnly，或建立 request-local product contract。不得用 cad-core output 伪造 FreeCAD expected。
- S3：若 S2 批准 request-local contract，补 C6-M7 fixtures、focused tests、object_fields 和 mapper/history 证据；若不批准，则强化 diagnostics / narrowed gap metadata 和 delete condition。
- S4：同步 capability、adapter assertions、expected metadata 和 C6-M7 矩阵；只删除或移动有证据闭环的 gap。
- S5：运行阶段回归并发布 C6-M7 状态；若 Loft active gap 清空，才允许进入后续 surface-family freeze 候选。

## 关键边界

- `Part::Loft.Sections` 的 FreeCAD 原生字段是 `App::PropertyLinkList`，不是 `PropertyLinkSubList`；不能把 selected subelement 当作 FreeCAD native expected。
- 如果采用 request-local DTO，它必须明确标成 CAD Core product contract non-parity，并与 FreeCAD expected 分开。
- 不重开 C5-M12 已关闭的 Loft complex profile family。
- 不实现 `PartDesign::AdditiveLoft` / `PartDesign::SubtractiveLoft` 新语义。
- 不处理 ProjectOnSurface GUI、Filling、Sweep、GeomPlate 或 full Part surface family。
- 不靠 fixture 名称、bbox、面积或 adapter 输出修补关闭 gap。

## 代码落点

| 方向 | 文件 |
| --- | --- |
| Loft executor / DTO | `cad-core/src/part/part_loft.cpp`、`cad-core/include/cad_core/part/part_feature.h` |
| Loft shape build / history | `cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/src/part/topo_shape.cpp` |
| expected collector | `cad-core/tools/collect_freecad_expected.py` |
| capability | `cad-core/src/runtime/capability_contract.cpp` |
| tests | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_expected_fixtures.py`、`cad-core/tests/test_adapters.py` |
| fixtures | `cad-core/fixtures/c6m7` |

## FreeCAD 依据

| 方向 | FreeCAD source |
| --- | --- |
| Part Loft executor | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Loft::execute()` |
| Sections property type | `/Users/li/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkList` |
| Loft shape builder | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementLoft()` |
| ThruSections history | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::MapperThruSections` |

## 验收分层

- 本轮短跑：相关 source / capability grep、C6-M7 focused tests、TSV 字段数检查、`git diff --check`。
- 阶段回归：`cmake --build build` 加 Loft / expected / adapter focused suites。
- release gate：S5 跑 P8 / expected / adapter 相关 suites；只有修改 `topo` / history 主路径时再加入 topology suite。

## 结论

推荐立即进入 C6-M7。这个包的最小完整语义批次是 Loft subelement assignment native-hidden 收口：先判断是否能形成 request-local CAD Core product contract；若不能，则把 current native-hidden evidence 升级为精确 narrowed gap，并为后续 surface-family freeze 留出明确条件。
