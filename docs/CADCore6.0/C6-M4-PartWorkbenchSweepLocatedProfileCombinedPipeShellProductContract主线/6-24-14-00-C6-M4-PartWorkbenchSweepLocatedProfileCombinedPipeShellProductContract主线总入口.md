# C6-M4 Part Workbench Sweep LocatedProfile Combined PipeShell Product Contract 主线总入口

## 主线目标

C6-M4 的目标是把 `Part::Sweep` / `BRepOffsetAPI_MakePipeShell` 的 located profile 与 auxiliary + transition + tolerance combined 高级组合，从当前 `FreeCADCmd wrapper build blocker` 状态推进为可执行、可验收、可发布的 CAD Core product contract 主线。

本包不声明 FreeCAD parity。FreeCAD `Part::Sweep::execute()` 只证明标准 `Sections/Spine/Solid/Frenet/Transition/Linearize` 调用链；located profile、per-section `WithContact/WithCorrection`、auxiliary spine、tolerance 等高级项来自 `Part.BRepOffsetAPI_MakePipeShell` request-local wrapper。当前 FreeCADCmd 对 `add(Profile, Location, WithContact, WithCorrection)` 在 `build()` 阶段稳定失败，因此 C6-M4 必须先冻结 blocker 边界，再决定何时以 CAD Core product contract 替代 known_gap。

## 当前 live 基线

- live repo：`/home/user/Chili3DProject/FreeCAD`
- S0 live HEAD：`fab981dc85`
- S0 live last commit：`fab981dc85 docs: 新建 C6-M4 Sweep LocatedProfile 主线方案包`
- S1 live HEAD：`571f1061d1`
- S1 live last commit：`571f1061d1 docs: 完成 C6-M4 S0 live 基线复核`
- S2 live HEAD：`6596fe5ed8`
- S2 live last commit：`6596fe5ed8 docs: 完成 C6-M4 S1 source oracle 矩阵冻结`
- capability：`cad-core/src/runtime/capability_contract.cpp` 中 `part_workbench.sweep.status` 为 `supported_multi_profile_linearize_c5m13_wrapper_expected_backed_with_location_overload_blockers`。
- remaining gaps：`part_sweep_located_profile_freecadcmd_wrapper_build_blocker` 与 `part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker` 仍在 `part_workbench.sweep.remaining_gaps`。
- S0 live 验证：blocker grep 仍命中 capability、expected 与 focused guard；`python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest.test_c5m10_part_sweep_located_profile_contract_keeps_freecadcmd_blocker cad-core.tests.test_p8_features.CadCoreP8FeatureTest.test_c5m10_part_sweep_combined_advanced_contract_and_diagnostic_priority` 通过，`Ran 2 tests`，`OK`。
- S1 source/oracle 冻结：`source_candidates.tsv` 已冻结 `C6M4-SRC-001..012`；oracle/input/non-goal 矩阵保持 non-parity product contract、notCollected/backendGap 和 remaining gap 边界。
- S2 oracle 复采集：本机 `FreeCADCmd --version` 为 `FreeCAD 1.2.0 Revision: 20260519 (Git shallow)`；复跑 checked-in C5-M13 probe 后 located 与 combined Location overload 仍在 `build` 阶段报 `OCCError: NCollection_Array1::Value`，plain/no-location controls 仍可 build，因此 `C6M4-SCOPE-101` 保留 `notCollected_retained_S2`，`C6M4-SCOPE-102` 进入 S3 的 non-parity product-contract backendGap 路线。
- S3 live HEAD：`6ede975075`
- S3 live last commit：`6ede975075 docs: 完成 C6-M4 S2 located profile 合同冻结`
- S3 implementation：`C6M4-SCOPE-102` 已 `closed_S3`。`cad-core` 新增显式 `SectionOptions[].ProfilePlacement=AnchorLocationToSpineStart` product-contract selector；valid located profile 输出 `contract=cad_core_product_contract`、`freecadcmd_location_overload_status=notCollected`、shape、metadata 与 NamedShape PipeShell history；invalid Location 与 bool fixtures 均先报 diagnostics。c5m10 known_gap 与 capability remaining gaps 保持不变。

