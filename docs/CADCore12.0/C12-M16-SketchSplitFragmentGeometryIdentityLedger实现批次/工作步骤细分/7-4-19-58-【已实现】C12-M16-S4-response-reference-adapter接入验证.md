# 【已实现】C12-M16 S4 response / reference / adapter 接入验证

## 目标

确认 split fragment ledger 已贯通 response、reference resolution 和 adapter public surface，并补 capability / docs wording。

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`7c5ce46eca`。
- `git log -1 --oneline`：`7c5ce46eca 实现 C12-M16 S3 split fragment ledger`。
- `git -c core.quotepath=false status --short -uall`：无输出。

## 必读文件

- `../README.md`
- S3 已实现后的 step 文档和矩阵更新
- `cad-core/src/runtime/recompute.cpp`
- `cad-core/src/runtime/reference_resolution.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`
- `docs/CADCore12.0/README.md`

## 操作

1. 已确认 `raw_edge_identity.byStableSubname` 发布 `g701 -> Edge5` 与 `g701:split1..3 -> InternalEdge3/InternalEdge8/InternalEdge9`；`byIndexed` 对应 fragment entry 均携带 `stableSubname=fragmentStableSubname=g701:splitN`、`sourceStableSubname=g701`、`sourceGeometryId=701`、`identityStatus=stable_split_fragment`。
2. 已确认 C API / CLI result 中 `mesh.edgeSegments[]`、`subshapes[]` 和 `elementReferenceUpdates` 使用同一 split fragment token；`StableSubList=["g701:split1"]` 解析到当前 `InternalEdge3`，`ReferenceShadow.stableSubname` 同步保留 `g701:split1`。
3. 已新增 `sketcher.split_fragment_identity_ledger` capability public wording，公开 token 格式、response/reference 字段、`split_fragment_missing` diagnostic、request-local boundaries 和 non-goals；明确不声称 persistent FreeCAD session parity。
4. 已补 `test_c_api_returns_c12m16_split_fragment_ledger_fields` 和 capability assertions，覆盖 adapter public result 与 capability surface。
5. 已更新 implementation / validation / blocker 矩阵，并将本步骤重命名为 `【已实现】`。

## 实现落点

- `cad-core/src/runtime/capability_contract.cpp`：新增 `sketcher.split_fragment_identity_ledger` public capability 节点。
- `cad-core/tests/test_adapters.py`：新增 C API split fragment ledger fields 断言，并扩展 capability public wording 断言。
- `docs/CADCore12.0/README.md`、本包 `README.md` 与矩阵：记录 S4 接入验证结论，下一步推进 S5。

## 关闭条件

- response/reference/adapter focused tests 已通过。
- capability / docs 表述和 request-local 边界一致。
- 下一步只剩 S5 发布闸门。

## 非目标

- 不新增无关 feature。
- 不跑全量 FreeCAD build。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_returns_split_internal_face_mesh_ids
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest
cd ..
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/矩阵/*.tsv
git diff --check
```

结果：

- `cmake --build build`：通过。
- `python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_returns_split_internal_face_mesh_ids`：1 test，OK。
- `python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_returns_c12m16_split_fragment_ledger_fields`：1 test，OK。
- `python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts`：1 test，OK。
- `python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest`：177 tests，OK。
- 队列 / TSV / `git diff --check`：S4 文档更新和重命名后执行并记录到 validation matrix。
