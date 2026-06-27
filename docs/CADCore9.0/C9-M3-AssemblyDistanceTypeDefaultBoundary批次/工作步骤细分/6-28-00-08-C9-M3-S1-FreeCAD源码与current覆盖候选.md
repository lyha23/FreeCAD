# C9-M3 S1 FreeCAD 源码与 current 覆盖候选

## 目标

复核 `DistanceType` 的 FreeCAD source authority、current cad-core 落点、checked-in expected inventory 和 diagnostics guard。S1 只形成 source candidates，不把候选直接写成 supported 或 backendGap。

## FreeCAD 依据

| 语义 | 源码 | S1 要确认 |
| --- | --- | --- |
| DistanceType 分类 | `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | Vertex/Edge/Face 与 primitive type 如何进入 `PointCurve`、`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 等。 |
| PointCurve solver | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` | `PointCurve` 是否明确创建 `ASMTPointInPlaneJoint` 并写 `offset`。 |
| default solver | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` | default 是否创建 `ASMTPlanarJoint` 并写 `offset`。 |
| current DTO | `cad-core/src/assembly/joint_solver.cpp` | `classifyDistanceType()`、`resolveDistanceJointMapping()`、`unsupportedReasonForOndselJoint()` 当前如何处理 diagnostic/default。 |
| expected collector | `cad-core/tools/collect_freecad_expected.py` | checked-in expected 仍保留 native oracle 字段还是只保留 diagnostic metadata。 |

## 必须回写的矩阵行

- `C9M3-SRC-101..404`
- `C9M3-SCOPE-101..404` 的 source linkage
- `C9M3-BLOCKER-101`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'DistanceType|getDistanceType|makeMbdJointDistance|PointCurve|PlaneCone|LineCylinder|CurvePlane|ASMTPointInPlaneJoint|ASMTPlanarJoint|default_or_todo_boundary' src/Mod/Assembly/App cad-core/src/assembly cad-core/tools/collect_freecad_expected.py cad-core/tests/test_p8_features.py cad-core/tests/test_adapters.py cad-core/fixtures/c3m6/expected docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- source candidates 覆盖 FreeCAD 分类、FreeCAD solver mapping、cad-core DTO / guard、collector / fixture、capability tests。
- 每条 source candidate 都有 source evidence、cad-core landing 和 owner step。
- S1 不采集 oracle、不改 expected、不改 C++，也不把 candidate 标成 backendGap。

## 非目标

- 不设计新的 DistanceType 枚举。
- 不引入非 FreeCAD 的几何猜测分类。
- 不处理 non-AssemblyLink primitive frame DTO。
