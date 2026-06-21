# 【已实现】C5-M8-S4 compound optional 与 wrapper 边界

状态：`done_expected_and_diagnostic_backed`

## 实现结果（2026-06-21）

- `cad-core/fixtures/c5m8/part-filling-compound-optional-boundary.json` 已新增并采集 `expected/part-filling-compound-optional-boundary.freecad.json`，证明 `TopoShapeExpansion.cpp::expandCompound()` 对 `Part.makeFilledFace(...)` helper 的 compound source 可以 expected-backed 进入 `BRepOffsetAPI_MakeFilling`。
- `cad-core/src/part/topo_shape_expansion.cpp` / `include/cad_core/part/topo_shape_expansion.h` 已把 compound source expansion 暴露为 `compound_source_count`、`compound_expanded_source_count`、`compound_source_expansion_status` 和 `part_filling:compound_source_expansion` history status；focused test 固定 source evidence，避免以后只靠几何结果碰巧通过。
- `cad-core/fixtures/c5m8/part-filling-wrapper-boundary.json` 与 `part-filling-wrapper-uv-point-boundary.json` 已新增为 diagnostic-backed fixture。直接 `Part.BRepOffsetAPI.MakeFilling` add/build/shape lifecycle 与 `Add(U,V,Support,Order)` UV point-on-support 分支均输出稳定 `unsupported_wrapper_lifecycle`，并保留 target/subname。
- wrapper 删除条件：只有证明 direct wrapper 能表达成同一 request-local Filling DTO，且不创建跨请求 mutable `BRepOffsetAPI_MakeFilling` builder，才能把这两个 diagnostic-backed fixture 改为 supported；当前不把 wrapper UV point-on-support 当作 `Part.makeFilledFace` helper 支持。
- C5M8-BLK-401 / ORC-401 / SCOPE-401 已关闭为 expected-backed compound + diagnostic-backed wrapper；S5 仍负责最终 capability/docs 队列收口。

## 目标

关闭 Filling compound optional case，并对直接 `Part.BRepOffsetAPI.MakeFilling` wrapper 做 owner 判定。直接 wrapper 共享 OCCT builder，但它是 Python mutable lifecycle；除非能映射到 cad-core request-local DTO，否则只能 diagnostic-backed。

## 必读

- C5-M8 方案与 `C5M8-ORC-401`。
- `src/Mod/Part/App/TopoShapeExpansion.cpp::expandCompound()` 与 `TopoShape::makeElementFilledFace()`
- `src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp::PyInit()`、`loadInitSurface()`、`add()`、`build()`、`shape()`
- `cad-core/src/part/part_filling.cpp`
- `cad-core/src/adapters/c_api/c_api.cpp`
- C5-M8 non-goal registry。

## 产物

- 新增 expected-backed fixture：`c5m8/part-filling-compound-optional-boundary`，覆盖 compound source expansion 与 evidence。
- 新增 wrapper boundary fixture：`c5m8/part-filling-wrapper-boundary`。
- 对 `c5m8/part-filling-wrapper-uv-point-boundary` 做明确 owner：若不支持，必须产出 `unsupported_wrapper_lifecycle` 或同级稳定 diagnostic，并写明删除条件。
- 更新 C5-M8 局部矩阵、non-goal registry、capability metadata 和本 step 状态。

## 非目标

- 不创建跨请求持久 BRepOffsetAPI builder。
- 不把 wrapper UV point-on-support 当作 `Part.makeFilledFace` helper 支持。
- 不关闭 GeomPlate / Sweep / ProjectOnSurface / Loft 的 future owners。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线 docs/CADCore3.0 cad-core
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```