## blocker 边界结论

| blocker | 当前真实边界 | C6-M4 处理原则 |
| --- | --- | --- |
| `part_sweep_located_profile_freecadcmd_wrapper_build_blocker` | `Part.BRepOffsetAPI_MakePipeShell.add(Profile, Location, WithContact, WithCorrection)` 可通过 `isReady=true/status_before_build=0`，但在 `builder.build()` 报 `OCCError: NCollection_Array1::Value`；free/profile/spine/open-wire location representatives 与 call-order variants 均失败，no-location wire profile control 可 build。 | S0-S2 先复核并冻结；S3 只能在明确 product contract / oracle 后落 `profile placement` 与 `Location` overload 替代路径，不能从 cad-core 当前输出倒推 FreeCAD expected。 |
| `part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker` | combined auxiliary + located section + tolerance 的失败依赖 `Location` overload；combined no-location control 可 build，auxiliary / tolerance 单独路径已有 expected-backed case。 | S4 只在 S3 located profile product path 明确后处理 combined；不得把 auxiliary/tolerance 单独能力重新标为 blocker。 |

## FreeCAD / CAD Core 依据

| 语义 | 源码入口 | 当前结论 |
| --- | --- | --- |
| native `Part::Sweep` | `src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` | 读取 `Sections`、`Spine`、`Solid`、`Frenet`、`Transition`、`Linearize`，调用 `makeElementPipeShell()`；不承接高级 wrapper 属性。 |
| PipeShell native history | `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementPipeShell()` | 使用 `BRepOffsetAPI_MakePipeShell`、`Add(profile)`、`Build()`、可选 `MakeSolid()`，再消费 maker history。 |
| Python wrapper overload | `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::add()` | 暴露 `add(Profile, WithContact, WithCorrection)` 与 `add(Profile, Location, WithContact, WithCorrection)` 两个 overload。 |
| auxiliary / tolerance wrapper | `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::setAuxiliarySpine()`、`setTolerance()` | combined case 只是在同一个 request-local builder 上叠加 auxiliary/tolerance 后再进入 located add。 |
| cad-core executor | `cad-core/src/part/part_sweep.cpp` | 解析 `SectionOptions[].Location/WithContact/WithCorrection/ProfilePlacement`、`AuxiliarySpine`、`Tolerance` 并写 advanced metadata；旧 c5m10 Location overload build failure 仍转 `known_gap`，显式 C6-M4 product selector 输出非 parity product metadata。 |
| low-level cad-core PipeShell | `cad-core/src/part/topo_shape_expansion.cpp::makeElementPipeShellFromSources()` | `PipeShellSectionOption` 增加 product profile placement 模式；S3 通过 request-local anchor Location vertex 到 spine start 后调用常规 `Add(profile, contact, correction)`，并保留 `part_sweep:pipeshell_history`。默认仍使用 OCCT `Add(profile, vertex, ...)`。 |
| capability contract | `cad-core/src/runtime/capability_contract.cpp` | `part_workbench.sweep` 已发布 auxiliary/binormal/support/tolerance first batch，并保留两个 Location overload blocker。 |

## 产物索引

