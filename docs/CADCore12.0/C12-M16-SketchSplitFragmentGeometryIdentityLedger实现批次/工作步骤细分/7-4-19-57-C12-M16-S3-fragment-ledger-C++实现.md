# C12-M16 S3 fragment ledger C++ 实现

## 目标

实现 split fragment identity ledger 的核心 C++ 能力，让 source one-to-many fragments 可以稳定发布 `g<ID>:splitN`。

## 必读文件

- `../README.md`
- S1 / S2 已实现后的 step 文档和矩阵更新
- `cad-core/include/cad_core/sketcher/sketch_edge_identity.h`
- `cad-core/src/sketcher/sketch_edge_identity.cpp`
- `cad-core/src/sketcher/sketch_internal_result.cpp`
- `cad-core/src/runtime/recompute.cpp`
- `cad-core/src/runtime/reference_resolution.cpp`
- S2 新增 focused tests / fixtures

## 操作

1. 在 sketch identity ledger 中增加 fragment identity 数据结构和 deterministic token 生成。
2. 从 split history / internal alias / current fragment edge 建立 source id -> fragment token -> indexed edge 映射。
3. stable split fragment 发布 `identityStatus=stable_split_fragment` 或等价显式状态，并带 `sourceGeometryId`、`sourceStableSubname=g<ID>`、`stableSubname=g<ID>:splitN`。
4. 无法唯一归属时输出 `split_requires_reselect`，不 fallback 到 order guessing。
5. 保持普通 `g<ID>` raw edge identity 不回归。
6. 更新 implementation / blocker / validation 矩阵。
7. 将本步骤重命名为 `【已实现】`。

## 关闭条件

- S2 red tests 变绿。
- 普通 raw edge identity focused tests 仍通过。
- 没有新增 persistent backend state。

## 非目标

- 不改 frontend。
- 不实现完整 solver identity。
- 不靠 mesh/bbox/source order 猜 split ownership。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_returns_split_internal_face_mesh_ids
cd ..
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/矩阵/*.tsv
git diff --check
```
