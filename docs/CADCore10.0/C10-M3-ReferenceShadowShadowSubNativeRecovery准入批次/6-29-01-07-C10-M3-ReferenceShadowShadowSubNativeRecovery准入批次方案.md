# C10-M3 ReferenceShadow / ShadowSub Native Recovery 准入批次方案

## 背景

C10-M2 已关闭 PartDesign DressUp / Hole producer history 第二阶段：DressUp / Hole 本身没有 expected-backed current mismatch，旧引用恢复只保留为 `diagnostic_retained`。下一轮不应继续扩展 C10-M2，也不应重开 CopyOnChange retained known gap。当前最值得推进的是 stale `ReferenceShadow` / `ShadowSub` recovery 的 native 可观测性：如果 FreeCAD 原生能在 FCStd / XML restore 后稳定暴露 `ShadowSub` / `ReferenceShadow` recovery lifecycle，并且 current cad-core 偏离，才有资格进入 C++ 实现。

## 实施原则

- 先证明 FreeCAD native 可观察，再比较 cad-core；不能用 cad-core current output 倒推 FreeCAD golden。
- 只处理 request-local old-reference recovery、`elementReferenceUpdates` 和单 subshape `ReferenceShadow.brep` evidence；不引入跨请求 shape / NamedShape / ElementMap cache。
- `backend_gap_candidate` 必须同时有 FreeCAD authority、native observable expected 和 current cad-core mismatch。
- `notCollected` 只触发 oracle / evidence 任务，不直接触发 C++。
- split 一对多、deleted、source evidence 不足继续发布 structured diagnostics，不写可解析 stable subname。

## S0-S6 拆分

| 步骤 | 目标 | 关键输出 |
| --- | --- | --- |
| S0 | 冻结 live baseline 和声明口径 | README、总入口、状态词典、forbidden claims 和 validation matrix 对齐。 |
| S1 | 复核 FreeCAD 源码候选和 current coverage | source candidate TSV 回写为真实源码 / cad-core 路径；不升级 supported 口径。 |
| S2 | 做范围准入与 blocker 路由 | scope / blocker / non-goal / backend-gap TSV 全部有 owner step 和 close condition。 |
| S3 | native 可观测性与 oracle 采集复审 | 判断 FCStd / XML restore 后 `ShadowSub` / `ReferenceShadow` / `getSubValues(false/true)` 是否可稳定观测。 |
| S4 | cad-core 恢复路径与 current mismatch 复审 | 只有 S3 有 native evidence 时，比较 parser / recovery / diagnostics 并打开或拒绝 implementation row。 |
| S5 | 前端协议诊断与 non-goal 边界复审 | 复核 `elementReferenceUpdates`、ReferenceShadow.brep 单 subshape 例外和禁止 shortcut。 |
| S6 | Oracle 实现与发布闸门 | 对已证明 mismatch 的 row 落 C++ / tests；无 mismatch 时发布 no-code retained diagnostic。 |

## 下一轮代码落点规则

S6 只有在 S3-S5 产生 `backend_gap_candidate` 时才改代码。允许的落点包括：

- Link property parser：`cad-core/src/app/property_links.cpp`、`cad-core/include/cad_core/app/property_links.h`。
- ReferenceShadow recovery：`cad-core/src/part/topo_shape_reference.cpp`、`cad-core/include/cad_core/part/topo_shape_reference.h`。
- ElementMap / NamedShape：`cad-core/src/part/topo_shape.cpp`、`cad-core/include/cad_core/part/topo_shape.h`、`cad-core/src/app/element_map.cpp`。
- Runtime updates：`cad-core/src/runtime/element_reference_update.cpp`、`cad-core/include/cad_core/runtime/element_reference_update.h`。
- Capability / tests：`cad-core/src/runtime/capability_contract.cpp`、`cad-core/tests/test_p7_features.py`、`cad-core/tests/test_adapters.py`，必要时新增 `cad-core/fixtures/c10m3` expected。

禁止在 adapter、JSON parser 输出修剪、fixture 名称分支、bbox / 面积 / 长度相似度或输出排序中补业务语义。

## 验收分层

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0/C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次 docs/CADCore10.0/README.md
git diff --check
```

代码闸门触发后：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_adapters
```

重型收口只在 S6 实际修改 expected、collector、capability 或核心 C++ 后执行；docs-only S0-S2 不跑 cad-core build。
