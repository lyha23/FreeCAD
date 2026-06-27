# C8-M6 ShapeBinder/SubShapeBinder 下游同步源头合同方案

## 目标

C8-M6 负责把 FreeCAD 仓库中已经完成并验证过的 ShapeBinder/SubShapeBinder 合同整理成后续下游同步的唯一源头包。它不重开 C8-M1 executor，不重做 C8-M5 expected 修复，也不实现 CopyOnChange full temporary-document cache。

## 范围

纳入本包的同步合同：

- TypeId：`PartDesign::ShapeBinder`、`PartDesign::SubShapeBinder`、`PartDesign::SubShapeBinderPython`。
- Capability：`part_design.shape_binder`、`part_design.sub_shape_binder` 的 status、covered、remaining_gaps、known_gaps 和 diagnostics vocabulary。
- Fixture：C8-M1 12 个 input fixture 和 12 个 FreeCAD expected，且以 C8-M5 修复后的 expected 为准。
- Diagnostics：`copy_on_change_full_temporary_document_cache_not_supported`、`cycle_rejected_by_property_link`、generic `cycle_dependency` 的边界。
- Output：request-local mesh / subshapes / full subname、`named_shapes.element_map`、maker history、`documentObjectUpdates`、`elementReferenceUpdates` 和 Body replay 证据。

排除本包的内容：

- 不修改 `opencascade-rs`、前端或其它下游仓库。
- 不声明 full FreeCAD temporary-document CopyOnChange cache supported。
- 不引入跨请求持久 BREP、TopoDS_Shape、NamedShape、ElementMap 或几何 cache。
- 不修改 FreeCAD upstream `src/`。

## 步骤

| 步骤 | 任务 | 关闭条件 |
| --- | --- | --- |
| S0 | live 基线与同步声明冻结 | HEAD、队列、capability、C8-M5 release gate 和 non-goal 都写入矩阵。 |
| S1 | 源头合同与能力面复核 | FreeCAD source、cad-core landing、fixtures 和 tests 都有 source candidate 行。 |
| S2 | 同步范围准入与 non-goal 路由 | 每个 scope 行被分类为 sync_required、known_gap_retained、release_gate_only 或 unexpected_mismatch。 |
| S3 | TypeId 与 DocumentGraph 合同复审 | TypeIds、DocumentObject graph、property links、request / response DTO 不漂移。 |
| S4 | fixture expected 与 diagnostics 合同复审 | C8-M5 delta 被吸收，expected fixture gate 和 diagnostics vocabulary 口径一致。 |
| S5 | capability 与前端消费边界发布 | capability payload 与前端可消费字段发布，持久状态和 BREP 非目标写清。 |
| S6 | 发布闸门与下游交接清单 | queue、TSV、whitespace、diff、focused tests 和 stage regression 按分层要求通过。 |

## 下一轮代码落点规则

本包默认是文档和合同发布包。如果 S1-S5 复核发现当前 `cad-core` 与合同不一致，S6 必须把该不一致升级为代码落点，而不是在文档中粉饰为同步项。

允许的代码落点只包括：

- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/src/runtime/reference_lifecycle.cpp`
- `cad-core/src/part_design/feature_shape_binder.cpp`
- `cad-core/src/part_design/body.cpp`
- `cad-core/tests/test_c8_shapebinder.py`
- `cad-core/tests/test_diagnostics.py`
- `cad-core/tests/test_adapters.py`
- `cad-core/tests/test_expected_fixtures.py`

禁止路径：

- adapter 层业务字符串改写。
- fixture 名称分支。
- 按几何类型、面积、长度或输出顺序猜测 ElementMap。
- 把 full BREP、TopoDS_Shape、NamedShape、ElementMap 或 temporary document cache 作为跨请求状态保存。
- 把 `copy_on_change_full_temporary_document_cache` 标成 supported。

## 验收

本轮短跑只要求文档包可排队、TSV 可解析、无尾随空白和 diff 检查通过。S3-S6 需要按矩阵决定是否运行 focused tests 或 stage regression。
