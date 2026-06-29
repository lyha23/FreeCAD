# 【已实现】C12-M2 S3 通用 NativeProbe harness 与 FreeCADCmd 基线

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

## S3 执行结论

本步 live baseline：

```text
pwd=/Users/li/Chili3DProject/FreeCAD
HEAD=dc5afa4f85
git log -1 --oneline=dc5afa4f85 docs: 完成 C12-M2 S2 范围准入矩阵
git -c core.quotepath=false status --short -uall=<clean>
```

S3 复用 `cad-core/tools/collect_freecad_expected.py` 的 FreeCADCmd 外部进程思路，但没有调用 fixture expected 写入路径。由于本步只冻结 native probe artifact schema 和 runtime baseline，新增的 C12-M2 专用脚本均放在 `docs/temp`：

- `docs/temp/6-29-20-12-c12m2-native-probe-schema.md`
- `docs/temp/6-29-20-12-c12m2-native-probe-harness.py`
- `docs/temp/6-29-20-12-c12m2-freecadcmd-baseline-probe.py`
- `docs/temp/6-29-20-12-c12m2-freecadcmd-baseline-native-probe.json`

FreeCADCmd baseline：

- path：`/Users/li/.cargo/bin/freecadcmd`
- version：`1.2.0 revision 20260519`
- OCCT：`7.8.1`
- LibPack / LibPackVersion：空
- command：由 harness 通过短 `-c` + file-backed probe script 执行；S3 发现长 raw multiline `-c` 字符串可在 FreeCADCmd 启动期失败，因此 S4/S5 不应直接使用长内联 probe。
- stdout/stderr/exit code：已写入 baseline JSON；本轮 baseline `exit_code=0`，`conclusion=expected_ready`。

`expected_ready` 在 S3 只表示 FreeCADCmd runtime metadata 可读，不代表 Sweep / Filling / GeomPlate / Loft / ProjectOnSurface 任一 family geometry expected 已发布。S4/S5 仍必须按 schema 明确分类为 `expected_ready`、`native_probe_blocked`、`helper_blocked`、`native_hidden`、`sandbox_runtime_limit`、`collector_bug`、`product_boundary_rejected` 或 `retained_no_expected`，并分别写入 request-local 判定和 current comparison path。

矩阵状态：

- `C12M2-PROBE-S3-RUNTIME` 已新增，绑定 schema/harness/baseline artifact。
- `C12M2-BLOCKER-001` 已关闭为 `closed_s3_runtime_baseline_ready`。
- `C12M2-BLOCKER-002` 已关闭为 `closed_s3_schema_frozen`。
- `C12M2-VAL-005` 已通过；`C12M2-VAL-301..304` 记录 S3 最终验收。

本步未运行 S4/S5 family probe，未改 `cad-core/src`、`cad-core/include`、fixtures expected、tests、adapters、capability wording，未比较 current cad-core，也未把任何 crash / timeout / notCollected / helper lifecycle 噪声当作 expected。
