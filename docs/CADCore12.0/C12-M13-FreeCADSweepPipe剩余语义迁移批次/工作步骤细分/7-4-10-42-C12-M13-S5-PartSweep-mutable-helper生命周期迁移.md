# C12-M13 S5 Part Sweep mutable helper 生命周期迁移

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

## 非目标

- 不把 helper product contract 强塞回 PartDesign Pipe。
- 不改前端。
- 不用 mesh response 证明 helper shape parity。
