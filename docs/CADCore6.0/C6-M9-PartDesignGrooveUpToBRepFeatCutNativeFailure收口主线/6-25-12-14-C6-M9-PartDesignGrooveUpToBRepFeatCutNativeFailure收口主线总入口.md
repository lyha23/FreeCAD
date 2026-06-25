# C6-M9 PartDesign Groove UpTo BRepFeat Cut Native Failure 收口主线总入口

本文是 `docs/CADCore6.0` 下 C6-M9 实施主线。C6-M9 不重开 C6-M8 的 Part Workbench surface family，也不扩大成 full PartDesign parity；它只处理 live capability 中仍作为 active exact blocker 发布的 `part_design.revolution_groove` Groove UpTo BRepFeat failure。

## 目标

- 冻结 `partdesign_groove_upto_brepfeat_cut_native_failure` 的 live capability、fixtures、tests 和 FreeCAD source authority。
- 复核 FreeCAD `FeatureGroove.cpp`、`FeatureRevolved.cpp`、`TopoShapeExpansion.cpp::makeElementRevolution()` 的调用链。
- 裁决 Groove UpToFirst / UpToFace 是继续保留 native failure exact blocker，还是进入 CAD Core request-local product contract non-parity 实现。
- 若进入实现，按同一 subtractive UpTo DTO/API 边界批量补 code、fixtures/product metadata、focused tests、capability/docs 和 release gate。
- 若不进入实现，发布为 historical/native failure evidence，并写清 delete/reopen condition，避免 active gap 长期悬空。

## live 起点

- S0 已冻结 `pwd=/Users/li/Chili3DProject/FreeCAD`。
- S0 已冻结 `HEAD=17116567e4`；`git log -1 --oneline` 为 `17116567e4 文档：完成 C6-M8 S5 发布闸门`。
- S0 执行起点 `git -c core.quotepath=false status --short -uall` 只包含根 `docs/CADCore6.0/README.md` 修改和本 C6-M9 包新增文件；未发现 C6-M9 / 根 README 范围外 dirty 文件。
- C6-M1 到 C6-M8 的 `工作步骤细分` 队列均为空；C6-M9 S0 执行前队列从 S0 到 S5 全部 pending。
- live capability 中 `part_design.revolution_groove.status=supported_c51s1_advanced_with_exact_groove_upto_blocker`，`remaining_gaps=["partdesign_groove_upto_brepfeat_cut_native_failure"]`，同一 blocker 位于 `exact_blockers`，fixtures 为 `c51m1/partdesign-groove-uptofirst-body` 和 `c51m1/partdesign-groove-uptoface-body`。
- adapter assertion 当前保持同一 exact blocker；focused P7 fixture 当前断言 `BRepFeat_MakeRevol could not revolve profile up to face` / `Could not revolve the sketch`，`Groove` 为 `error`，`Body` 为 `skipped`。
- S1 已在 `HEAD=bb03433646` 上复核 source authority、cad-core 落点、current diagnostics 和 adapter assertions；`Groove Type=UpToFirst` 与 `Groove Type=UpToFace` 必须作为同一 subtractive UpTo / `BRepFeat_MakeRevol` 语义批次进入 S2 裁决。
- S2 已在 `HEAD=b850d03f46` 上裁决唯一 route：`Groove Type=UpToFirst` 与 `Groove Type=UpToFace` 均为 `historical_native_failure`。S3/S4 只做 capability/test/docs publication assertion，不改 C++、fixtures 或 expected。

## Source authority

