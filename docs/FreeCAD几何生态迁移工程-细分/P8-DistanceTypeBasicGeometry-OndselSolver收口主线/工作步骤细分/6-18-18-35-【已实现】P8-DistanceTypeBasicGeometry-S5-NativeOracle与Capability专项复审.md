# P8 DistanceTypeBasicGeometry S5 Native Oracle 与 Capability 专项复审【已实现】

## 目标

在 S3 / S4 runtime 语义成立后，补齐基础 DistanceType 的 native oracle、fixture expected、focused tests 和 capability publication。S5 是从“代码可运行”进入“可发布支持声明”的闸门。

当前结论：S5 已完成基础 DistanceType 的 solver DTO / resolved class / scalar field oracle 和 C ABI capability 发布；半径类、曲线类、GUI/session 和 persistent solver state 仍不进入本包。新 fixture 的完整 native placement writeback 仍暴露 subshape marker / JCS placement parity 风险，不能把本步表述为“完整 Distance native placement parity”。

## FreeCAD 依据

| 证据 | 来源 | 验证点 |
| --- | --- | --- |
| native resolved class | FreeCADCmd + `cad-core/tools/collect_freecad_expected.py` | expected 中记录 `DistanceType` 和 Ondsel joint class |
| scalar field | `AssemblyObject.cpp::makeMbdJointDistance()` | `distanceIJ` / `offset` 与 FreeCAD 一致 |
| point zero behavior | `PointPoint` 分支 | 零距离必须是 `ASMTSphericalJoint` |
| capability 边界 | C ABI capabilities | 只发布 basic DistanceType，不发布 radius-bearing / curve/default |

## fixtures 建议

| fixture | 覆盖 |
| --- | --- |
| `assembly-distance-point-point-nonzero-real-solver.json` | `PointPoint` -> `ASMTSphSphJoint.distanceIJ` |
| `assembly-distance-point-point-zero-real-solver.json` | `PointPoint` -> `ASMTSphericalJoint` |
| `assembly-distance-line-line-real-solver.json` | `LineLine` -> `ASMTRevCylJoint.distanceIJ` |
| `assembly-distance-point-line-real-solver.json` | `PointLine` -> `ASMTCylSphJoint.distanceIJ` |
| `assembly-distance-plane-plane-real-solver.json` | `PlanePlane` -> `ASMTPlanarJoint.offset` |
| `assembly-distance-point-plane-real-solver.json` | `PointPlane` -> `ASMTPointInPlaneJoint.offset` |
| `assembly-distance-line-plane-real-solver.json` | `LinePlane` -> `ASMTLineInPlaneJoint.offset` |

以上 7 个 fixture 及其 `cad-core/fixtures/c3m6/expected/*.freecad.json` 已入库。fixture 使用两个 `Part::Box` 经 `Assembly::AssemblyLink` 进入 Assembly，DistanceJoint 引用 `Vertex1` / `Edge1` / `Face1`，避免直接 Part primitive 与 GroundedJoint 的 native solver 不稳定路径。

## 实现结果

| 项 | 结果 |
| --- | --- |
| collector | `cad-core/tools/collect_freecad_expected.py` 已为 `Distance` Joint 输出 `distance_type`、`jcs_swapped_for_solver`、reference element / primitive、`solver_joint_class`、`distance_ij` 或 `offset` |
| focused fixtures | 已新增 7 个 c3m6 basic DistanceType fixture 和 7 个 FreeCAD expected |
| focused tests | `CadCoreP8FeatureTest -k distance_type` 对比 cad-core runtime 与 FreeCAD expected 的 solver DTO / class / scalar 字段 |
| C ABI capability | `basic_distance_type` 发布为 `covered_full`，列出 6 类基础 DistanceType、对应 Ondsel class、`distance_ij` / `offset` 字段 |
| remaining boundary | `LineCircle`、`CircleCircle`、`PlaneCylinder`、`PlaneSphere`、`CylinderCylinder`、`CylinderSphere`、`PointCylinder`、`PointSphere` 仍留给半径类第二批；`PointCurve` / `CurvePlane` / `Other` 和 GUI/session 仍为 nonGoal |

## 必须回写的矩阵行

- `DTC-BLOCK-005`：已关闭；7 个 focused fixtures 和 FreeCADCmd expected 入库，单 fixture `--check` 通过。
- `DTC-BLOCK-006`：已关闭；C ABI capability、tests、docs / matrices 已同步 basic DistanceType support。
- `DTC-SCOPE-006`：已从 `notCollected` 转为 `supported`，范围限定为 solver DTO / class / scalar oracle。
- `DTC-SCOPE-007`：已从 `releaseGate` 转为 `supported`，范围限定为 public capability 的 basic DistanceType 子集。

