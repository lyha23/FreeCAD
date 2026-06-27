# C8-M6 ShapeBinder/SubShapeBinder 下游同步源头合同主线

## 定位

C8-M6 不修改下游仓库，也不新增 `cad-core` 几何能力。它承接 C8-M1、C8-M2 和 C8-M5 的已验证结论，把 `PartDesign::ShapeBinder` / `PartDesign::SubShapeBinder` 的下游同步源头合同重新整理成可执行包。

本包只回答一件事：下游同步时应该以 FreeCAD 仓库里的哪些 TypeId、capability、diagnostic、fixture expected、ElementMap / NamedShape 输出和 request-local 边界为准。

## 当前基线

- 仓库：`/home/user/Chili3DProject/FreeCAD`
- S0 live 基线提交：`9361ddc83a`（`docs: 新增 C8-M6 下游同步源头合同包`）
- S0 开始工作区干净：`git -c core.quotepath=false status --short -uall` 无输出；C8-M1 到 C8-M5 工作步骤队列均为空，C8-M6 队列首项为 S0。
- current capability：`part_design.shape_binder.status=supported_c8m1_expected_backed_request_local` 且 `remaining_gaps=[]`；`part_design.sub_shape_binder.status=supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap` 且唯一 `remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- C8-M5 已恢复 C8-M1 expected fixture 阶段回归：`BodyBaseFeature` stale expected 已刷新，`SubShapeBinder Support` self-link 诊断已落为 `cycle_rejected_by_property_link`；旧 C8-M2 合同不能覆盖这两个最终口径。
- 本包 S0 只冻结文档和 TSV 同步声明；不修改 `cad-core/src`、fixtures、expected 或测试。
- S1 已完成源头合同与能力面复核：`C8M6-SRC-101..403` 均已绑定 FreeCAD source、cad-core landing、C8-M1/C8-M5 fixture 或 focused test 证据；`C8M6-SCOPE-101..104` 标为 `sync_required_source_contract`，CopyOnChange full temporary document cache 继续由 `C8M6-SCOPE-204` 保持 `known_gap_retained`。
- S2 已完成同步范围准入与 non-goal 路由：`C8M6-SCOPE-101..104/201..203/301` 均为 `sync_required_source_contract`，`C8M6-SCOPE-204` 为 `known_gap_retained`，`C8M6-SCOPE-001/302` 为 `release_gate_only`；未发现 `unexpected_mismatch`。
- S2 明确非目标：不审计或修改下游仓库，不把 UI 展示策略写成 FreeCAD 必须实现项，不在本步修改 `cad-core/src`、fixtures、tests 或 expected；request-local ElementMap / NamedShape 可以同步，跨请求持久 ElementMap / NamedShape 不能同步。
- S3 已完成 TypeId 与 DocumentGraph 合同复审：`PartDesign::ShapeBinder`、`PartDesign::SubShapeBinder`、`PartDesign::SubShapeBinderPython` 与 `/cad/capabilities` 可追溯到 `capability_contract.cpp` 和 FreeCAD `ShapeBinder.h/.cpp`；`DocumentObject graph`、property-link 字段、request-local `documentObjectUpdates` / `elementReferenceUpdates`、Body replay no-synthesis 边界都有 fixture 或 focused test 证据；`ReferenceShadow.brep` 例外未扩展为完整 BREP 传输。
- S4 已完成 fixture expected 与 diagnostics 合同复审：12 个 C8-M1 input fixture 与 12 个 C8-M5-current expected 文件发布为下游黑盒合同；`shape-binder-subshape-binder-element-map-namedshape-body-replay` 不要求无 input `Body.BaseFeature` 时出现 `BodyBaseFeature`；`subshape-binder-setlinks-normalization-diagnostics` 使用 setter-level `cycle_rejected_by_property_link`，generic graph cycle 仍保持 `cycle_dependency`；CopyOnChange fixture 只证明 property-state 和 diagnostic 边界，full temporary-document copied-object cache 继续保持 known gap。
- S5 已完成 capability 与前端消费边界发布：`part_design.shape_binder` 保持 C8-M1 expected-backed request-local supported 且 `remaining_gaps=[]`；`part_design.sub_shape_binder` 保持 C8-M1 expected-backed request-local supported，并只保留 `copy_on_change_full_temporary_document_cache` known gap / diagnostic；前端可消费 capability status / covered / remaining_gaps / known_gaps / diagnostics、C8-M1 fixture seeds、request-local mesh / subshapes / full subname / reference update evidence、`documentObjectUpdates` 与 `elementReferenceUpdates` 建议，但不得持久消费 full BREP、TopoDS_Shape、NamedShape / ElementMap 原始内核对象、temporary-document cache 或 request 结束后的 shape cache。

## 与既有 C8 包的关系

- C8-M1 是 ShapeBinder/SubShapeBinder executor、ElementMap / NamedShape 和 fixture expected 的能力闭环。
- C8-M2 是 CopyOnChange DTO 准入与下游同步源头合同首版；它明确不把 full temporary-document cache 标成 supported。
- C8-M5 是 C8-M1 expected drift 的阶段回归恢复；本包的合同必须吸收 C8-M5 的最终口径，不能继续引用旧的 `BodyBaseFeature` stale expected 或 generic `cycle_dependency` 结论。

## S1 源头复核结论

- FreeCAD source authority 已现场复核：`ShapeBinder::updatedShape()` 覆盖 filtered support、datum fallback 和 `TraceSupport` placement；`SubShapeBinder::update()` 覆盖 support subelements、Relative、MakeFace、Offset2D、Fuse、Refine 与 CopyOnChange 分支；`SubShapeBinder::setLinks()` 在 setter 阶段拒绝 self/cyclic Support；`Body::onChanged(BaseFeature)` 只在 `BaseFeature.getValue()` 存在时创建 `PartDesign::FeatureBase`。
- cad-core landing 已现场复核：`feature_shape_binder.cpp` 发布 request-local binder output 与 lifecycle diagnostics，`body.cpp` 不在缺少 `Body.BaseFeature` 时合成 `BodyBaseFeature`，`capability_contract.cpp` 发布 ShapeBinder supported 与 SubShapeBinder known gap，`reference_lifecycle.cpp` 输出 `cycle_rejected_by_property_link`。
- C8-M1/C8-M5 证据已绑定：12 个 `cad-core/fixtures/c8m1` input 与 12 个 C8-M5-current expected 仍是黑盒合同，`tests/test_c8_shapebinder.py` 锁定 Body replay、CopyOnChange known gap 和 self-link diagnostic，`tests/test_expected_fixtures.py` 继续作为 expected fixture gate。
- S1 未发现 source / capability / test 证据矛盾；没有新增 `unexpected_mismatch`，`C8M6-BLOCKER-101` 已关闭并转入 S2。

## S2 范围准入结论

- `sync_required_source_contract`：TypeId / capability、request-local Binder output、fixture seed、C8-M5-current expected、BodyBaseFeature no-synthesis boundary、setter-level `cycle_rejected_by_property_link`、diagnostics vocabulary。
- `known_gap_retained`：`copy_on_change_full_temporary_document_cache` 继续保持 C8-M2 `known_gap` / `oracle_blocked` 和 capability `remaining_gaps`，不进入 supported 或 implementation gate。
- `release_gate_only`：C8-M5 release baseline 与最终 focused / expected / stage gate 只在 S6 证明未漂移。
- `non_goal`：下游仓库审计或修改、UI 展示策略、跨请求持久 full BREP / TopoDS_Shape / NamedShape / ElementMap / temporary cache、本步修改 `cad-core/src` / fixtures / tests / expected 均不在 C8-M6 S2 范围。
- `C8M6-BLOCKER-201` 已关闭；scope/category/non-goal 矩阵不再保留 `seed_pending`、`baseline_frozen` 或非标准分类；S3 已复核 TypeId 与 DocumentGraph 合同。

## S3 TypeId 与 DocumentGraph 结论

- `C8M6-SYNC-101` 已发布：ShapeBinder capability 保持 `supported_c8m1_expected_backed_request_local` 且无 remaining gap；SubShapeBinder capability 保持 request-local supported，并只保留 `copy_on_change_full_temporary_document_cache` known gap。
- `C8M6-SYNC-104` 已发布 request-local DTO 边界：`results`、`elementReferenceUpdates`、`documentObjectUpdates`、`diagnostics`、`binaryPayloads` 是单次 recompute 结果通道；下游不得从该合同推导持久 full BREP / TopoDS_Shape / NamedShape / ElementMap / temporary document cache。
- `C8M6-SCOPE-201..203` 的 fixture / expected / Body replay 证据在 S3 被复核为 DTO 字段证据，精确 fixture 表和 diagnostics vocabulary 仍留给 S4 发布。
- `C8M6-SCOPE-204` 保持 `known_gap_retained`，没有升级为 supported 或 implementation gate。
- `C8M6-BLOCKER-301` 已关闭；S3 未发现 `unexpected_mismatch`，S4 已复核 fixture expected 与 diagnostics 合同。

## S4 fixture expected 与 diagnostics 结论

- `C8M6-SYNC-102` 已发布 diagnostics vocabulary：`copy_on_change_full_temporary_document_cache_not_supported` 是 known-gap diagnostic，`cycle_rejected_by_property_link` 只用于 SubShapeBinder Support self-link setter rejection，generic graph cycle 继续由 `cycle_dependency` 表达，禁止 adapter 字符串改写。
- `C8M6-SYNC-103` 已发布 fixture seeds 和 expected contract：`cad-core/fixtures/c8m1` 下 12 个 input fixture 与 `expected` 下 12 个 C8-M5-current expected 文件作为下游黑盒合同；本包未刷新 expected。
- `C8M6-SYNC-105` 已发布 C8-M5 delta contract：无 input `Body.BaseFeature` 时不合成 `BodyBaseFeature`；SubShapeBinder setter-cycle 诊断为 `cycle_rejected_by_property_link`。
- `C8M6-BLOCKER-401` 已关闭；`python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics` 通过 18 tests，expected fixture gate 通过且保持 35 skipped known-gap fixtures；未发现 `unexpected_mismatch`。

## S5 capability 与前端消费边界结论

- `C8M6-SYNC-101..105` 已统一发布为下游 source package：TypeId / capability payload、diagnostics vocabulary、C8-M1 12 input fixture 与 12 个 C8-M5-current expected、request-local output contract 和 C8-M5 delta contract 均以当前 FreeCAD 仓库事实为准。
- `part_design.shape_binder` 的 capability 保持 `supported_c8m1_expected_backed_request_local`，`covered` 覆盖 whole / subshape / multi-subshape / TraceSupport / datum fallback / ElementMap/NamedShape retag / Body replay，`remaining_gaps=[]`。
- `part_design.sub_shape_binder` 的 capability 保持 `supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap`，`covered` 覆盖 whole / selected support / Relative / MakeFace / Offset2D / Fuse / Refine / profile consumer / ElementMap/NamedShape / Body replay / BindMode request-local subset，`remaining_gaps=["copy_on_change_full_temporary_document_cache"]`，对应 diagnostic 为 `copy_on_change_full_temporary_document_cache_not_supported`。
- diagnostics 发布边界：CopyOnChange full temporary-document cache 是 known-gap diagnostic；SubShapeBinder Support self-link 是 setter-level `cycle_rejected_by_property_link`；generic dependency cycle 仍是 `cycle_dependency`，不得在 adapter 或前端渲染层改写字符串。
- 前端可消费：capability status / covered / remaining_gaps / known_gaps / diagnostics；request-local mesh、subshapes、full subname、ElementMap / NamedShape evidence、maker history、reference update evidence；`documentObjectUpdates` 和 `elementReferenceUpdates` 建议。
- 前端不应持久消费：full BREP、TopoDS_Shape、NamedShape / ElementMap 原始内核对象、temporary-document cache、request 结束后的 shape cache；`ReferenceShadow.brep` 仍只允许作为单个旧 subshape snapshot 的引用恢复证据，不扩展为完整 BREP transport。
- `C8M6-BLOCKER-501` 已关闭；S5 未修改 `cad-core/src`、fixtures、tests、expected、下游 adapter 或前端 UI，未发现 `unexpected_mismatch`。

## 主文件

- 总入口：`6-27-11-28-C8-M6-ShapeBinderSubShapeBinder下游同步源头合同主线总入口.md`
- 方案：`6-27-11-28-C8-M6-ShapeBinderSubShapeBinder下游同步源头合同方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 验收分层

本轮短跑默认验收：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M6-ShapeBinderSubShapeBinder下游同步源头合同主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M6-ShapeBinderSubShapeBinder下游同步源头合同主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M6-ShapeBinderSubShapeBinder下游同步源头合同主线 docs/CADCore8.0/README.md
git diff --check
```

阶段合同复核：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
./cad-core capabilities > /tmp/c8m6-capabilities.json
python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics tests.test_adapters
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results
```

发布闸门：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters tests.test_diagnostics
```
