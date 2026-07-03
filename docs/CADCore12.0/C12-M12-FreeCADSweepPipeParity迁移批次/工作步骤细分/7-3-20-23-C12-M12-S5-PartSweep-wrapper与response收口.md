# C12-M12 S5 Part Sweep wrapper 与 response 收口

## 目标

确保 `Part::Sweep` wrapper、advanced helper DTO 与 response history 在 S4 后仍符合 FreeCAD source，并把前端可消费字段稳定发布。

## 必读文件

- `../矩阵/c12m12_sweep_oracle_matrix.tsv`
- `../矩阵/c12m12_sweep_validation_matrix.tsv`
- `cad-core/src/part/part_sweep.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/src/runtime/recompute.cpp`
- `cad-core/tests/test_p8_features.py`

## 操作

1. 按 S3 red cases 增加或修正 Part Sweep focused tests。
2. 确认 `Sweep::execute()` 对应 wrapper：sections、solid、frenet、transition 与 advanced DTO 不混入 PartDesign Pipe。
3. 复核 response fields：spine、sections、mode、transition、solid、advanced、history、subshapes、mesh。
4. 对 mesh normals / edgeSegments 做 quality gate；不允许用 mesh 修补替代 BRep 修复。
5. 若后端 response 已正确但前端仍失败，记录 frontend follow-up package，不在本仓库伪造兼容字段。
6. 更新 validation matrix 与 README；将本步骤重命名为 `【已实现】`。

## 关闭条件

- Part Sweep focused tests 通过。
- S4 shared builder 修改没有破坏 Part Workbench expected。
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
