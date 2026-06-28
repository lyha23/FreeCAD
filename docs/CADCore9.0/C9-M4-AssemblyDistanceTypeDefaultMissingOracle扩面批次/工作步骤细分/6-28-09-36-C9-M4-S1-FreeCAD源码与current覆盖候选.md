# C9-M4 S1 FreeCAD 源码与 current 覆盖候选

## 目标

复核缺 oracle default rows 的 FreeCAD source authority、current cad-core 落点、collector 行为、fixture inventory 和 diagnostics guard。S1 只形成 source candidates，不把候选直接写成 supported 或 backendGap。

## FreeCAD 依据

| 语义 | 源码 | S1 要确认 |
| --- | --- | --- |
| DistanceType 分类 | `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | Face / Face、Vertex / Face、Edge / Face 如何进入 13 个 default rows。 |
| default solver | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` | default 是否创建 `ASMTPlanarJoint` 并写 `offset`。 |
| current DTO | `cad-core/src/assembly/joint_solver.cpp` | `classifyDistanceType()`、`resolveDistanceJointMapping()` 当前如何处理 missing default rows。 |
| expected collector | `cad-core/tools/collect_freecad_expected.py` | default boundary metadata 是否仍保护未采 rows。 |
| capability / tests | `cad-core/src/runtime/capability_contract.cpp`、`cad-core/tests` | supported rows 与 missing rows 是否可区分。 |

## 必须回写的矩阵行

- `C9M4-SRC-101..402`
- `C9M4-SCOPE-101..701` 的 source linkage
- `C9M4-BLOCKER-101`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'getDistanceType|makeMbdJointDistance|CylinderCone|ConeCone|ConeTorus|ConeSphere|PointCone|PointTorus|LineSphere|LineCone|LineTorus|CurveCylinder|CurveSphere|CurveCone|CurveTorus|ASMTPlanarJoint|default_or_todo_boundary' src/Mod/Assembly/App cad-core/src/assembly cad-core/tools/collect_freecad_expected.py cad-core/tests/test_p8_features.py cad-core/tests/test_adapters.py cad-core/fixtures/c3m6 docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- source candidates 覆盖 FreeCAD 分类、FreeCAD default solver mapping、cad-core DTO / guard、collector / fixture、capability tests。
- 每条 source candidate 都有 source evidence、cad-core landing 和 owner step。
- S1 不采集 oracle、不改 expected、不改 C++，也不把 candidate 标成 backendGap。

## 非目标

- 不设计新的 DistanceType 枚举。
- 不引入非 FreeCAD 的几何猜测分类。
- 不处理 non-AssemblyLink primitive frame DTO。
