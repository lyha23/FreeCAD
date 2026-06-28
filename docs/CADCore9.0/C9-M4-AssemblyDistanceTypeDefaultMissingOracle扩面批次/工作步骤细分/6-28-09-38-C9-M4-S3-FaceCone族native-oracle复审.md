# C9-M4 S3 FaceCone 族 native oracle 复审

## 目标

围绕 Face / Face cone family 批量关闭 oracle evidence：为 `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere` 创建或复核 input fixture，采集 FreeCAD native expected，比较 current cad-core 输出，并判定每行是 expected-backed current match、backend gap，还是仍需 `notCollected`。

## FreeCAD 依据

- `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()`：Face / Face 按 plane / cylinder / cone / torus / sphere 优先级分类，形成 `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`。
- `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()`：这些未显式 case 进入 `default`，创建 `ASMTPlanarJoint`，`offset = getJointDistance(joint)`。

## 范围

| scope | DistanceType | S3 判定 |
| --- | --- | --- |
| `C9M4-SCOPE-101` | `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere` | native expected 与 current runtime 比较。 |
| `C9M4-SCOPE-501` | capability publication | 采集成功但未实现前不得从 default boundary 消失。 |
| `C9M4-SCOPE-601` | diagnostics guard | oracle 失败或 current unsupported 仍需可见。 |

## 必须回写的矩阵行

- `C9M4-SCOPE-101`
- `C9M4-BLOCKER-301`
- `C9M4-BG-101`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'CylinderCone|ConeCone|ConeTorus|ConeSphere|ASMTPlanarJoint|default_or_todo_boundary|notCollected|backend_gap_candidate' cad-core/fixtures/c3m6 cad-core/tools/collect_freecad_expected.py cad-core/tests/test_p8_features.py cad-core/src/assembly/joint_solver.cpp docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- 对每个 S3 row，记录 input fixture 是否存在、native expected 是否采集、native solver class / offset / placement evidence。
- 若 current mismatch 存在，只路由为 S6 `backend_gap_candidate`；S3 不直接改 C++。
- 若 FreeCADCmd / oracle 采集不可用，记录 `notCollected` 或 `native_oracle_blocked`，不写 backendGap。
- diagnostics guard 的后续责任写入 S6。

## 非目标

- 不把 point / line / curve default families 混入 S3。
- 不靠 surface 名称猜测 supported。
- 不刷新 unrelated expected。
