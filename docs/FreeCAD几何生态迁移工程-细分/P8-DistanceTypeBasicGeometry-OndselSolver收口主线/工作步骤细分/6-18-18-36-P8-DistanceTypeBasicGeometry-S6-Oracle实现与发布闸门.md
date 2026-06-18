# P8 DistanceTypeBasicGeometry S6 Oracle 实现与发布闸门

## 目标

消费 `notCollected`、`backendGap`、`releaseGate` 和边界保护项，把本包推进成可执行的代码落地队列；完成后只发布 basic DistanceType support，不发布 radius-bearing / curve / GUI-session。

## 当前待消费项

| 类型 | 行 | 处理方式 |
| --- | --- | --- |
| `backendGap` | `DTC-SCOPE-002` | S3 落 reference classification / JCS ordering DTO |
| `backendGap` | `DTC-SCOPE-003` | S4 落 `PointPoint` 零 / 非零 Ondsel 映射 |
| `backendGap` | `DTC-SCOPE-004` | S4 落 `LineLine` / `PointLine` Ondsel 映射 |
| `backendGap` | `DTC-SCOPE-005` | S4 落 `PlanePlane` / `PointPlane` / `LinePlane` Ondsel 映射 |
| `notCollected` | `DTC-SCOPE-006` | S5 采集 c3m6 native expected |
| `releaseGate` | `DTC-SCOPE-007` | S5 / S6 同步 capability、tests、docs / matrices |
| `notCollected` / `nonGoal` | `DTC-SCOPE-008..009` | S6 确认不发布，记录下一批 |

## 下一轮代码落点

| blocker | C++ / Python 落点 | FreeCAD authority | tests / fixtures | 成功标准 |
| --- | --- | --- | --- | --- |
| `DTC-BLOCK-001` | `cad-core/include/cad_core/assembly/joint_solver.h`、`cad-core/src/assembly/joint_solver.cpp`、`cad-core/src/assembly/assembly_utils.cpp` | `AssemblyUtils.cpp::getDistanceType()` | `test_p8_features.py -k distance_type` | solver JSON 暴露基础 `distance_type`、primitive、`jcs_swapped_for_solver` |
| `DTC-BLOCK-002` | `cad-core/src/assembly/joint_solver.cpp::makeOndselJointOfType()` | `AssemblyObject.cpp::makeMbdJointDistance()` `PointPoint` 分支 | point-point zero / nonzero fixtures | 零距离为 `ASMTSphericalJoint`，非零为 `ASMTSphSphJoint.distanceIJ` |
| `DTC-BLOCK-003` | 同上 | `LineLine`、`PointLine` 分支 | line-line / point-line fixtures | class 与 `distance_ij` 符合 FreeCAD |
| `DTC-BLOCK-004` | 同上 | `PlanePlane`、`PointPlane`、`LinePlane` 分支 | plane-plane / point-plane / line-plane fixtures | class 与 `offset` 符合 FreeCAD |
| `DTC-BLOCK-005` | `cad-core/tools/collect_freecad_expected.py`、`cad-core/fixtures/c3m6/expected` | FreeCADCmd native execution | collector `--check` | 7 个 fixtures 都有 expected，且不刷新 unrelated expected |
| `DTC-BLOCK-006` | `cad-core/src/adapters/c_api/c_api.cpp`、`cad-core/tests/test_adapters.py`、本包矩阵和 upstream P8 matrix | C ABI capability publication | adapter capability test | public matrix 发布 basic support，radius-bearing 仍未发布 |
| `DTC-BLOCK-007` | 本包 nonGoal / backend gap matrix | S0 exclusion table | TSV assertions | radius-bearing、curve/default、GUI/session 边界仍有效 |

## 禁止路径

- 禁止在 adapter 输出层根据 fixture 名称修正 `solver_joint_class`、`distance_ij` 或 `offset`。
- 禁止用 bbox、volume、shape 数量、subshape 顺序或几何距离猜测 FreeCAD ownership。
- 禁止把 radius-bearing DistanceType 简化成 basic DistanceType。
- 禁止持久保存 shape、NamedShape、ElementMap、BREP 或 MBD solver session。
- 禁止把 `swapJCS` 实现成跨请求 DocumentObject graph mutation。

## 验收标准

本轮短跑：

```bash
cmake --build cad-core/build
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k distance_type
git diff --check -- cad-core/include/cad_core/assembly cad-core/src/assembly cad-core/tests/test_p8_features.py docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线
```

oracle / 发布阶段：

```bash
python3 cad-core/tools/collect_freecad_expected.py --fixture-dir cad-core/fixtures/c3m6 --check
python3 -m unittest cad-core.tests.test_adapters.CadCoreAdapterTest -k capabilities
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/*.tsv
```

重型收口条件：

- 只有修改 collector、capability publication 或 upstream P8 matrix 时，才要求执行 adapter capability test。
- 只有新增 / 刷新 expected 时，才要求 FreeCADCmd collector `--check`。
- 不要求全量 FreeCAD build。

发布关闭条件：

- `DTC-BLOCK-001..006` 关闭，`DTC-BLOCK-007` 证明边界保护成立。
- `DTC-SCOPE-002..007` 状态与代码、fixtures、expected、capability 一致。
- `DTC-SCOPE-008` 保持 radius-bearing `notCollected`，并指向第二批 package。
- `DTC-SCOPE-009` 保持 curve/default 和 GUI/session `nonGoal`。
- 所有 S0-S6 文档和 root 入口在 evidence 通过后才可逐个重命名为 `【已实现】`。

## 非目标

- 不关闭 radius-bearing DistanceType。
- 不关闭 curve/default DistanceType。
- 不实现 full Assembly transaction、GUI drag / postDrag 或 persistent solver state。
