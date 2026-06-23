# 【已实现】C6-M2 S4 GeometryTolerance 与 OCCT 差异收口

## 目标

收口 S2/S3 后剩余的 4 条 bbox / geometry / OCCT mismatch：`ORC-001`、`ORC-003`、`ORC-006`、`ORC-013`。本轮不处理 `ORC-007` Pocket / Body，仍交给 S5。

## 本轮基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`e0a1c91a99`
- `git log -1 --oneline`：`e0a1c91a99 完成 C6-M2 S3 schema drift 收口`
- `git -c core.quotepath=false status --short -uall`：无输出，S4 开始时工作区干净。

## FreeCAD / 合同依据

- FreeCAD：`/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.cpp::TopoShape::getBoundBoxOptimal()` 调用 `BRepBndLib::AddOptimal(_Shape, bounds, false, false)` 并 `bounds.SetGap(0.0)`。
- CAD Core expected 合同：`docs/CADCore方案/细化方案/03-接口与验收样例.md` 明确 native expected `bbox` 使用 FreeCAD `optimalBoundingBox()` / OCCT `AddOptimal` 的 tighter bbox；导入 shape 或 section edge 的 runtime 差异用局部 `bbox_delta` 标明。
- cad-core：S4 新增 `part::objectBBoxForShape()`，把 object `bbox` 字段和 mesh summary bbox 分开；mesh summary 保持原 `bboxForShape()`，避免把显示三角化 bbox 误当 object oracle。

## 逐项结论

| ORC | fixture / object | expected / current delta | decision | 落点 |
| --- | --- | --- | --- | --- |
| `ORC-001` | `c3m1/element-map-child-map-recursive-compound` / `CompoundNested` | S4 后 current `[0,0,0]..[45,5,0]`，delta 全 0；S4 前为 triangulation `+-0.1`。 | `fix_implementation` | `objectBBoxForShape()` + `Part::Compound` 经 `publishPartShape()` 输出 object bbox。 |
| `ORC-003` | `c4m4/topo-reference-pressure-import-unchanged` / `ImportedStep` | S4 后 current `[122.4999999,25,-7]..[175,125,80]`，最大 delta `1.0041e-7`，在 expected `1e-6` 内。 | `fix_implementation` | `Part::ImportStep` 经 `publishPartShape()` 输出 object bbox。 |
| `ORC-006` | `c5m1/partdesign-revolution-profile-linked-face` / `RevolutionFromFace` | S4 后 current `[0,0,5]..[25,5,18.027756377319946]`，最大 delta `1.8e-15`。 | `fix_implementation` | `part_design/feature_revolved.cpp` object bbox 改用 `objectBBoxForShape()`。 |
| `ORC-013` | `p8/app-link-imported-element-map-chain` / `ImportedLinkGroup` | S4 后 current `[-2,-2,0.00999999999996]..[155,80,30]`；相对 FreeCAD 1.2 expected 的 min x/y delta 为 `0.0153279653` / `0.0271520195`。 | `known_environment_gap` | 保留 FreeCAD 1.2 bbox；在该 expected 加 `bbox_delta=0.028` 和 local OCCT 7.9.3 known_gap，删除条件写入 JSON。 |

## 变更文件

- `cad-core/include/cad_core/part/shape_exporter.h`
- `cad-core/src/part/shape_exporter.cpp`
- `cad-core/src/assembly/assembly_utils.cpp`
- `cad-core/src/app/link.cpp`
- `cad-core/src/mesh/feature_mesh_import.cpp`
- `cad-core/src/part/part_boolean.cpp`
- `cad-core/src/part/part_feature.cpp`
- `cad-core/src/part/part_feature_support.cpp`
- `cad-core/src/part/primitive_feature.cpp`
- `cad-core/src/part_design/body.cpp`
- `cad-core/src/part_design/feature_base.cpp`
- `cad-core/src/part_design/feature_dress_up.cpp`
- `cad-core/src/part_design/feature_extrude.cpp`
- `cad-core/src/part_design/feature_hole.cpp`
- `cad-core/src/part_design/feature_pipe.cpp`
- `cad-core/src/part_design/feature_revolved.cpp`
- `cad-core/src/part_design/feature_transformed.cpp`
- `cad-core/tests/test_expected_fixtures.py`
- `cad-core/fixtures/p8/expected/app-link-imported-element-map-chain.freecad.json`
- C6-M2 fixture oracle / backend gap / blocker queue / scope review 矩阵与工作步骤索引。

## 非目标保持

- 未修改 `ORC-007` / `p2/pocket-without-base`。
- 未刷新 `ORC-001/003/006/013` 的 expected bbox 数值。
- 未改全局 bbox tolerance 或 expected fixture 断言逻辑。
- 未采集 native FreeCAD expected，未运行全量 FreeCAD build。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
```

结果：通过；仅出现 OCCT 头文件 `NCollection_UBTreeFiller.hxx` 的既有 `sprintf` deprecated warning。

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_c6m2_s4_geometry_bbox_rows_match_object_oracle
```

结果：通过。

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results
```

结果：仅剩 1 个失败：`p2/pocket-without-base` 的 `execution_failed`，即 `ORC-007`，按计划留给 S5。

## 下一步

- S5 只处理 `ORC-007` Body/Pocket subtractive-without-base 实现或写明 deletion condition。
- S6 在 S5 后跑阶段回归并发布状态。
