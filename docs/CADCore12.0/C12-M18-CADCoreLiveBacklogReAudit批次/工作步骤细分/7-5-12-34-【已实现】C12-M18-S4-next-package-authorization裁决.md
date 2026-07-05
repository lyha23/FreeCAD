# C12-M18 S4 next package authorization 裁决

根据 S2/S3 结果决定下一包类型。

## 必读

- `../README.md`
- `../7-5-12-29-C12-M18-CADCoreLiveBacklogReAudit批次方案.md`
- `../矩阵/c12m18_live_backlog_backend_gap_classification.tsv`
- `../矩阵/c12m18_live_backlog_next_package_authorization.tsv`
- `../矩阵/c12m18_live_backlog_blocker_queue.tsv`

## 操作

1. 查找是否存在 `mismatch_confirmed` 行。
2. 若存在，写出后续 implementation package 的最小完整语义批次、source authority、cad-core landing、fixtures/tests 和 non-goals。
3. 若缺 expected 或 product contract，输出 oracle/product-contract package，不授权 C++。
4. 若只有 frontend consumer work，输出 my-chili3d frontend sync package 建议。
5. 若三者都没有，发布 `no_code_backlog_gate`。
6. 验证后把本文件重命名为带 `【已实现】` 的同名文件。

## 非目标

- 不写后续包的所有文件；S4 只授权和描述分流。
- 不发明 source landing。
- 不把单一 fixture 当成最小完整语义批次。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M18-CADCoreLiveBacklogReAudit批次/矩阵/*.tsv
git diff --check
```

