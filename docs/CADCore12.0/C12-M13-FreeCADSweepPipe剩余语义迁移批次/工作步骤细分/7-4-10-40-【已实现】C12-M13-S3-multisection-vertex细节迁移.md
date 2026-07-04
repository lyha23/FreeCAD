# 【已实现】C12-M13 S3 multisection vertex 细节迁移

## 目标

在 S2 证明 mismatch 后，对齐 FreeCAD `Pipe::execute()` 的 profile vertex、last section vertex 和 wiresection 分组规则。

## 必读文件

- `../README.md`
- `../矩阵/c12m13_sweep_remainder_source_matrix.tsv`
- `../矩阵/c12m13_sweep_remainder_oracle_matrix.tsv`
- `../矩阵/c12m13_sweep_remainder_validation_matrix.tsv`

## 操作

1. 修改 `cad-core/src/part_design/feature_pipe.cpp` 和必要的 shared builder，表达 FreeCAD vertex / wiresection 规则。
2. 保持 C12-M12 multi-wire cap/sewing 通过，不回退其 shared cap face sewing 路径。
3. 为新增 `c12m13` fixtures 补 focused P7 tests。
4. 更新 diagnostics / response 字段时同步 expected 和 capability wording。
5. 回写 validation / scope / blocker matrix，并将本步骤重命名为 `【已实现】`。

## 关闭条件

- S2 中 multisection vertex 相关 mismatch red-to-green。
- 现有 `c5m3`、`c51m4`、`c12m12` Pipe focused tests 不回归。
- 没有新增 fixture 特判或按 fixture 名称分支。

## 关闭记录

- `cad-core/src/part/topo_shape_expansion.cpp::preparePipeShellProfileLanes()` 已按 FreeCAD `FeaturePipe.cpp::Pipe::execute()` 的 `addWiresToWireSections` / outer `catch (...)` 路径对齐 unequal-inner-wire diagnostic：后续 multisection section 比 base lane 多 wire 时返回 `A fatal error occurred when making the pipe`。
- `cad-core/fixtures/c12m13/expected/partdesign-pipe-vertex-wire-diagnostics.freecad.json` 已移除 `known_gap`，记录 S3 关闭证据；P7 focused test 断言 ORACLE-103 诊断与 FreeCADCmd evidence 一致。
- `ORACLE-101/102` success paths 与继承的 `c5m3`、`c51m4`、`c12m12` Pipe regression 已通过 focused tests；未修改 AddSubShape/rawShape lifecycle、Part Sweep helper 或 fixture-name 分支。

## 非目标

- 不处理 AddSubShape / Boolean lifecycle。
- 不处理 Part Sweep mutable helper。
- 不修改前端。
