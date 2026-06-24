# C6-M7 Part Workbench Loft Subelement Assignment Native Hidden 收口主线总入口

本文是 `docs/CADCore6.0` 下 C6-M7 实施主线。当前 C6-M1 到 C6-M6 队列均已关闭；C6-M7 已完成 S0 到 S5，处理并发布 `part_workbench.loft` 的原唯一 active remaining gap：`part_loft_subelement_assignment_native_hidden`。

## 目标

- 复核 `Part::Loft.Sections` subelement assignment 的 FreeCAD 原生可见性。
- 明确当前 native-hidden diagnostic-only 证据是否可以转成 request-local CAD Core product contract。
- 若可实现，补 C6-M7 fixture、focused tests、expected metadata、capability 和 docs。
- 若不可实现，发布更精确的 narrowed gap / diagnostic boundary / delete condition。
- 为后续 Surface Family freeze 清理最后一个 Loft active gap 前置条件。

## live 起点

- S0 冻结时间的 live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=5caad308a9`，`git log -1=5caad308a9 发布 C6-M6 GeomPlate release gate`。
- S0 开始前工作区仅包含 C6-M7 文档包与 `docs/CADCore6.0/README.md` 的未提交文档改动；本步不改 C++、capability、fixture 或 build 产物。
- C6-M1 到 C6-M6 `step_goal_queue.py` 均返回空表；S2 完成后 C6-M7 队列从 S3 继续。
- S0 active blocker 已冻结为唯一项：`part_workbench.loft.remaining_gaps=["part_loft_subelement_assignment_native_hidden"]`。S4 已将该项从 active `remaining_gaps` 移入 `narrowed_gaps` / historical native-hidden evidence；当前发布状态见 `cad-core/src/runtime/capability_contract.cpp` 和 `cad-core/tests/test_adapters.py`。
- C5-M12 已关闭 Loft broad `complex_profile_family`，不重开完整 Loft surface family；`cad-core/fixtures/c5m12/expected/part-loft-subelement-assignment-diagnostic.freecad.json` 记录 native-hidden diagnostic evidence：`TypeError: Type must be App.DocumentObject or None, not tuple`，未采集 `object_fields.sections[].subname` 和 selected Sketch subelement `shape_summary`。
- S1 起点 live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=1c197bf648`，`git log -1=1c197bf648 冻结 C6-M7 S0 live 基线`，工作区干净。
- S2 起点 live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=5587487eaa`，`git log -1=5587487eaa 完成 C6-M7 S1 Loft Sections 源码复核`，工作区干净；S2 完成后队列从 S3 继续。
- S3 起点 live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=1b0c3ce588`，`git log -1=1b0c3ce588 完成 C6-M7 S2 路由判定`，工作区干净；S3 完成后队列从 S4 继续。
- S4 起点 live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=b831431c8f`，`git log -1=b831431c8f feat: 实现 C6-M7 Loft 子元素合同`，工作区干净；S4 完成后队列从 S5 继续。
- S5 起点 live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=755e91bdc4`，`git log -1=755e91bdc4 发布 C6-M7 S4 Loft 能力合同`，工作区干净；S5 完成后队列为空。

## FreeCAD source authority

