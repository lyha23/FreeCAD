# 【已实现】C7-M7 S3 native oracle 采集与 expected 固化

## 目标

按 S2 候选批次采集 FreeCAD native oracle，或记录 native oracle blocker / diagnostic non-goal。S3 可以新增 oracle fixture / expected / known_gap；不改 runtime C++ 主路径。

## 必读文件

- S2 完成后的 C7-M7 README、方案和矩阵。
- `cad-core/tools/collect_freecad_expected.py`
- S2 指定的 fixture / expected / focused test 文件。
- S1 记录的 FreeCAD source authority。

## 执行要点

1. 记录 live baseline 和 C7-M7 queue。
2. 按 S2 的 oracle plan 执行 collector 或 probe。
3. 如果采到 native oracle，expected 必须记录 FreeCAD version、ElementMap / LinkSub / writeback / reference update evidence、source authority 和 deletion conditions。
4. 如果无法证明 native lifecycle，写 known_gap 和删除条件。
5. 如果明确超出无状态 CAD Core 边界，写 diagnostic non-goal。
6. 更新 S3 相关矩阵和方案。
7. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S4。

## S3 结论

- live 基线：执行时 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=7e7a99627e`（`7e7a99627e 文档：完成 C7-M7 S2 oracle 候选矩阵`），`git status --short -uall` 无输出；队列显示 S3-S6 pending。
- FreeCADCmd 可用：`/Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd --version` 返回 `FreeCAD 1.2.0 Revision: 20260519 (Git shallow)`。
- ORACLE-202：对 `part-import-brep.json`、`part-import-step.json`、`part-import-iges.json`、`app-link-imported-element-map-chain.json` 运行 collector 到 `/tmp/c7m7-s3-oracle/*.freecad.json`。四个 payload 均只包含 `bbox/freecad_version/object/reference/schema_version/topology_counts/volume`，没有 `named_shapes`、完整 Faces/Edges/Vertices `ElementMap`、`elements[*].sources` 或 stable reference update evidence；route=`native_oracle_blocked`，不更新 existing expected。
- ORACLE-302：对全部 `cad-core/fixtures/p8/app-link-show-element*.json` 运行 collector。`element-list-child-sync`、`element-list-copy-on-change-owned-child`、`element-list-owner-sync`、`toggle-off-sync` 均失败为 `failed to set property ElementList: Object attribute 'ElementList' is read-only`；其余成功 payload 只有 shape / `object_fields`，没有 owner `ElementList`、`ElementCount`、child `_LinkOwner` / `LinkedObject` / `LinkTransform` 或 create/claim/delete transaction evidence；route=`native_oracle_blocked`，不改 response contract。
- ORACLE-402：`app-link-full-sublist-external-tag.json` 与 `app-link-element-list-nested-label-sublist.json` collector 只生成 shape / `object_fields`；`app-link-multilevel-label-qualified-sublist.json` 在 native FreeCAD 中 `FaceLink: Link broken! Object: ChainLink`，`target object FaceLink has no shape`。补充 FreeCADCmd save/restore probe 后，before/after 仍只暴露本地 `LinkedObject` tuple 和 subvalue，`full-sublist` 的 external tag 在 native property 中不可见，未出现 file/stamp/hash、DocMap、restored FullSubList、ReferenceShadow 或 mapped postfix lifecycle evidence；route=`native_oracle_blocked`，cross-request/session state 仍保持 diagnostic boundary。
- ORACLE-203：保持 S2 的 `oracle_blocker`。STL import 仍是 `Mesh::Import` / mesh summary / `indexed_only` 边界，S3 未打开 mesh-specific oracle 包。
- S3 没有新增或修改 fixture、expected、test、runtime C++、adapter 或 collector；只同步文档和矩阵，S4 输入是上述 blocker / diagnostic 结论。

## 合法产物

- 可以新增或更新 `cad-core/fixtures/p8/*link*` / `*import*` / `*element*` 相关 fixture。
- 可以新增或更新 `cad-core/fixtures/p8/expected/*.freecad.json`。
- 可以新增 focused oracle tests。
- 不允许改 `cad-core/src/app/link.cpp`、`cad-core/src/part/*`、adapter 或 runtime 主路径。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线 docs/CADCore7.0/README.md
git diff --check
```

S3 具体 FreeCADCmd / unittest 命令以 S2/S3 矩阵记录为准。

## 完成标准

- 每个 S2 oracle candidate 都有 native oracle、native blocker 或 diagnostic non-goal 结论。
- S3 不改 C++ runtime 主路径。
- 队列推进到 S4。
