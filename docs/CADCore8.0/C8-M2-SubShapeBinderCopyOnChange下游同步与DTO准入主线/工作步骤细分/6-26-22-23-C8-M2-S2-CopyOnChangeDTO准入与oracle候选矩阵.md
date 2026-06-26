# C8-M2-S2 CopyOnChange DTO 准入与 oracle 候选矩阵

## 目标

把 S1 source / current coverage 复核转成 CopyOnChange DTO 准入、native oracle 候选、下游同步合同、blocker route、non-goal 和 implementation gate。S2 不采 oracle，不改 C++。

## 分类规则

| route | 使用条件 | 后续 |
| --- | --- | --- |
| `already_supported` | C8-M1 已 expected-backed 支持 | S4/S5 同步发布 |
| `sync_required` | 下游需要消费 C8-M1 合同 | S4 写同步方案 |
| `oracle_candidate` | FreeCAD source 明确且 native probe 可能观察 | S3 采集 |
| `known_gap_retained` | 当前 known_gap 证据仍成立 | S5/S6 发布边界 |
| `backend_gap_candidate` | source 明确但缺 native expected | S3 后由 S6 裁决 |
| `diagnostic_non_goal` | GUI/session/persistent cache/Rust 下游 | S5 发布边界 |

## 必须纳入同一轮的候选

- C8-M1 ShapeBinder / SubShapeBinder capability 下游同步。
- C8-M1 fixtures / expected / diagnostics 下游同步。
- CopyOnChange Disabled / Enabled / Mutated property-state probe。
- PartialLoad allow-partial native observability probe。
- Full temporary-document copied-object cache blocker。
- Request-local DTO product decision。

## 必须回写的矩阵行

- `C8M2-ORACLE-101..103`
- `C8M2-SYNC-101..103`
- `C8M2-BG-101..201`
- `C8M2-BLOCKER-201`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C8M2-ORACLE|C8M2-SYNC|sync_required|oracle_candidate|known_gap_retained|backend_gap_candidate|diagnostic_non_goal|CopyOnChange|PartialLoad' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/矩阵/*.tsv
git diff --check
```

验收通过后，将本文件重命名为 `6-26-22-23-【已实现】C8-M2-S2-CopyOnChangeDTO准入与oracle候选矩阵.md`。

## 非目标

- 不把 `backend_gap_candidate` 当作 S6 implementation gate。
- 不因 C8-M1 已支持 SubShapeBinder 主路径就关闭 CopyOnChange lifecycle blocker。
