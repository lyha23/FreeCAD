# C8-M1-S3 native oracle 批量采集与 expected 固化

## 目标

按 S2 oracle plan 批量采集 FreeCAD native expected，或记录 source-backed native blocker。S3 可以新增 collector、fixtures、expected 和 known_gap evidence；不得改 runtime C++ 主路径。

## 采集批次

| 批次 | 对应 oracle | 目标 |
| --- | --- | --- |
| ShapeBinder core | `C8M1-ORACLE-101..104` | whole / subshape / multi-subshape / TraceSupport |
| SubShapeBinder geometry | `C8M1-ORACLE-201..204` | support / MakeFace / Offset / Fuse / Refine |
| Downstream consumer | `C8M1-ORACLE-205` | binder as profile / Body chain consumer |
| ElementMap | `C8M1-ORACLE-206` | ShapeBinder / SubShapeBinder output ElementMap |
| lifecycle | `C8M1-ORACLE-301..302` | BindMode / CopyOnChange observability |

## 预期产物

- `cad-core/fixtures/c8m1/*.json`
- `cad-core/fixtures/c8m1/expected/*.freecad.json`
- 可选：`cad-core/tools/collect_c8m1_shapebinder_expected.py`
- 若 native lifecycle 不可观察：`known_gap.kind`、`route`、`delete_condition`、`freecadcmd_evidence`

## collector 要求

- expected 必须记录 FreeCAD version / revision、source fixture、shape summary、topology counts、bbox、volume 或 area、ElementMap / childShapes evidence。
- CopyOnChange / Frozen / Detached 若只能观察 Python-visible state，必须写明不可观察字段，不得补猜。
- collector 输出不能来自 current `cad-core`。

## 必须回写的矩阵行

- `c8m1_shapebinder_oracle_plan.tsv`：每个 oracle row 改为 collected / blocked / diagnostic。
- `c8m1_shapebinder_blocker_queue.tsv`：关闭 S3 可关闭 blocker，保留不可采 blocker。
- `c8m1_shapebinder_backend_gap_classification.tsv`：只把有 expected 且 current mismatch 的 row 交给 S4。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
find cad-core/fixtures/c8m1 -maxdepth 2 -type f | sort
rg -n 'freecad_version|PartDesign::ShapeBinder|PartDesign::SubShapeBinder|ElementMap|known_gap|native_oracle_blocked|C8M1-ORACLE' cad-core/fixtures/c8m1 docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/矩阵/*.tsv
git diff --check
```

如果本机 FreeCADCmd / Qt 环境不可用，记录为 native oracle blocked 或手动采集前置，不得用当前 `cad-core` output 替代。

验收通过后，将本文件重命名为 `6-26-16-19-【已实现】C8-M1-S3-native-oracle批量采集与expected固化.md`。

## 非目标

- 不实现 executor。
- 不修改 capability supported status。
- 不放宽 expected comparator 来容纳环境差异。
