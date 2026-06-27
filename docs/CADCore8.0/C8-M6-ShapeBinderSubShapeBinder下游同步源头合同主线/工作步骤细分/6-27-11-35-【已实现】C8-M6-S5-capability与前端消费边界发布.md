# 【已实现】C8-M6 S5 capability 与前端消费边界发布

## 目标

把 S0-S4 复核过的合同发布成下游可消费的 source package：capability payload、diagnostics vocabulary、fixture seeds、request-local 输出和前端消费边界。

## 发布内容

- `part_design.shape_binder`：保持 C8-M1 expected-backed request-local supported。
- `part_design.sub_shape_binder`：保持 C8-M1 expected-backed request-local supported，保留 `copy_on_change_full_temporary_document_cache` known gap。
- diagnostics：发布 `copy_on_change_full_temporary_document_cache_not_supported`、`cycle_rejected_by_property_link`、generic `cycle_dependency` 的适用边界。
- fixture seeds：发布 C8-M1 12 个 input fixture 和 12 个 C8-M5 后 expected。
- output：发布 mesh / subshapes / full subname、ElementMap / NamedShape evidence、maker history、`documentObjectUpdates`、`elementReferenceUpdates` 的 request-local 合同。

## 前端消费边界

前端可以消费：

- capability status / covered / remaining_gaps / known_gaps / diagnostics。
- request-local mesh、subshape、full subname 和 reference update evidence。
- `documentObjectUpdates` 和 `elementReferenceUpdates` 建议。

前端不应消费为持久状态：

- full BREP。
- TopoDS_Shape。
- NamedShape / ElementMap 原始内核对象。
- temporary-document cache。
- request 结束后的 shape cache。

## 必须回写的矩阵行

- `C8M6-SYNC-101` 到 `C8M6-SYNC-105`
- `C8M6-SCOPE-301` 到 `C8M6-SCOPE-305`
- `C8M6-BLOCKER-501`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
./cad-core capabilities > /tmp/c8m6-s5-capabilities.json
python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics tests.test_adapters
```

## S5 完成记录

- 执行基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=75e7526703`，`git log -1 --oneline=75e7526703 docs: 完成 C8-M6 S4 合同复审`，开始工作区干净。
- 队列基线：S5 执行前 `step_goal_queue.py` 显示当前 pending 为 S5、S6；S5 重命名后队列应跳到 S6。
- capability 发布：`part_design.shape_binder.status=supported_c8m1_expected_backed_request_local`，`remaining_gaps=[]`；`part_design.sub_shape_binder.status=supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap`，唯一 `remaining_gaps=["copy_on_change_full_temporary_document_cache"]`，对应 `known_gaps.copy_on_change_full_temporary_document_cache.diagnostic=copy_on_change_full_temporary_document_cache_not_supported`。
- diagnostics 发布：`copy_on_change_full_temporary_document_cache_not_supported` 只表达 CopyOnChange full temporary-document cache known gap；`cycle_rejected_by_property_link` 只表达 SubShapeBinder Support self-link setter-level rejection；generic graph cycle 继续由 `cycle_dependency` 表达，不在 adapter 或前端渲染层改写。
- fixture seeds 发布：`cad-core/fixtures/c8m1` 下 12 个 input fixture 与 `cad-core/fixtures/c8m1/expected` 下 12 个 C8-M5-current expected 作为下游黑盒合同；本步不刷新 expected。
- output 发布：mesh / subshapes / full subname、ElementMap / NamedShape evidence、maker history、reference update evidence、`documentObjectUpdates` 和 `elementReferenceUpdates` 只作为单次 recompute 的 request-local 输出或建议。
- 前端可消费：capability status / covered / remaining_gaps / known_gaps / diagnostics；request-local mesh、subshape、full subname、reference update evidence；`documentObjectUpdates` 与 `elementReferenceUpdates` 建议。
- 前端不应持久消费：full BREP、TopoDS_Shape、NamedShape / ElementMap 原始内核对象、temporary-document cache、request 结束后的 shape cache；`ReferenceShadow.brep` 仍只允许作为单个旧 subshape snapshot 的引用恢复证据。
- `C8M6-SYNC-101..105`、`C8M6-SCOPE-301..305` 与 `C8M6-BLOCKER-501` 已回写为 S5 发布结论；未修改 `cad-core/src`、fixtures、tests、expected、下游 adapter 或前端 UI，未发现 `unexpected_mismatch`。
- 验证：`./cad-core capabilities > /tmp/c8m6-s5-capabilities.json` 通过；`python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics tests.test_adapters` 通过，47 tests OK；C8-M6 TSV 字段数检查通过；C8-M6 包和 `docs/CADCore8.0/README.md` 尾随空白扫描无匹配，`rg` exit 1 按规则视为通过；`git diff --check` 通过；S5 重命名后队列下一项为 S6。

## 非目标

- 不写前端 UI 代码。
- 不修改下游 adapter。
- 不增加 BREP 或 session state。
