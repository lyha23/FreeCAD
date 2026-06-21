# C5-M8-S1 helper Surface / Support / Order 实现

## 目标

在同一 `Part.makeFilledFace(...)` source-backed request DTO 内实现 `Surface` / `Supports` / `Orders`，对齐 FreeCAD `LoadInitSurface`、`getSupport()`、`getOrder()` 和 boundary edge `maker.Add(edge, support, order, IsBound=true)`。

## 必读

- C5-M8 总入口、方案和局部矩阵。
- `src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()`
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()`
- `cad-core/src/part/part_filling.cpp`
- `cad-core/include/cad_core/part/topo_shape_expansion.h`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/tests/test_p8_features.py`

## 产物

- 扩展 Filling DTO / parser / core API，表达 initial surface、support face map 和 order map。
- 新增 expected-backed fixtures：`c5m8/part-filling-initial-surface-boundary`、`c5m8/part-filling-support-order-edge-face`。
- 新增 diagnostic fixture：`c5m8/part-filling-invalid-support-order`。
- 删除或收敛 `Surface` / `Supports` / `Orders` 的 broad `unsupported_property`，保留 target/subname 可定位诊断。
- 更新 capabilities、C5-M8 局部矩阵和本 step 状态。

## 阻塞记录（2026-06-21）

状态：`blocked_native_support_order_oracle`

S1 未实现，原因是 support/order 的 native FreeCAD oracle 在当前本机 FreeCADCmd 上无法收敛到可写入 expected 的短跑基线；不能伪造 `part-filling-support-order-edge-face` expected。

已验证事实：

- `FreeCADCmd` 可用，版本为 `FreeCAD 1.2.0devR20260519`；现有 `c3m4/part-filling-closed-wire-default` collector smoke 可以完成。
- `part-filling-initial-surface-boundary` 单独采集可完成，但 S1 的最小完整语义批次还包含 support face map 与 order map，不能只提交 initial surface。
- support/order 最小 probe 使用 `Part::Plane` 的 `Face1` 和同一 face 的 `Edge1..Edge4` 作为 boundary，调用 `Part.makeFilledFace(edges, supports=[(Edge1, Face1)], orders=[(Edge1, 0)])`。脚本输出到 `probe:before support c0` 后 30 秒未返回，需中断。
- 另两次 collector probe 分别使用 regular polygon boundary + coplanar support face、单 edge support + `G1` / `C0` order，也在 support face 进入 native helper 后超过 30 秒未返回。
- 主线程复核发现当前 `AppPartPy.cpp::parseSequence()` 传给 callback 的是 tuple 第一个元素而非第二个 value；同机 `FreeCADCmd` probe 调用 `Part.makeFilledFace(edges, surface=Part.makePlane(...))` 在 `before surface` 后以 `139` 退出，说明 S1 native helper oracle 在 `surface` / `supports` / `orders` kwargs 入口仍不稳定，不能据此生成 checked-in expected。
- 复现脚本保留在 `cad-core/tools/probe_filling_s1_contract.py`；运行方式示例：`/opt/homebrew/bin/timeout 30 FreeCADCmd cad-core/tools/probe_filling_s1_contract.py surface`。

当前边界：

- 不生成 `cad-core/fixtures/c5m8/*support-order*` expected。
- 不把 `Surface` / `Supports` / `Orders` 从 broad diagnostic 改成 supported。
- 不落半成品 DTO/parser/core API。
- `C5M8-BLK-101` 保持阻塞，后续需要先找到可返回的 FreeCAD support/order oracle case，或由 scope 明确允许将 support face map 降级为 diagnostic-backed 后再继续。

## 非目标

- 不处理非默认 params。
- 不处理 non-boundary constraints。
- 不发布直接 `Part.BRepOffsetAPI.MakeFilling` wrapper。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线 docs/CADCore3.0 cad-core
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```
