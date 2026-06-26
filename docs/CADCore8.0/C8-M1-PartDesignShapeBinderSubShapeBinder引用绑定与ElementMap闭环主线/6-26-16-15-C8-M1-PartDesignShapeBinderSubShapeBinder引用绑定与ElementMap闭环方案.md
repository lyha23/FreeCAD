# C8-M1 PartDesign ShapeBinder / SubShapeBinder 引用绑定与 ElementMap 闭环方案

## 背景

C7-M7 已关闭 P8 Link / imported-shape stable reference 方向的 implementation gate。剩余 imported ElementMap、ShowElement persistent writeback 和 cross-document hash / postfix 生命周期缺 native oracle，不适合继续作为 C++ 实现包推进。

C8-M1 选择 `PartDesign::ShapeBinder` / `PartDesign::SubShapeBinder`，因为它们同时满足三点：

- FreeCAD 源码集中在 `src/Mod/PartDesign/App/ShapeBinder.cpp`，调用链清晰。
- FreeCAD 自带测试已有跨 Body、offset、before / after Pad、ElementMap 等代表场景。
- 当前 `cad-core` registry 未覆盖这两个 TypeId，存在可落地的 executor / fixture / capability 缺口。

## 原则

- 同一轮批量覆盖同一调用链，不做单 fixture first slice。
- S3 先采 FreeCAD native oracle，再由 S4 对 current `cad-core` mismatch 做 implementation gate。
- CopyOnChange、Frozen、Detached、PartialLoad 必须在本包审计；若无法 request-local 化，作为本包内 blocker / non-goal 发布，不另开薄包。
- 输出必须保留 ElementMap / NamedShape / stable subname 语义，不用 bbox、面积、几何类型排序或 fixture 名称猜测替代。

## 最小完整语义批次

| 批次 | 场景 | S0 冻结 route |
| --- | --- | --- |
| ShapeBinder support | whole support、Face / Edge / Vertex、同对象多 subshape compound | `backend_gap_candidate`，S3 expected 后由 S4 裁决 |
| ShapeBinder placement | `TraceSupport=false/true`，source / target Body placement | `oracle_candidate`，S3 先采 transform / bbox 证据 |
| ShapeBinder datum fallback | `App::Line`、`App::Plane`、`App::Point` | `oracle_candidate`，S3 采 native 或记录 blocker |
| SubShapeBinder support | whole object、Face、Edge list、Sketch before / after Pad | `backend_gap_candidate`，S3 expected 后由 S4 裁决 |
| SubShapeBinder geometry ops | `MakeFace`、`Offset`、`Fuse`、`Refine` | `backend_gap_candidate`，不得用输出修剪替代 |
| SubShapeBinder relative route | `Relative`、`Context`、nested support `getSubObject()` | `oracle_candidate` 或 `oracle_blocker` |
| lifecycle | `BindMode`、`BindCopyOnChange`、`PartialLoad` | `oracle_candidate`，只能发布 request-local 子集或 `diagnostic_non_goal` |
| topo | source ElementMap retag、`NamedShape`、Body Tip replay、reference update | `backend_gap_candidate`，必须有 ElementMap evidence |

## S0 live 基线与边界冻结

冻结当前 C7-M7 closed 状态、`cad-core` registry 缺口、ShapeBinder / SubShapeBinder 批量范围、状态词典和禁止声明。

S0 不采 oracle，不改 C++。

S0 结论：`HEAD=29da94dd13`，当前 registry 未覆盖 `PartDesign::ShapeBinder` / `PartDesign::SubShapeBinder` / `PartDesign::SubShapeBinderPython`；`C8M1-BLOCKER-000` 关闭。

## S1 FreeCAD 源码与 current coverage 复核

已复核 `ShapeBinder.cpp`、`ShapeBinder.h`、`Body.cpp`、`Feature.cpp`、上游 PartDesign tests 和 current `cad-core` 落点。S1 确认每个 scope 都有 FreeCAD authority 与 current cad-core coverage 结论：registry 未注册 Binder TypeId，`body.cpp`、`profile_resolver.cpp`、`topo_shape_expansion.cpp`、`property_topo_shape.cpp`、`copy_on_change.cpp`、`reference_resolution.cpp` 可复用但不是 Binder 支持。

S1 不采 oracle，不改 C++。

S1 结论：`C8M1-BLOCKER-101` 已关闭到 source authority；所有 scope 仍保持 `backend_gap_candidate` / `oracle_candidate` / `diagnostic_non_goal`，不提升为 supported 或 `backend_gap_requires_implementation`。S2 已完成 oracle 候选矩阵，下一步进入 S3 native oracle 批量采集。

## S2 oracle 候选矩阵

按同一 DTO / executor 边界形成 oracle 批量清单。候选必须覆盖：

- ShapeBinder whole / subshape / multi-subshape / TraceSupport。
- SubShapeBinder support / MakeFace / Offset / Fuse / Refine。
- ElementMap / NamedShape / Body replay。
- BindMode / CopyOnChange lifecycle。

S2 只能输出 `oracle_candidate`、`backend_gap_candidate`、`diagnostic_non_goal`、`oracle_blocker` 或 `oracle_blocked`，不能直接发布 supported。

S2 结论：`C8M1-ORACLE-101..104`、`201..206`、`301..302` 已全部写入批量 `oracle_candidate`；`C8M1-BG-101..401` 保持 `backend_gap_candidate`；`C8M1-BLOCKER-201` 关闭到矩阵完整性；GUI / session / persistent state / full temporary-document CopyOnChange / downstream Rust / adapter patch / C7-M7 writeback 保持 `diagnostic_non_goal`。

## S3 native oracle 批量采集

新增或扩展 FreeCAD native collector，生成 `cad-core/fixtures/c8m1` input 与 `cad-core/fixtures/c8m1/expected/*.freecad.json`。如果 FreeCAD Python API 无法观察 lifecycle，则把对应 row 写成 `oracle_blocked` 并记录 delete / reopen condition。

S3 不改 runtime C++。

## S4 cad-core 实现

只有 S3 有 source-backed expected 且 current `cad-core` 缺失或 mismatch 时，S4 才落 C++：

- 新增 `cad-core/include/cad_core/part_design/feature_shape_binder.h`。
- 新增 `cad-core/src/part_design/feature_shape_binder.cpp`。
- 修改 `cad-core/src/runtime/feature_registry.cpp` 注册 Binder TypeId。
- 修改 `cad-core/CMakeLists.txt` source list。
- 复用 `part/topo_shape_expansion.cpp`、`part/property_topo_shape.cpp`、`runtime/reference_resolution.cpp`、`app/copy_on_change.cpp`，不得在 adapter 层补业务逻辑。

## S5 fixtures / tests / capability 发布

补齐 fixtures、expected、focused tests、diagnostic tests、capability contract 和 docs。发布口径必须区分：

- expected-backed supported subset。
- request-local product contract。
- oracle-blocked lifecycle。
- diagnostic non-goal。

## S6 release gate

S6 做队列、TSV、focused tests、build 和必要阶段回归。只有 S4/S5 修改 C++ 或 fixtures/tests/capability 时才跑 `cmake --build build` 和 focused unittest；docs-only 变更只跑 docs/matrix/diff 检查。

## 验收分层

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/矩阵/*.tsv
git diff --check
```

实现短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_c8_shapebinder
python3 -m unittest tests.test_diagnostics
```

阶段回归：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_mvp
python3 -m unittest tests.test_p7_features tests.test_p8_features
```

阶段回归只在 release gate、topo/reference/capability 改动或用户明确要求时执行。
