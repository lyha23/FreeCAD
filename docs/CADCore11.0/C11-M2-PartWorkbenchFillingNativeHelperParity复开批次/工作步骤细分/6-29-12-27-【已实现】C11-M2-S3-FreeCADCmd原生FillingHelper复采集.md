# 【已实现】C11-M2 S3 FreeCADCmd 原生 Filling helper 复采集

## 目标

在当前 FreeCAD / OCCT 基线下复采集 `Part.makeFilledFace(...)` helper native oracle。S3 只判断 `stable_native_expected` / `notCollected` / `blocked_by_environment` / `dependency_retained`；不比较 cad-core，不改 C++、fixtures、tests、collectors 或 capability。

## live baseline

本轮 S3 执行基线：

```text
pwd=/home/user/Chili3DProject/FreeCAD
HEAD=f293d4ed12
git log -1 --oneline=f293d4ed12 docs: 完成 C11-M2 S2 范围准入路由
git -c core.quotepath=false status --short -uall=<clean>
```

S3 起点工作区干净；本步只更新 C11-M2 S3 文档、总入口 / 索引必要状态、S3 指定矩阵行和 `docs/temp` probe 证据。

## FreeCAD 依据

- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp`

源码复核结论：

- `makeFilledFace()` 的 `kwd_list` 声明 `surface` / `supports` / `orders` / params，但当前可见解析调用没有把 `supports` 变量传入，也只把 `pySurface` 存入局部 `surface`，未写回 `params.surface`。
- `TopoShape::makeElementFilledFace()` 仍是正确 builder 主路径：按 `BRepFillingParams` 构造 `BRepOffsetAPI_MakeFilling`，可 `LoadInitSurface(face)`，按 edge/support/order/non-boundary source 调 `Add(...)`，最后 `Build()`。
- direct wrapper 的 `LoadInitSurface`、`Add(edge, face, order, isBound)` 和 `Set*Param` 只能作为诊断对照，不是 request-local `Part.makeFilledFace` expected。

## 本轮 FreeCADCmd 复采集

入口检查：

```bash
cd /home/user/Chili3DProject/FreeCAD
command -v FreeCADCmd || command -v freecadcmd || command -v freecadcmd-daily
# /home/user/.local/bin/FreeCADCmd
```

probe 文件：

- `docs/temp/6-29-12-27-c11m2-s3-filling-native-helper-probe.py`

运行方式：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 docs/temp/6-29-12-27-c11m2-s3-filling-native-helper-probe.py
```

该 probe 外层逐 case 启动：

```text
/home/user/.local/bin/FreeCADCmd -c exec(compile(open('/home/user/Chili3DProject/FreeCAD/docs/temp/6-29-12-27-c11m2-s3-filling-native-helper-probe.py', encoding='utf-8').read(), 'c11m2_s3_filling_native_helper_probe.py', 'exec'))
```

逐 case 单独进程是为了把 native crash / timeout 记录成该 case 的 raw evidence，避免一个 helper 崩溃吞掉整份 JSON。

原始证据：

- `docs/temp/6-29-12-27-c11m2-s3-filling-native-helper-probe-output.json`
- `docs/temp/6-29-12-27-c11m2-s3-freecadcmd-version.txt`

版本记录：

| 项 | 本轮结果 |
| --- | --- |
| FreeCADCmd | `/home/user/.local/bin/FreeCADCmd` |
| FreeCAD | `1.2.0 revision 20260519`；`BuildRevision=20260519 (Git shallow)`；`BuildRevisionDate=2026/05/19 20:59:45` |
| OCCT | `Part.OCC_VERSION=7.8.1` / `FreeCAD.ConfigGet("OCC_VERSION")=7.8.1` |
| LibPack | 当前 AppImage runtime 未暴露 LibPack：`LibPack=""`、`LibPackVersion=""`、`FreeCADLibs=""`、`env_FREECAD_LIBPACK=None` |
| runtime path | `AppHomePath=/tmp/.mount_FreeCA*/usr//` |

## S3 复采集结论

