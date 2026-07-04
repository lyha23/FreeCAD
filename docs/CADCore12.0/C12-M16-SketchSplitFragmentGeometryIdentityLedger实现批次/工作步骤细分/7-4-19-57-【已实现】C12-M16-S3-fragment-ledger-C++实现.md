# 【已实现】C12-M16 S3 fragment ledger C++ 实现

## 目标

实现 split fragment identity ledger 的核心 C++ 能力，让 source one-to-many fragments 可以稳定发布 `g<ID>:splitN`。

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`fe38378677`。
- `git log -1 --oneline`：`fe38378677 测试：锁定 C12-M16 S2 split fragment red 用例`。
- `git -c core.quotepath=false status --short -uall`：无输出。

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

1. 已在 `RawSketchEdgeIdentity` / `RawSketchEdgeIdentityLedger` 中增加 request-local split fragment fields，并从 `NamedShape.history` 的 split 记录生成 `g<ID>:splitN`。
2. 已在 `buildSketchInternalResult()` 先构造 InternalShape `NamedShape`，再消费其 split history，建立 source id -> fragment token -> indexed edge 映射。
3. 已让 `raw_edge_identity`、`mesh.edgeSegments[]`、`subshapes[]` 发布 `sourceGeometryId=701`、`sourceStableSubname=g701`、`stableSubname=fragmentStableSubname=g701:splitN`、`identityStatus=stable_split_fragment`。
4. 已让 `StableSubList=["g<ID>:splitN"]` 通过 raw identity ledger 解析到当前 `InternalEdgeN`；缺失 token 输出 `split_fragment_missing`。
5. 普通未 split `g<ID>` raw edge identity 保持原有路径；没有 source geometry id 的 split fragment 不发布 durable token。

## 实现落点

- `cad-core/include/cad_core/sketcher/sketch_edge_identity.h` / `cad-core/src/sketcher/sketch_edge_identity.cpp`：fragment fields、`addSplitFragmentIdentitiesFromInternalHistory()`、`byStableSubname` token view。
- `cad-core/src/sketcher/sketch_internal_result.cpp`：先发布 InternalShape history，再用同一 ledger 标注 mesh/subshapes/object fields。
- `cad-core/src/runtime/recompute.cpp`：response 透传 `fragmentStableSubname`，并把 `stable_split_fragment` 的 response stable name 固定为 fragment token。
- `cad-core/src/runtime/reference_resolution.cpp`：`g<ID>:splitN` 解析到 current InternalShape subshape；缺失时输出 `split_fragment_missing`。
- `cad-core/src/sketcher/sketch_object_external.cpp` / `cad-core/src/runtime/element_reference_update.cpp`：ExternalGeometry 执行期消费 split fragment token，并刷新 ReferenceShadow 时保留 fragment stable name 与 source stable name 的区别。

## 关闭条件

- S2 red tests 已变绿。
- 普通 raw edge identity focused tests 已随 `CadCoreP5SketchTest` 通过。
- 未新增 backend persistent state；fragment ledger 由本次 recompute 的 InternalShape history 派生。

## 非目标

- 不改 frontend。
- 不实现完整 solver identity。
- 不靠 mesh/bbox/source order 猜 split ownership。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_returns_split_internal_face_mesh_ids
cd ..
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/矩阵/*.tsv
git diff --check
```

结果：

- `cmake --build build`：通过。
- `python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest`：177 tests，OK。
- `python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_returns_split_internal_face_mesh_ids`：1 test，OK。
- 队列 / TSV / `git diff --check`：S3 文档更新后执行并记录到 validation matrix。
