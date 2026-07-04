# C12-M14 S1 source 与 current helper landing 复核

## 目标

把 C12-M14 的 helper method 集合绑定到 FreeCAD source、cad-core 当前落点和 focused P8 surface。

## 必读文件

- `../README.md`
- `../矩阵/c12m14_helper_lifecycle_source_matrix.tsv`
- `../矩阵/c12m14_helper_lifecycle_scope_matrix.tsv`
- `../矩阵/c12m14_helper_lifecycle_blocker_queue.tsv`

## 操作

1. 复核 `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp` 中 `add/remove/isReady/getStatus/build/shape/firstShape/lastShape/generated/simulate/makeSolid`。
2. 复核 `src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` wrapper 主路径。
3. 对照 `cad-core/src/part/part_sweep.cpp`、`cad-core/src/part/topo_shape_expansion.cpp` 和 `cad-core/tests/test_p8_features.py` 当前落点。
4. 回写 source / scope / blocker / validation matrix，并将本步骤重命名为 `【已实现】`。

## 关闭条件

- 每条 scope row 都有 source authority、current landing、S2 oracle owner 和 S3/S4 owner。
- wrapper no-mix guard 已写入 non-goal。
- 未证明 source authority 的候选从 S2/S3/S4 中移出。

## 非目标

- 不采集 oracle。
- 不修改 C++。
- 不新增 fixture。
