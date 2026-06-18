# P8 DistanceTypeBasicGeometry S5 Native Oracle 与 Capability 专项复审

## 目标

在 S3 / S4 runtime 语义成立后，补齐基础 DistanceType 的 native oracle、fixture expected、focused tests 和 capability publication。S5 是从“代码可运行”进入“可发布支持声明”的闸门。

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

## 必须回写的矩阵行

- `DTC-BLOCK-005`：fixtures 和 expected 关闭。
- `DTC-BLOCK-006`：C ABI capability、tests、docs / matrices 同步。
- `DTC-SCOPE-006`：从 `notCollected` 转为 `supported` 或明确列出剩余 expected gap。
- `DTC-SCOPE-007`：实现后通过 release gate 才能转为 `supported`。

## 验收标准

本步代码 / expected 验收至少包括：

```bash
cmake --build cad-core/build
python3 cad-core/tools/collect_freecad_expected.py --fixture-dir cad-core/fixtures/c3m6 --check
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