| 类型 | 路径 | 状态 | 用途 |
| --- | --- | --- | --- |
| 方案 | `6-24-14-00-C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract方案.md` | 已创建 | 框架级主线方案。 |
| README | `README.md` | 已创建 | 人工入口和队列命令。 |
| 工作步骤总入口 | `工作步骤细分/6-24-14-01-【已实现】C6-M4工作步骤总入口.md` | 已实现 | S0-S6 队列索引，脚本应跳过。 |
| S0 | `工作步骤细分/6-24-14-02-【已实现】C6-M4-S0-live基线与blocker边界复核.md` | 已实现 | 已复核 live baseline、capability、known_gap 与 focused tests。 |
| S1 | `工作步骤细分/6-24-14-03-【已实现】C6-M4-S1-FreeCAD源码与wrapper-oracle候选矩阵.md` | 已实现 | 已冻结 FreeCAD wrapper source authority、cad-core DTO/builder 落点与 oracle/input/non-goal 候选。 |
| S2 | `工作步骤细分/6-24-14-04-【已实现】C6-M4-S2-LocatedProfile合同与oracle复采集.md` | 已实现 | 已复跑 FreeCADCmd wrapper probe，冻结 located profile input/output/diagnostic/product contract 与 retained known_gap。 |
| S3 | `工作步骤细分/6-24-14-05-【已实现】C6-M4-S3-ProfilePlacement与LocationOverload实现.md` | 已实现 | 已实现 located profile profile-placement / Location overload product path，新增 c6m4 product 与 diagnostics fixtures/tests。 |
| S4 | `工作步骤细分/6-24-14-06-C6-M4-S4-AdvancedCombinedAuxiliaryTransitionTolerance实现.md` | 待执行 | 实现 combined auxiliary + transition + tolerance + located section。 |
| S5 | `工作步骤细分/6-24-14-07-C6-M4-S5-fixtures-tests-capability-docs发布.md` | 待执行 | fixtures、expected、focused tests、capability/docs 发布。 |
| S6 | `工作步骤细分/6-24-14-08-C6-M4-S6-阶段回归与release-gate.md` | 待执行 | 阶段回归、heavy 条件与 release gate。 |

## 矩阵索引

| 矩阵 | 用途 |
| --- | --- |
| `矩阵/c6m4_sweep_located_profile_combined_source_candidates.tsv` | FreeCAD/cad-core/source authority。 |
| `矩阵/c6m4_sweep_located_profile_combined_scope_review_matrix.tsv` | scope、状态、落点、下一步；S3 已关闭 `C6M4-SCOPE-102`。 |
| `矩阵/c6m4_sweep_located_profile_combined_input_contract_matrix.tsv` | request/response/diagnostics/product contract。 |
| `矩阵/c6m4_sweep_located_profile_combined_blocker_queue.tsv` | blocker、证据、落点、关闭条件。 |
| `矩阵/c6m4_sweep_located_profile_combined_oracle_fixture_matrix.tsv` | current known_gap 与 C6-M4 fixture/oracle 路线。 |
| `矩阵/c6m4_sweep_located_profile_combined_backend_gap_classification.tsv` | notCollected/backendGap/releaseGate 分类。 |
| `矩阵/c6m4_sweep_located_profile_combined_non_goal_registry.tsv` | 非目标与 reopen condition。 |
| `矩阵/c6m4_sweep_located_profile_combined_validation_matrix.tsv` | 短跑、focused、阶段、heavy 验收命令。 |

## 非目标

- 不把 Filling、Loft、Groove 混进本主线；只作为后续候选保留在 non-goal registry。
- 不声明 FreeCAD parity 或 native `Part::Sweep` advanced direct property support。
- 不实现 GUI/TaskPanel/Web session 行为。
- 不引入 persistent Python wrapper lifecycle 或跨请求 PipeShell session。
- 不从 cad-core 输出倒推 FreeCAD expected。
- 不用输出顺序、bbox、fixture 名称特判绕过 PipeShell history / maker history。

## 验收分层

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore6.0
for f in docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/工作步骤细分 --format markdown
```

Focused：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest.test_c5m10_part_sweep_located_profile_contract_keeps_freecadcmd_blocker cad-core.tests.test_p8_features.CadCoreP8FeatureTest.test_c5m10_part_sweep_combined_advanced_contract_and_diagnostic_priority
```

S3 focused：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c6m4_part_sweep_located_profile_product_contract_builds_shape tests.test_p8_features.CadCoreP8FeatureTest.test_c6m4_part_sweep_located_profile_location_diagnostics_are_locatable tests.test_p8_features.CadCoreP8FeatureTest.test_c6m4_part_sweep_located_profile_bool_diagnostics_are_locatable
```

阶段回归：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

重型收口：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p6_topology tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```
