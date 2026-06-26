# C8-M2-S3 native CopyOnChange 生命周期探针与 blocker 证据

## 目标

按 S2 oracle plan 探测 FreeCAD native `BindCopyOnChange` / `PartialLoad` 生命周期，或记录 source-backed native blocker。S3 可以新增 native probe、evidence fixture 或 known_gap expected；不得改 runtime C++ 主路径。

## 探针批次

| 批次 | 对应 oracle | 目标 |
| --- | --- | --- |
| property-state | `C8M2-ORACLE-101` | Disabled / Enabled / Mutated 可观察属性 |
| PartialLoad | `C8M2-ORACLE-102` | allow-partial / Support 行为是否可 request-local 观察 |
| copied-object cache | `C8M2-ORACLE-103` | temporary document copied-object cache 是否能稳定导出 |

## 预期产物

- 可选：`cad-core/tools/probe_c8m2_subshapebinder_copyonchange.py`
- 可选：`cad-core/fixtures/c8m2/*.json`
- 可选：`cad-core/fixtures/c8m2/expected/*.freecad.json`
- 若 native lifecycle 不可观察：`known_gap.kind`、`route`、`delete_condition`、`freecadcmd_evidence`

## collector / probe 要求

- 输出必须记录 FreeCAD version / revision、source fixture、观测字段和不可观察字段。
- 若只观察 Python-visible property state，必须明确不能证明 copied-object cache。
- 输出不能来自 current `cad-core`。

## 必须回写的矩阵行

- `c8m2_copyonchange_oracle_plan.tsv`
- `c8m2_copyonchange_blocker_queue.tsv`
- `c8m2_copyonchange_backend_gap_classification.tsv`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C8M2-ORACLE|freecad_version|BindCopyOnChange|PartialLoad|known_gap|oracle_blocked|delete_condition|reopen_condition' cad-core/fixtures/c8m2 cad-core/tools docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线 2>/dev/null || true
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/矩阵/*.tsv
git diff --check
```

如果本机 FreeCADCmd / Qt 环境不可用，记录为 native oracle blocked 或手动采集前置，不得用 current `cad-core` output 替代。

验收通过后，将本文件重命名为 `6-26-22-24-【已实现】C8-M2-S3-native-CopyOnChange生命周期探针与blocker证据.md`。

## 非目标

- 不实现 executor。
- 不修改 capability supported status。
- 不放宽 C8-M1 expected comparator。
