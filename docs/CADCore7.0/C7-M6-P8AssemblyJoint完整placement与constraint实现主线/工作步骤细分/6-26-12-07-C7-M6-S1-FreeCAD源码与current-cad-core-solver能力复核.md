# C7-M6 S1 FreeCAD 源码与 current cad-core solver 能力复核

## 目标

复核 Assembly Joint placement / constraint 的 FreeCAD source authority、当前 `cad-core` implementation、fixtures、expected 和 focused tests。S1 只写文档和矩阵，不新增 fixtures/expected/tests，不运行 FreeCAD oracle，不改 C++。

## 必读文件

- `src/Mod/Assembly/App/AssemblyObject.cpp`
- `src/Mod/Assembly/App/AssemblyUtils.cpp`
- `src/Mod/Assembly/JointObject.py`
- `cad-core/src/assembly/assembly_object.cpp`
- `cad-core/src/assembly/assembly_utils.cpp`
- `cad-core/src/assembly/joint_group.cpp`
- `cad-core/src/assembly/joint_solver.cpp`
- `cad-core/include/cad_core/assembly/joint_solver.h`
- `cad-core/tests/test_p8_features.py`
- `cad-core/fixtures/c3m6`
- C7-M6 README、方案和矩阵

## 执行要点

1. 记录 live baseline 和 C7-M6 queue。
2. 记录 FreeCAD source authority：solve order、GroundedJoint sync、marker placement、JointType mapping、DistanceType、current value、placement writeback。
3. 复核 current `cad-core` 能力：request-local real Ondsel adapter、marker placement、solver DTO、documentObjectUpdates、unsupported diagnostics、capability publication。
4. 复核哪些 fixture / expected 已经覆盖，哪些只是 current runtime diagnostic，哪些缺 native lifecycle。
5. 更新 `source_authority.tsv`、`scope.tsv`、`blocker_queue.tsv` 和方案 S1 小节。
6. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S2。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'AssemblyObject::solve|makeMbdJointOfType|handleOneSideOfJoint|getDistanceType|getJointCurrentValue|JointType|GroundedJoint|assembly_set_placement|documentObjectUpdates|Ondsel' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线 src/Mod/Assembly cad-core/src/assembly cad-core/tests/test_p8_features.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线 docs/CADCore7.0/README.md
git diff --check
```

## 完成标准

- S2 有明确的 oracle candidate 输入池和 already-covered baseline。
- S1 没有修改 C++、fixtures、expected 或 tests。
- 队列推进到 S2。
