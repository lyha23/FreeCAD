# C10-M3-S4 cad-core 恢复路径与 current mismatch 专项复审

## 目标

消费 S3 的 native evidence，比较 current cad-core `ReferenceShadow` / `ShadowSub` parser、BREP / fingerprint recovery、`ElementMap` split / deleted diagnostics 和 focused tests。只有 native evidence 与 current mismatch 同时成立，S4 才能把行升级为 `backend_gap_candidate`。

## 输入

- S3 native 可观测性结论和 expected / probe 输出。
- `c10m3_reference_shadow_recovery_source_candidates.tsv`
- `c10m3_reference_shadow_recovery_scope_review_matrix.tsv`
- `cad-core/src/app/property_links.cpp`
- `cad-core/src/part/topo_shape_reference.cpp`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/app/element_map.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_adapters.py`

## cad-core 复审轴

| scope | current 路径 | 判断 |
| --- | --- | --- |
| `C10M3-SCOPE-201` | `property_links.cpp` `readShadowSubList` / `readReferenceShadowList` | parser 是否保留 native required fields、index alignment 和 diagnostics。 |
| `C10M3-SCOPE-201` | `topo_shape_reference.cpp` `recoverReferenceShadowSubshape()` | BREP / fingerprint recovery 是否能覆盖 S3 single-target expected。 |
| `C10M3-SCOPE-202` | `topo_shape.cpp` / `element_map.cpp` | split / deleted / merge 是否仍保持 structured diagnostic，而非猜测 stable subname。 |
| `C10M3-SCOPE-301` | adapter focused tests | `elementReferenceUpdates` 是否只发布前端 graph update 建议，不持久化后端 geometry state。 |

## 必须回写的矩阵行

- `C10M3-SCOPE-201`：关闭为 `no_gap`、`backend_gap_candidate` 或 `diagnostic_retained`。
- `C10M3-SCOPE-202`：如 S4 提前发现 split / deleted mismatch，可交给 S5 或标为 `backend_gap_candidate`，但必须有 native evidence。
- `C10M3-BLOCKER-401`：S4 完成后改为 `closed_s4` 或 evidence-backed S6 implementation row。
- `C10M3-CAT-102`：按 current comparison 改为 `no_gap`、`backend_gap_candidate` 或 `diagnostic_retained`。

## 代码闸门

S4 可以提出实现行，但不应直接把未经 S6 审核的 C++ 合并为发布结论。允许的未来落点：

| blocker / scope | C++ 落点 | 测试 |
| --- | --- | --- |
| `C10M3-BLOCKER-401` / `C10M3-SCOPE-201` | `cad-core/src/app/property_links.cpp`、`cad-core/src/part/topo_shape_reference.cpp`、`cad-core/src/part/topo_shape.cpp` | `cad-core/tests/test_p7_features.py`、`cad-core/tests/test_adapters.py` |
| `C10M3-BLOCKER-401` / `C10M3-SCOPE-202` | `cad-core/src/app/element_map.cpp`、`cad-core/src/part/topo_shape.cpp` | focused split / deleted / merge diagnostic tests |

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "ReferenceShadow|ShadowSub|StableSubList|recoverReferenceShadowSubshape|resolveElementReference|unsupportedReferenceShadowBrepReason|element_history_status|elementReferenceUpdates" cad-core/src/app cad-core/src/part cad-core/src/runtime cad-core/include/cad_core cad-core/tests/test_p7_features.py cad-core/tests/test_adapters.py
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次/矩阵/*.tsv
git diff --check
```

若 S4 修改 tests、capability 或 C++，必须补充：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_adapters
```

S4 验收通过后，文件才能重命名为 `6-29-01-13-【已实现】C10-M3-S4-cad-core恢复路径与current mismatch专项复审.md`。

## 非目标

- 不从 current cad-core output 倒推 FreeCAD golden。
- 不在 adapter、JSON 输出、fixture 名称分支或输出排序里补业务语义。
- 不把 split 一对多或 deleted reference 猜成唯一 stable subname。
- 不引入跨请求 geometry cache。
