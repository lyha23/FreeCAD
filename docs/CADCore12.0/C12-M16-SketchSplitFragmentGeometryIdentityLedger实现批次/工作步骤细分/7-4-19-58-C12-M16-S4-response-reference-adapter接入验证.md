# C12-M16 S4 response / reference / adapter 接入验证

## 目标

确认 split fragment ledger 已贯通 response、reference resolution 和 adapter public surface，并补 capability / docs wording。

## 必读文件

- `../README.md`
- S3 已实现后的 step 文档和矩阵更新
- `cad-core/src/runtime/recompute.cpp`
- `cad-core/src/runtime/reference_resolution.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`
- `docs/CADCore12.0/README.md`

## 操作

1. 确认 `mesh.edgeSegments[]`、`subshapes[]`、`rawSketchEdgeIdentity` 和 `elementReferenceUpdates` 均发布同一 split fragment token。
2. 确认 `StableSubList=["g<ID>:splitN"]` 能解析到当前 fragment。
3. adapter / capability wording 暴露 split fragment ledger 状态，不误称 persistent FreeCAD session parity。
4. 更新 implementation / validation / blocker 矩阵。
5. 将本步骤重命名为 `【已实现】`。

## 关闭条件

- response/reference/adapter focused tests 通过。
- capability / docs 表述和 request-local 边界一致。
- 下一步只剩发布闸门。

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