| owner | source |
| --- | --- |
| Loft executor | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Loft::execute()` |
| Sections property | `/Users/li/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkList` |
| Loft builder | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementLoft()` |
| ThruSections mapper | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::MapperThruSections` |

## S1 source review conclusion

- `PartFeatures.cpp::Loft::execute()` 原文调用顺序：`Sections.getValues()` -> `getTopoShape(obj, ShapeOption::ResolveLink | ShapeOption::Transform)` -> `result.makeElementLoft(shapes, isSolid, isRuled, isClosed, degMax)`。
- `Loft::Loft()` 对 `Sections` 的属性声明是 `ADD_PROPERTY_TYPE(Sections, (nullptr), "Loft", App::Prop_None, "List of sections")`；结合 `PropertyLinkList::getPyValue()` 的 `Base::PyTypeCheck(&item, &DocumentObjectPy::Type)`、`PropertyLinkList::getLinks()` 对 `subs/newStyle` 的忽略和 `getLinksTo()` 对 `subname` 的忽略，S1 认定 `Sections` 是 object-level `App::PropertyLinkList`，不是 native `PropertyLinkSubList`。
- `TopoShapeExpansion.cpp::TopoShape::makeElementLoft()` 使用 `BRepOffsetAPI_ThruSections`，执行 `SetMaxDegree`、profile `AddVertex/AddWire`、`CheckCompatibility(Standard_True)`、`Build()`，再通过 `MapperThruSections(aGenerator, profiles)` 写入 history。
- `cad-core/src/part/part_loft.cpp` 当前只通过 `app::readLinks(object, "Sections")` 读取 object-level sections；`cad-core/src/part/topo_shape_expansion.cpp::makeElementLoftFromSources()` 已有 `BRepOffsetAPI_ThruSections` 和 thru-sections history 落点。因此 S1 不把现状写成 C++/topo 缺口；S2 已把 selected subelement 支持批准为 request-local product contract non-parity DTO。

## S2 route decision

- S2 判定 route decision 为 `cad_core_product_contract_non_parity`：批准 S3 实现 request-local CAD Core selected subelement DTO / fixture / focused tests / capability docs。
- FreeCAD 原生边界不变：`Part::Loft.Sections` 仍是 object-level `App::PropertyLinkList`，不能保存 subname；不得生成 FreeCAD native expected，不声明 FreeCAD parity。
- C5-M12 `part-loft-subelement-assignment-diagnostic.freecad.json` 继续作为 `part_loft_subelement_assignment_native_hidden` 的 `nativeHidden` / `diagnosticOnly` evidence。S3/S4 若发布 product contract，必须把 product contract expected 与 FreeCAD expected 分开。
- S3 应走“实现 product contract”，不是继续堆 diagnostic-only 文档；delete condition 仍限定为 upstream FreeCAD native subelement storage 出现，或 C6-M7 request-local product contract 以 non-parity 形式完成发布并由 capability/tests 证明。

## S3 product contract evidence

- S3 已在 `cad-core/src/app/property_links.cpp` 接收 request-local `PropertyLinkList.values[]` rich link item，并在 `cad-core/src/part/part_loft.cpp` 解析 `Sections` item 的 `SubList/StableSubList`。
- Loft 输出保留 `sections` object list，同时新增 `contract=cad_core_product_contract`、`contract_provenance=cad_core_product_contract_non_parity`、`freecad_native_expected=false`、`section_entries` 和 `selected_sections`。
- 新增 `cad-core/fixtures/c6m7/part-loft-subelement-product.json` / `part-loft-subelement-product-invalid.json` 及 expected/product metadata；valid case 构建 selected `LowerProfile.Edge1` Loft，invalid case 给 `invalid_subshape`。
- C5-M12 FreeCAD expected 继续作为 native-hidden diagnostic-only evidence，S3 未修改该文件、未生成 FreeCAD native expected、未删除 `remaining_gaps`。

## S4 capability publication

- S4 已将 S3 product contract 发布到 `cad-core/src/runtime/capability_contract.cpp` 的 `part_workbench.loft`：status 更新为 expected-backed + C6-M7 product contract non-parity，fixtures 包含 `c6m7/part-loft-subelement-product` 和 invalid diagnostic case。
- `part_workbench.loft.remaining_gaps=[]`；原 `part_loft_subelement_assignment_native_hidden` 不再是 active implementation gap，而是保留在 `narrowed_gaps` / field boundary 中，指向 C5-M12 native-hidden diagnostic expected 与 C6-M7 request-local product contract。
- `cad-core/tests/test_adapters.py::test_c_api_capabilities_exposes_web_contract_facts` 已同步断言 covered、fixtures、request_local_boundaries、field_boundaries、narrowed_gaps、remaining_gaps 和 non_goals。

## S5 release gate

- `cmake --build build` 通过。
- 阶段回归通过：`python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters`，`Ran 254 tests in 85.216s`，`OK (skipped=31)`。
- C6-M7 工作步骤队列返回空表；矩阵 TSV 字段数检查和 `git diff --check` 作为 release gate 执行。
- C6-M7 已按 CAD Core request-local product contract non-parity 发布；不声明 FreeCAD parity，不新增 FreeCAD native expected，不做 Surface Family freeze。

## cad-core 落点

| owner | file |
| --- | --- |
| executor | `cad-core/src/part/part_loft.cpp` |
| topo/history | `cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/src/part/topo_shape.cpp` |
| expected collector | `cad-core/tools/collect_freecad_expected.py` |
| capability | `cad-core/src/runtime/capability_contract.cpp` |
| tests | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_expected_fixtures.py`、`cad-core/tests/test_adapters.py` |
| fixtures | `cad-core/fixtures/c6m7` |

