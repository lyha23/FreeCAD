# C12-M14 S4 helper lifecycle 实现或 no-code 收口

## 目标

按 S3 裁决执行：若授权，补 Part Sweep helper lifecycle；若未授权，关闭为 no-code retained blocker。

## 必读文件

- `../README.md`
- `../矩阵/c12m14_helper_lifecycle_scope_matrix.tsv`
- `../矩阵/c12m14_helper_lifecycle_oracle_matrix.tsv`
- `../矩阵/c12m14_helper_lifecycle_blocker_queue.tsv`
- `../矩阵/c12m14_helper_lifecycle_validation_matrix.tsv`

## 操作

1. 若 S3 授权，修改 `cad-core/src/part/part_sweep.cpp` 和必要 shared builder DTO。
2. 补 `cad-core/fixtures/c12m14/` 与 focused P8 tests。
3. 保持 `Part::Sweep` wrapper 主路径 no-mix regression。
4. 若 S3 未授权，只更新 docs/matrix，说明 no-code retained blocker。
5. 回写 validation / scope / blocker matrix，并将本步骤重命名为 `【已实现】`。

## 关闭条件

- implementation path：helper lifecycle expected/product contract 与 current 对齐，focused P8 green。
- no-code path：blocker 有明确 reopen condition，不改 C++。
- wrapper no-mix guard 不回归。

## 非目标

- 不改 PartDesign Pipe。
- 不用 mesh-only 证明 helper method parity。
- 不扩大到所有 Part Workbench Python helpers。
