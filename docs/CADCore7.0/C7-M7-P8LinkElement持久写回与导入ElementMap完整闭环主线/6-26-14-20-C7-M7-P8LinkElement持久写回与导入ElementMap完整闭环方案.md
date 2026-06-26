# C7-M7 P8 LinkElement 持久写回与导入 ElementMap 完整闭环方案

## 背景

C7-M6 已把 P8 Assembly Joint 完整 placement / constraint 方向收口为 expected-backed / oracle-blocked / no backendGap。P8 当前更核心的剩余后端语义是稳定引用闭环：导入 shape 仍使用 indexed `NamedShape`，Link / LinkSub / LinkGroup / LinkElement 虽已覆盖请求内 display、拾取、alias retag、history 传播和前端图更新建议，但完整 FreeCAD Link 账本、`ShowElement=true` 持久写回事务、cross-document hash / postfix 生命周期和复杂多层 LinkSub 链还没有迁移。

C7-M7 的核心不是先扩大 Link 功能声明，而是按 FreeCAD source authority 和 checked-in native expected 审计 current `cad-core` 是否存在真实 mismatch。没有 source-backed native oracle 时，只能保持 `oracle_pending` / `oracle_blocked` / `diagnostic_non_goal`，不能直接改 C++。

## 目标

- 从 P8 live 文档、FreeCAD App / Part source、current `cad-core` Link / import / topo 实现和 fixtures/tests 中抽出稳定引用闭环候选。
- 形成最小完整语义批次：source authority、fixture / expected 候选、collector 命令、focused tests、capability / docs 发布口径。
- 若 native oracle 证明 current `cad-core` mismatch，打开 S5 implementation gate。
- 若 current `cad-core` 已匹配，发布 `already_closed_expected_backed`。
- 若缺 oracle 或 lifecycle 不可复现，发布 `oracle_blocked` 或 `diagnostic_non_goal`，不改 C++。

## 最小完整语义批次

| 批次 | 代表项 | 判定方式 |
| --- | --- | --- |
| Import ElementMap | BREP / STEP / IGES / STL import subshape stable alias、source evidence、ElementMap / reference update | FreeCAD import / Part source + current p8 import fixtures |
| ShowElement writeback | LinkElement create / claim / sync / delete、LinkGroup / ElementList owner sync、ShowElement true->false 收回 | FreeCAD `LinkBaseExtension` source + current Link fixtures |
| Complex LinkSub chain | numeric index、object name、`$Label`、mapped postfix、`FullSubList`、cross-document hash / postfix | FreeCAD `getSubObject()` / property link source + native expected |
| Graph update boundary | `documentObjectUpdates`、`elementReferenceUpdates`、stateless request graph boundary | current response DTO + C ABI capability |
| Publication gate | supported / oracle_blocked / diagnostic_non_goal / backend_gap routes | S4 parity result |

## 实施纪律

- S0/S1 不改 C++、fixtures、expected 或 tests；只冻结状态、source 和 current coverage。
- S2 只输出 oracle 候选和最小批次；不能把 existing expected-backed rows 误写成 active gap。
- S3 可以新增 oracle fixture / expected / known_gap，但不能从 current `cad-core` 输出倒推 expected。
- S4 只做 parity 和 gate 裁决；只有 route=`backend_gap_requires_implementation` 才打开 S5 code edit gate。
- S5 若实现，必须落到当前正式路径：`cad-core/src/app/link.cpp`、`cad-core/src/app/property_links.cpp`、`cad-core/src/part/*`、`cad-core/src/runtime/element_reference_update.cpp`、`cad-core/src/mesh/*`、ElementMap / NamedShape 或 C ABI capability 路径，不允许 adapter 输出端修正、fixture 名称分支或 LinkSub 猜测特判。

## 步骤

### S0 live baseline 与 P8 引用边界冻结

