# C11-M2 S3 FreeCADCmd 原生 Filling helper 复采集

## 目标

在当前 FreeCAD / OCCT 基线下复采集 `Part.makeFilledFace(...)` helper native oracle。S3 只判断 stable / notCollected / dependency-retained；不比较 cad-core，不改 C++。

## 复采集范围

| scope | case | 复采集目标 |
| --- | --- | --- |
| `C11M2-SCOPE-101` | Surface initial face | `Part.makeFilledFace(shapes, surface=Face)` 是否返回 stable `shape_summary` 和 initial-surface evidence。 |
| `C11M2-SCOPE-102` | Supports/Orders G1/G2 | boundary edge + support face + G1/G2 order 是否返回 stable `shape_summary`。 |
| `C11M2-SCOPE-201` | Explicit params all | PtsOnCurve、Anisotropy、TolG1、TolG2、MaxSegments、all params 是否返回 stable `shape_summary`。 |
| `C11M2-SCOPE-202` | Non-boundary support/order | non-boundary edge/wire/face/vertex with support/order 是否返回 stable `shape_summary`。 |
| `C11M2-SCOPE-203` | Direct wrapper controls | `LoadInitSurface`、`Add(edge, face, order, isBound)`、`Set*Param` 只作为 helper 对照，不作为 request-local expected。 |

## 输出要求

- 输出 JSON 写入 `docs/temp/6-29-12-27-c11m2-s3-filling-native-helper-probe-output.json`。
- FreeCAD version 写入 `docs/temp/6-29-12-27-c11m2-s3-freecadcmd-version.txt`。
- 每个 case 必须包含 `status`、`freecad_version`、`occt_version`、`shape_summary` 或 failure evidence、是否可作为 native expected。
- `notCollected` 必须保留原始错误类型、阶段、shell exit 或 timeout 信息。

## 必须回写的矩阵行

- `C11M2-SCOPE-101`
- `C11M2-SCOPE-102`
- `C11M2-SCOPE-201`
- `C11M2-SCOPE-202`
- `C11M2-SCOPE-203`
- `C11M2-BLOCKER-301..304`
- `C11M2-ORC-101..401`
- `C11M2-CAT-101..202`
- `C11M2-VAL-301..304`

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
command -v FreeCADCmd || command -v freecadcmd || command -v freecadcmd-daily
python3 - <<'PY'
import json
from pathlib import Path
p = Path('docs/temp/6-29-12-27-c11m2-s3-filling-native-helper-probe-output.json')
if p.exists():
    data = json.loads(p.read_text())
    assert isinstance(data, dict)
PY
rg -n 'C11M2-SCOPE-101|C11M2-SCOPE-102|C11M2-SCOPE-201|C11M2-SCOPE-202|stable_native_expected|notCollected|dependency_retained' docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/矩阵
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/矩阵/*.tsv
git diff --check
```

通过条件：

- 每个 S3 scope 明确为 `stable_native_expected`、`notCollected`、`blocked_by_environment` 或 `dependency_retained`。
- 只有 stable `shape_summary` 才能进入 S4 parity comparison。
- Direct wrapper control 不能单独成为 CAD Core request-local expected。
- 未修改 C++、fixtures、tests 或 capability。

## 非目标

- S3 不运行 cad-core comparison。
- S3 不改 expected。
- S3 不把 helper crash expected 写成支持状态。
