# C12-M6 S2 collector 与 expected 准入验证【已实现】

## 目标

验证 wire/wire fixture 和 checked-in expected 是否满足旧 S2 要求的 source-backed collector 条件，而不是 cad-core 自证或手写 expected。

## 必读来源

- `cad-core/fixtures/c4m1/part-ruled-surface-wire-wire.json`
- `cad-core/fixtures/c4m1/expected/part-ruled-surface-wire-wire.freecad.json`
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/tests/test_p8_features.py::test_c4m1_part_ruled_surface_wire_wire_builds_shell_with_provenance`
- `docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/矩阵/part_surface_fixture_oracle_matrix.tsv`

## 操作

1. 确认 fixture 使用 `TypeId=Part::RuledSurface`，输入对象是可重算 wire producers，`Curve1` / `Curve2` 为 `App::PropertyLinkSub`。
2. 确认 expected reference 指向 FreeCADCmd oracle，并记录 FreeCAD / OCCT baseline、shape、topology_counts、bbox、volume 和 named_shapes expectation。
3. 检查 collector 是否支持 `Part::RuledSurface` native type；如需重跑，只允许在 S2 记录 runtime baseline，不在本步改 expected。
4. 若 expected 缺失、reference 不可信或 collector 无法复现，关闭为 `retained_validation_blocker`，不进入 implementation。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c4m1_part_ruled_surface_wire_wire_builds_shell_with_provenance
```

S2 可只运行 focused test；不要默认刷新 expected 或跑 full build。

## S2 live baseline

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=af6477dd3b`。
- `git log -1 --oneline=af6477dd3b docs: 完成 C12-M6 S1 源码证据复核`。
- `git -c core.quotepath=false status --short -uall` 无输出，S2 起点为 clean。

## fixture 与 expected 复核结论

- `cad-core/fixtures/c4m1/part-ruled-surface-wire-wire.json` 使用 request-local `DocumentObject graph`：`LowerWire` / `UpperWire` 为 `Part::RegularPolygon` wire producers，`RuledSurface` 为 `TypeId=Part::RuledSurface`。
- `Curve1` / `Curve2` 均为 `App::PropertyLinkSub`，分别指向 `LowerWire` / `UpperWire`；`Orientation=Automatic`；`recompute.objs=["RuledSurface"]`。
- fixture 输入未携带 `BREP`、`TopoDS`、persistent `NamedShape`、persistent `ElementMap`、mesh 或旧 shape cache 字段。
- checked-in expected 的 `reference` 为 `FreeCADCmd oracle from part-ruled-surface-wire-wire.json; objects: Part::RegularPolygon, Part::RegularPolygon, Part::RuledSurface`，不是 cad-core recompute 自证引用。
- expected baseline 记录 `freecad_version=1.2.0 revision 20260519`；当前 expected schema 未序列化独立 `occt_version` 字段，OCCT 基线随该 FreeCADCmd oracle 隐式绑定。S2 不刷新 expected，后续若要显式记录 OCCT 版本，应另做 collector metadata 增强，不阻塞本次 checked-in expected 准入。
- expected 几何信号：`shape=occt_shell`，`topology_counts={faces:4, edges:12, vertices:8}`，`bbox.min=[-2.0000001,-2.0000001000000003,-1e-07]`，`bbox.max=[2.0000001,2.0000001,3.0000001]`，`volume=10.5625`。
- expected object fields：`feature=part_ruled_surface`，`status=ok`，`orientation=Automatic`，`source_curve1=LowerWire`，`source_curve2=UpperWire`。
- expected named shape / element map 关键信号：`element_history_status_contains=["part_ruled_surface:wire_wire_brepfill_shell"]`，`element_map_contains=["LowerWire.Edge1","UpperWire.Edge1"]`，`element_kind_by_source` 均为 `edge`，`history_sources_any` 覆盖 Lower/Upper 两个 wire 的代表 edge。S2 只确认这些 checked-in expected 信号存在并被 focused test 消费；其 provenance 强度由 S4 专门裁决。

## collector 复核结论

- `cad-core/tools/collect_freecad_expected.py` 的 `SUPPORTED_NATIVE_TYPES` 包含 `Part::RegularPolygon` 和 `Part::RuledSurface`。
- collector 的 native path 通过 `create_native_object()` / `doc.addObject(type_id, name)` 创建 FreeCAD native object，并通过 `set_property()` 写入 `App::PropertyLinkSub` 与 `App::PropertyEnumeration`。
- `object_expected_payload()` 对 `Part::RuledSurface` 进入 `ruled_surface_payload()`，从 FreeCAD native `obj.Shape` 生成 `shape_summary()`，并从 fixture properties 记录 `feature`、`source_curve1`、`source_curve2`、`orientation`。
- 裁决：collector 对 `Part::RuledSurface` native type 是 source-backed；checked-in expected 对 geometry / object_fields 足以 admission。expected 中 topo provenance 期望属于 checked-in current-provenance 信号，已由 focused test 验证是否满足当前输出，强度仍留给 S4。

## focused test

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c4m1_part_ruled_surface_wire_wire_builds_shell_with_provenance
```

结果：`Ran 1 test in 0.202s`，`OK`。

## S2 裁决

- 未发现 expected 缺失。
- 未发现 reference 不可信或 cad-core 自证风险。
- collector 支持 `Part::RuledSurface` native type；checked-in expected 的 FreeCADCmd reference、FreeCAD baseline、geometry / object_fields 足以作为 S2 admission evidence。
- focused expected-backed unittest 通过，没有 current/expected mismatch。
- S2 关闭为 `collector_expected_admitted`，只关闭 `C12M6-BLOCKER-201/202/203/204`；S3 input schema gate、S4 shell/topo provenance gate 和 S5 publication gate 保持 open。

## 输出

- `c12m6_ruled_surface_wire_wire_collector_validation_matrix.tsv` 的 S2 行更新为 `passed_s2`。
- `c12m6_ruled_surface_wire_wire_backend_gap_classification.tsv`、`scope_review_matrix.tsv`、`blocker_queue.tsv`、`validation_matrix.tsv` 只关闭 S2 owner 行。
- 未修改 `cad-core/src`、fixtures、expected、tests、adapters 或 capability source，未刷新 expected。

## 下一步

下一步为 S3 input schema gate：验证 request-local `DocumentObject graph` / `PropertyLinkSub` / `recompute` schema 是否足以表达 wire/wire，不允许 BREP、TopoDS、persistent `NamedShape` / `ElementMap` 或 adapter shortcut 进入请求合同。
