# 【已实现】C51-S2 Boolean Compound / Section 实现

## 目标

将 C5 的 `C5-NG-005` 重开为实现范围：为 PartDesign Boolean 的 `Compound` / `Section` 明确产品语义、native expected、cad-core executor 和 topology history。

## 必读

- `src/Mod/PartDesign/App/FeatureBoolean.cpp`
- `src/Mod/PartDesign/App/Body.cpp`
- `src/Mod/PartDesign/App/Feature.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `docs/CADCore5.1-PartDesign-剩余deferred语义实现/矩阵/cadcore51_non_goal_registry.tsv`

## 工作内容

- 先关闭矩阵 child blockers：`C51-BLK-211` Compound、`C51-BLK-212` Section、`C51-BLK-213` Body ownership/diagnostics；对应 oracle 为 `C51-ORC-211`..`213`，validation 为 `C51-VAL-211`..`213`。
- 复核 FreeCAD 当前 `FeatureBoolean.cpp::TypeEnums` 与 disabled `Compound` / `Section` 分支，明确 cad-core 产品化语义是否直接采用 `TopoShape::makeElementBoolean(Compound/Section)`。
- 为 Compound / Section 采集 native expected 或写产品化 fixture；若 FreeCAD UI 不暴露，必须在方案里标明来源和差异。
- 实现 Group order、Body Tip replacement、AllowCompound、maker history 和 failure diagnostics。
- 删除或更新 C5 capability 中 `C5-NG-005` 的 non-goal 表述，改成 C5.1 supported / exact diagnostic。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.1-PartDesign-剩余deferred语义实现 cad-core
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 完成条件

- Boolean `Compound` / `Section` 有明确 runtime 行为和 focused tests。
- 不再把这两个 Type 归入 C5 non-goal。

## 实现记录

- FreeCAD 依据：`FeatureBoolean.cpp::Boolean::execute()` 的 `TypeEnums` 只暴露 `Fuse/Cut/Common`，`Compound/Section` 分支仍被注释；C5.1 产品化来源明确限定为 `TopoShapeExpansion.cpp::TopoShape::makeElementBoolean()` 的 `OpCodes::Compound` / `OpCodes::Section` maker path，以及 `Feature.cpp::singleSolidRuleMode()/getSolid()`、`Body.cpp::Body::execute()` 的 Body ownership 规则。
- cad-core 落点：`part/topo_shape.*` 新增 source-preserving Compound build 并扩展 Section 多输入；`part_design/feature_boolean.cpp` 支持 `Type=Compound/Section`，保留 Group/BaseFeature 顺序和 maker history；`part_design/body.cpp` 对 Section 非 solid Tip 返回 `partdesign_body_tip_non_solid`；`c_api.cpp` 只同步 capability/diagnostic metadata。
- product policy：Compound 是 Body replacement shape，受 `AllowCompound` 单实体规则约束；Section 是 standalone edge/wire result，不进入 Body Tip，无交集返回 `no_intersection`。
- checked-in fixtures：`cad-core/fixtures/c51m2/partdesign-boolean-{compound-body-tip,compound-disallowed,section-standalone,section-body-tip-diagnostic,section-no-intersection}.json`；success expected 为 `expected/partdesign-boolean-{compound-body-tip,section-standalone}.freecad.json`，文件内标明这是 C5.1 product contract，不伪称 FreeCAD PartDesign UI native expected。
- 旧 unsupported-Type fixture 已从 `Type=Section` 改为 `Type=Fragments`；Section 不再作为 C5-NG-005 broad non-goal。
- 验证通过：`cmake --build build`；`python3 -m unittest tests.test_p7_features`；`python3 -m unittest tests.test_expected_fixtures`；`python3 -m unittest tests.test_adapters`。
