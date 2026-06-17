# P6 MakerHistory S1 FreeCAD 源码候选矩阵

## 目标

建立 ShapeFix、DressUp、taper 和 transformed history 的 FreeCAD source authority 与 cad-core landing 候选。S1 只产出候选，不判定 supported / backendGap。

## FreeCAD 依据

| 轴 | 必查入口 | 关注点 |
| --- | --- | --- |
| ShapeFix mapper | `src/Mod/Part/App/TopoShape.h::MapperHistory` | `ShapeFix_Root` / `BRepTools_History` / `BRepTools_ReShape` history API |
| ShapeFix producer | `src/Mod/Part/App/AppPartPy.cpp::ShapeFixModule`、`src/Mod/Part/App/ShapeFix/*` | `RemoveSmallEdges`、`FixVertexPosition`、`ShapeFix_Shape` 对 context history 的写入 |
| DressUp cache | `src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getAddSubShape()` | `SupportTransform`、consecutive DressUp skip、AddSubShape compound |
| Fillet / Chamfer | `src/Mod/PartDesign/App/FeatureFillet.cpp`、`FeatureChamfer.cpp` | maker history、ShapeFix tolerance、RefineModel 后处理 |
| taper | `src/Mod/Part/App/ExtrusionHelper.cpp`、`src/Mod/Part/App/TopoShapeExpansion.cpp::MapperThruSections`、`src/Mod/PartDesign/App/FeatureExtrude.cpp` | `BRepOffsetAPI_ThruSections` history 和 Pad / Pocket / Part::Extrusion 复用 |
| transformed consumer | `src/Mod/Part/App/TopoShapeExpansion.cpp::makeElementTransform()`、`src/Mod/PartDesign/App/FeatureTransformed.cpp` | copy ElementMap、terminal split / deleted / merge 传播 |

## 候选 TSV 字段

`candidate_id	source_file	freecad_symbol	semantic_axis	source_evidence	cad_core_landing	scope_hint	next_step`

## seed 候选路由

- `P6MH-CAND-001` 到 `P6MH-CAND-003`：ShapeFix / ReShape producer、ShapeFix_Root mapper 和 ShapeFix wrapper producer 边界。
- `P6MH-CAND-004` 到 `P6MH-CAND-006`：DressUp AddSubShape、Fillet / Chamfer 和 Refine 传播。
- `P6MH-CAND-007` 到 `P6MH-CAND-008`：taper ThruSections history。
- `P6MH-CAND-009`：C ABI capabilities / tests 发布口径。
- `P6MH-CAND-010`：transformed consumer 对 AddSubShape / ElementMap / terminal history 的消费路径。

## S1 复核结论

- live baseline：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=6d35327fcb`，`git log -1=6d35327fcb fix: 收敛 P7 transformed 拓扑 oracle`；工作区已有大量 pre-existing dirty / untracked 内容，本步骤只回写 P6 S1 文档和 P6 矩阵。
- 候选矩阵已补成 10 条可追溯候选，覆盖 ShapeFix mapper / producer、DressUp AddSubShape、Fillet / Chamfer replacement、Refine propagation、taper geometry producer、taper ThruSections mapper、transformed consumer、capability / test 发布口径。
- FreeCAD 源码证据以 `TopoShape.h::MapperHistory`、`TopoShapeExpansion.cpp::MapperHistory / MapperThruSections / makeElementTransform`、`AppPartPy.cpp::ShapeFixModule`、`ShapeFix/*PyImp.cpp`、`FeatureDressUp.cpp::DressUp::getAddSubShape`、`FeatureFillet.cpp::Fillet::execute`、`FeatureChamfer.cpp::Chamfer::execute`、`ExtrusionHelper.cpp::makeElementDraft`、`FeatureExtrude.cpp`、`FeatureTransformed.cpp::Transformed::execute` 为候选入口。
- cad-core landing 已对齐到 `cad-core/src/part/topo_shape.cpp`、`shape_fix.cpp`、`refine_model.cpp`、`extrusion_helper.cpp`、`cad-core/src/part_design/feature_dress_up.cpp`、`feature_dress_up_support.h`、`cad-core/src/adapters/c_api/c_api.cpp` 和 `cad-core/tests/test_adapters.py`。
- 本步骤只更新 `source_candidate` 反链，不改 `current_status`、`scope_reason`、blocker、nonGoal 或 backend gap 分类；S2-S5 才裁决状态。

## 必须回写的矩阵行

- `p6_maker_history_source_candidates.tsv`：补齐 FreeCAD source evidence 和 cad-core landing。
- `P6MH-SCOPE-002` 到 `P6MH-SCOPE-006`：只能更新 source_candidate，不改状态。

## 验收标准

- `p6_maker_history_source_candidates.tsv` 至少包含 ShapeFix mapper / producer、DressUp、Fillet/Chamfer、Refine、taper、transformed consumer、capability / test 七类候选。
- 每个候选必须有 FreeCAD source path、FreeCAD symbol、cad-core landing 和 next_step。
- 候选行不得使用 `supported`、`backendGap` 等状态词替代证据。
- 执行：

```bash
rg -n "ShapeFix|MapperHistory|RemoveSmallEdges|DressUp::getAddSubShape|makeElementFillet|makeElementChamfer|MapperThruSections|makeElementTransform" src/Mod/Part/App src/Mod/PartDesign/App
awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P6-MakerHistory-ShapeFix-DressUp-Taper收敛主线/矩阵/p6_maker_history_source_candidates.tsv
for f in docs/FreeCAD几何生态迁移工程-细分/P6-MakerHistory-ShapeFix-DressUp-Taper收敛主线/矩阵/*.tsv; do
  awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"
done
git diff --check
```

## 非目标

- 不把 S1 候选当成实现状态。
- 不采 oracle。
- 不写 C++。
