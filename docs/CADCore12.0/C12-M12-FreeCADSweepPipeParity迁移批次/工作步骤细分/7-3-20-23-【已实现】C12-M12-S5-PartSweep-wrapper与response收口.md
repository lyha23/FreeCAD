# C12-M12 S5 Part Sweep wrapper 与 response 收口

## 目标

确保 `Part::Sweep` wrapper、advanced helper DTO 与 response history 在 S4 后仍符合 FreeCAD source，并把前端可消费字段稳定发布。

本步实际关闭为 `no_code_regression_closeout`：S4 没有 C++ / shared-builder source delta，S5 实现 gate 未授权；本步只验证既有 Part Sweep focused controls 与完整 P8 regression，确认当前 response contract 仍 supported。

## 关闭结论

- 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=c8ffeab56d`（`c8ffeab56d 文档：关闭 C12-M12 S4 PartDesign Pipe 主路径`），起点 `git status --short -uall` 干净。
- 指定 8 个 Part Sweep focused controls 通过：`Ran 8 tests in 1.109s OK`。
- 完整 `python3 -m unittest tests.test_p8_features` 通过：`Ran 219 tests in 44.917s OK`。
- 现有 P8 controls 已覆盖 `spine`、`sections`、mode / `frenet`、`transition`、`solid`、`advanced`、history、`subshapes` 与 `mesh` response。
- `mesh` 只作为 response quality gate，不作为 BRep parity 证据；ORACLE-007 保持该边界。
- 本步未修改 Part Sweep wrapper、response schema、C++、fixtures、expected 或 capability wording；`C12M12-BLOCKER-601` 关闭为 `closed_no_code_regression_closeout`。
- `C12M12-ORACLE-001` 仍等待用户 repro；S6 继续发布闸门，不得编造用户失败 fixture。

## 必读文件

- `../矩阵/c12m12_sweep_oracle_matrix.tsv`
- `../矩阵/c12m12_sweep_validation_matrix.tsv`
- `cad-core/src/part/part_sweep.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/src/runtime/recompute.cpp`
- `cad-core/tests/test_p8_features.py`

## 操作与实际处理

1. S5 实现 gate 未授权，未增加或修正 Part Sweep focused tests；复用 S3 已列 8 个 focused controls 验证。
2. 确认 `Sweep::execute()` 对应 wrapper：sections、solid、frenet、transition 与 advanced DTO 不混入 PartDesign Pipe。
3. 复核 response fields：spine、sections、mode、transition、solid、advanced、history、subshapes、mesh。
4. 对 mesh normals / edgeSegments 做 quality gate；不允许用 mesh 修补替代 BRep 修复。
5. 若后端 response 已正确但前端仍失败，记录 frontend follow-up package，不在本仓库伪造兼容字段。
6. 更新 validation matrix 与 README；将本步骤重命名为 `【已实现】`。

## 关闭条件

- Part Sweep focused tests 通过。
- S4 没有 shared builder source delta，且 Part Workbench expected 未回归。
- response contract 足以让前端使用后端 token，不需要 prefix guessing。

## 非目标

- 不改 `my-chili3d`。
- 不新增非 FreeCAD advanced option。
- 不把 preview-only 输出写成 source-backed expected。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features
```
