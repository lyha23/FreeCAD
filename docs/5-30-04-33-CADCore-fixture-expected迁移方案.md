# CAD Core fixture expected 迁移方案

本文固定 `cad-core` fixture 的 expected 迁移口径：成功几何 fixture 的几何 golden 必须进入 `fixtures/<phase>/expected/*.freecad.json`，测试代码里不继续保留 bbox、volume、topology count、mesh summary、named shape 等几何预期的内联断言。

## 核心结论

- `expected/*.freecad.json` 是成功几何 fixture 的 FreeCAD / oracle golden。
- 错误 fixture 只固定 diagnostics code，不新增空 expected 文件。
- 老 fixture 已经存在内联几何断言时，也必须迁移；不能以“历史上已经这么写”为理由继续保留。
- 测试代码可以保留流程性断言，例如 diagnostics 为空、对象存在、mesh 非空、字段不缺失；但几何数值和拓扑数量必须来自 expected 文件。
- expected 不能从 cad-core 当前输出反抄。来源必须是 FreeCAD oracle、已验证的 geometry-equivalent oracle，或明确标注的 known gap。
- `cad-core/tests/test_mvp.py` 不应继续承载全部 fixture 校验；后续要改成类似 `opencascade-rs/docs/草图支持/fixtures/freecad_internal_faces/collect_internal_faces.py` 的模式：fixture 输入、结构化 expected、通用比较器、少量命令入口和少量流程性单测。
- `collect_internal_faces.py` 本身作为参照模式，不把 CAD Core 业务塞进该脚本；CAD Core 侧新增自己的 fixture validator / oracle collector / data-driven tests。

## 目标测试架构

当前 `cad-core/tests/test_mvp.py` 已接近三千行，问题不是单纯“文件长”，而是职责混在一起：

- CLI / FFI adapter 测试；
- diagnostics code 矩阵；
- recompute 运行工具；
- bbox、volume、topology、mesh、named shape golden 比较；
- fixture 专属流程断言；
- expected 文件读取与容差逻辑。

目标拆分如下：

| 文件 | 职责 | 是否承载几何 golden |
| --- | --- | --- |
| `cad-core/tests/fixture_runner.py` | 运行 `cad-core recompute`、临时输出、FFI/CLI 共用工具 | 否 |
| `cad-core/tests/fixture_expected.py` | 读取 `fixtures/<phase>/expected/*.freecad.json`，比较 bbox / volume / topology / mesh / named shape | 否，只读 expected |
| `cad-core/tests/test_expected_fixtures.py` | 数据驱动遍历 expected 文件，自动运行同名 fixture 并调用通用比较器 | 否 |
| `cad-core/tests/test_diagnostics.py` | diagnostics code、对象/属性/错误定位矩阵 | 否 |
| `cad-core/tests/test_adapters.py` | CLI export、C ABI、capabilities、文件导入导出 smoke | 否，必要几何回读走 expected 或专用 helper |
| `cad-core/tests/test_feature_flows.py` | 少量不适合纯 expected 的流程断言，例如 body tip、RefineModel path、history kind 是否存在 | 不写数值 golden |
| `cad-core/tests/test_mvp.py` | 过渡期入口；最终只保留极少数 MVP smoke，或拆空后删除 | 否 |

拆分后，新增成功几何 fixture 的常规流程应变成：

1. 新增 `fixtures/<phase>/<fixture>.json`。
2. 用 FreeCAD oracle / geometry-equivalent oracle 生成 `fixtures/<phase>/expected/<fixture>.freecad.json`。
3. `test_expected_fixtures.py` 自动发现并校验，不再手写一个 `test_xxx` 只为了写 bbox / volume / topology。
4. 如果该 fixture 还要约束业务流程，例如 `Body.Tip`、`transform_mode`、`element_map_status`，才在 `test_feature_flows.py` 写少量字段断言。

## 对齐 `collect_internal_faces.py` 的方式

`collect_internal_faces.py` 里值得复用的是模式，不是直接复用脚本：

- 输入 fixture 是独立 JSON；
- 采集脚本把 FreeCAD 运行结果整理成结构化 JSON；
- 输出含 `schemaVersion`、case、FreeCAD 版本、shape summary、internal face/edge/vertex、element map；
- 业务测试只比较结构化结果，而不是把所有 expected 数值写进测试函数。

CAD Core 对应落点：

