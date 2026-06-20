# C5-M4 Datum Attachment 引用稳定主线总入口

## 目标

评估并推进非 GUI Datum AttachmentSupport / MapMode 语义：选定产品需要的 AttachEngine map modes，补 expected-backed support 或保持稳定 unsupported diagnostics。

## 当前状态

C5-S4 已关闭为 diagnostic gate：existing Datum placement/link slice 继续 supported；本包不发布 selected AttachEngine map mode support；active `AttachmentSupport`、`MapMode`、`AttachmentOffset`、`MapReversed`、`MapPathParameter` 均以 `c5m4` focused fixture 覆盖 locatable diagnostic；下游引用 attached Datum 时稳定跳过且不产生引用写回。

## 必读文件

- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M4-DatumAttachment-引用稳定主线/6-20-10-48-【已实现】C5-M4-DatumAttachment引用稳定方案.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M4-DatumAttachment-引用稳定主线/矩阵/datum_attachment_scope.tsv`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M4-DatumAttachment-引用稳定主线/矩阵/datum_attachment_blocker_queue.tsv`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M4-DatumAttachment-引用稳定主线/工作步骤细分/6-20-10-49-C5-S4-M4-DatumAttachment引用稳定.md`

## 队列

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M4-DatumAttachment-引用稳定主线/工作步骤细分 --format markdown
```
