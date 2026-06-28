# C10-M1-S1 FreeCAD 源码与 current 覆盖候选矩阵

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

- `cad-core/src/features/sketch_object.cpp`
- `cad-core/src/sketcher/sketch_internal_builder.cpp`
- `cad-core/src/part/face_maker.cpp`
- `cad-core/src/part/wire_joiner.cpp`
- `cad-core/src/part/internal_shape_history_publisher.cpp`
- `cad-core/src/part_design/profile_resolver.cpp`
- `cad-core/src/topo/element_map.cpp`
- `cad-core/src/topo/named_shape.cpp`
- `cad-core/tests/test_p5_sketch.py`
- `cad-core/tests/test_p6_topology.py`
- `cad-core/tests/test_adapters.py`

## 必须回写的矩阵行

- `C10M1-SRC-101` 到 `C10M1-SRC-208`
- `C10M1-SCOPE-101` 到 `C10M1-SCOPE-105`
- `C10M1-BLOCKER-101`

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

## 非目标

- 不运行 FreeCADCmd。
- 不改 `cad-core` 业务代码。
