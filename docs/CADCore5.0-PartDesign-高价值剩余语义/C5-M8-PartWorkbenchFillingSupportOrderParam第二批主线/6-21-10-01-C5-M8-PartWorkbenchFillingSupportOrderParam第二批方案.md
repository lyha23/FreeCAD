# C5-M8 Part Workbench Filling Support / Order / Param 第二批方案

## 当前基线

`part_workbench.filling` 第一批已经发布 source-backed `Part.makeFilledFace(...)` helper：它覆盖 `Boundary=App::PropertyLinkSubList`、closed wire default、connected boundary edges default、`BRepOffsetAPI_MakeFilling` 默认参数、`maker_history:filling`、source evidence、invalid diagnostics 和 `c3m4` expected-backed fixtures。

当前缺口不是一个孤立 case。FreeCAD 把 `surface`、`supports`、`orders`、非默认构造参数和非边界约束全部放在同一个 helper 参数结构和同一个 `TopoShape::makeElementFilledFace()` builder 路径内。若只做其中一个 fixture，会继续留下 DTO、source map、history 和 diagnostics 的不完整边界。

2026-06-21 范围纠偏：C5-M8 只交付 `cad-core`，FreeCAD `src/` 只作为语义依据读取。若安装版 `FreeCADCmd` 的 `Part.makeFilledFace(...)` helper kwargs oracle 不能稳定返回，只能把对应 fixture 记录为 source-backed known_gap 或 diagnostic-backed，不能修改 FreeCAD 上游源码来恢复 oracle，也不能伪造 expected。

## FreeCAD 调用链

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()` 解析 `shapes`、`surface`、`supports`、`orders`、`degree`、`ptsOnCurve`、`numIter`、`anisotropy`、`tol2d`、`tol3d`、`tolG1`、`tolG2`、`maxDegree`、`maxSegments`、`op`，再调用 `TopoShape(...).makeElementFilledFace(shapes, params, op)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()` 构造 `BRepOffsetAPI_MakeFilling`，在 `params.surface` 为 face 时调用 `LoadInitSurface`，展开 compound，查找或构造 boundary wire，`getOrder()` / `getSupport()` 读取 `params.orders` / `params.supports`，对 boundary edge 调用 `maker.Add(edge, support, order, IsBound=true)`。
- 同一函数把剩余 wire/edge 当作 non-boundary constraints 调 `IsBound=false`，把 face 调 `maker.Add(face, order)`，把 vertex 调 `maker.Add(point)`，最后 `Build()` 并通过 `makeElementShape(maker, _shapes, op)` 消费 maker history。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp` 是直接 wrapper：constructor 暴露同一组 OCCT 参数，`loadInitSurface()` 与多个 `add()` overload 暴露 point、support face、edge、edge+support、UV point-on-support、`build()`、`shape()`。它共享 OCCT builder，但 Python wrapper lifecycle 不等于 `Part.makeFilledFace` source-backed request DTO。

## cad-core 落点

- `cad-core/src/part/part_filling.cpp`：从当前默认-only executor 扩展为第二批 DTO 解析 owner；删除或收敛 `Surface` / `Supports` / `Orders` / non-default params 的 broad diagnostic；保留 locatable invalid diagnostics。
- `cad-core/include/cad_core/part/topo_shape_expansion.h`：以 `FilledFaceParams` 表达 constructor params，并继续承载 initial surface、support/order map、boundary/non-boundary source、point/face constraints 的稳定 core API。
- `cad-core/src/part/topo_shape_expansion.cpp`：在 `makeElementFilledFaceFromSources()` 中补 `LoadInitSurface`、`Add(edge, support, order, IsBound=true/false)`、`Add(face, order)`、`Add(point)`，并继续用 maker history 生成 `NamedShape` / boundary evidence。
- `cad-core/tools/collect_freecad_expected.py` 与 `cad-core/fixtures/c5m8`：批量采集 expected，不能用 cad-core 输出倒推 fixture。
- `cad-core/tests/test_p8_features.py`、`tests/test_expected_fixtures.py`、`tests/test_adapters.py`：覆盖 helper metadata、diagnostic target/subname、capability metadata 和 expected parity。
- `cad-core/src/adapters/c_api/c_api.cpp`：只暴露 schema/capability/diagnostic 字段，不写 Filling 业务语义。