## 工作步骤

| step | file | 目标 |
| --- | --- | --- |
| S0 | `工作步骤细分/6-25-00-54-【已实现】C6-M7-S0-live基线与Loft唯一remainingGap冻结.md` | 已冻结 live baseline、queue、capability 和 C5-M12 evidence。 |
| S1 | `工作步骤细分/6-25-00-55-【已实现】C6-M7-S1-FreeCAD源码与PropertyLinkList边界复核.md` | 已复核 FreeCAD Loft executor、PropertyLinkList 和 ThruSections history。 |
| S2 | `工作步骤细分/6-25-00-56-【已实现】C6-M7-S2-准入路由与产品合同判定.md` | 已批准 request-local `cad_core_product_contract_non_parity`，同时保留 FreeCAD native-hidden diagnostic boundary。 |
| S3 | `工作步骤细分/6-25-00-57-【已实现】C6-M7-S3-LoftSubelement合同或诊断实现.md` | 已实现 request-local product contract，并与 FreeCAD expected 分开。 |
| S4 | `工作步骤细分/6-25-00-58-【已实现】C6-M7-S4-fixtures-tests-capability-docs发布.md` | 已同步 capability、adapter assertion、发布口径和矩阵。 |
| S5 | `工作步骤细分/6-25-00-59-【已实现】C6-M7-S5-阶段回归与release-gate.md` | 已完成阶段回归、queue empty 和发布闸门。 |

## 矩阵

| matrix | 用途 |
| --- | --- |
| `矩阵/c6m7_loft_subelement_assignment_source_candidates.tsv` | FreeCAD / cad-core source 候选。 |
| `矩阵/c6m7_loft_subelement_assignment_scope_review_matrix.tsv` | scope 准入和当前状态。 |
| `矩阵/c6m7_loft_subelement_assignment_backend_gap_classification.tsv` | backend gap / diagnostic / non-goal 分类。 |
| `矩阵/c6m7_loft_subelement_assignment_blocker_queue.tsv` | blocker 和关闭条件。 |
| `矩阵/c6m7_loft_subelement_assignment_input_contract_matrix.tsv` | DTO / expected / capability 字段合同。 |
| `矩阵/c6m7_loft_subelement_assignment_oracle_fixture_matrix.tsv` | oracle / fixture 路由。 |
| `矩阵/c6m7_loft_subelement_assignment_non_goal_registry.tsv` | 不做项和重开条件。 |
| `矩阵/c6m7_loft_subelement_assignment_validation_matrix.tsv` | 验收命令分层。 |

## 非目标

- 不声明 FreeCAD parity。
- 不做 `PartDesign::AdditiveLoft` / `PartDesign::SubtractiveLoft` 新语义。
- 不重开 C5-M12 已关闭的 complex profile family。
- 不处理 ProjectOnSurface GUI、Sweep、Filling、GeomPlate、Groove 或 full Part surface family。
- 不从 cad-core output 伪造 FreeCAD native expected。

## 当前结论

C6-M7 已处理 Loft 唯一 active remaining gap。S1 已证明 FreeCAD 原生 `Sections` 边界仍是 object-level `PropertyLinkList` native-hidden evidence；S2 已批准引入 CAD Core request-local non-parity DTO；S3 已实现 product contract 并保持 FreeCAD expected 分离；S4 已发布 capability / adapter 口径并把 `part_workbench.loft.remaining_gaps` 清空；S5 已通过阶段回归与 release gate。本包已按 CAD Core product contract non-parity 口径发布，后续可另开 CADCore6 Surface Family freeze / publication audit 包。
