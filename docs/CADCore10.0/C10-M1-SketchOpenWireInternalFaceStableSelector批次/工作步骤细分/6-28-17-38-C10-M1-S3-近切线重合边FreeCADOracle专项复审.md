# C10-M1-S3 近切线重合边 FreeCAD Oracle 专项复审

## 目标

用 FreeCAD oracle 固定 near-tangent / coincident-edge sketch internal behavior，判断这些场景是否能成为 expected-backed C++ 实现任务。S3 不做产品决策，不直接改 C++。

## FreeCAD 依据

- `src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildInternals()`
- `src/Mod/Part/App/FaceMakerBuildFace.cpp::FaceMakerBuildFace::splitSelfIntersecting()`
- `src/Mod/Part/App/FaceMakerBuildFace.cpp::FaceMakerBuildFace::splitAtIntersections()`
- `src/Mod/Part/App/FaceMakerBuildFace.cpp::FaceMakerBuildFace::Build_Essence()`
- `src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::checkIntersection()` / `splitEdges()`

## probe 范围

| probe 轴 | 必须观察 | 允许结论 |
| --- | --- | --- |
| near tangent | 近切线圆弧 / 线段是否 split、是否生成 bounded face | `native_expected_collected` 或 `notCollected`。 |
| coincident edge | 重合边 / 共边 profile 是否保留 shared boundary 或 collapse | `native_expected_collected` 或 `diagnostic_retained`。 |
| touching cutter | touching open cutter 是否只 split boundary，不产生假 open profile | `current_mismatch_candidate` 或 `already_covered`。 |
| bounded counts | `InternalFaceN` / `InternalEdgeN` / `InternalVertexN` count 与 FreeCAD expected | `backend_gap_candidate` 或 `no_gap`。 |

## 必须回写的矩阵行

- `C10M1-SCOPE-101`
- `C10M1-SCOPE-102`
- `C10M1-BLOCKER-301`
- `C10M1-CAT-101`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'near|tangent|coincident|touching|InternalFace|FaceMakerBuildFace|WireJoiner|freecad_version|notCollected' cad-core/fixtures/p5 cad-core/fixtures/c10m1 docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- S3 必须明确记录 FreeCADCmd / native collector 版本或环境阻断原因。
- 若新增 `cad-core/fixtures/c10m1` oracle，必须记录 input、expected、reference source 和 current comparison route。
- 若没有 native expected，不得把 scope 升级为 implementation。
- S3 不允许用 cad-core 当前输出倒推 FreeCAD expected。

## 非目标

- 不实现 FaceMaker / WireJoiner C++。
- 不修改 `profile_resolver.cpp`。
