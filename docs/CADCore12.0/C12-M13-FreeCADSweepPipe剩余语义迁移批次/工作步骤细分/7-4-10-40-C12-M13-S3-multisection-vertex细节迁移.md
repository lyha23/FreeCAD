# C12-M13 S3 multisection vertex 细节迁移

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

## 非目标

- 不处理 AddSubShape / Boolean lifecycle。
- 不处理 Part Sweep mutable helper。
- 不修改前端。
