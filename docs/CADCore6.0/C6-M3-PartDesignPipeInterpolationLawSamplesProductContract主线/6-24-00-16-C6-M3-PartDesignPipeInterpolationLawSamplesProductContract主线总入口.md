# C6-M3 PartDesign Pipe Interpolation LawSamples Product Contract 主线总入口

## 主线目标

C6-M3 的目标是把 C6-M1/C6-M2 保留的 Pipe `Transformation=Interpolation` / `LawSamples` 从 `product_contract_required` 诊断边界提升为可执行的 CAD Core product contract。该能力必须继续保持无状态 CAD Core 边界：请求里的 DocumentObject graph 和 `LawSamples` 是唯一输入真相，law、shape、NamedShape、ElementMap 和 mesh 都是单次 recompute 产物。

本包不是 FreeCAD parity。FreeCAD 源码只证明 `Interpolation` 是暴露的 enum，不能证明几何执行语义；C6-M3 必须把 DTO、diagnostics、fixture、capability 和 docs 都写成 CAD Core product extension。

## 当前基线

- live repo：`/Users/li/Chili3DProject/FreeCAD`
- 建包 HEAD：`a930114e89`
- 建包 last commit：`a930114e89 完成 C6-M2 S6 发布闸门`
- C6-M1 队列：空队列，Linear / S-shape law 和 tangent ledger 已发布；Interpolation 仍是 precise remaining gap。
- C6-M2 队列：空队列，expected fixture gate、阶段回归和 heavy 收口均通过。
- 当前 executor：`cad-core/src/part_design/feature_pipe.cpp::resolvePipeLaw()` 对 `Transformation=Interpolation` 返回 `product_contract_required`，message 明确要求重开 `LawSamples` product contract。
- 当前 capability：`cad-core/src/runtime/capability_contract.cpp` 把 `partdesign_pipe_interpolation_law_product_contract_required` 放在 exact blocker / remaining gap。

## FreeCAD / CAD Core 依据

| 语义 | 源码入口 | 当前结论 |
| --- | --- | --- |
| Interpolation enum | `src/Mod/PartDesign/App/FeaturePipe.cpp::TransformEnums` | enum 包含 `Interpolation`，但不是可执行语义。 |
| Linear / S-shape 注释 law | `src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::execute()` | 只有 `Law_Linear` / `Law_S` 注释块，没有 Interpolation 分支。 |
| cad-core diagnostic boundary | `cad-core/src/part_design/feature_pipe.cpp::resolvePipeLaw()` | Interpolation 当前停在 `product_contract_required`。 |
| PipeShell law hook | `cad-core/src/part/topo_shape_expansion.cpp` 与 `topo_shape_expansion.h` | 已有 Linear / S-shape `PipeScalingLaw` 与 PipeShell law 接入点。 |
| capability publication | `cad-core/src/runtime/capability_contract.cpp` | 当前 remaining gap 必须由 C6-M3 关闭或继续精确保留。 |

## 证明链条

```text
live baseline and source authority
  -> LawSamples DTO / diagnostics contract
  -> scope / blocker / fixture matrix
  -> OCCT law kernel and metadata route
  -> feature_pipe executor integration
  -> fixtures / focused tests / capability publication
  -> stage regression release gate
```

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-24-00-17-【已实现】C6-M3工作步骤总入口.md` | S0-S6 执行索引。 |
| S0 | `工作步骤细分/6-24-00-18-【已实现】C6-M3-S0-live基线与源码authority复核.md` | 已复核 Interpolation 边界、capability remaining gap 和源码依据。 |
| S1 | `工作步骤细分/6-24-00-19-【已实现】C6-M3-S1-LawSamplesDTO合同冻结.md` | 已冻结 request / response / diagnostics 合同。 |
| S2 | `工作步骤细分/6-24-00-20-【已实现】C6-M3-S2-scope-blocker-fixture矩阵.md` | 已把 S1 合同拆成 scope、blocker、oracle、non-goal 和 validation 执行矩阵。 |
| S3 | `工作步骤细分/6-24-00-21-C6-M3-S3-InterpolationLawKernel实现.md` | 实现 OCCT law kernel 或明确低层 blocker。 |
| S4 | `工作步骤细分/6-24-00-22-C6-M3-S4-feature_pipe接入与diagnostics.md` | 接入 `feature_pipe.cpp`，替换 Interpolation 诊断边界。 |
| S5 | `工作步骤细分/6-24-00-23-C6-M3-S5-fixtures-tests-capability发布.md` | 增加 fixtures、focused tests、capability 和 adapter assertions。 |
| S6 | `工作步骤细分/6-24-00-24-C6-M3-S6-阶段回归发布闸门.md` | 阶段回归、heavy 条件判断和发布状态。 |
| source candidates | `矩阵/c6m3_pipe_interpolation_law_source_candidates.tsv` | FreeCAD / cad-core source authority。 |
| scope review | `矩阵/c6m3_pipe_interpolation_law_scope_review_matrix.tsv` | 语义范围和状态。 |
| input contract | `矩阵/c6m3_pipe_interpolation_law_input_contract_matrix.tsv` | `LawSamples` request / response / diagnostics 合同。 |
| blocker queue | `矩阵/c6m3_pipe_interpolation_law_blocker_queue.tsv` | S0-S6 blocker 与 close condition。 |
| oracle fixture | `矩阵/c6m3_pipe_interpolation_law_oracle_fixture_matrix.tsv` | fixture / test oracle 计划。 |
| non-goal registry | `矩阵/c6m3_pipe_interpolation_law_non_goal_registry.tsv` | 禁止路径和 reopen condition。 |
| backend gap classification | `矩阵/c6m3_pipe_interpolation_law_backend_gap_classification.tsv` | 产品合同、实现、发布分类。 |
| validation matrix | `矩阵/c6m3_pipe_interpolation_law_validation_matrix.tsv` | 短跑、focused、阶段、heavy 验收命令。 |

## 非目标

- 不把 Interpolation 写成 FreeCAD parity。
- 不把 Interpolation fallback 到 Linear / S-shape。
- 不做 GUI / TaskPanel / Web session law editor。
- 不引入跨请求 law cache。
- 不修改上游 FreeCAD `src/`。
- 不重开 C6-M2 的 expected fixture recovery 事项。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore6.0 cad-core
for f in docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/工作步骤细分 --format markdown
```

Focused：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```
