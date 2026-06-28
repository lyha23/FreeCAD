# 【已实现】C10-M2-S4 Hole 生产者 History 专项复审

## 目标

复核 Hole 生产者 history：`findHoles()` 的 profile-source / tool-face mapper history，ModelThread pipe-shell tool，head-cut dynamic resource rows，以及最终 Body subtractive cut history。S4 只在 expected-backed mismatch 出现时打开 `backend_gap_candidate`。

## FreeCAD 依据

- `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp::Hole::execute()`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp::Hole::findHoles()`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp::Hole::makeThread()`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp::Hole::readCutDefinitions()`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp::Hole::updateThreadDepthParam()`

## 范围

| scope | 内容 | 默认路由 |
| --- | --- | --- |
| `C10M2-SCOPE-201` | `findHoles()`、profile-source、protoHole / protoThread tool-face mapper history | `oracle_candidate` |
| `C10M2-SCOPE-202` | ModelThread、head-cut dynamic resources、Body subtractive cut history | `oracle_candidate` |
| `C10M2-SCOPE-301` | Hole 后续 split / deleted / old reference diagnostics | 只记录给 S5 |

## 必须检查的 current 证据

- `part_design.hole.history.covered` 中 `find_holes_make_shape_with_element_map`、`profile_source_tool_face_mapper_history`、`model_thread_tool_face_history`、`subtractive_body_cut_history` 是否仍和 tests 对齐。
- `cad-core/tests/test_p7_features.py` 中 Hole expected-backed rows 是否约束 profile source、point profile、head-cut、ModelThread 和 Body cut history。
- `cad-core/src/part_design/feature_hole.cpp` 是否仍是 Hole producer history 正式落点；Body cut 传播是否在 `body.cpp` 消费。
- 如果新增 native expected，必须使用带 `AttachmentSupport` / `Support` 的 Profile sketch，不把 placement-only CAD Core 等价值 fixture 升格成 FreeCAD native golden。

## 必须回写的矩阵行

- `C10M2-SCOPE-201`
- `C10M2-SCOPE-202`
- `C10M2-BLOCKER-401`
- `C10M2-CAT-102`
- 若发现 current mismatch，S6 code landing 只能指向 `feature_hole.cpp`、`body.cpp`、`topo_shape.*`、`element_map.cpp`、focused tests / expected。

## S4 复核结果

- live baseline：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=ed7119a8fb`（`docs: 完成 C10-M2 S3 DressUp History 复审`），起点工作区干净。
- `C10M2-SCOPE-201=expected_backed_no_gap`：FreeCAD `Hole::findHoles()` 对 profile Edge / Vertex source 先 `mapper.populate(Part::MappingStatus::Modified, baseshape, TopoShape(protoHole).getSubTopoShapes(TopAbs_FACE))`，再调用 `makeShapeWithElementMap(protoHole, mapper, {baseshape})`；current `cad-core/src/part_design/feature_hole.cpp` 在 `namedShapeForHoleToolHistory()` / `holeHistoryFreezeJson()` 中发布 `find_holes_make_shape_with_element_map`、`profile_source_tool_face_mapper_history`、`hole_find_holes:profile_source` 和 mapper_history evidence，`cad-core/tests/test_p7_features.py` 用 `hole-supported-threaded-dynamic-iso2009`、`hole-point-profile`、`hole-supported-point-counterbore` 等 checked-in expected 覆盖 Edge / Vertex profile-source 映射，未发现 current mismatch。
- `C10M2-SCOPE-202=expected_backed_no_gap`：FreeCAD `Hole::execute()` 在 `Threaded && ModelThread` 时把 `protoHole` / `protoThread` 组为 compound，`Hole::makeThread()` 构建 pipe-shell solid，`readCutDefinitions()` 读取 `Resources/Hole/*.json` 动态 head-cut，`updateThreadDepthParam()` 约束 thread depth；current `feature_hole.cpp`、`body.cpp` 和 focused tests 已覆盖 `model_thread_tool_face_history`、`model_thread_compound_tool_shape`、`threaded_model_thread_head_cut_native_oracle`、`subtractive_body_cut_history`、Body `history_consumed:generated_modified` / `terminal_history:split_deleted`，`hole-supported-model-thread-metric`、`hole-supported-model-thread-counterbore`、`hole-supported-threaded-dynamic-din7984` 和 `hole-supported-threaded-dynamic-iso2009` checked-in expected 均未暴露 mismatch。
- `C10M2-BLOCKER-401=closed_s4`：S4 没有打开 Hole C++ / tests / fixtures / expected / capability 改动；S6 只需发布 no-code no-gap release gate。
- `C10M2-CAT-102=no_gap`：Hole producer-history 分类从 `oracle_candidate` 收口为 no-code no-gap。跨 Body / transformed / Link retag 的 split / deleted / stale old-reference diagnostic 仍留给 `C10M2-SCOPE-301` 和 S5，不在 S4 声明 supported。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "Hole|find_holes|profile_source|model_thread|head_cut|subtractive_body_cut|makeShapeWithElementMap|element_history_status" cad-core/tests/test_p7_features.py cad-core/src/runtime/capability_contract.cpp cad-core/src/part_design/feature_hole.cpp cad-core/src/part_design/body.cpp
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M2-PartDesignDressUpHoleTopoHistory批次/矩阵/*.tsv
git diff --check
```

S4 验收必须给出 `C10M2-SCOPE-201` / `C10M2-SCOPE-202` 的明确状态：`no_gap`、`expected_backed_no_gap`、`current_mismatch_candidate`、`backend_gap_candidate`、`diagnostic_retained` 或 `notCollected`。不能只写“待定”。

验收通过后，S4 文件已重命名为 `6-28-22-59-【已实现】C10-M2-S4-Hole生产者History专项复审.md`。

## 验收记录

- `rg -n "Hole|find_holes|profile_source|model_thread|head_cut|subtractive_body_cut|makeShapeWithElementMap|element_history_status" ...`：通过，确认 Hole source / tests / capability / Body history 证据可定位。
- `step_goal_queue.py ... --format markdown`：通过，S4 重命名后下一项为 S5。
- TSV 字段数检查：通过。
- trailing whitespace 检索：无匹配，退出码 1 为预期通过。
- `git diff --check`：通过。

## 非目标

- 不把 Hole `ModelThread` 内部 PipeShell 的 full Part surface family 纳入本包。
- 不把无 support 的 placement-only Hole fixture 当 native golden。
- 不从 current output 倒推 FreeCAD `makeShapeWithElementMap()` 行为。
