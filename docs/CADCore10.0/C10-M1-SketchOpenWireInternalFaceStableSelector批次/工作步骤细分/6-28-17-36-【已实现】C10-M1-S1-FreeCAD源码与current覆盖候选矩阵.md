# 【已实现】C10-M1-S1 FreeCAD 源码与 current 覆盖候选矩阵

## 目标

复核 Sketch internal geometry source authority 和 current cad-core coverage，把可进入 S2/S3/S4/S5 的候选写入 source candidates。S1 只做源码和 current 状态审计，不采 oracle、不改 C++。

## FreeCAD 依据

| 语义 | 源码入口 | S1 要确认的事实 |
| --- | --- | --- |
| buildInternals | `src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildInternals()` | FaceMakerBuildFace 后接 WireJoiner open-wire，结果进入 `InternalShape`。 |
| FaceMaker | `src/Mod/Part/App/FaceMakerBuildFace.cpp::Build_Essence()` | self-intersection / inter-edge split 与 bounded face producer evidence。 |
| FaceMaker history | `src/Mod/Part/App/FaceMaker.cpp::Build()` / `postBuild()` | pre-split 与 post-build history 是 topo producer 来源。 |
| WireJoiner | `src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::build()` / `getOpenWires()` | EdgeInfo / WireInfo / `aHistory` / openWireCompound 决定 open-wire export。 |
| Profile consumer | `src/Mod/PartDesign/App/FeatureSketchBased.cpp::ProfileBased::getTopoShapeVerifiedFace()` | PartDesign profile 消费 closed face，不接受 open wire 假 profile。 |

## current cad-core 依据