- `cad-core/tools/collect_freecad_expected.py`：后续新增，用本机 `FreeCADCmd` 按 phase/fixture 生成 expected；优先覆盖 Part primitive、PartDesign、App::Link、import/export 等能由 FreeCAD 原生运行得到的 case。
- `cad-core/tests/fixture_expected.py`：先实现通用比较器，支持单对象、多对象、bbox_delta、volume_delta、topology_counts、mesh_summary，后续再扩展 `named_shapes` / `history` / `stable_subnames`。
- `expected/*.freecad.json`：保留 oracle 来源字段，例如 `reference`、`freecad_version`；必要时增加 `schema_version`，但不强制一次性改完旧文件。
- `test_expected_fixtures.py`：以 expected 文件为准自动运行，形成“有 expected 就必须能过”的统一验收面。

重要边界：

- 不用 CAD Core 当前输出反抄 expected；若用 geometry-equivalent oracle，必须在 `reference` 写清来源。
- diagnostics fixture 不创建空 expected。
- named shape / history 这类拓扑命名 golden 不能只用 bbox/volume helper 凑过去；需要扩展 expected schema 后再迁移。
- `collect_internal_faces.py` 仍属于 `opencascade-rs` 草图内部面 oracle 采集脚本，不承接 CAD Core 的测试运行职责。

## 需要迁移的断言

以下断言如果出现在 `cad-core/tests/test_mvp.py` 或后续测试文件中，属于迁移对象：

- `bbox.min` / `bbox.max`
- `volume`
- `topology_counts` 或 `assert_topology_counts(...)`
- `mesh.summary.bbox` / `mesh.summary.volume` / `mesh.summary.triangle_count`
- `named_shapes`、stable subname、split / deleted / merge history 的 golden 结果
- 任何能代表 FreeCAD 几何语义的固定数值、固定拓扑数量或固定命名结果

以下断言可以留在测试代码里：

- diagnostics code 矩阵
- recompute 成功或失败状态
- 对象、mesh、subshape map、named shape map 是否存在
- adapter 行为，例如 CLI / C ABI 输出是否可解析
- 错误 fixture 的字段定位、错误码、对象名、属性名

## expected 文件格式

成功几何 fixture 的 expected 文件优先使用以下字段：

```json
{
  "object": "Body",
  "reference": "FreeCAD / oracle source description",
  "freecad_version": "1.2.0 revision 20260519",
  "bbox": {
    "min": [0.0, 0.0, 0.0],
    "max": [10.0, 5.0, 10.0]
  },
  "volume": 320.0,
  "topology_counts": {
    "faces": 10,
    "edges": 24,
    "vertices": 16
  }
}
```

需要 mesh 或 topo naming golden 时再补：

- `mesh_summary`
- `named_shapes`
- `stable_subnames`
- `history`
- `known_gap`

`known_gap` 只能说明 oracle 暂不可用或 FreeCAD 语义尚未完整迁移，不能把失败用例伪装成通过。

多对象 fixture 使用：

```json
{
  "reference": "FreeCAD / oracle source description",
  "freecad_version": "1.2.0 revision 20260519",
  "bbox_delta": 0.02,
  "objects": {
    "Feature": {
      "bbox": {"min": [0.0, 0.0, 0.0], "max": [4.0, 2.0, 2.0]},
      "volume": 16.0
    },
    "Body": {
      "bbox": {"min": [0.0, 0.0, 0.0], "max": [4.0, 2.0, 2.0]},
      "volume": 16.0,
      "topology_counts": {"faces": 10, "edges": 24, "vertices": 16}
    }
  }
}
```

mesh summary 使用：

```json
{
  "object": "ImportedStl",
  "reference": "FreeCAD / mesh oracle source description",
  "freecad_version": "1.2.0 revision 20260519",
  "bbox": {"min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 0.0]},
  "volume": 0.0,
  "topology_counts": {"faces": 2, "edges": 5, "vertices": 4},
  "mesh_summary": {"vertex_count": 4, "triangle_count": 2}
}
```

## 迁移流程

1. 先抽公共测试支撑。
   - 新增 `fixture_runner.py`，把 `run_recompute_file(...)`、`run_recompute(...)`、FFI library loader 等公共运行逻辑移出 `test_mvp.py`。
   - 新增 `fixture_expected.py`，把 `expected_freecad(...)`、bbox/volume/topology/mesh 比较器移出 `test_mvp.py`。