已冻结当前 live 起点、C7-M1..M6 队列、P8 Link / import 已发布边界和 fixture/test/capability 基线。执行基线为 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=7be2d4e937`（`7be2d4e937 docs: 完成 C7-M6 S6 发布闸门`）；开始状态只包含 `docs/CADCore7.0/README.md` modified 和本 C7-M7 文档包 untracked 文件。C7-M1 到 C7-M6 队列均为空，C7-M7 从 S0 起步并推进到 S1。

S0 冻结后的边界：

- already-covered：基础 Link / LinkSub / LinkGroup / LinkElement display、ElementList / ElementCount / ShowElement 请求内 `documentObjectUpdates` 建议、FullSubList / mapped postfix / cross-document alias 更新、Link retag terminal / merge history、plain group 展开、BREP / STEP / IGES import `history_partial` ElementMap、`app-link-imported-element-map-chain` 的 imported Link chain coverage。
- remaining：完整导入 shape ElementMap、STL import `indexed_only` 后续、完整 FreeCAD Link 账本、ShowElement 持久写回事务、复杂多层 LinkSub lifecycle、完整 cross-document hash / postfix 生命周期。
- diagnostic / non-goal：GUI / ViewProvider / Workbench、前端同步协议、跨请求 backend cache / persistent BREP、Worker / WASM / Web 产品化、Assembly Joint blocked rows、从 current `cad-core` 输出倒推 expected。

S0 不采 oracle、不新增 fixture/expected/test、不改 C++；`C7M7-BLOCKER-000` / `C7M7-GATE-000` 已关闭。

### S1 FreeCAD source 与 current coverage 复核

已复核 FreeCAD source authority：

- `src/App/Link.cpp`、`src/App/Link.h`
- `src/App/DocumentObject.cpp`、`src/App/DocumentObject.h`
- `src/App/PropertyLinks.cpp`
- `src/Mod/Part/App/PartFeature.cpp`
- `src/Mod/Part/App/FeaturePartImportBrep.cpp`、`FeaturePartImportStep.cpp`、`FeaturePartImportIges.cpp`
- `src/Mod/Part/App/PropertyTopoShape.cpp`
- `src/Mod/Part/App/TopoShape*.cpp`
- `src/Mod/Mesh/App/FeatureMeshImport.cpp`

S1 结论：

- FreeCAD source authority：`LinkBaseExtension::update()` 负责 ElementCount / ElementList / ShowElement 和 LinkElement create / claim / sync / delete；`DocumentObject::getSubObject()` 通过 Link extension 处理 numeric index、child object、`$Label`、collapsed `_iN`、linked target name/label 和 group / child-cache recursion；`PropertyXLink*` 保存 object + subvalue 并参与引用更新。
- Import / ElementMap source authority：BREP / STEP / IGES import 读入 `TopoShape` 后由 `PropertyPartShape::setValue()` remap / restore `ElementMap`；`Mesh::Import` 是独立 mesh path；`PartFeature::getTopoShape()`、`PropertyTopoShape` 和 `TopoShapeMapper` 是 linked/imported `NamedShape` / ElementMap retag 的依据。
- Current `cad-core` coverage：实际路径是 `cad-core/src/app`、`cad-core/src/part`、`cad-core/src/runtime`、`cad-core/src/mesh`，不是旧假设 `cad-core/src/features`、`cad-core/src/document`、`cad-core/src/topo`。已覆盖 request-local Link display / alias / FullSubList / mapped postfix / LinkElement / LinkGroup / ElementList / ElementCount / ShowElement `documentObjectUpdates`、`elementReferenceUpdates`、BREP / STEP / IGES `history_partial` import、STL `indexed_only` import 和 imported Link chain fixtures/tests。
- S2 输入池：完整 imported-shape `ElementMap` / stable reference lifecycle、ShowElement LinkElement / LinkGroup persistent writeback transaction、复杂多层 LinkSub / cross-document hash-postfix lifecycle。S1 未采 oracle、未新增 fixture/expected/test、未改 C++；S1 blocker/gate 已关闭，implementation gate 仍关闭。

### S2 oracle 候选矩阵与批次裁决

已完成。执行时 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=d6f62daad5`（`d6f62daad5 文档：完成 C7-M7 S1 源码覆盖复核`），开始状态干净，队列显示 S2-S6 pending。

S2 将 S1 source / coverage 结果裁成最小完整语义批次：

- `already_covered`：既有 Link / LinkSub / LinkGroup / LinkElement display 与 alias、`FullSubList` / mapped postfix、terminal / merge history、imported Link chain、BREP / STEP / IGES `history_partial` import ElementMap，以及 ShowElement request-local `documentObjectUpdates` apply-stable graph。
- `oracle_candidate`：完整 BREP / STEP / IGES imported-shape `ElementMap` / stable reference lifecycle；ShowElement `LinkElement` / `LinkGroup` persistent writeback transaction；复杂多层 `LinkSub` / cross-document hash-postfix save/restore lifecycle。
- `oracle_blocker`：STL complete Part-style ElementMap，因为 FreeCAD `Mesh::Import` 走 mesh path，不经过 `PropertyPartShape::setValue()` / TopoShape ElementMap。
- `backend_gap_candidate`：仅作为 S4 parity 后的候选状态保留在 `C7M7-GATE-601`，S2 不把任何行升级为 `backend_gap_requires_implementation`。
- `diagnostic_non_goal`：GUI / ViewProvider / Workbench、frontend sync protocol、cross-request backend cache / persistent BREP、Worker / WASM / Web 产品化。