| family | source |
| --- | --- |
| Groove execute | `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureGroove.cpp::Groove::execute()` |
| Revolved UpTo path | `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolved.cpp::Revolved::tryToRevolveToFace()` |
| Revolved generator | `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolved.cpp::Revolved::generateRevolution()` |
| Native BRepFeat path | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRevolution()` |
| CAD Core revolved executor | `cad-core/src/part_design/feature_revolved.cpp` |
| CAD Core BRepFeat helper | `cad-core/src/part/topo_shape_expansion.cpp::makeElementRevolutionUntilFromSources()` |

## Cad-core 落点

| owner | file |
| --- | --- |
| revolved feature executor | `cad-core/src/part_design/feature_revolved.cpp` |
| BRepFeat helper / maker history | `cad-core/src/part/topo_shape_expansion.cpp` |
| public headers if needed | `cad-core/include/cad_core/part/topo_shape_expansion.h` |
| capability | `cad-core/src/runtime/capability_contract.cpp` |
| adapter assertion | `cad-core/tests/test_adapters.py` |
| focused feature tests | `cad-core/tests/test_p7_features.py` |
| fixtures / expected | `cad-core/fixtures/c51m1` and possible `cad-core/fixtures/c6m9` |

## 工作步骤

| step | file | 目标 |
| --- | --- | --- |
| S0 | `工作步骤细分/6-25-12-15-【已实现】C6-M9-S0-live基线与exact-blocker冻结.md` | 【已实现】冻结 live baseline、C6-M1..C6-M8 queue、capability exact blocker 和当前 focused test 失败语义。 |
| S1 | `工作步骤细分/6-25-12-16-【已实现】C6-M9-S1-FreeCAD源码与native失败证据复核.md` | 【已实现】复核 FreeCAD source、native failure evidence、cad-core helper 和 current fixture/test expectation。 |
| S2 | `工作步骤细分/6-25-12-17-【已实现】C6-M9-S2-准入路由与产品合同裁决.md` | 【已实现】裁决 exact blocker route 为 `historical_native_failure`，同批覆盖 Groove UpToFirst 与 UpToFace。 |
| S3 | `工作步骤细分/6-25-12-18-C6-M9-S3-BRepFeat实现或native-failure发布收口.md` | 按 S2 route 发布 historical/native failure evidence，同步 capability/test/docs，不改 C++、fixtures 或 expected。 |
| S4 | `工作步骤细分/6-25-12-19-C6-M9-S4-fixtures-tests-capability-docs发布.md` | 发布 fixtures/tests/capability/docs，保证 adapter assertion 与矩阵一致。 |
| S5 | `工作步骤细分/6-25-12-20-C6-M9-S5-阶段回归与release-gate.md` | 运行 build、focused regression、queue empty、TSV 和 diff checks，关闭 C6-M9。 |

## 矩阵

| matrix | 用途 |
| --- | --- |
| `矩阵/c6m9_groove_upto_brepfeat_source_candidates.tsv` | FreeCAD / cad-core / test source authority。 |
| `矩阵/c6m9_groove_upto_brepfeat_scope_review_matrix.tsv` | scope 准入和状态。 |
| `矩阵/c6m9_groove_upto_brepfeat_backend_gap_classification.tsv` | backend gap / product contract / native failure 分类。 |
| `矩阵/c6m9_groove_upto_brepfeat_blocker_queue.tsv` | blocker、验证和关闭条件。 |
| `矩阵/c6m9_groove_upto_brepfeat_input_contract_matrix.tsv` | Groove UpTo DTO、CapabilityResponse、metadata 字段合同。 |
| `矩阵/c6m9_groove_upto_brepfeat_oracle_fixture_matrix.tsv` | oracle、fixtures、expected/product metadata 批量路线。 |
| `矩阵/c6m9_groove_upto_brepfeat_non_goal_registry.tsv` | 非目标和 reopen condition。 |
| `矩阵/c6m9_groove_upto_brepfeat_validation_matrix.tsv` | 验收命令分层。 |

## 非目标

- 不声明 FreeCAD parity 或 full PartDesign revolved parity。
- 不重开已支持的 `PartDesign::Revolution` UpToFirst / UpToLast / UpToFace。
- 不靠 bbox、输出顺序、fixture 名称或后处理修剪伪造 Groove UpTo 成功。
- 不修改 FreeCAD native expected，除非 S1/S2 证明 collector/oracle 本身错误。
- 不引入 GUI、TaskPanel、Workbench session 或 Rust / `opencascade-rs` 同步实现。

## 当前结论

C6-M9 已完成 S0/S1/S2。当前队列应推进到 S3；`partdesign_groove_upto_brepfeat_cut_native_failure` 的公开 route 已裁决为 `historical_native_failure`。S3/S4 的收口方式是把 active `remaining_gaps` 迁出到 historical/narrowed evidence，同步 `capability_contract.cpp`、`test_adapters.py`、矩阵和 docs；本 route 不执行 C++、fixtures 或 expected 语义实现。S5 用 build、focused tests、queue empty、TSV 和 diff check 作为 release gate。
