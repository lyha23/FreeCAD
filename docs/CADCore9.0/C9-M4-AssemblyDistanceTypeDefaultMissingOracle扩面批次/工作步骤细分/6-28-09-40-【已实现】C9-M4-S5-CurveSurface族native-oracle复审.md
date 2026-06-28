# 【已实现】C9-M4 S5 CurveSurface 族 native oracle 复审

## 目标

围绕 non-line Edge / Face default branch 批量关闭 oracle evidence：处理 `CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`，并复核 `part_workbench.conic_curves.distance_type_publication` mirror。

## FreeCAD 依据

- `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()`：non-line Edge / Face 被作为 curve family，进入 `CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`。
- `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()`：这些未显式 case 进入 `default`，创建 `ASMTPlanarJoint`，`offset = getJointDistance(joint)`。

## 范围

| scope | DistanceType | S5 判定 |
| --- | --- | --- |
| `C9M4-SCOPE-301` | `CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` | native expected 与 current runtime 比较。 |
| `C9M4-SCOPE-501` | capability / conic publication mirror | accepted / missing rows 在 capability 和 conic mirror 中一致。 |
| `C9M4-SCOPE-601` | diagnostics guard | 未采 curve rows 继续可见。 |

## 必须回写的矩阵行

- `C9M4-SCOPE-301`
- `C9M4-BLOCKER-501`
- `C9M4-BG-301`

## 关闭证据

- live baseline：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=aeedc692ab`，`git log -1 --oneline` 为 `aeedc692ab docs: 关闭 C9-M4 S4 PointLineSurface oracle复审`，S5 起始 `git -c core.quotepath=false status --short -uall` 无输出。队列复核显示 S5-S6 pending。
- S5 新增并采集四个 c3m6 input / expected：
  - `assembly-distance-curve-cylinder-default-boundary.json` -> `expected/assembly-distance-curve-cylinder-default-boundary.freecad.json`，`distance_type=CurveCylinder`。
  - `assembly-distance-curve-sphere-default-boundary.json` -> `expected/assembly-distance-curve-sphere-default-boundary.freecad.json`，`distance_type=CurveSphere`。
  - `assembly-distance-curve-cone-default-boundary.json` -> `expected/assembly-distance-curve-cone-default-boundary.freecad.json`，`distance_type=CurveCone`。
  - `assembly-distance-curve-torus-default-boundary.json` -> `expected/assembly-distance-curve-torus-default-boundary.freecad.json`，`distance_type=CurveTorus`。
- native collector 命令均成功：`FREECADCMD=/home/user/.local/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c3m6/<fixture>.json --fixtures-root fixtures`。采集基线为 FreeCAD `1.2.0 revision 20260519`，四条 expected 均有 `native_solver.return_code=0`、`solver_adapter.status=solved`、`placement_updates[0].properties.Placement.Base=[8.0,0.0,-1.5]`，并保留 `known_gap` / `nonGoal.ids=["DTE-NG-003"]` 诊断口径。
- FreeCAD classification / ordering 证据：`src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` 在 Edge / Face 分支先 `swapJCS(joint)` 让 Face 成为 solver 第一侧；non-line Edge 走 “For other curves we consider them as planes for now” 分支，形成 `CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`。四条 native expected 与 current DTO 均记录 `jcs_swapped_for_solver=true`，solver `reference1=ComponentB/Face1`、`reference2=ComponentA/Edge1`。
- native solver class / offset source：`src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` 对这些未显式 case 进入 `default`，源码为 `ASMTPlanarJoint` 且 `offset = getJointDistance(joint)`；collector expected 为保留 default boundary publication guard，仍记录 `distance_type_mapping_status=default_boundary_not_mapped`、`distance_type_boundary=default_or_todo_boundary`，不在 S5 发布 supported。
- current 比较命令均成功：`CAD_CORE_TEST_LEGACY_OUTPUT=1 ./cad-core recompute fixtures/c3m6/<fixture>.json --output /tmp/c9m4-s5-<fixture>.current.json`。四条 current 均为 `solver_adapter.status=unsupported`、`diagnostics=["unsupported_assembly_solver"]`、`unsupported_joints[0].reason=default_boundary_not_mapped`；`distance_type`、`jcs_swapped_for_solver=true`、solver reference ordering、`distance_type_mapping_status=default_boundary_not_mapped`、`distance_type_boundary=default_or_todo_boundary` 与 expected 的 DTO 口径一致。
- conic publication mirror 复核：`cad-core/src/runtime/capability_contract.cpp` 和 `cad-core/tests/test_adapters.py` 仍把 `part_workbench.conic_curves.distance_type_publication.default_or_todo_boundaries` 固定为 `["CurveCylinder","CurveSphere","CurveCone","CurveTorus"]`，与 Assembly `distance_type_extended_geometry.default_or_todo_boundaries` 中仍未实现的 CurveSurface rows 对齐；S5 未修改 capability publication。
- S5 判定：`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 都不是 `notCollected` 或 `native_oracle_blocked`；它们是 expected-backed native solved / current unsupported 的 `backend_gap_candidate`，交 S6 实现或保留显式诊断。S5 未修改生产 C++、capability publication、collector 或 tests。
- 已回写 `矩阵/c9m4_distance_type_default_missing_oracle_scope_review_matrix.tsv` 的 `C9M4-SCOPE-301`、`矩阵/c9m4_distance_type_default_missing_oracle_blocker_queue.tsv` 的 `C9M4-BLOCKER-501`、`矩阵/c9m4_distance_type_default_missing_oracle_backend_gap_classification.tsv` 的 `C9M4-BG-301`，并更新 `validation_matrix` 的 `C9M4-VAL-501`。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'CurveCylinder|CurveSphere|CurveCone|CurveTorus|conic_curves|distance_type_publication|ASMTPlanarJoint|default_or_todo_boundary|notCollected|backend_gap_candidate' cad-core/fixtures/c3m6 cad-core/tools/collect_freecad_expected.py cad-core/tests/test_p8_features.py cad-core/tests/test_adapters.py cad-core/src/runtime/capability_contract.cpp cad-core/src/assembly/joint_solver.cpp docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- 每个 curve row 有 input fixture / native expected / current comparison / final route。
- `part_workbench.conic_curves.distance_type_publication` 与 Assembly capability 保持一致，不隐藏 missing rows。
- 若 current mismatch 存在，只路由为 S6 `backend_gap_candidate`；S5 不直接改 C++。
- 采集失败或缺 fixture 时保持 `notCollected`，不继承 `CurvePlane` supported。

## 非目标

- 不实现完整 conic GUI 或 Sketcher full solver。
- 不靠 ellipse / curve 几何形态猜测 marker placement。
- 不刷新 unrelated expected。