每个 `oracle_candidate` 已在 `oracle_plan.tsv` 写清 fixture/probe 输入、FreeCAD source authority、expected 字段、S3 collector/probe 命令或 blocker 判定和 focused test 名称。S2 已关闭 `C7M7-BLOCKER-201` 与 S2 分类 gate；未采 native oracle，未新增或修改 fixture/expected/test，未改 C++，S5 implementation gate 仍关闭。

### S3 native oracle 采集

已完成。执行时 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=7e7a99627e`（`7e7a99627e 文档：完成 C7-M7 S2 oracle 候选矩阵`），开始状态干净。对 S2 的 `oracle_candidate` 批次采集 FreeCAD expected / probe，未得到可固化为 checked-in native expected 的完整 lifecycle payload。

| route | 条件 | 输出 |
| --- | --- | --- |
| `native_oracle_collected` | FreeCAD native fixture 可复现 ElementMap / LinkSub / writeback evidence | expected / evidence JSON |
| `native_oracle_blocked` | collector、FreeCADCmd、document lifecycle 或 source instrumentation 不可观察 | known_gap JSON |
| `diagnostic_non_goal` | GUI、cross-request state、front-end-only protocol 等超边界 | diagnostic expected 或 docs row |

S3 结果：

- ORACLE-202：`part-import-brep`、`part-import-step`、`part-import-iges`、`app-link-imported-element-map-chain` collector 均成功，但 payload 只有 shape summary，没有完整 `named_shapes` / Faces-Edges-Vertices ElementMap / `elements[*].sources` / stable reference update evidence。route=`native_oracle_blocked`。
- ORACLE-302：全部 `app-link-show-element*.json` 已探测。显式 `ElementList` 事务相关 fixture 在 native collector 中失败为 `ElementList` 只读；其余成功 payload 只含 shape / `object_fields`，没有 owner / child 持久图字段。route=`native_oracle_blocked`，不把 request-local `documentObjectUpdates` 改写成 backend persistence。
- ORACLE-402：seed fixtures 的 collector/save-restore probe 只能观察本地 `LinkedObject` tuple；`full-sublist` 的 external tag 未在 native property 中出现，`multilevel-label` native shape broken，未采到 file/stamp/hash、DocMap、restored `FullSubList`、ReferenceShadow 或 mapped postfix lifecycle。route=`native_oracle_blocked`，cross-request/session 依赖保持 diagnostic boundary。
- ORACLE-203：没有打开 mesh-specific oracle 包，继续 `oracle_blocker`。

S3 没有新增或修改 fixture、expected、test、collector、adapter 或 runtime C++；S4 只允许基于上述 blocker / diagnostic 结论做 parity / gate 裁决。

### S4 cad-core parity 与 implementation gate

如果 S3 得到 native oracle，则比较 current `cad-core`：

- 匹配：`already_closed_expected_backed`
- 不匹配：`backend_gap_requires_implementation`
- 缺 oracle：`oracle_blocked`
- 超边界：`diagnostic_non_goal`

S4 必须写清 S5 是否允许改 C++、允许文件范围、focused tests 和 non-goals。

### S5 实现或 no-code 发布

若 S4 打开 code gate，S5 实现顺序固定：

1. 在 `app` / `part` / `runtime` / `mesh` / ElementMap / NamedShape 正式路径补语义。
2. 写 focused tests，约束 ElementMap、NamedShape alias、`documentObjectUpdates`、`elementReferenceUpdates`、diagnostics、capability 和 expected parity。
3. 删除临时 diagnostic 或保持 known_gap 时同步矩阵。

若 S4 未打开 code gate，S5 只做 no-code publication closure。

### S6 release gate

运行本包 queue、TSV、trailing whitespace、`git diff --check`。若 S5 改 C++、fixtures、expected、tests 或 capability，再跑 focused P8 tests 和 `cmake --build build`。

## 验收分层

### 本轮短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线 docs/CADCore7.0/README.md
git diff --check
```

### 实现短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

只有 S5 改 C++、expected、tests 或 capability 时，这组才是必须执行项。
