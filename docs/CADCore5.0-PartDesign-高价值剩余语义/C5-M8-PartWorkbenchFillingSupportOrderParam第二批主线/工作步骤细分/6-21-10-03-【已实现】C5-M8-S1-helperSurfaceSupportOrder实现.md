# 【已实现】C5-M8-S1 helper Surface / Support / Order 实现

状态：`done_cad_core_source_backed_known_gap`

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
- 优先新增 expected-backed fixtures：`c5m8/part-filling-initial-surface-boundary`、`c5m8/part-filling-support-order-edge-face`；若现有 `FreeCADCmd` helper kwargs oracle 仍不可稳定返回，则不得伪造 expected，改为 source-backed / known_gap / diagnostic-backed 记录。
- 新增 diagnostic fixture：`c5m8/part-filling-invalid-support-order`。
- 删除或收敛 `Surface` / `Supports` / `Orders` 的 broad `unsupported_property`，保留 target/subname 可定位诊断。
- 更新 capabilities、C5-M8 局部矩阵和本 step 状态。

## 实现结果（2026-06-21）

- `cad-core` 已在同一 source-backed `Part::FilledFace` request DTO 内解析 `Surface`、`Supports`、`Orders`：`Surface` 解析为单 face source；`Supports` 使用 target edge + nested `Support` face link；`Orders` 使用 target edge + `Order` / `Continuity` 字段，支持 `C0/G1/C1/G2/C2/C3/CN` 名称或同 FreeCAD / OCCT 枚举编号。
- `cad-core/include/cad_core/part/topo_shape_expansion.h`、`cad-core/src/part/topo_shape_expansion.cpp` 已把 initial surface、support face map、order map 传入 `BRepOffsetAPI_MakeFilling`，并按 FreeCAD 调用顺序执行 `LoadInitSurface` 与 boundary `Add(edge, support, order, IsBound=true)`。
- `cad-core/src/app/document_object.cpp` 与 `cad-core/src/app/property_links.cpp` 已让 nested `Support` link 参与 recompute dependency，不改变 feature parser 读取的顶层 `Supports` target link。
- `cad-core/fixtures/c5m8/part-filling-initial-surface-boundary.json` 与 `part-filling-support-order-edge-face.json` 是 source-backed fixtures；对应 `expected/*.freecad.json` 只记录 native helper oracle known_gap / 删除条件，不伪造 FreeCAD geometry expected。
- `cad-core/fixtures/c5m8/part-filling-invalid-support-order.json` 是 diagnostic-backed fixture，固定 `invalid_support_target`、`invalid_order_source` 的 target/subname。
- `c4m1/part-filling-advanced-deferred` 不再把 `Surface` 记为 broad unsupported；`SurfaceDeferred` source-backed 成功，旧 schema 的 `SupportsDeferred` / `OrdersDeferred` 收敛为具体 invalid diagnostics，非默认参数仍留给 S2。
- 当前 source-backed support/order fixture 使用 G1 order 稳定通过；G2 order parser 已进入 DTO，但本机 OCCT 组合下 G2 support/order geometry 仍失败，需等稳定 native helper expected 或后续专门 fixture 再关闭 `filling_support_order_g2_expected`。

## 范围纠偏（2026-06-21）

状态：`done_cad_core_source_backed_known_gap`

S1 仍未实现，但不再要求先修改 FreeCAD 上游源码来恢复 helper oracle。C5-M8 只交付 `cad-core`；`src/Mod/Part/App/AppPartPy.cpp` 和 `TopoShapeExpansion.cpp` 只作为语义依据读取。

已验证事实：

- `FreeCADCmd` 可用，版本为 `FreeCAD 1.2.0devR20260519`；现有 `c3m4/part-filling-closed-wire-default` collector smoke 可以完成。
- `part-filling-initial-surface-boundary` 单独采集可完成，但 S1 的最小完整语义批次还包含 support face map 与 order map，不能只提交 initial surface。
- support/order 最小 probe 使用 `Part::Plane` 的 `Face1` 和同一 face 的 `Edge1..Edge4` 作为 boundary，调用 `Part.makeFilledFace(edges, supports=[(Edge1, Face1)], orders=[(Edge1, 0)])`。脚本输出到 `probe:before support c0` 后 30 秒未返回，需中断。
- 另两次 collector probe 分别使用 regular polygon boundary + coplanar support face、单 edge support + `G1` / `C0` order，也在 support face 进入 native helper 后超过 30 秒未返回。
- 主线程复核发现同机 `FreeCADCmd` probe 调用 `Part.makeFilledFace(edges, surface=Part.makePlane(...))` 在 `before surface` 后以 `139` 退出，说明 S1 native helper oracle 在 `surface` / `supports` / `orders` kwargs 入口仍不稳定，不能据此生成 checked-in expected。
- 历史复现脚本 C5-M8 Filling native helper probe 已移除；保留结论为 `surface` / `supports` / `orders` kwargs 入口不稳定，不能据此生成 checked-in expected。

当前边界：

- 不生成 `cad-core/fixtures/c5m8/*surface*` / `*support-order*` geometry expected；checked-in `expected/*.freecad.json` 只允许写 known_gap / diagnostic-backed 记录。
- `Surface` / `Supports` / `Orders` 已从 broad `unsupported_property` 收敛为 source-backed DTO / builder path、known_gap expected 与 locatable diagnostics；不冒充 expected-backed parity。
- `C5M8-BLK-101` 已关闭为 cad-core-only source-backed known_gap；后续 native helper expected、G2 geometry、non-default params、non-boundary constraints 和 compound/wrapper 边界继续由 S2-S5 接管。

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