## 代表 fixtures

| 分组 | 目标 fixture | 验收重点 |
| --- | --- | --- |
| live guard | `c3m4/part-filling-closed-wire-default`、`c3m4/part-filling-boundary-edges-default`、`c3m4/part-filling-invalid-inputs`、`c4m1/part-filling-advanced-deferred` | 默认路径和 deferred guard 不回退 |
| surface / support / order | `c5m8/part-filling-initial-surface-boundary`、`c5m8/part-filling-support-order-edge-face`、`c5m8/part-filling-invalid-support-order` | `LoadInitSurface`、support face、G1 order source-backed fixture、C0/G1/G2 parser、target/subname diagnostics；native helper expected 与 G2 stable geometry 保持 known_gap |
| non-default params | `c5m8/part-filling-non-default-params`、`c5m8/part-filling-param-diagnostics` | params evidence、constructor field parity、invalid ranges；explicit params native helper geometry expected 目前为 known_gap |
| non-boundary constraints | `c5m8/part-filling-non-boundary-edge-support`、`c5m8/part-filling-non-boundary-face-point`、`c5m8/part-filling-non-boundary-wire`、`c5m8/part-filling-non-boundary-diagnostics` | `IsBound=false`、face support、vertex point constraint、source evidence、locatable diagnostics |
| compound / wrapper | `c5m8/part-filling-compound-optional-boundary`、`c5m8/part-filling-wrapper-boundary`、`c5m8/part-filling-wrapper-uv-point-boundary` | compound expansion with source mapping；direct wrapper lifecycle diagnostic or proven request DTO |

## 实施顺序

1. S0：冻结 live baseline，确认 C5-M8 root / local matrices 和当前 first-batch fixture 状态。
2. S1：把 `Surface` / `Supports` / `Orders` 作为同一 DTO 批次处理；已在 `cad-core` 中实现 source-backed DTO / builder path / locatable diagnostics，并以 known_gap 记录 native helper expected 与 G2 geometry 删除条件，不修改 FreeCAD `src/`。
3. S2：已补非默认 Filling 参数，一次覆盖 OCCT constructor 同组字段，保留无效值 diagnostic；当前 explicit params native helper oracle 退出 245，valid fixture 以 source-backed known_gap 记录删除条件。
4. S3：已补非边界约束分支，boundary wire 选择后剩余 edge / wire / face / vertex 不再被丢弃；wire 与 face/point 有 FreeCAD expected，edge support/order 保持 source-backed known_gap。
5. S4：补 compound optional case；对直接 wrapper 做 owner 判定，不能证明 request-local 生命周期就保持 diagnostic。
6. S5：同步 capabilities、root matrices、docs、fixtures/test 列表和 remaining gaps；队列清空后再宣告收口。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线/工作步骤细分 --format markdown
```

实现短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

阶段收口：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 收口标准

- `part_workbench.filling` supported 声明只覆盖 expected-backed 或明确 source-backed 的 `Part.makeFilledFace(...)` helper 分支。
- `surface` / `supports` / `orders` / non-default params / non-boundary constraints 不再是 broad gap；要么有 fixture expected，要么有具体 diagnostic / known_gap 删除条件。
- 直接 wrapper、Surface Workbench feature、native DocumentObject、GUI 和完整 surface family 均有 non-goal 或 future owner，不留“完整 Filling 未实现”这种泛化 blocker。
- 本包 `工作步骤细分` 队列为空，全局 `cadcore5_blocker_queue.tsv` 的 C5-BLK-801 关闭后才能宣告 C5-M8 完成。
