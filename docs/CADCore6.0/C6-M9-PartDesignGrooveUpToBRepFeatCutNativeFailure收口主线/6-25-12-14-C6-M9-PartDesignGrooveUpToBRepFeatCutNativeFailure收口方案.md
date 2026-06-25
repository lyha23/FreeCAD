# C6-M9 PartDesign Groove UpTo BRepFeat Cut Native Failure 收口方案

## 背景

C6-M8 关闭后，Part Workbench surface family 的 active `remaining_gaps` 已清空并发布为明确的 expected-backed / product-contract-non-parity / historical / non-goal 口径。当前 live capability 中最明确、仍处于 active 状态的 CAD Core 6.0 blocker 是 `part_design.revolution_groove` 下的 `partdesign_groove_upto_brepfeat_cut_native_failure`。

这个 blocker 有清晰的 FreeCAD source 与 checked-in fixture 证据：`PartDesign::Groove` 的 `Type=UpToFirst` / `Type=UpToFace` 走 `FeatureGroove.cpp::Groove::execute()` -> `FeatureRevolved.cpp` UpTo path -> `TopoShapeExpansion.cpp::TopoShape::makeElementRevolution()`，底层依赖 `BRepFeat_MakeRevol`。cad-core 当前也走 `cad-core/src/part/topo_shape_expansion.cpp::makeElementRevolutionUntilFromSources()`，并在 `c51m1/partdesign-groove-uptofirst-body`、`c51m1/partdesign-groove-uptoface-body` 上稳定失败。

## 本轮做什么

- S0：已冻结 live HEAD `17116567e4`、C6-M1..C6-M8 queue empty、C6-M9 起点队列、`part_design.revolution_groove` capability、adapter assertion 和 focused P7 fixture 当前状态；本步未改 C++、fixtures、expected 或测试语义。
- S1：已批量复核 FreeCAD / cad-core source authority、native failure message、fixtures、current diagnostics 和 adapter assertions；确认 `Groove Type=UpToFirst` 与 `Groove Type=UpToFace` 是同一 subtractive UpTo / `BRepFeat_MakeRevol` 语义批次。
- S2：已做准入路由。Groove UpToFirst / UpToFace 同批裁决为 `historical_native_failure`；不进入 `backend_gap_requires_implementation` 或 `cad_core_product_contract_non_parity`。
- S3：已按 S2 route 发布 historical native failure evidence，同步 `capability_contract.cpp`、`test_adapters.py`、C6-M9 矩阵和 docs；`part_design.revolution_groove.remaining_gaps=[]`、`exact_blockers={}`，同一 id 保留在 `narrowed_gaps` / historical native evidence；不改 `feature_revolved.cpp` / `topo_shape_expansion.cpp`，不新增 fixtures/product metadata，不改 expected。
- S4：发布 capability/docs：确保 `remaining_gaps=[]`、historical/narrowed evidence、adapter assertion、C6-M9 矩阵和 root README 一致。
- S5：release gate：运行 build、focused suites、TSV 字段检查、queue empty 和 diff check。

## 最小完整语义批次

C6-M9 的最小批次不是单个 fixture，而是同一 subtractive UpTo 边界下的两个代表场景：

| item | 当前状态 | C6-M9 批量检查 |
| --- | --- | --- |
| `Groove Type=UpToFirst` | c51m1 fixture 当前断言 BRepFeat failure | S2 route=`historical_native_failure`；S3/S4 作为 historical evidence 发布，不改 code、fixture、test 语义。 |
| `Groove Type=UpToFace` | c51m1 fixture 当前断言 BRepFeat failure | S2 route=`historical_native_failure`；与 UpToFirst 同批发布，不能只处理一个 fixture。 |
| `Revolution UpToFirst/UpToLast/UpToFace` | 已 supported | 只作为 regression guard，不重开实现范围。 |
| `Groove Angle/TwoAngles/ThroughAll` | 已 supported | 只作为 regression guard，不扩大本包。 |

## 批量 oracle / fixture 策略

