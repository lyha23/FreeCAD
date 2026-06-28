# 【已实现】C9-M4 S4 PointLineSurface 族 native oracle 复审

## 目标

围绕 Vertex / Face 与 line Edge / Face default branch 批量关闭 oracle evidence：处理 `PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`。

## FreeCAD 依据

- `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()`：Vertex / Face 形成 `PointCone`、`PointTorus`；line Edge / Face 形成 `LineSphere`、`LineCone`、`LineTorus`，并按 FreeCAD 顺序 `swapJCS(joint)`。
- `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()`：这些未显式 case 进入 `default`，创建 `ASMTPlanarJoint`，`offset = getJointDistance(joint)`。

## 范围

| scope | DistanceType | S4 判定 |
| --- | --- | --- |
| `C9M4-SCOPE-201` | `PointCone`、`PointTorus` | native expected 与 current runtime 比较。 |
| `C9M4-SCOPE-202` | `LineSphere`、`LineCone`、`LineTorus` | native expected 与 current runtime 比较。 |
| `C9M4-SCOPE-601` | diagnostics guard | missing marker / unsupported diagnostics 不能被隐藏。 |

## 必须回写的矩阵行

- `C9M4-SCOPE-201..202`
- `C9M4-BLOCKER-401`
- `C9M4-BG-201`

## 关闭证据

- live baseline：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=7fd956ea30`，`git log -1 --oneline` 为 `7fd956ea30 docs: 关闭 C9-M4 S3 FaceCone oracle复审`，S4 起始 `git -c core.quotepath=false status --short -uall` 无输出。队列复核显示 S4-S6 pending。
- S4 新增并采集五个 c3m6 input / expected：
  - `assembly-distance-point-cone-default-boundary.json` -> `expected/assembly-distance-point-cone-default-boundary.freecad.json`，`distance_type=PointCone`。
  - `assembly-distance-point-torus-default-boundary.json` -> `expected/assembly-distance-point-torus-default-boundary.freecad.json`，`distance_type=PointTorus`。
  - `assembly-distance-line-sphere-default-boundary.json` -> `expected/assembly-distance-line-sphere-default-boundary.freecad.json`，`distance_type=LineSphere`。
  - `assembly-distance-line-cone-default-boundary.json` -> `expected/assembly-distance-line-cone-default-boundary.freecad.json`，`distance_type=LineCone`。
  - `assembly-distance-line-torus-default-boundary.json` -> `expected/assembly-distance-line-torus-default-boundary.freecad.json`，`distance_type=LineTorus`。
- native collector 命令均成功：`FREECADCMD=/home/user/.local/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c3m6/<fixture>.json --fixtures-root fixtures`。采集基线为 FreeCAD `1.2.0 revision 20260519`，五条 expected 均有 `native_solver.return_code=0`、`solver_adapter.status=solved`、`placement_updates[0].properties.Placement.Base=[8.0,0.0,-1.5]`，并保留 `known_gap` / `nonGoal.ids=["DTE-NG-003"]` 诊断口径。
- FreeCAD ordering 证据：`PointCone`、`PointTorus` 的 input 仍是 `Reference1=ComponentA/Vertex1`、`Reference2=ComponentB/Face1`，native expected 记录 `jcs_swapped_for_solver=true` 且 solver `reference1=ComponentB/Face1`、`reference2=ComponentA/Vertex1`；`LineSphere`、`LineCone`、`LineTorus` 的 input 仍是 `Reference1=ComponentA/Edge1`、`Reference2=ComponentB/Face1`，native expected 记录 `jcs_swapped_for_solver=true` 且 solver `reference1=ComponentB/Face1`、`reference2=ComponentA/Edge1`。
- native solver class / offset source：FreeCAD `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` 对这些未显式 case 进入 `default`，源码为 `ASMTPlanarJoint` 且 `offset = getJointDistance(joint)`；collector expected 保持 `distance_type_mapping_status=default_boundary_not_mapped`、`distance_type_boundary=default_or_todo_boundary`，用于证明 native route 而不是发布 cad-core support。
- current 比较命令均成功：`CAD_CORE_TEST_LEGACY_OUTPUT=1 ./cad-core recompute fixtures/c3m6/<fixture>.json --output /tmp/c9m4-s4-<fixture>.current.json`。五条 current 均为 `solver_adapter.status=unsupported`、`diagnostics=["unsupported_assembly_solver"]`、`unsupported_joints[0].reason=default_boundary_not_mapped`；`distance_type`、`jcs_swapped_for_solver=true`、solver reference ordering、`distance_type_mapping_status=default_boundary_not_mapped`、`distance_type_boundary=default_or_todo_boundary` 与 expected 的 DTO 口径一致。
- S4 判定：`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus` 都不是 `notCollected` 或 `native_oracle_blocked`；它们是 expected-backed native solved / current unsupported 的 `backend_gap_candidate`，交 S6 实现或保留显式诊断。S4 未修改生产 C++、capability publication、collector 或 tests。
- 已回写 `矩阵/c9m4_distance_type_default_missing_oracle_scope_review_matrix.tsv` 的 `C9M4-SCOPE-201..202`、`矩阵/c9m4_distance_type_default_missing_oracle_blocker_queue.tsv` 的 `C9M4-BLOCKER-401`、`矩阵/c9m4_distance_type_default_missing_oracle_backend_gap_classification.tsv` 的 `C9M4-BG-201`，并更新 `validation_matrix` 的 `C9M4-VAL-401`。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'PointCone|PointTorus|LineSphere|LineCone|LineTorus|ASMTPlanarJoint|default_or_todo_boundary|notCollected|backend_gap_candidate' cad-core/fixtures/c3m6 cad-core/tools/collect_freecad_expected.py cad-core/tests/test_p8_features.py cad-core/src/assembly/joint_solver.cpp docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- 每个 row 有 input fixture / native expected / current comparison / final route。
- `swapJCS` 和 solver reference ordering 必须写入 expected comparison。
- 若 current mismatch 存在，只路由为 S6 `backend_gap_candidate`；S4 不直接改 C++。
- 采集失败或缺 fixture 时保持 `notCollected`，不继承 C9-M3 supported。

## 非目标

- 不把 Face / Face cone 或 Curve / Surface families 混入 S4。
- 不靠 line / surface 类型名直接发布 supported。
- 不引入 persistent solver state。