- `cad-core/src/sketcher/sketch_object.cpp`
- `cad-core/src/sketcher/sketch_internal_builder.cpp`
- `cad-core/src/part/face_maker.cpp`
- `cad-core/src/part/wire_joiner.cpp`
- `cad-core/src/part/internal_shape_history_publisher.cpp`
- `cad-core/src/part_design/profile_resolver.cpp`
- `cad-core/src/app/element_map.cpp`
- `cad-core/include/cad_core/part/topo_shape.h`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/tests/test_p5_sketch.py`
- `cad-core/tests/test_p6_topology.py`
- `cad-core/tests/test_adapters.py`

## S1 live 基线

| 命令 | S1 记录 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `3493d948f5` |
| `git log -1 --oneline` | `3493d948f5 docs: 修正 C10-M1 S1 cad-core 路径口径` |
| `git -c core.quotepath=false status --short -uall` | 无输出，工作区干净。 |
| C10-M1 queue | 下一项为 S1。 |

## S1 审计结果

| 方向 | 当前结论 |
| --- | --- |
| FreeCAD `SketchObject::buildInternals()` | 先 `makeElementFace(..., "Part::FaceMakerBuildFace")`，再启用 `WireJoiner` 的 tight bound / merge 并调用 `getOpenWires(openWires, "SKF")`，最后把 face result 和 open-wire result 组合进 request-local `InternalShape`。 |
| FreeCAD `SketchObject::getInternalElementMap()` | 只遍历 `TopAbs_VERTEX` 与 `TopAbs_EDGE`，通过 `findSubShapesWithSharedVertex(... CheckGeometry | SingleResult)` 写 InternalEdge/InternalVertex alias；没有 raw `FaceN` 到 `InternalFaceN` 的稳定 alias。 |
| FreeCAD `FaceMakerBuildFace` / `FaceMaker` | `Build_Essence()` 先做 `splitSelfIntersecting()` / `splitAtIntersections()`，再交给 `BOPAlgo_BuilderFace`；`FaceMaker::postBuild()` 使用 `MapperHistory(myPreSplitHistory)` 与 `MapperMaker(mySplitter)` 连接 producer history。 |
| FreeCAD `WireJoiner` | `EdgeInfo` / `WireInfo` / `openWireCompound` / `aHistory` 是 open-wire export 和 naming evidence 的主账本，`getOpenWires()` 用 `MapperHistory(aHistory)` 写入输出。 |
| FreeCAD `ProfileBased::getTopoShapeVerifiedFace()` | PartDesign profile 消费 verified face；无 face 且 `allowOpen=false` 时是 `Cannot make face from profile`，open wire 不能伪装成可拉伸 profile。 |
| current `cad-core` builder | `sketch_internal_builder.cpp` 已按 FaceMaker bounded result 后接 WireJoiner open-wire result 组合 `internalShape`，同时保持 `profileShape` 与 `internalShape` 分离。 |
| current `cad-core` history | `face_maker.cpp`、`wire_joiner.cpp` 和 `internal_shape_history_publisher.cpp` 已有 FaceMakerBuildFace / WireJoiner producer evidence、open-export mapper events、summary-only diagnostic 与唯一 alias 子集。 |
| current `cad-core` selector | `profile_resolver.cpp` 已要求 request-local `StableSubList=InternalFaceN` 必须有 `Sketch.InternalShape` `NamedShape` / `ElementMap` evidence；缺证据仍是 `unsupported_stable_subname`。 |
| current tests | `test_p5_sketch.py`、`test_p6_topology.py`、`test_adapters.py` 覆盖 InternalFace mesh / NamedShape、WireJoiner open export、ReferenceShadow-backed recovery、StableSubList diagnostic 和 C API reference updates。 |

## 必须回写的矩阵行

- `C10M1-SRC-101` 到 `C10M1-SRC-208`
- `C10M1-SCOPE-101` 到 `C10M1-SCOPE-105`
- `C10M1-BLOCKER-101`

## 回写结果

- `source_candidates.tsv` 已拆为 FreeCAD authority `C10M1-SRC-101..108` 与 current cad-core coverage `C10M1-SRC-201..208`。
- `scope_review_matrix.tsv` 保留 `C10M1-SCOPE-001` 的 S0 冻结口径，并回写 `C10M1-SCOPE-101..105`：near-tangent/coincident 和 complex open-wire 仍是 `native_oracle_required`；FaceMaker producer history 与 without-ReferenceShadow InternalFace selector 仍是 `backend_gap_candidate`；ambiguous open-wire ElementMap 保持 `diagnostic_retained`。
- `blocker_queue.tsv` 已把 `C10M1-BLOCKER-101` 标为 `closed_s1`；后续 S2-S6 blocker 仍待执行。
- 未采 oracle，未新增 fixture / expected / tests，未修改 C++。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'buildInternals|FaceMakerBuildFace|getOpenWires|WireJoiner|InternalFace|StableSubList|getTopoShapeVerifiedFace' src/Mod/Sketcher/App src/Mod/Part/App src/Mod/PartDesign/App cad-core/src cad-core/tests
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- `source_candidates.tsv` 每条 source 都有 FreeCAD 或 cad-core 落点。
- `scope_review_matrix.tsv` 没有把 near-tangent、complex open-wire 或 without-ReferenceShadow stable selector 直接写成 supported。
- S1 文档记录 current cad-core coverage，不新增 fixture、expected、tests 或 C++。

## 验收记录

- `rg -n 'buildInternals|FaceMakerBuildFace|getOpenWires|WireJoiner|InternalFace|StableSubList|getTopoShapeVerifiedFace' src/Mod/Sketcher/App src/Mod/Part/App src/Mod/PartDesign/App cad-core/src cad-core/tests`：通过，源码和 current 覆盖入口均可定位。
- `awk -F '\t' ... docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/矩阵/*.tsv`：通过，TSV 字段数一致。
- `rg -n '[ \t]$' docs/CADCore10.0`：无输出。
- `git diff --check`：通过。
- `step_goal_queue.py docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/工作步骤细分 --format markdown`：下一项为 S2。

## 非目标

- 不运行 FreeCADCmd。
- 不改 `cad-core` 业务代码。
