# 【已实现】C12-M13 S1 source 与 current landing 批量复核

## 目标

把 C12-M13 的三条实现主线和 ORACLE-001 分流全部绑定到 FreeCAD source、cad-core 当前落点和 focused test surface。

## 必读文件

- `../README.md`
- `../矩阵/c12m13_sweep_remainder_source_matrix.tsv`
- `../矩阵/c12m13_sweep_remainder_scope_matrix.tsv`
- `../矩阵/c12m13_sweep_remainder_blocker_queue.tsv`

## 操作

1. 复核 `src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::execute()` 中 multisection vertex / wiresection 规则。
2. 复核 `FeaturePipe.cpp::Pipe::execute()` 中 `AddSubShape`、`rawShape`、Boolean fuse/cut、refine 和 final `Shape` 生命周期。
3. 复核 `src/Mod/PartDesign/App/FeatureAddSub.cpp::FeatureAddSub::getAddSubShape()` 对 downstream consumer 的缓存语义。
4. 复核 `src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` 与 `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp` helper mutable API。
5. 对照 `cad-core/src/part_design/feature_pipe.cpp`、`cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/src/part/part_sweep.cpp` 和 runtime cache，写入 current landing。
6. 回写 source / scope / blocker / validation matrix，并将本步骤重命名为 `【已实现】`。

## 关闭记录

- `C12M13-SRC-001..010` 已复核为 `reviewed`；未发现需要移出 S2/S3/S4/S5 的无 source-authority 候选。
- `C12M13-VAL-006` 已定位 `FeaturePipe.cpp::Pipe::execute()` 的 `profilePoint` / `isLastSectionVertex` / `wiresections` 与 `AddSubShape` / `rawShape` 生命周期、`FeatureAddSub.cpp::getAddSubShape()`、`PartFeatures.cpp::Sweep::execute()` 和 `BRepOffsetAPI_MakePipeShellPyImp.cpp` mutable API。
- open / waiting scope row 已写明 source authority、current landing、S2 oracle owner 和 S3/S4/S5 implementation owner；`ORACLE-001` 保持非阻塞 `waiting_user_repro`。
- `C12M13-BLOCKER-201` 已关闭；后续队列应从 S2 `oracle 批量采集与用户复现分流` 继续。

## 关闭条件

- 每条 scope row 都有 source authority、current landing、oracle owner step 和 implementation owner step。
- 未证明 source authority 的候选从 S2/S3/S4/S5 中移出。
- ORACLE-001 只保留为用户输入分流，不作为其它 source-backed 子项的阻塞条件。

## 非目标

- 不采集 oracle。
- 不修改 C++。
- 不新增 fixture。
