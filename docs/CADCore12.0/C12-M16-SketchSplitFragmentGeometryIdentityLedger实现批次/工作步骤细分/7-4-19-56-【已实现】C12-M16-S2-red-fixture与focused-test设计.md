# 【已实现】C12-M16 S2 red fixture 与 focused test 设计

## 目标

先补能失败的 focused tests / fixtures，锁定 split fragment ledger 的 public response 和 reference resolution 期望。

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`daee0a5f78`。
- `git log -1 --oneline`：`daee0a5f78 文档：关闭 C12-M16 S1 split history 复核`。
- `git -c core.quotepath=false status --short -uall`：无输出。

## 必读文件

- `../README.md`
- S1 已实现后的 step 文档和矩阵更新
- `../矩阵/c12m16_split_fragment_identity_contract_matrix.tsv`
- `../矩阵/c12m16_split_fragment_identity_implementation_matrix.tsv`
- `cad-core/tests/test_p5_sketch.py`
- `cad-core/tests/test_adapters.py`
- `cad-core/fixtures/p5/`

## S2 fixture / test 选择

- 新增 `cad-core/fixtures/c12m16/sketch-split-fragment-line-identity.json`：复用 P5 through-open-cutter 最小几何，只给 cutter source edge 加 `id=701`。当前 FreeCAD/cad-core history 证据显示 `Edge5 -> InternalEdge3/InternalEdge8/InternalEdge9`，S2 以该 history 顺序锁定 `g701:split1..3`。
- 新增 `cad-core/fixtures/c12m16/sketch-split-fragment-line-reference.json`：在同一 BaseSketch 上加 Consumer `ExternalGeometry`，旧引用为 `StableSubList=["g701:split1"]`、stale `SubList=["InternalEdge99"]`，用于锁定 split token recovery / reference update。
- 新增 `cad-core/tests/test_p5_sketch.py` 中 4 个 focused tests：
  - `test_c12m16_split_fragment_identity_publishes_response_ledger`
  - `test_c12m16_split_fragment_missing_id_fallback_has_no_durable_token`
  - `test_c12m16_split_fragment_stable_sublist_resolves_current_fragment`
  - `test_c12m16_split_fragment_missing_stable_sublist_reports_reselect`

## red 失败记录

命令：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest
```

结果：177 个测试运行，3 个 S2 新增测试按预期 red，既有测试未新增失败。

- `test_c12m16_split_fragment_identity_publishes_response_ledger`：`raw_edge_identity.byStableSubname` 当前只有 `g701 -> Edge5`，新增期望 `g701:split1 -> InternalEdge3`、`g701:split2 -> InternalEdge8`、`g701:split3 -> InternalEdge9` 均为 `None`。
- `test_c12m16_split_fragment_stable_sublist_resolves_current_fragment`：期望 `StableSubList=["g701:split1"]` 解析到 `InternalEdge3` 并刷新 `ReferenceShadow`；当前 diagnostic 为 `subname_resolve_failed`，`subname=InternalEdge99`。
- `test_c12m16_split_fragment_missing_stable_sublist_reports_reselect`：期望缺失 token `g701:split99` 输出 `split_fragment_missing`；当前仍走旧 ReferenceShadow 路径输出 `subname_resolve_failed`。
- `test_c12m16_split_fragment_missing_id_fallback_has_no_durable_token` 当前通过，作为无 geometry id split fragment 不发布 durable token 的防回归 guard。

## S3 实现落点

- `cad-core/include/cad_core/sketcher/sketch_edge_identity.h` / `cad-core/src/sketcher/sketch_edge_identity.cpp`：扩展 raw edge identity ledger 或新增 fragment ledger view，支持 `sourceStableSubname=g701` 与 `fragmentStableSubname/stableSubname=g701:splitN`。
- `cad-core/src/sketcher/sketch_internal_result.cpp`：消费 FaceMaker / WireJoiner / internal alias history，把 `Edge5 -> InternalEdge3/InternalEdge8/InternalEdge9` 这类 one-to-many history materialize 到 sketch object fields。
- `cad-core/src/runtime/recompute.cpp`：让 `edgeSegments[]` 与 `subshapes[]` 从同一 fragment ledger 透传 `sourceGeometryId`、`sourceGeometryKind`、`sourceStableSubname`、`fragmentStableSubname`、`identityStatus=stable_split_fragment`。
- `cad-core/src/runtime/reference_resolution.cpp`：让 `StableSubList=["g701:splitN"]` 解析到 current fragment；缺失或漂移时输出 `split_fragment_missing` / reselect diagnostic，不能回退到 `g701` 或 bbox/order 猜测。

## 关闭结论

- red tests 已约束 `g<ID>:splitN` response 与 reference resolution。
- fixture/test 命名清楚，覆盖 source one-to-many、fragment missing / reselect 和 missing id fallback。
- 未修改 production C++、include 或旧 expected；下一步可直接进入 S3 C++ 实现。

## 非目标

- 不让测试按 mesh/bbox/order 猜 fragment。
- 不改 production C++。
- 不刷新无关 expected。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest
cd ..
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/矩阵/*.tsv
git diff --check
```
