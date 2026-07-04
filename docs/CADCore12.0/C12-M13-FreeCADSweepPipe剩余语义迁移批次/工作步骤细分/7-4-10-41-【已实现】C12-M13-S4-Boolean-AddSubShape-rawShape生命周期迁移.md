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

## 关闭记录

- `cad-core/src/part_design/feature_pipe.cpp::executePipeFeature()` 已按 FreeCAD `FeaturePipe.cpp::Pipe::execute()` 的 `AddSubShape.setValue(...)`、`Part::OpCodes::Fuse/Cut`、`this->rawShape = boolOp`、`Shape.setValue(getSolid(boolOp))` 顺序拆分：AddSubShape add/sub slot 保留 pre-boolean tool，base Fuse/Cut 后的 feature `Shape` / mesh / subshapes / named shape 发布 post-boolean 结果。
- `cad-core/src/graph/recompute_plan.cpp` 为同一 Body 内的 Pipe 增加最近前序 PartDesign feature 依赖，使 Pipe producer 能在自身执行时拿到 request-local Body 前缀；Body replay 仍通过 `context.addSubShapes` 消费 add/sub tool，没有重写 Body replay family。
- `cad-core/fixtures/c12m13/expected/partdesign-pipe-additive-lifecycle.freecad.json` 与 `partdesign-pipe-subtractive-lifecycle.freecad.json` 已移除 `known_gap` 并记录 `s4_resolution`；`test_c12m13_partdesign_pipe_lifecycle_matches_native_oracle` 验证 ORACLE-201/202 red-to-green 和 Body AddSubShape consumer replay。
- S4 focused regression 已覆盖 C12-M13 lifecycle、S3 diagnostic、C12-M12 multi-wire sewing，以及 C4-M2 additive/subtractive Pipe body oracle；Part Sweep helper lifecycle 仍留给 S5。

## 非目标

- 不实现完整 topological naming。
- 不重写 Body replay 全家族。
- 不处理 Part Sweep helper lifecycle。
