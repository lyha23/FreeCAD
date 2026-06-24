# C6-M5-S2 Filling 合同与 oracle 复采集

## 目标

把 S0/S1 确认的 helper oracle blocker 拆成两类：继续保留为 `notCollected` 的 native helper 证据，以及下一轮必须实现的 CAD Core request-local product contract。S2 可以补 oracle probe 或复核现有 expected，但不能把崩溃 / timeout 的 native helper 当成实现阻断。

## 输入

- S0 live baseline 结果。
- S1 FreeCAD source / wrapper source scan。
- 既有 c5m8 / c5m12 / c5m13 Filling fixtures 与 `test_p8_features.py`。
- `矩阵/c6m5_filling_surface_support_order_param_*`。

## 合同分组

- Surface：`Surface` 或 initial surface face link 是 request-local helper DTO 字段，目标是稳定 object fields、diagnostics、shape summary 或明确 product-backed fallback。
- Supports / Orders：支持 C0/G1/G2 order 解析、support face 解析、invalid locatable diagnostics，目标不依赖 mutable wrapper。
- Explicit params：覆盖 PtsOnCurve、Anisotropy、TolG1/TolG2、MaxSegments、all params，并保留 Degree / NumIter / Tol2d+Tol3d / MaxDegree 子集。
- Non-boundary support/order：将 non-boundary edge/wire/face/vertex 的 support/order 解析与 builder 路径纳入产品合同。

## 必须回写的矩阵行

- `IN-101`、`IN-102`、`IN-201`、`IN-202`：输入合同状态。
- `BLK-101`、`BLK-102`、`BLK-201`、`BLK-202`：blocker 是否转 implementation-ready。
- `GAP-101`、`GAP-102`、`GAP-201`、`GAP-202`：backend gap 分类。
- `ORC-101`、`ORC-102`、`ORC-201`、`ORC-202`：新 fixture 路由。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 -m unittest cad-core/tests/test_p8_features.py -k filling
git diff --check -- docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线/矩阵/*.tsv
```

如果本地 Python unittest 的 `-k` 不可用，改用 `python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest` 或直接指定 Filling 相关测试名。验收通过后，将本文重命名为 `6-24-16-22-【已实现】C6-M5-S2-Filling合同与oracle复采集.md`。

## 非目标

- 不要求重新跑不稳定 FreeCADCmd 作为发布前置。
- 不用 crash / timeout expected 覆盖 CAD Core 输出。
- 不删除未实现的 capability `remaining_gaps`。
