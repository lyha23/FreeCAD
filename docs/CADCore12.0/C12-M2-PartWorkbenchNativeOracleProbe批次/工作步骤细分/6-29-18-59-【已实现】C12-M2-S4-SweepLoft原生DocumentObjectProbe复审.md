# C12-M2 S4 Sweep / Loft 原生 DocumentObject Probe 复审

## 目标

对更接近原生 DocumentObject / Part feature 的 Sweep 和 Loft 行执行 C12-M2 native probe 复审。S4 只产出 oracle candidate、current-covered 或 blocker，不写 implementation。

## Sweep 重点

- Location overload：`BRepOffsetAPI_MakePipeShellPyImp.cpp` 中 profile + location + contact/correction 参数。
- auxiliary spine / binormal / tolerance / support surface normal：优先复核已有 c5m10、c5m12、c6m4 fixture 与 C11-M1 retained 结论。
- Product boundary：只接受 request-local 输入；不接受 GUI session 或跨请求 native shape cache。

## Loft 重点

- selected subelement assignment：复核 `part-loft-subelement-product` 与 native-hidden 历史结论。
- 判断是否能通过稳定 FreeCADCmd / Part API 路径采出 native expected。
- 如果仍只能得到 native-hidden 或 wrapper-only 证据，写 blocker，不升级为 mismatch。

## 执行步骤

1. 按 S3 schema 执行或设计 Sweep / Loft native probe。
2. 将运行 artifact、FreeCAD 版本、输入、输出摘要和失败分类回写 probe matrix。
3. 若 stable expected 出现，补 current cad-core comparison path；若 current 已一致，标 `current_covered`。
4. 若 current mismatch 成立，先写为 `oracle_expected_ready`，不要直接写代码；S6 决定是否另开 implementation 包。
5. 若 probe 失败，区分 sandbox limitation、native-hidden、helper/wrapper、collector bug、product boundary rejection。

## 更新目标

- `矩阵/c12m2_partworkbench_native_oracle_probe_matrix.tsv`
- `矩阵/c12m2_partworkbench_native_oracle_backend_gap_classification.tsv`
- `矩阵/c12m2_partworkbench_native_oracle_blocker_queue.tsv`
- `矩阵/c12m2_partworkbench_native_oracle_scope_review_matrix.tsv`
- 必要时新增 S4 probe artifact。

## 验收命令

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次 docs/CADCore12.0/README.md
git diff --check
```

## 完成条件

Sweep 和 Loft 必须分别有明确结果：`oracle_expected_ready`、`current_covered`、`native_probe_blocked`、`product_boundary_rejected` 或 `retained_no_expected`。

## S4 执行结论

本步 live baseline：

```text
pwd=/Users/li/Chili3DProject/FreeCAD
HEAD=0ed591c456
git log -1 --oneline=0ed591c456 docs: 完成 C12-M2 S3 native probe 基线
git -c core.quotepath=false status --short -uall=<clean>
```

S4 新增 file-backed probe 脚本与三份 S3 schema artifact：

- `docs/temp/6-29-21-55-c12m2-s4-sweep-loft-native-probe.py`
- `docs/temp/6-29-21-55-c12m2-s4-sweep-native-probe-output.json`
- `docs/temp/6-29-21-55-c12m2-s4-sweep-options-native-probe-output.json`
- `docs/temp/6-29-21-55-c12m2-s4-loft-subelement-native-probe-output.json`

三份 artifact 均记录 FreeCADCmd path `/Users/li/.cargo/bin/freecadcmd`、FreeCAD `1.2.0 revision 20260519`、OCCT `7.8.1`、LibPack / LibPackVersion 空、运行 command、stdout/stderr、exit code、expected_summary、request-local judgement、current comparison path 和 conclusion。

Sweep final classification：`native_probe_blocked` for Location overload；`current_covered` for no-location auxiliary / binormal / tolerance controls。fresh Location artifact 中 `located_free_vertex`、`located_profile_owned_vertex`、`located_open_wire_profile`、call-order / tolerance variants 均在 `is_ready_before_build=true` / `status_before_build=0` 后于 build 阶段返回 `OCCError: NCollection_Array1::Value`；`plain_control` 可 build。no-location auxiliary / binormal / tolerance / combined controls 可 build，作为现有 c5m10 expected-backed current-covered context；带 Location 的 combined row 仍依赖 Location blocker。`C12M2-BLOCKER-101` 已关闭为 `closed_s4_native_probe_blocked`，S6 不得比较 Location current mismatch。

Loft final classification：`native_hidden`。fresh Loft artifact 中 object-level `Sections` control 可 build；`[(object, ["Edge1"]), ...]`、`[(object, "Edge1"), ...]` 和 stable-subname tuple assignment 均被 `App::PropertyLinkList` 拒绝，错误为 `TypeError: Type must be App.DocumentObject or None, not tuple`。`C12M2-BLOCKER-401` 已关闭为 `closed_s4_native_hidden`，c5m12/c6m7 继续只是 native-hidden / product-contract context，不能升级为 native expected。

本步未修改 `cad-core/src`、`cad-core/include`、fixtures expected、tests、adapters 或 capability wording，未运行全量 FreeCAD build。
