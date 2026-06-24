# 【已实现】C6-M4-S2 LocatedProfile 合同与 oracle 复采集

## 目标

冻结 located profile 的 request / response / diagnostics / oracle / delete condition，并把 S3-S6 的 scope、blocker、backendGap、fixture 路线写入矩阵。S2 可以复跑 FreeCADCmd wrapper probe；如果本机环境不能运行 FreeCADCmd，必须保留当前 checked-in blocker 证据，不得用 cad-core 输出替代 native oracle。

## S2 live baseline

- live repo：`/home/user/Chili3DProject/FreeCAD`
- S2 live HEAD：`6596fe5ed8`
- S2 live last commit：`6596fe5ed8 docs: 完成 C6-M4 S1 source oracle 矩阵冻结`
- 起始工作区：`git -c core.quotepath=false status --short -uall` 无输出。
- 队列入口：`step_goal_queue.py .../工作步骤细分 --format markdown` 在本步骤执行前显示 S2-S6 pending，S2 是当前首个未实现步骤。

## 合同范围

| 项 | C6-M4 合同 |
| --- | --- |
| request | `Objects[].Properties.SectionOptions[].Location` 为单个 vertex link；`WithContact`、`WithCorrection` 为 bool。multi-subname、missing target、invalid subname 和非 vertex subshape 都是参数错误。 |
| response metadata | `advanced.sections[].location.target/subname`、`with_contact`、`with_correction`、`topo_naming_history=maker_history:pipeshell`。 |
| valid located product | 只有 S3 落地后才能输出 shape / named_shapes；S2 只能冻结目标合同。 |
| invalid diagnostics | missing target、invalid subname、non-vertex、multi-subname、bad bool 必须返回 locatable diagnostics。 |
| FreeCADCmd oracle | 当前 `build()` 阶段 `OCCError: NCollection_Array1::Value` 是 notCollected evidence，不是 supported expected。 |

## 复采集结果

- `command -v FreeCADCmd freecadcmd freecadcmd-daily` 命中 `/home/user/.local/bin/FreeCADCmd` 与 `/home/user/.local/bin/freecadcmd`。
- `FreeCADCmd --version` 返回 `FreeCAD 1.2.0 Revision: 20260519 (Git shallow)`，与 checked-in expected 记录的 `1.2.0 revision 20260519` 对齐。
- 复跑 `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/docs/temp/6-22-04-46-c5m13-s2-sweep-location-combined-probe.py`：`located_free_vertex`、`located_profile_owned_vertex`、`located_profile_coordinate_free_vertex`、`located_spine_owned_vertex`、`located_open_wire_profile` 和 call-order variants 均为 `is_ready_before_build=true`、`status_before_build=0` 后在 `build` 报 `OCCError: NCollection_Array1::Value`。
- 同次复跑中 `plain_control` 与 `combined_no_location_control` 仍可 build 并返回 Shell `faces=4, edges=12`；`combined_*_add` variants 只要包含 `Location` overload 仍在 `build` 报同一错误。
- 结论：不刷新 expected，不把 c5m10 known_gap 改成 supported；`C6M4-ORC-001/002` 保留 current known_gap，`C6M4-ORC-101/102/103` 作为 planned non-parity c6m4 fixtures 交给 S3/S5。

## 复采集规则

- 优先复用 `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/docs/temp/6-22-04-46-c5m13-s2-sweep-location-combined-probe.py`。
- 只在本机 FreeCADCmd/LibPack/OCCT 与 expected 采集基线可用时刷新 oracle。
- 若 FreeCADCmd 仍失败：保持 c5m10 known_gap，并把 C6-M4 product contract 与 FreeCAD parity 明确分开。
- 若 FreeCADCmd 返回稳定 shape：S2 更新 oracle matrix，S3 可以按 expected-backed route 实现；仍需保留 no-output-fixup 纪律。

## 矩阵更新

- `scope_review_matrix.tsv`：关闭 `C6M4-SCOPE-000`，冻结 `C6M4-SCOPE-101/102/201/301`。
- `input_contract_matrix.tsv`：写入 located profile、diagnostics、combined 预备合同。
- `blocker_queue.tsv`：`C6M4-BLK-101` 必须有 FreeCADCmd evidence 与 S3 close condition。
- `backend_gap_classification.tsv`：区分 `notCollected` 与 `backendGap`，不能混写。
- `oracle_fixture_matrix.tsv`：列出 c5m10 guard 与 c6m4 product fixtures。

## 矩阵冻结结果

- `C6M4-SCOPE-101` 冻结为 `notCollected_retained_S2`；`C6M4-SCOPE-102` 冻结为 `backendGap_frozen_for_S3`，S3 可继续实现 non-parity product path。
- `C6M4-CAT-101` 保留 `notCollected_retained_S2`；`C6M4-CAT-102` 保留 `backendGap` 并指向 S3。
- `C6M4-IN-101/102/103` 分别覆盖 vertex link + metadata、missing/invalid/non-vertex/multi-subname diagnostics、bad bool diagnostics。
- `C6M4-BLK-101` 的 delete condition 冻结为：FreeCADCmd 对 located overload 返回稳定 shape，或 C6-M4 product contract 已完整发布并保留 non-parity provenance。
- `C6M4-ORC-001/002` 保留 current known_gap；`C6M4-ORC-101/102/103` 指向 planned non-parity c6m4 fixtures。

## 验收标准

通过条件：

- `C6M4-IN-101/102/103` 存在并覆盖 located profile request、metadata、missing/invalid/non-vertex/multi-subname diagnostics 与 bad bool diagnostics。
- `C6M4-ORC-001/002` 保留 current known_gap guard；`C6M4-ORC-101/102/103` 指向 S3/S5 待新增 planned non-parity c6m4 fixtures。
- `C6M4-BLK-101` 写清 delete condition：FreeCADCmd stable shape 或 C6-M4 product contract 完整发布并保留 non-parity provenance。
- 没有 `supported` 状态跳过 S3/S5。

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C6M4-IN-101|C6M4-ORC-101|C6M4-BLK-101|notCollected|backendGap|part_sweep_located_profile_freecadcmd_wrapper_build_blocker' docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest.test_c5m10_part_sweep_located_profile_contract_keeps_freecadcmd_blocker cad-core.tests.test_p8_features.CadCoreP8FeatureTest.test_c5m10_part_sweep_combined_advanced_contract_and_diagnostic_priority
for f in docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
git diff --check -- docs/CADCore6.0
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/工作步骤细分 --format markdown
```
