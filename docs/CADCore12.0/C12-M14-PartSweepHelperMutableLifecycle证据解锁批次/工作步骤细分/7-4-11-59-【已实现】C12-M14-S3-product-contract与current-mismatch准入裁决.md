# 【已实现】C12-M14 S3 product contract 与 current mismatch 准入裁决

## 目标

基于 S2 native helper probe，裁决每条 helper lifecycle 行是否进入 `implementation_authorized`、`product_contract_only` 或 `no_code_retained_blocker`。

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`fa3fde076d`。
- `git log -1 --oneline`：`fa3fde076d 文档：关闭 C12-M14 S2 helper native probe`。
- `git -c core.quotepath=false status --short -uall`：无输出。
- S3 开始前队列：S3-S5 pending，第一项为本步骤。

## S2 证据输入

- Native probe artifact：`docs/temp/7-4-12-15-c12m14-helper-lifecycle-native-probe-output.json`。
- FreeCAD / OCCT baseline：`1.2.0 revision 20260519` / `7.8.1`。
- `C12M14-ORACLE-101` remove：`remove` before add、after add before build、after build、remove/readd ordering 均有 stable payload 或 stable `PipeShell` diagnostic。
- `C12M14-ORACLE-102` firstShape/lastShape：unbuilt、failed build 与 build success 三态均稳定；未 build / failed build 是 null-shape diagnostic，build success 返回 stable shapes。
- `C12M14-ORACLE-103` generated：before build、after build、unknown profile 均返回 stable empty list payload。
- `C12M14-ORACLE-104` simulate：pre-build、post-build、count=0 均返回 stable two-item list payload；unready case 返回 stable `PipeShell` diagnostic。
- `C12M14-ORACLE-105` remove/readd/simulate/build：`simulate(2)` 先返回 list，但后续 `build()` 与 `shape()` 均抛 `NCollection_Sequence::ChangeValue`，保留 native instability，不能写成 FreeCAD native parity。

## source/current audit

- FreeCAD helper binding：`src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp` 直接调用 `Delete(s)`、`FirstShape()`、`LastShape()`、`Generated(s)`、`Simulate(nbsec, list)`，并把 `Standard_Failure` 映射为 `PartExceptionOCCError`。
- FreeCAD wrapper no-mix：`src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` 只读取 `Sections/Spine/Solid/Frenet/Transition/Linearize` 并调用 `makeElementPipeShell(...)`，没有 helper lifecycle 方法。
- cad-core current：`cad-core/src/part/part_sweep.cpp::executePartSweep()` 只发布 wrapper / advanced metadata；没有 `remove/firstShape/lastShape/generated/simulate` lifecycle response 字段。
- cad-core shared builder：`cad-core/src/part/topo_shape_expansion.cpp::makeElementPipeShellFromSources()` 内部 `Simulate(2)` 仅服务 open-shell cap/sewing，不是 Python helper `simulate(nbsec)` parity。
- 因此 current mismatch 以 source/current response 字段缺口成立，不以 final mesh output 判定。

## 裁决

| row | 裁决 | 依据 | S4 边界 |
| --- | --- | --- | --- |
| `C12M14-ORACLE-101` remove | `implementation_authorized` | stable native diagnostics/payload + current 缺 helper remove lifecycle response | 可实现 source-backed expected，不改变 plain `Part::Sweep` wrapper。 |
| `C12M14-ORACLE-102` firstShape/lastShape | `implementation_authorized` | stable 三态 native diagnostics/payload + current 缺 first/last response | 可实现 source-backed expected。 |
| `C12M14-ORACLE-103` generated | `implementation_authorized` | stable native list payload + current 缺 generated response | 可实现 source-backed expected，不从 final mesh 倒推 history。 |
| `C12M14-ORACLE-104` simulate | `implementation_authorized` | stable standalone simulate payload/diagnostic + current 缺 helper simulate response | 可实现 standalone helper simulate response；不得混同 cap/sewing internal `Simulate(2)`。 |
| `C12M14-ORACLE-105` remove/readd/simulate/build | `product_contract_only` | native `NCollection_Sequence::ChangeValue` instability，不能作为 parity | 仅允许按 `../7-4-13-26-C12-M14-helper-lifecycle-request-local产品契约.md` 实现 CAD Core request-local product contract，必须标注 `native_parity=false`。 |

## 矩阵回写

- `c12m14_helper_lifecycle_scope_matrix.tsv`：remove、first/last、generated、standalone simulate 标为 `implementation_authorized`；新增 remove/readd/simulate/build contract row 标为 `product_contract_only`。
- `c12m14_helper_lifecycle_oracle_matrix.tsv`：`ORACLE-101..104` 标为 `implementation_authorized` 与 source/current audit mismatch；`ORACLE-105` 标为 `product_contract_only`。
- `c12m14_helper_lifecycle_blocker_queue.tsv`：`BLOCKER-401` 关闭；`BLOCKER-302` 保留为 native instability，但由 request-local product contract 接管产品路径。
- `c12m14_helper_lifecycle_validation_matrix.tsv`：记录 S3 source/current audit 与文档短跑验证。

## 结论

- S3 已解锁 S4：S4 可为 `ORACLE-101..104` 做 source-backed implementation，并可为 `ORACLE-105` 做 CAD Core request-local product contract implementation。
- `ORACLE-105` 不是 FreeCAD native parity；任何 fixture、expected 或 response wording 必须保留 `contract_provenance=cad_core_product_contract_non_parity` / `native_parity=false` 等等价标记。
- 本步骤未修改 C++、fixtures、expected、tests、adapter 或 capability source。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次 docs/temp docs/CADCore12.0/README.md
git diff --check
```

## 非目标

- 不修改 C++。
- 不新增 implementation fixture。
- 不修改 capability source 或 adapter wording。