- 首先保留当前 `c51m1` Groove UpTo fixtures 作为 native failure / exact blocker evidence，不直接改 expected。
- 如果 S2 认定 FreeCAD native failure 是正式行为，则 S3/S4 只做 publication/assertion 收口，不把失败伪装成 CAD Core support。
- 如果 S2 认定 CAD Core 应提供 request-local product contract non-parity，则新增 `c6m9` product fixtures，必须明确 `freecad_native_expected=false` 或等价 product metadata，并保留 `c51m1` native failure evidence。
- 如果 S1/S2 证明当前 expected / collector 有误，必须先修 oracle 或 collector；不能从 cad-core 输出倒推 expected。
- S2 裁决前，`c51m1/partdesign-groove-uptofirst-body` 与 `c51m1/partdesign-groove-uptoface-body` 只能作为 native failure / exact blocker evidence；不得把当前 BRepFeat failure 记录成 expected-backed success。

## 代码和文档落点

| 方向 | 文件 |
| --- | --- |
| Groove/Revolution executor | `cad-core/src/part_design/feature_revolved.cpp` |
| BRepFeat helper / maker history | `cad-core/src/part/topo_shape_expansion.cpp` |
| public helper API | `cad-core/include/cad_core/part/topo_shape_expansion.h` |
| capability | `cad-core/src/runtime/capability_contract.cpp` |
| adapter assertion | `cad-core/tests/test_adapters.py` |
| focused tests | `cad-core/tests/test_p7_features.py`、`cad-core/tests/test_expected_fixtures.py` |
| fixtures | `cad-core/fixtures/c51m1`、可能新增 `cad-core/fixtures/c6m9` |
| C6-M9 docs | `docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线`、`docs/CADCore6.0/README.md` |

## FreeCAD 依据

| source | 支撑语义 |
| --- | --- |
| `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureGroove.cpp::Groove::execute()` | `Groove` 调用 `executeRevolved(Part::RevolMode::CutFromBase)`，是 subtractive UpTo 的 owner。 |
| `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolved.cpp::Revolved::tryToRevolveToFace()` | UpTo path 将 base、support face、profile face、axis 和 up-to face 交给 `makeElementRevolution()`。 |
| `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolved.cpp::Revolved::generateRevolution()` | `ToFirst/ToFace/ToLast` 分支委托 `TopoShape::makeElementRevolution()`。 |
| `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRevolution()` | 使用 `BRepFeat_MakeRevol::Init()` 和 `Perform(uptoface)`，失败时报 `Revolution: Up to face: Could not revolve the sketch!`。 |

## 验收分层

- 本轮短跑：queue 输出、source/capability grep、C6-M9 TSV 字段数检查、`git diff --check`。
- 实现短跑：若 S3 改 C++ 或 fixtures，至少跑 `python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers` 或替代后的 C6-M9 focused tests，以及 adapter capability focused test。
- 阶段回归：`cmake --build build`，`python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters`。
- 重型收口：只有修改 maker history、ElementMap/history 主路径、collector expected 或批量 expected 文件时，才补跑 topology / broader expected suites。

## 非目标

- 不声明 FreeCAD parity 或 full PartDesign revolved parity。
- 不重开已经 supported 的 `PartDesign::Revolution` UpTo path。
- 不为了清空 gap 做输出端修剪、bbox 猜测、fixture 名分支或后处理排序。
- 不把 FreeCAD native failure 改写为 expected-backed success。
- 不处理 GUI、TaskPanel、Workbench session、Rust / `opencascade-rs` 同步。

## 结论

推荐继续执行 C6-M9。S2 已同时处理 Groove UpToFirst 和 UpToFace，并明确二者 route 为 `historical_native_failure`。S3 已完成 publication/assertion 第一段：把 `partdesign_groove_upto_brepfeat_cut_native_failure` 从 active `remaining_gaps` 迁出到 historical/narrowed evidence，保留 delete/reopen condition 和 c51m1 failure guard，不声明 FreeCAD parity，也不发布 CAD Core product success。下一步 S4 做发布一致性复核，S5 做 release gate。
