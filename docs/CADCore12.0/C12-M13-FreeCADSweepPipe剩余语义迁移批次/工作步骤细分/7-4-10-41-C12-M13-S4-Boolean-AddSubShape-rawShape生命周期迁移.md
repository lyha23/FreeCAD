# C12-M13 S4 Boolean / AddSubShape / rawShape 生命周期迁移

## 目标

对齐 FreeCAD PartDesign Pipe 的 pre-boolean tool cache、`rawShape`、fuse/cut、refine 后 `Shape` 和 downstream AddSubShape consumer 生命周期。

## 必读文件

- `../README.md`
- `../矩阵/c12m13_sweep_remainder_source_matrix.tsv`
- `../矩阵/c12m13_sweep_remainder_oracle_matrix.tsv`
- `../矩阵/c12m13_sweep_remainder_validation_matrix.tsv`

## 操作

1. 修改 `cad-core/src/part_design/feature_pipe.cpp` 和 runtime request-local cache，明确 additive/subtractive `AddSubShape`。
2. 必要时补 `ComputeContext` 字段，使 downstream DressUp / Transformed 能读取 pre-boolean tool，而不是从 final shape 猜。
3. 锁定 no-base additive、base fuse、base cut 三类 response / history / diagnostics。
4. 补 focused P7 tests，必要时复用已有 DressUp / Transformed AddSubShape consumer 断言。
5. 回写 validation / scope / blocker matrix，并将本步骤重命名为 `【已实现】`。

## 关闭条件

- AdditivePipe 和 SubtractivePipe 的 `AddSubShape`、`rawShape`、final `Shape` 均有 expected/current 证据。
- downstream consumer 不再只比较 final shape。
- C12-M12 / S3 focused tests 不回归。

## 非目标

- 不实现完整 topological naming。
- 不重写 Body replay 全家族。
- 不处理 Part Sweep helper lifecycle。
