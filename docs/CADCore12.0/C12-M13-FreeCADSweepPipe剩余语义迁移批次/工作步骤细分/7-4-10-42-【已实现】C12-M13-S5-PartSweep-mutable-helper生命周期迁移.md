# 【已实现】C12-M13 S5 Part Sweep mutable helper 生命周期迁移

## 目标

把 FreeCAD `BRepOffsetAPI_MakePipeShellPy` mutable helper 的状态序列表达为 CAD Core request-local helper lifecycle，同时保持 `Part::Sweep` wrapper 主路径不混线。

## 必读文件

- `../README.md`
- `../矩阵/c12m13_sweep_remainder_source_matrix.tsv`
- `../矩阵/c12m13_sweep_remainder_oracle_matrix.tsv`
- `../矩阵/c12m13_sweep_remainder_validation_matrix.tsv`

## 操作

1. 修改 `cad-core/src/part/part_sweep.cpp` 的 advanced helper DTO / diagnostics。
2. 必要时修改 `cad-core/src/part/topo_shape_expansion.cpp` 的 shared builder options，使 helper mutation 顺序可被稳定复现。
3. 覆盖 `add/remove/isReady/status/build/shape/firstShape/lastShape/generated/simulate/makeSolid` 的最小 request-local lifecycle。
4. 补 focused P8 tests；若 FreeCAD native helper 状态不可直接导出，明确 product-contract 边界。
5. 回写 validation / scope / blocker matrix，并将本步骤重命名为 `【已实现】`。

## 关闭条件

- helper lifecycle expected 或 product-contract artifact 与 current 对齐。
- Workbench `Part::Sweep` wrapper 主属性路径不回归。
- advanced helper diagnostic 不被写成 FreeCAD native parity，除非 S2 有 native evidence。

## 关闭记录

- S5 先复核 S2 证据与当前矩阵：`C12M13-ORACLE-301` 只覆盖 `add/isReady/getStatus/build/shape/makeSolid`，`remove/firstShape/lastShape/generated/simulate` 仍未形成 checked-in native expected 或已批准 product-contract artifact。
- 本步未修改 C++：`cad-core/src/part/part_sweep.cpp` 继续保持 Workbench `Part::Sweep` wrapper 主路径与 advanced helper DTO 分离；`cad-core/src/part/topo_shape_expansion.cpp` 内部 `Simulate(2)` 只服务 PartDesign cap/sewing，不能冒充 helper API `simulate()` parity。
- S5 额外做了临时 FreeCADCmd 调查：基础 `add/isReady/getStatus/build/shape/makeSolid` 与单独 `remove`、`firstShape`、`lastShape`、`generated`、`simulate` 可被观察；但组合 `remove/readd/simulate/build` 会触发 `NCollection_Sequence::ChangeValue`。该临时调查没有形成稳定 checked-in artifact，因此不解锁 C++ 实现。
- `ORACLE-301` 保持 collected subset current-supported；`ORACLE-302` wrapper no-mix current-supported；`C12M13-BLOCKER-601` 保持 `blocked_partial_helper_oracle`，重开条件是新增 dedicated native helper probe 或明确批准的 request-local product-contract artifact，且覆盖方法集合、调用顺序、失败/成功状态和 current response 字段。
- 本步只更新 S5 closure 文档、README、方案、总入口和矩阵；未改 `cad-core/src`、fixtures、expected、tests 或 capability source。`cmake --build build` 与完整 P8 未运行，原因是没有 C++ / fixture / test 改动；已运行 focused helper test 与文档/JSON/TSV hygiene。

## 非目标

- 不把 helper product contract 强塞回 PartDesign Pipe。
- 不改前端。
- 不用 mesh response 证明 helper shape parity。
