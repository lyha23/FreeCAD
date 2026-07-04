# C12-M14 S2 dedicated native helper probe schema 与采集

## 目标

为 `BRepOffsetAPI_MakePipeShellPy` 未采证方法建立稳定 native helper probe，或者明确记录 native instability blocker。

## 必读文件

- `../README.md`
- `../矩阵/c12m14_helper_lifecycle_source_matrix.tsv`
- `../矩阵/c12m14_helper_lifecycle_oracle_matrix.tsv`
- `../矩阵/c12m14_helper_lifecycle_blocker_queue.tsv`
- `../矩阵/c12m14_helper_lifecycle_validation_matrix.tsv`

## 操作

1. 设计 `docs/temp` native helper probe schema，记录 FreeCAD / OCCT baseline、method sequence、return summary、diagnostics 和 crash/timeout。
2. 采集 baseline subset：`add/isReady/getStatus/build/shape/makeSolid`。
3. 采集 uncollected methods：`remove`、`firstShape`、`lastShape`、`generated`、`simulate`。
4. 采集组合序列：`remove/readd/simulate/build` 和 build 前/后调用差异。
5. 回写 oracle / blocker / validation matrix，并将本步骤重命名为 `【已实现】`。

## 关闭条件

- 每个 helper method 都有 stable native expected、native instability blocker 或明确 product-contract 候选。
- 没有 checked-in artifact 的临时 probe 不解锁 S4 C++。
- 采集失败时记录具体 FreeCADCmd、OCCT、异常或 timeout 证据。

## 非目标

- 不实现 helper lifecycle。
- 不把 crash 当 supported。
- 不修改 existing expected。
