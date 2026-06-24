# C6-M7 Part Workbench Loft Subelement Assignment Native Hidden 收口主线总入口

本文是 `docs/CADCore6.0` 下 C6-M7 实施主线。当前 C6-M1 到 C6-M6 队列均已关闭；C6-M7 只处理 `part_workbench.loft` 的唯一 active remaining gap：`part_loft_subelement_assignment_native_hidden`。

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
- 当前 active blocker 已冻结为唯一项：`part_workbench.loft.remaining_gaps=["part_loft_subelement_assignment_native_hidden"]`。代码证据见 `cad-core/src/runtime/capability_contract.cpp` 的 `part_workbench.loft` capability 和 `cad-core/tests/test_adapters.py` 对 `loft["remaining_gaps"]` 的等值断言。
- C5-M12 已关闭 Loft broad `complex_profile_family`，不重开完整 Loft surface family；`cad-core/fixtures/c5m12/expected/part-loft-subelement-assignment-diagnostic.freecad.json` 记录 native-hidden diagnostic evidence：`TypeError: Type must be App.DocumentObject or None, not tuple`，未采集 `object_fields.sections[].subname` 和 selected Sketch subelement `shape_summary`。
- S1 起点 live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=1c197bf648`，`git log -1=1c197bf648 冻结 C6-M7 S0 live 基线`，工作区干净。
- S2 起点 live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=5587487eaa`，`git log -1=5587487eaa 完成 C6-M7 S1 Loft Sections 源码复核`，工作区干净；S2 完成后队列从 S3 继续。

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
| S3 | `工作步骤细分/6-25-00-57-C6-M7-S3-LoftSubelement合同或诊断实现.md` | 实现 request-local product contract，并与 FreeCAD expected 分开。 |
| S4 | `工作步骤细分/6-25-00-58-C6-M7-S4-fixtures-tests-capability-docs发布.md` | 同步 fixtures、tests、capability、expected metadata 和矩阵。 |
| S5 | `工作步骤细分/6-25-00-59-C6-M7-S5-阶段回归与release-gate.md` | 阶段回归、queue empty 和发布闸门。 |

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

C6-M7 应先处理 Loft 唯一 active remaining gap，再考虑 Surface Family freeze。S1 已证明 FreeCAD 原生 `Sections` 边界仍是 object-level `PropertyLinkList` native-hidden evidence；S2 已批准引入 CAD Core request-local non-parity DTO，route decision 为 `cad_core_product_contract_non_parity`。S3 需要实现 product contract 并保持 FreeCAD expected 分离；若 S5 后 `part_workbench.loft.remaining_gaps=[]`，下一步才适合开 CADCore6 surface family freeze / publication audit 包。
