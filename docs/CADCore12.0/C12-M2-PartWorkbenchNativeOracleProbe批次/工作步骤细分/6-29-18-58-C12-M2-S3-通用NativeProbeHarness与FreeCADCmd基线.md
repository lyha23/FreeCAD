# C12-M2 S3 通用 NativeProbe harness 与 FreeCADCmd 基线

## 目标

建立 C12-M2 可复用的 native probe 运行口径和 artifact schema。S3 可以创建或整理 probe 脚本/输出文档，但仍不得修改 `cad-core/src`、fixtures expected 或 tests。

## 必须固定的 schema

每个 native probe artifact 至少记录：

- probe id、family、case id、source authority、input artifact。
- FreeCADCmd 路径、FreeCAD 版本、OCCT/LibPack 版本、运行命令。
- stdout/stderr、exit code、异常分类。
- native expected summary：shape kind、subshape counts、bbox/volume/area 或 diagnostics。
- request-local 判定和 current comparison path。
- 结论：`expected_ready`、`native_probe_blocked`、`helper_blocked`、`native_hidden`、`sandbox_runtime_limit`、`product_boundary_rejected`。

## 执行步骤

1. 复用 `cad-core/tools/collect_freecad_expected.py` 的可用机制，判断是否需要新增 C12-M2 专用 probe 脚本或只写运行说明。
2. 若要新增 probe 脚本，脚本只能采集 native oracle，不得改 expected/support 状态；输出放到 `docs/temp` 或 C12-M2 明确记录的 artifact path。
3. 固定失败分类：sandbox Qt/processor 错误、FreeCAD helper lifecycle、native-hidden API、collector bug、product boundary rejection、真实 stable expected。
4. 回写 probe matrix 和 validation matrix，让 S4/S5 能按同一口径执行。
5. 如 FreeCADCmd 在当前环境不可运行，记录 blocker，并明确需要本机非 sandbox 环境采集。

## 更新目标

- `矩阵/c12m2_partworkbench_native_oracle_probe_matrix.tsv`
- `矩阵/c12m2_partworkbench_native_oracle_validation_matrix.tsv`
- `矩阵/c12m2_partworkbench_native_oracle_blocker_queue.tsv`
- 必要时新增 `docs/temp/6-29-*-c12m2-*-native-probe.*`，但不新增正式 expected。

## 验收命令

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次 docs/CADCore12.0/README.md
git diff --check
```

## 完成条件

S4/S5 在不重新解释口径的情况下，就能知道每个 probe 的命令、artifact path、失败分类和 expected-ready 标准。