2. 盘点测试中的内联几何断言。
   - 建议先查：`rg -n "bbox|volume|topology_counts|assert_topology_counts|mesh_summary|named_shapes" cad-core/tests`
   - 只统计成功几何 fixture；diagnostics 矩阵单独保留。
3. 给每个成功几何 fixture 建立同名 expected 文件。
   - 路径固定为 `cad-core/fixtures/<phase>/expected/<fixture>.freecad.json`。
   - expected 内容必须来自 FreeCAD / oracle 或已写明依据的 geometry-equivalent oracle。
4. 新增数据驱动 expected 测试。
   - `test_expected_fixtures.py` 遍历 expected 文件，按 `<phase>/<fixture>` 自动运行 recompute。
   - 文件存在 expected 即代表它是成功几何 fixture；diagnostics fixture 不进入该遍历。
5. 修改旧测试读取 expected 或移除重复测试。
   - 优先复用 `fixture_expected.assert_result_matches_expected(...)` 这类 helper。
   - 如果字段不够，扩展 helper，而不是在测试里重新写固定数值。
6. 删除迁移目标的内联几何 golden。
   - 同一个 fixture 不允许同时存在 expected 文件和重复的内联 bbox / volume / topology golden。
   - 流程性断言可留，但不能承载几何预期。
7. 按职责拆分 `test_mvp.py`。
   - diagnostics 矩阵迁入 `test_diagnostics.py`。
   - CLI / FFI 迁入 `test_adapters.py`。
   - body tip、feature mode、history kind 这类流程断言迁入 `test_feature_flows.py`。
   - `test_mvp.py` 最终只保留少量 smoke 或删除。
8. 对应阶段运行最小测试范围。
   - 文档迁移只记录目标；真正改测试和 expected 后，再跑该 phase 相关 unittest。

## 实施顺序

第一阶段先减测试体积，不改变 CAD Core 行为：

1. 提取 `fixture_runner.py` 与 `fixture_expected.py`。
2. 新增 `test_expected_fixtures.py`，先只覆盖已经存在 expected 的阶段：`mvp`、`p2`、`p3a`、`p3b`、`p4`、`p5`、`p7`，以及 P8 已补 expected 的 case。
3. 从 `test_mvp.py` 删除这些 fixture 的重复 bbox / volume / topology / mesh count 断言，保留流程断言。
4. 跑 `python3 -m unittest tests.test_expected_fixtures` 与被拆分出来的 focused tests。

第二阶段再拆大文件：

1. 把 diagnostics 矩阵迁到 `test_diagnostics.py`。
2. 把 CLI / FFI / export / import roundtrip 迁到 `test_adapters.py`。
3. 把 PartDesign、App::Link、Part primitive 的流程断言迁到 `test_feature_flows.py` 或按 phase 拆成更小文件。
4. `test_mvp.py` 只保留极少数兼容入口；若没有必要，直接删除。

第三阶段补 oracle 生成器：

1. 新增 `cad-core/tools/collect_freecad_expected.py`。
2. 先支持单 fixture 生成：`collect_freecad_expected.py fixtures/p8/part-box.json --out fixtures/p8/expected/part-box.freecad.json`。
3. 再支持 phase 批量生成和 `--check` 模式。
4. 对无法直接由 FreeCAD 原生跑出的 geometry-equivalent oracle，继续允许手写 expected，但必须写清 `reference`。

第四阶段迁移 named shape / history：

1. 扩展 expected schema，新增 `named_shapes`、`element_map_status`、`element_map`、`history`、`stable_subnames`。
2. 只迁移已经有明确 FreeCAD / topo naming oracle 的 case。
3. P6 named-shape/history 不用 bbox/volume 方案硬套；必须等 schema 和 oracle 依据明确后再迁。

## P2 第一批迁移

当前 `cad-core/fixtures/p2` 有 6 个 fixture：

| fixture | 分类 | 迁移要求 |
| --- | --- | --- |
| `rect-pad-pocket` | 成功几何 | 已有 `expected/rect-pad-pocket.freecad.json`；补齐 `object`、`reference`、`freecad_version` 元数据 |
| `body-basefeature-pad` | 成功几何 | 新增 `expected/body-basefeature-pad.freecad.json`；删除 `test_body_basefeature_pad_uses_base_solid` 中的 bbox / volume 内联 golden，并补 topology count expected |
| `missing-basefeature` | 错误诊断 | 保留 diagnostics code，不新增 expected |
| `pocket-without-base` | 错误诊断 | 保留 diagnostics code，不新增 expected |
| `pocket-open-sketch` | 错误诊断 | 保留 diagnostics code，不新增 expected |
| `unsupported-pocket-type` | 错误诊断 | 保留 diagnostics code，不新增 expected |

