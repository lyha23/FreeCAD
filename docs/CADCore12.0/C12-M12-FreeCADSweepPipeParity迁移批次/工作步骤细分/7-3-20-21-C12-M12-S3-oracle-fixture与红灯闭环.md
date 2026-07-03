# C12-M12 S3 oracle fixture 与红灯闭环

## 目标

把用户失败样例和代表性 Sweep / Pipe 行为变成 native/current 可比较证据。只有出现稳定 FreeCAD expected 与 current mismatch，才授权 S4/S5 实现。

## 必读文件

- `../矩阵/c12m12_sweep_oracle_matrix.tsv`
- `../矩阵/c12m12_sweep_drift_audit.tsv`
- `cad-core/fixtures/c3m4`
- `cad-core/fixtures/c4m2`
- `cad-core/fixtures/c5m3`
- `cad-core/fixtures/c51m4`
- `cad-core/fixtures/c5m10`
- `cad-core/fixtures/c5m12`
- `cad-core/fixtures/c6m1`
- `cad-core/fixtures/c6m3`
- `cad-core/fixtures/c6m4`

## 操作

1. 从用户失败 input/output 中提取最小 repro，记录到 oracle matrix。
2. 用 FreeCADCmd 或现有 native expected 证明 FreeCAD 行为；若 native 失败，记录 blocker。
3. 用 `cad-core/build/cad-core recompute <fixture> --output <out>` 或现有 unittest 复现 current 行为。
4. 为 Standard vs Frenet、cap/sewing、multi-section、auxiliary/binormal/support、Part Sweep wrapper 建立 red case。
5. 若 current 已支持，关闭为 `current_supported_with_regression_added` 或 `no_mismatch`。
6. 若 mismatch 成立，写入 S4/S5 implementation rows；将本步骤重命名为 `【已实现】`。

## 关闭条件

- 至少一个用户失败或代表性 fixture 有 native/current 对照结论。
- 每个 oracle row 都有 native status、current status、mismatch、owner step。
- 未通过 native oracle 的行不会进入 code gate。

## 非目标

- 不直接修 C++。
- 不用截图或 mesh 法线替代 native expected。
- 不把前端 preview 失败当成 backend mismatch，除非 response payload 已证明后端错误。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features tests.test_p8_features
```

可根据 S3 实际裁剪为 focused tests；若运行范围过大，必须在闭合记录中写明裁剪理由。