| scope | 结果 | 证据 | 后续路由 |
| --- | --- | --- | --- |
| `C11M2-SCOPE-101` Surface initial face | `notCollected` | `helper_surface_initial_face` 在 `Part.makeFilledFace([wire], surface=face)` 阶段返回 `TypeError: argument 2 must be , not Part.Face`，shell exit `0`，无 stable `shape_summary`。direct wrapper `LoadInitSurface` 能稳定返回 Face shape summary，但只作为 diagnostic control。 | 不创建 C11-M2 native expected；S4 不比较该 row；不打开 backend gap。 |
| `C11M2-SCOPE-102` Supports/Orders G1/G2 | `notCollected` | `helper_support_order_boundary_g1` 与 `helper_support_order_boundary_g2` 均返回 `TypeError: argument 2 must be , not Part.Face`，shell exit `0`。direct wrapper `Add(edge, face, order, True)` G1/G2 稳定，但只作为 diagnostic control。 | 不创建 C11-M2 native expected；S4 不比较该 row；不打开 backend gap。 |
| `C11M2-SCOPE-201` explicit params blocked subset | `notCollected` | `helper_params_pts_on_curve`、`helper_params_tol_g1_g2`、`helper_params_max_segments`、`helper_params_all` 均无 stable payload，stderr 记录 `Program received signal SIGSEGV`；`helper_params_anisotropy` timeout 45s。 | `PtsOnCurve`、`Anisotropy`、`TolG1/TolG2`、`MaxSegments` 和 all params 不进入 S4；C5-M13 `Degree` / `NumIter` / `Tol2d+Tol3d` / `MaxDegree` 子集继续只是 expected-backed subset。 |
| `C11M2-SCOPE-202` non-boundary support/order | `notCollected` | `helper_nonboundary_support_order_g1` 与 `helper_nonboundary_support_order_g2` 均无 stable payload，stderr 记录 `Program received signal SIGSEGV`。 | 不创建 C11-M2 native expected；C5-M12 no-support/order 子集继续只是 expected-backed subset；不打开 backend gap。 |
| `C11M2-SCOPE-203` direct wrapper controls | `dependency_retained` | default boundary、Degree、NumIter、Tol2d/Tol3d、MaxDegree、non-boundary no support/order、`LoadInitSurface`、`Add` support/order、`Set*Param` 和 constructor params controls 均有 stable control shape summary。 | 只作为 direct wrapper diagnostic；不能成为 request-local helper expected；交给 S5 关闭 non-goal/protocol boundary。 |

S3 没有任何稳定 `Part.makeFilledFace(...)` helper `shape_summary` 可作为 native expected。因此本步不创建 `cad-core/fixtures/c11m2`，不改 C6-M5 / C5-M13 / C5-M12 expected，不改 capability，不改 cad-core C++ / tests。S4 只能消费 `notCollected` / `dependency_retained` 结论，不能基于本步打开 implementation row。

## S3 矩阵回写

- `c11m2_part_workbench_filling_native_helper_scope_review_matrix.tsv`：`C11M2-SCOPE-101/102/201/202` 标为 `notCollected_s3_reconfirmed`，`C11M2-SCOPE-203` 标为 `dependency_retained_s3_diagnostic_control_only`。
- `c11m2_part_workbench_filling_native_helper_blocker_queue.tsv`：`C11M2-BLOCKER-301..304` 关闭为 `closed_s3_notCollected`；notCollected 不升级为 backend gap。
- `c11m2_part_workbench_filling_native_helper_oracle_fixture_matrix.tsv`：`C11M2-ORC-101/102/201/202` 标为 `notCollected_s3_reconfirmed`；`C11M2-ORC-301` 记录 comparison gated by S3 notCollected；`C11M2-ORC-401` 保留 S6 release gate。
- `c11m2_part_workbench_filling_native_helper_backend_gap_classification.tsv`：`C11M2-CAT-101/102/201/202` 均为 no S3 backend decision。
- `c11m2_part_workbench_filling_native_helper_validation_matrix.tsv`：补齐 `C11M2-VAL-301..304` 的 FreeCADCmd、probe、classification grep 和 S3 后队列检查。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
command -v FreeCADCmd || command -v freecadcmd || command -v freecadcmd-daily
python3 - <<'PY'
import json
from pathlib import Path
p = Path('docs/temp/6-29-12-27-c11m2-s3-filling-native-helper-probe-output.json')
data = json.loads(p.read_text())
assert isinstance(data, dict)
PY
rg -n 'C11M2-SCOPE-101|C11M2-SCOPE-102|C11M2-SCOPE-201|C11M2-SCOPE-202|stable_native_expected|notCollected|blocked_by_environment|dependency_retained' docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/矩阵
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次 docs/CADCore11.0/README.md docs/temp/6-29-12-27-c11m2-s3-filling-native-helper-probe.py docs/temp/6-29-12-27-c11m2-s3-freecadcmd-version.txt || true
git diff --check
```

通过条件：

- 每个 S3 scope 明确为 `notCollected` 或 `dependency_retained`；本轮没有 `stable_native_expected` helper row。
- 只有 stable helper `shape_summary` 才能进入 S4；本轮没有可进入 S4 的 native expected。
- Direct wrapper control 不能单独成为 CAD Core request-local expected。
- 未修改 C++、fixtures、tests、collectors 或 capability。

## 非目标

- S3 不运行 cad-core comparison。
- S3 不创建 expected。
- S3 不把 helper crash / timeout / TypeError 写成支持状态。
- S3 不声明 native `Part::FilledFace` DocumentObject parity。