P2 迁移完成后的测试形态：

- `test_p2_fixture_diagnostics` 仍作为 diagnostics code 矩阵。
- `test_rect_pad_pocket_outputs_cut_body` 从 expected 读取 object、bbox、volume、topology count。
- `test_body_basefeature_pad_uses_base_solid` 从 expected 读取 object、bbox、volume、topology count。
- 两个成功几何 fixture 的 diagnostics 必须为空。
- 四个错误 fixture 不进入 expected 目录。

2026-05-30 落地状态：

- 同步收口了已有 expected 的 `mvp/rect-pad` 与 P3A 成功几何测试：测试改为复用 `expected_freecad(...)` / `assert_object_matches_expected(...)`，不再重复写 bbox、volume、topology count。
- `rect-pad-pocket` 已补齐 `object`、`reference`、`freecad_version` 元数据。
- `body-basefeature-pad` 已新增 `expected/body-basefeature-pad.freecad.json`，使用仓库既有 `FreeCADCmd Part.makeBox(10, 5, 5)` geometry-equivalent oracle。
- `test_rect_pad_pocket_outputs_cut_body` 与 `test_body_basefeature_pad_uses_base_solid` 已改为读取 expected 校验 object、bbox、volume、topology count。
- 四个错误 fixture 继续只在 diagnostics 矩阵中验收。
- P3B 已补齐 24 个成功几何 fixture 的 expected；6 个错误 fixture 继续只验 diagnostics。
- P4 已补齐 7 个 shape 成功 fixture 的 expected；`datum-point-part-placement` 只验点对象字段，暂不套 shape expected。
- P5 expected 已扩展到 56 个文件，并新增 `sketch_internal.min_internal_counts`，用于把复杂 InternalShape 的 face count、最低 edge/vertex count、基于 InternalFace 的 Pad volume、SketchPlaneFrame mesh bbox、open cutter split fragment 子集从测试内联断言迁入 expected；`sketch-bspline-profile` 与 `sketch-internal-face-figure8-bspline` 已登记为 `known_gap: internal_shape_oracle_pending`，等待 dedicated BSpline InternalShape oracle 后再冻结内部 face / edge 命名/count。
- `named_shapes` expected schema 已扩展到 owner、`element_map_status`、element map 精确/缺失/前缀、元素种类、元素状态、history kind include/absent/source/prefix/entry 等通用断言；P6 `named-shape-indexed-pad`、P6 body maker/split/merge history、P3B profile/taper history、P7 refine/dress-up/transform history，以及 P8 App::Link、primitive、import、boolean/fragments/section 等 named-shape golden 已从测试内联断言迁入 expected。
- P6 expected 当前已有 13 个文件；stable subname preserved/indexed-reference 等尚无 dedicated native ElementMap oracle 的成功 fixture 已显式登记为 `known_gap: stable_subname_oracle_pending`，不再隐藏成“无 expected”。
- P7 expected 已扩展到 Hole、DressUp、Pattern、Transform 等成功几何 fixture；当前工作树中 `cad-core/fixtures/p7/expected` 已有 64 个 expected 文件，动态 Hole thread、thread clearance 和 chained dress-up mirror 中尚无 native geometry oracle 的 case 已显式登记为 known gap。
- P8 expected 已补齐 App::Link、Assembly、Part primitive、import、Boolean、Fragments、Section 等成功几何 fixture；当前工作树中 `cad-core/fixtures/p8/expected` 已有 57 个 expected 文件，P8 feature flow 已拆到 `cad-core/tests/test_p8_features.py`，固定 bbox / volume / topology 改为读取 expected。
- 已新增 `cad-core/tests/fixture_runner.py`、`cad-core/tests/fixture_expected.py` 和 `cad-core/tests/test_expected_fixtures.py`；`test_expected_fixtures.py` 会自动遍历 expected 文件并运行同名 fixture。
- diagnostics 矩阵已拆到 `cad-core/tests/test_diagnostics.py`；C ABI / CLI export / capabilities 已拆到 `cad-core/tests/test_adapters.py`；P2-P4 flow 已拆到 `cad-core/tests/test_feature_flows.py`；P5、P6、P7、P8 feature flow 分别拆到 `test_p5_sketch.py`、`test_p6_topology.py`、`test_p7_features.py`、`test_p8_features.py`；`test_mvp.py` 已收缩为单个 MVP smoke。
- 已新增 `cad-core/tools/collect_freecad_expected.py`，采用本机 `FreeCADCmd` 作为 oracle 入口；当前支持单 fixture 生成、phase 批量、`--check`、`--skip-unsupported`，并已覆盖 P8 中 26 个可直接对齐原生 FreeCAD 的 fixture：Part primitive、App::Link、Part boolean / multi-boolean 与 IGES import。BREP/STEP import、Section、Torus、Ellipsoid 等仍保留为既有 geometry-equivalent expected，等 CAD Core 与原生 FreeCAD bbox 口径对齐后再纳入 collector。
- 当前宽泛 `rg -n "bbox|volume|topology_counts|assert_topology_counts|mesh_summary|named_shapes|element_map_status|[\"element_map\"]|[\"history\"]" cad-core/tests` 只剩 expected helper 本身、adapter roundtrip 使用 expected 字段、对象/mesh 之间的一致性关系、diagnostics history 语义检查，以及 FFI/CLI 输出一致性；P6/P7/P8 直接 named-shape golden 已迁出测试代码。
- 当前全 fixture 分类中，成功但没有 expected 的 fixture 只剩 `mvp/empty`、`p4/datum-point-part-placement`、`p7/origin-identity-placement`，三者都不产生 shape/mesh/bbox/volume 这类几何 golden；错误 fixture 未挂 expected。
- 已验证：
  - `python3 -m unittest tests.test_expected_fixtures`
  - `python3 -m unittest tests.test_diagnostics`
  - `python3 -m unittest tests.test_adapters`
  - `python3 -m unittest tests.test_feature_flows`
  - `python3 -m unittest tests.test_p5_sketch`（当前 `Ran 48 tests`, `OK (skipped=1)`）
  - `python3 -m unittest tests.test_expected_fixtures`（当前 `OK (skipped=20)`；P5 BSpline InternalShape、P6 stable subname、P7 dynamic thread / chained dress-up 等 pending oracle case 当前按 known gap 跳过）
  - `python3 -m unittest tests.test_p6_topology tests.test_p7_features`
  - `python3 -m unittest tests.test_p8_features`
  - `python3 -m unittest tests.test_expected_fixtures tests.test_diagnostics tests.test_adapters tests.test_feature_flows tests.test_p5_sketch tests.test_p6_topology tests.test_p7_features tests.test_p8_features tests.test_mvp`（当前 `Ran 218 tests`, `OK (skipped=21)`）
  - `python3 -m unittest tests.test_mvp`
  - `python3 cad-core/tools/collect_freecad_expected.py cad-core/fixtures/p8/part-box.json --check`
  - `python3 cad-core/tools/collect_freecad_expected.py --phase p8 --check --skip-unsupported`（当前 `processed=26 skipped=26 failed=0`）
  - `python3 -m py_compile cad-core/tools/collect_freecad_expected.py cad-core/tests/fixture_expected.py cad-core/tests/test_expected_fixtures.py cad-core/tests/test_feature_flows.py cad-core/tests/test_p5_sketch.py cad-core/tests/test_p6_topology.py cad-core/tests/test_p7_features.py cad-core/tests/test_p8_features.py cad-core/tests/test_mvp`
  - `git diff --check -- cad-core/tests cad-core/tools cad-core/fixtures/p3b/expected cad-core/fixtures/p5/expected cad-core/fixtures/p6/expected cad-core/fixtures/p7/expected cad-core/fixtures/p8/expected docs/5-30-04-33-CADCore-fixture-expected迁移方案.md`

## 验收标准

- 成功几何 fixture 的 bbox、volume、topology、mesh 或 named shape golden 不再内联在测试代码中。
- 每个成功几何 fixture 都有同名 expected 文件，或明确标注为 pending / known gap 且不伪装成已完成。
- 错误 fixture 只在 diagnostics 矩阵中验收。
- 文档、fixture 和 tests 同步更新。
- 迁移后 `rg -n "bbox|volume|topology_counts|assert_topology_counts|mesh_summary|named_shapes" cad-core/tests` 不应再出现未解释的几何 golden 内联断言。
- `test_mvp.py` 不再是主要 fixture 验收文件；新增 fixture expected 后，应优先由 `test_expected_fixtures.py` 自动发现并校验。
- `python3 -m unittest tests.test_expected_fixtures` 必须能单独验证所有 expected 文件。
