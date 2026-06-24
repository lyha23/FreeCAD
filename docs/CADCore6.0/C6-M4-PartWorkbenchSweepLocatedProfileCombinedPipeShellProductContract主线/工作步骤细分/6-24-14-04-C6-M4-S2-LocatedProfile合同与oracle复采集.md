# C6-M4-S2 LocatedProfile 合同与 oracle 复采集

## 目标

冻结 located profile 的 request / response / diagnostics / oracle / delete condition，并把 S3-S6 的 scope、blocker、backendGap、fixture 路线写入矩阵。S2 可以复跑 FreeCADCmd wrapper probe；如果本机环境不能运行 FreeCADCmd，必须保留当前 checked-in blocker 证据，不得用 cad-core 输出替代 native oracle。

## 合同范围

| 项 | C6-M4 合同 |
| --- | --- |
| request | `Objects[].Properties.SectionOptions[].Location` 为单个 vertex link；`WithContact`、`WithCorrection` 为 bool。 |
| response metadata | `advanced.sections[].location.target/subname`、`with_contact`、`with_correction`、`topo_naming_history=maker_history:pipeshell`。 |
| valid located product | 只有 S3 落地后才能输出 shape / named_shapes；S2 只能冻结目标合同。 |
| invalid diagnostics | missing target、non-vertex、multi-subname、bad bool 必须返回 locatable diagnostics。 |
| FreeCADCmd oracle | 当前 `build()` 阶段 `OCCError: NCollection_Array1::Value` 是 notCollected evidence，不是 supported expected。 |

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

## 验收标准

通过条件：

- `C6M4-IN-101/102/103` 存在并覆盖 located profile request、metadata、invalid diagnostics。
- `C6M4-ORC-001/002` 保留 current known_gap guard；`C6M4-ORC-101/102` 指向 S3/S5 待新增 c6m4 fixtures。
- `C6M4-BLK-101` 写清 delete condition：FreeCADCmd stable shape 或 C6-M4 product contract 完整发布。
- 没有 `supported` 状态跳过 S3/S5。

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C6M4-IN-101|C6M4-ORC-101|C6M4-BLK-101|notCollected|backendGap|part_sweep_located_profile_freecadcmd_wrapper_build_blocker' docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线
for f in docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
git diff --check -- docs/CADCore6.0
```
