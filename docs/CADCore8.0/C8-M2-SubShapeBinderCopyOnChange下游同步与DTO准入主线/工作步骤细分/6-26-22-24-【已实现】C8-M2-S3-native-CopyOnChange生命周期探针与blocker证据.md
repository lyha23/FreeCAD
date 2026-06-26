# 【已实现】C8-M2-S3 native CopyOnChange 生命周期探针与 blocker 证据

## 目标

按 S2 oracle plan 探测 FreeCAD native `BindCopyOnChange` / `PartialLoad` 生命周期，并把 full temporary-document copied-object cache 的 retained blocker evidence 入库。S3 只新增 native probe / fixture / expected 和文档矩阵，不改 runtime C++ 主路径。

## live 基线

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `HEAD=12be750a30`（`12be750a30 docs: 完成 C8-M2 S2 DTO 准入矩阵`）
- S3 开始时 `git -c core.quotepath=false status --short -uall` 为空，未发现无关 dirty 文件。
- 队列首项是本 S3 文件；S4/S5/S6 仍 pending。

## native probe 产物

- Probe：`cad-core/tools/probe_c8m2_subshapebinder_copyonchange.py`
- Source fixture：`cad-core/fixtures/c8m2/subshape-binder-copyonchange-lifecycle-probe.json`
- Native evidence：`cad-core/fixtures/c8m2/expected/subshape-binder-copyonchange-lifecycle-probe.freecad.json`
- FreeCADCmd：`/home/user/.local/bin/freecadcmd`
- Native version：`freecad_version=1.2.0 revision 20260519`，`freecad_revision=20260519`
- Evidence route：`native_evidence_collected_with_known_gap_blocker`

## S3 结论

| oracle | S3 结果 | 关键证据 | 边界 |
| --- | --- | --- | --- |
| `C8M2-ORACLE-101` | `native_property_state_collected` | Disabled / Enabled / Mutated Python-visible `BindCopyOnChange`；源对象动态 `CopyOnChange` 属性可被 Binder 复制；修改 Binder 复制属性后 Enabled 进入 Mutated | 只能证明 property-state 和 mutation trigger，不证明 full cache supported |
| `C8M2-ORACLE-102` | `native_partialload_property_state_collected` | `PartialLoad=True`；`Support` type 为 `App::PropertyXLinkSubList`；`Support` status 含 `AllowPartial`、`ReadOnly` | Python API 未暴露 `getAllowPartial` 类 DTO；只能作为 allow-partial property-state 证据 |
| `C8M2-ORACLE-103` | `oracle_blocked` retained | Mutated recompute 后 `FreeCAD.listDocuments()` 可见 `_tmp_binder`；`_CopiedLink` 指向 copied `SupportBox001` | `_CopiedObjs` private vector、`copyObject` dependency order、`recomputeFeature(true)` ElementMap 生命周期仍不可导出为稳定 request-local DTO |

这次 native evidence 比 C8-M1 property-state seed 更强，因为它实际触发了 Enabled -> Mutated，并观察到 `_tmp_binder` 和 `_CopiedLink`。但它仍不能关闭 full temporary-document cache known gap：只观察 Python-visible property state、隐藏 link 或临时文档名，不等于支持 `_CopiedObjs/_tmp_binder/copyObject/recomputeFeature` 全生命周期。

## known gap / blocker

- `kind=c8m2_copy_on_change_full_temporary_document_cache_blocker`
- `route=oracle_blocked`
- `delete_condition`：只有 native probe 能导出不依赖 backend session / persistent temporary document cache 的稳定 request-local DTO，且 S6 明确打开 implementation gate，才删除本 blocker。
- `reopen_condition`：若 FreeCADCmd 或 native C++ probe 能导出 copied-object cache contents 和 mutation lifecycle 的确定性证据，重开 S3。
- `C8M2-BLOCKER-301` 已关闭为 evidence/blocker 入库；S4/S5/S6 blocker 不关闭。

## 矩阵回写

- `c8m2_copyonchange_oracle_plan.tsv`：`C8M2-ORACLE-101` / `102` 写为 S3 native property-state collected；`C8M2-ORACLE-103` 写为 S3 retained `oracle_blocked`。
- `c8m2_copyonchange_blocker_queue.tsv`：仅关闭 `C8M2-BLOCKER-301`，并指向 probe 与 expected evidence。
- `c8m2_copyonchange_backend_gap_classification.tsv`：`C8M2-BG-101` / `102` 保持候选；`C8M2-BG-301` 保持 `known_gap_retained_after_S3`。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C8M2-ORACLE|freecad_version|freecad_revision|source_fixture|observed_fields|unobservable_fields|BindCopyOnChange|PartialLoad|known_gap|oracle_blocked|delete_condition|reopen_condition' cad-core/fixtures/c8m2 cad-core/tools docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线 2>/dev/null || true
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线 docs/CADCore8.0/README.md cad-core/tools cad-core/fixtures/c8m2 2>/dev/null || true
git diff --check
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/工作步骤细分 --format markdown
git status --short -uall
```

验证通过后，将本文件重命名为 `6-26-22-24-【已实现】C8-M2-S3-native-CopyOnChange生命周期探针与blocker证据.md`，索引链接同步更新。

## 非目标

- 不实现 executor。
- 不修改 capability supported status。
- 不放宽 C8-M1 expected comparator。
- 不改 Rust。
- 不把 full temporary-document cache 写成 supported。
