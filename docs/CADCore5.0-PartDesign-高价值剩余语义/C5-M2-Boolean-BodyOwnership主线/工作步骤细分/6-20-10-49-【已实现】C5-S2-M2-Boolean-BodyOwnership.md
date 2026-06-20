# 【已实现】C5-S2 M2 Boolean / Body ownership

## 目标

补齐 PartDesign Boolean 的 Body ownership 压力主线，覆盖 AllowCompound、multi-solid policy、multi-tool Group、BaseFeature / Tip replay 和 failure diagnostics。

## 必读

- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M2-Boolean-BodyOwnership主线/6-20-10-48-C5-M2-Boolean-BodyOwnership方案.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M2-Boolean-BodyOwnership主线/矩阵/boolean_body_scope.tsv`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M2-Boolean-BodyOwnership主线/矩阵/boolean_body_blocker_queue.tsv`
- `src/Mod/PartDesign/App/FeatureBoolean.cpp`
- `src/Mod/PartDesign/App/Body.cpp`

## 工作内容

- 记录 FreeCAD Boolean / Body 调用链，特别是 Group tools、BaseFeature、AllowCompound、multiple solids error 和 PreviewShape。
- 补 native expected 或稳定 diagnostics，不从 cad-core 输出反推 expected。
- 实现或更新 cad-core Boolean / Body ownership、topo history、tests 和 capability metadata。
- 保持 LinkStage3-only Compound / Section 为 non-goal 或 unsupported Type diagnostic。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义 cad-core
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 完成条件

- AllowCompound true/false、multi-tool ownership 和 failure diagnostic 均有 fixture 或 capability 证据。
- Boolean capability 不 overclaim Compound / Section 或完整 LinkStage3-only 行为。

## 实施结果

- FreeCAD 调用链已记录到 `../6-20-10-48-C5-M2-Boolean-BodyOwnership方案.md`：`Boolean::execute()` 只支持 `Fuse/Cut/Common`，`Group` 顺序进入 `makeElementBoolean()`；`Feature::singleSolidRuleMode()` 从所属 Body 的 `AllowCompound` 决定是否提取唯一 solid；`Body::execute()` 用 Tip shape 替换 Body shape。
- supported：C4 `Fuse/Cut/Common` first slice 继续通过；`AllowCompound=true` 多 solid Boolean / Body 输出 `occt_compound` 并有 native expected；multi-tool `Group` 顺序 fixture 有 native expected；Body Tip replacement、subshape map 和 `maker_history:boolean` 保持。
- diagnostic-backed：`AllowCompound=false` 多 solid 输出 `multiple_solids_disallowed`，带 `object=BooleanRejected`、`property=AllowCompound`、`stage=part_design.single_solid_rule`、`target=MainBody`；缺失 tool、非 solid tool 和 `Section` unsupported Type 均有 focused diagnostics。
- deferred / non-goal：LinkStage3-only `Compound` / `Section` 仍按 `C5-NG-005` 保持 non-goal / unsupported Type diagnostic，未进入 supported。
