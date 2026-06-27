# C8-M7 ImportShape changed-file / deleted-reference recovery 准入收口方案

## 目标

处理 current capability 中 `topo_history.producer_matrix.import_shape.remaining=["changed_file_deleted_reference_recovery"]` 的最后残留。C8-M7 必须给出三选一结论：

- `already_covered_publication_closure`：current import first slice 已经覆盖 request-local changed-file 重导入，S6 只更新 capability / docs。
- `request_local_backend_gap`：存在 source-backed、可无状态实现的恢复缺口，S6 进入受限代码落点。
- `oracle_blocked_or_non_goal`：deleted-reference 或完整持久缓存类语义不能在无状态 CAD Core 中实现，S6 发布 non-goal / known-gap 边界。

## 范围

纳入本包：

- FreeCAD `ImportBrep`、`ImportStep`、`ImportIges` 对 `FileName`、可读性和 `TopoShape::import*()` 的语义。
- cad-core import executors 对当前文件读入、OCCT reader、`NamedShape`、ElementMap、mapper history 和 diagnostics 的处理。
- `ReferenceShadow`、`StableSubList`、`ShadowSub`、owner-qualified alias 对 import shape 引用恢复的 request-local 边界。
- capability `topo_history.producer_matrix.import_shape` 的 residual 发布。

排除本包：

- 完整 imported ElementMap 的跨请求持久生命周期。
- ShowElement / LinkElement / LinkGroup 持久写回事务。
- cross-document hash / postfix save-restore lifecycle。
- 文件删除后依赖后端上次请求 cache 的恢复。
- GUI、Worker、WASM、Web 或前端同步实现。

## 步骤

| 步骤 | 任务 | 关闭条件 |
| --- | --- | --- |
| S0 | live 基线与 residual 声明冻结 | HEAD、队列、capability residual、C7-M7 inherited non-goal 和 C8-M7 初始矩阵写清。 |
| S1 | FreeCAD source 与 current coverage 复核 | FreeCAD source、cad-core landing、existing tests、capability row 全部进入 source candidate。 |
| S2 | 准入路由与 blocker 矩阵 | changed-file、deleted-file、ReferenceShadow、persistent cache、capability drift 均被分类。 |
| S3 | import 文件生命周期 oracle 复审 | 当前文件重导入和文件不可读诊断边界有 source-backed 裁决。 |
| S4 | ElementMap 与 ReferenceShadow 恢复边界复审 | 只保留 request-local 恢复路径，跨请求 cache 进入 non-goal。 |
| S5 | capability 残留与 non-goal 发布准入 | 决定 remaining 删除、known_gap 保留或 S6 implementation gate。 |
| S6 | 实现或 no-code 发布闸门 | 队列、TSV、whitespace、diff 和必要 focused tests 通过；队列清空。 |

## 代码落点规则

只有 S5 将某行标为 `request_local_backend_gap` 时，S6 才允许代码修改。允许落点：

- `cad-core/src/part/part_import.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/runtime/reference_resolution.cpp`
- `cad-core/src/runtime/element_reference_update.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_p6_topology.py`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_adapters.py`

若 S5 裁为 no-code，S6 只允许更新 docs / matrices / capability publication，不得写 C++。

## 禁止路径

- 不新增跨请求 backend cache。
- 不持久保存 full BREP、TopoDS_Shape、NamedShape 或 ElementMap。
- 不把 `ReferenceShadow.brep` 用作建模输入或完整对象 BREP。
- 不在 adapter 层改字符串来隐藏 residual。
- 不按文件名、fixture 名、几何顺序或输出索引猜测引用恢复。

## 验收

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口主线 docs/CADCore8.0/README.md
git diff --check
```

若 S6 进入代码 gate：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p6_topology tests.test_p8_features tests.test_adapters
```
