# C6-M1 PartDesign Pipe Product Extension TransformationLawAndTangentExpansion 主线总入口

本文是 `docs/CADCore6.0` 下的 C6-M1 实施主线。当前 S0 已完成 live 基线复核；S1-S6 仍待执行。矩阵中的 product extension 口径已经冻结，但不是发布闸门结论。

## 主线目标

- 把 C51X 保留的 `partdesign_pipe_transformation_laws_source_commented` 与 `partdesign_pipe_spine_tangent_source_commented` 从 exact blocker 转成明确的 CAD Core product extension 工作包。
- 定义并实现 request-local Pipe law DTO、continuous-edge expansion ledger、PipeShell `SetLaw` / `SetMode` 集成、front/back sewing history 回归和 capability product-contract。
- 保持无状态 CAD Core 边界：DocumentObject graph 是输入真相；Shape、NamedShape、ElementMap、mesh 都只在单次请求内产生。

## 当前基线

- S0 live 基线（2026-06-23）：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=b5768acf40`，`git log -1 --oneline` 为 `b5768acf40 docs: 补充 C6-M1 Pipe 扩展方案包`，S0 开始前工作区干净。
- C51-S4 已支持 `Mode=Fixed/Auxiliary/Binormal`、`Transition=Round corner`、selected spine / multisection path、front/back MapperSewing。
- C51X 已确认 `Transformation=Linear/S-shape/Interpolation` 与 `SpineTangent/AuxiliarySpineTangent` 是 FreeCAD source-commented exact blocker，而不是可声明的 FreeCAD parity。
- `cad-core/src/part_design/feature_pipe.cpp` 当前在 `rejectSourceBlockedPipeBranches()` 中对这些字段返回 source-backed `unsupported_property`。
- `cad-core/src/runtime/capability_contract.cpp` 当前 `part_design.pipe` 仍是 `supported_c51s4_pipe_advanced_with_exact_source_blockers`，exact blockers 仍列出 `partdesign_pipe_transformation_laws_source_commented` 与 `partdesign_pipe_spine_tangent_source_commented`。
- 本主线如果实施，必须改成产品扩展口径，并让 capability 显示 product extension，不得删除 FreeCAD blocker 证据后伪装成 upstream parity。

## 证明链条

```text
声明口径
  -> FreeCAD 源码候选
  -> scope review / nonGoal / blocker queue
  -> Transformation law DTO 专项
  -> continuous-edge ledger 专项
  -> PipeShell history / capability 专项
  -> C++ / fixtures / focused tests
  -> 发布闸门
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| Pipe 属性与 enum | `~/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::Pipe()` | 暴露 `SpineTangent`、`AuxiliarySpineTangent`、`Transformation`；`TransformEnums` 包含 `Constant`、`Multisection`、`Linear`、`S-shape`、`Interpolation`。 |
| scaling law blocker | `~/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::execute()` | `Law_Linear` / `Law_S` 代码块存在但被注释；`Interpolation` 只有 enum，没有可执行 law 分支。 |
| tangent blocker | `~/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp::buildPipePath()` | `if (SpineTangent.getValue()) getContinuousEdges(shape, subedge);` 被注释，`getContinuousEdges()` 主体也整体注释。 |
| PipeShell setup | `~/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp::setupAlgorithm()` | 已有 transition / mode / auxiliary spine setup；product extension 只能接入同一 request-local PipeShell 构建过程。 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| PartDesign executor | `cad-core/src/part_design/feature_pipe.cpp` | 解析 Pipe request、拒绝 source-commented blocker、后续接入 law DTO / tangent ledger。 |
| Part geometry helper | `cad-core/include/cad_core/part/topo_shape_expansion.h`、`cad-core/src/part/topo_shape_expansion.cpp` | 承接 PipeShell law / spine expansion 的可复用低层 API，不把 OCCT 细节塞进 adapter。 |
| topology/history | `cad-core/src/part/topo_shape.cpp`、`cad-core/src/topo` | 保持 PipeShell maker history、MapperSewing front/back history 与 ElementMap 传播。 |
| capability/test | `cad-core/src/runtime/capability_contract.cpp`、`cad-core/tests/test_p7_features.py`、`cad-core/tests/test_adapters.py` | 发布 product extension contract，保护 diagnostics、fixtures 和禁止声明。 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-23-19-43-【已实现】C6-M1工作步骤总入口.md` | S0-S6 执行索引。 |
| S0 | `工作步骤细分/6-23-19-44-【已实现】C6-M1-S0-声明口径与live基线复核.md` | 已冻结产品扩展口径与 live blocker。 |
| S1 | `工作步骤细分/6-23-19-45-C6-M1-S1-FreeCAD源码候选矩阵.md` | 建 FreeCAD source authority 和候选矩阵。 |
| S2 | `工作步骤细分/6-23-19-46-C6-M1-S2-范围准入与blocker矩阵.md` | 路由 scope、blocker、non-goal 与 backend gap。 |
| S3 | `工作步骤细分/6-23-19-47-C6-M1-S3-TransformationLawDTO专项复审.md` | 定义 Linear / S-shape / Interpolation law DTO 与实施边界。 |
| S4 | `工作步骤细分/6-23-19-48-C6-M1-S4-ContinuousEdgeLedger专项复审.md` | 定义 SpineTangent / AuxiliarySpineTangent 连续边账本。 |
| S5 | `工作步骤细分/6-23-19-49-C6-M1-S5-PipeShellHistoryCapability专项复审.md` | 保护 PipeShell history、front/back sewing 和 capability 文案。 |
| S6 | `工作步骤细分/6-23-19-50-C6-M1-S6-Oracle实现与发布闸门.md` | 代码落点、fixture、focused tests 和发布闸门。 |
| source candidates | `矩阵/c6m1_pipe_product_extension_source_candidates.tsv` | FreeCAD / cad-core 源码候选。 |
| scope review | `矩阵/c6m1_pipe_product_extension_scope_review_matrix.tsv` | 语义范围与当前状态。 |
| blocker queue | `矩阵/c6m1_pipe_product_extension_blocker_queue.tsv` | 待执行 blocker 和关闭条件。 |
| non-goal registry | `矩阵/c6m1_pipe_product_extension_non_goal_registry.tsv` | 禁止声明与 reopen 条件。 |
| backend gap classification | `矩阵/c6m1_pipe_product_extension_backend_gap_classification.tsv` | 产品扩展 / backendGap / releaseGate 分类。 |
| oracle fixture matrix | `矩阵/c6m1_pipe_product_extension_oracle_fixture_matrix.tsv` | 产品扩展 fixture / test oracle 计划。 |
| input contract matrix | `矩阵/c6m1_pipe_product_extension_input_contract_matrix.tsv` | request DTO 和 response metadata 合同。 |
| validation matrix | `矩阵/c6m1_pipe_product_extension_validation_matrix.tsv` | 短跑、focused 和重型验收命令。 |

## 当前状态

S0 已完成并改名为 `【已实现】`；S1-S6 仍待执行。S0 只冻结产品扩展口径和 live exact blocker 证据，不表示 law / tangent 已支持。任何后续步骤文件都不得在验收通过前改名为 `【已实现】`。