## 剩余风险

- 新 expected 中的 `placement_updates` 已记录 native FreeCAD 输出，但 cad-core 当前 subshape marker / JCS placement 仍与部分 native fixture 不一致；差异集中在 LineLine、PointLine、PointPlane、LinePlane 的 full placement writeback，不影响本步锁定的 `distance_type`、resolved Ondsel class、`distance_ij` / `offset` 和 request-local `jcs_swapped_for_solver` 证据。
- `python3 cad-core/tools/collect_freecad_expected.py --phase c3m6 --check` 当前仍被既有 unrelated c3m6 expected 缺失 / unsupported properties 阻塞，包括 copy-on-change、RackPinion/Screw diagnostic 等历史项；本步只重新采集并单独校验 7 个 basic DistanceType fixture，不刷新 unrelated expected。

## 验收标准

本步代码 / expected 验收包括：

```bash
cmake --build cad-core/build
python3 cad-core/tools/collect_freecad_expected.py cad-core/fixtures/c3m6/assembly-distance-point-point-nonzero-real-solver.json --out cad-core/fixtures/c3m6/expected/assembly-distance-point-point-nonzero-real-solver.freecad.json --check --freecadcmd /Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd
python3 cad-core/tools/collect_freecad_expected.py cad-core/fixtures/c3m6/assembly-distance-point-point-zero-real-solver.json --out cad-core/fixtures/c3m6/expected/assembly-distance-point-point-zero-real-solver.freecad.json --check --freecadcmd /Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd
python3 cad-core/tools/collect_freecad_expected.py cad-core/fixtures/c3m6/assembly-distance-line-line-real-solver.json --out cad-core/fixtures/c3m6/expected/assembly-distance-line-line-real-solver.freecad.json --check --freecadcmd /Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd
python3 cad-core/tools/collect_freecad_expected.py cad-core/fixtures/c3m6/assembly-distance-point-line-real-solver.json --out cad-core/fixtures/c3m6/expected/assembly-distance-point-line-real-solver.freecad.json --check --freecadcmd /Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd
python3 cad-core/tools/collect_freecad_expected.py cad-core/fixtures/c3m6/assembly-distance-plane-plane-real-solver.json --out cad-core/fixtures/c3m6/expected/assembly-distance-plane-plane-real-solver.freecad.json --check --freecadcmd /Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd
python3 cad-core/tools/collect_freecad_expected.py cad-core/fixtures/c3m6/assembly-distance-point-plane-real-solver.json --out cad-core/fixtures/c3m6/expected/assembly-distance-point-plane-real-solver.freecad.json --check --freecadcmd /Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd
python3 cad-core/tools/collect_freecad_expected.py cad-core/fixtures/c3m6/assembly-distance-line-plane-real-solver.json --out cad-core/fixtures/c3m6/expected/assembly-distance-line-plane-real-solver.freecad.json --check --freecadcmd /Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k distance_type
python3 -m unittest cad-core.tests.test_adapters.CadCoreAdapterTest -k capabilities
git diff --check -- cad-core/fixtures/c3m6 cad-core/tests cad-core/tools/collect_freecad_expected.py cad-core/src/adapters/c_api/c_api.cpp docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线
```

文档 / 矩阵验收：

```bash
rg -n 'DTC-BLOCK-005.*Closed|DTC-BLOCK-006.*Closed' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵
rg -n 'basic_distance_type|PointPoint|LineLine|PlanePlane|PointPlane|LinePlane|PointLine' cad-core/src/adapters/c_api/c_api.cpp cad-core/tests/test_adapters.py
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/*.tsv
```

关闭条件：

- 所有基础 DistanceType fixture 都有 checked-in expected。
- C ABI capability 明确列出 basic DistanceType support，并把 radius-bearing / curve/default 保留在 remaining / nonGoal / next batch 中。
- docs / matrix 状态与 tests 一致。
- 若 FreeCADCmd 在 sandbox 内因 Qt / CPU feature 失败，不得把失败当作 implementation failure；需要在本机非 sandbox 或既有 collector 路径重新采集。

## 非目标

- 不重采 unrelated c3m6 expected。
- 不发布完整 Distance geometry matrix。
- 不处理 full Assembly transaction 或 GUI lifecycle。
