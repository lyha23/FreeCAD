# 【已实现】C5-M20 PartWorkbenchFillingPreciseBlockerRecovery 主线

状态：`done_precise_blocker_source_audited`

本包承接 C5-M13 已收口后仍保留的 `Part.makeFilledFace(...)` / `BRepOffsetAPI_MakeFilling` Filling precise blockers。范围只包括 `Surface`、boundary support/order G1/G2、`PtsOnCurve`、`Anisotropy`、`TolG1+TolG2`、`MaxSegments`、all explicit params、non-boundary support/order；不重开 C5-M14 至 C5-M19，也不扩张到完整 Part surface family。

## 目标

- 复核 C5-M13 的 `C5M13-SCOPE-301`、`C5M13-FLD-301~303`、`C5M13-BLK-301` 和现有 c5m8/c5m13 fixtures / expected。
- 用本机 FreeCADCmd 1.2.0 revision 20260519 逐 case probe 重新分类剩余 Filling blocker。
- 只有 `Part.makeFilledFace(...)` request-local helper 代表场景能稳定返回 shape summary 时才补 collector、fixture、expected、C++、focused tests 和 capability。
- 本轮 probe 证明请求范围内没有可稳定采集的 `Part.makeFilledFace(...)` geometry expected；因此不新增 fixture / expected / cad-core executor 代码。
- direct `Part.BRepOffsetAPI.MakeFilling` wrapper control 可 build，但只作为低层 owner 对照，不作为 cad-core persistent wrapper lifecycle 或 `Part.makeFilledFace` expected 替代。

## 当前基线

- C5-M13 已将 `Degree`、`NumIter`、`Tol2d+Tol3d`、`MaxDegree` 单字段 representatives 变成 expected-backed。
- C5-M13 仍保留 Surface/support/order/G2、`PtsOnCurve`、`Anisotropy`、`TolG1+TolG2`、`MaxSegments`、all-params、non-boundary support/order precise blockers。
- 当前 `cad-core/src/part/part_filling.cpp` 已解析 request-local `Surface`、`Supports`、`Orders`、constructor params 与 non-boundary constraints，并在 `cad-core/src/part/topo_shape_expansion.cpp::makeElementFilledFaceFromSources()` 走 `BRepOffsetAPI_MakeFilling`。
- 当前 `cad-core/tools/collect_freecad_expected.py` 仍不会把 `Surface` / `Supports` / `Orders` 当 supported expected 采集；这是正确的，因为本轮 native helper probe 未产生稳定 `Part.makeFilledFace` expected。

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| Python helper kwargs | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()` | 暴露 `surface` / `supports` / `orders` / constructor kwargs，随后调用 `TopoShape::makeElementFilledFace()` |
| Filling builder | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()` | 构造 `BRepOffsetAPI_MakeFilling`，可选 `LoadInitSurface`，对 boundary / non-boundary edge 调用 `maker.Add(edge, support, order, IsBound)` |
| Direct wrapper | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp` | 暴露 mutable Python builder `loadInitSurface()`、`add()`、`build()`、`shape()`；cad-core 不把该生命周期持久化 |

## cad-core 落点

| 层 | 当前代码落点 | 本包结论 |
| --- | --- | --- |
| collector | `cad-core/tools/collect_freecad_expected.py` | 不新增 supported expected route；blocked rows 保持 known_gap / diagnostic evidence |
| executor | `cad-core/src/part/part_filling.cpp` | 当前 request-local DTO/source evidence 保持；不新增 fixture-specific fallback |
| geometry/topo | `cad-core/src/part/topo_shape_expansion.cpp` | direct OCCT builder path 已存在；不因 direct wrapper control 成功伪造 helper expected |
| capability | `cad-core/src/runtime/capability_contract.cpp` | remaining gaps 继续保留，但证据由 C5-M20 package 精化为逐字段 blocker |
| tests | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_adapters.py` | 现有 focused tests 保持；本轮无新增 expected-backed case |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-23-19-18-【已实现】C5-M20工作步骤总入口.md` | S0-S6 队列索引 |
| S0 | `工作步骤细分/6-23-19-19-【已实现】C5-M20-S0-liveBlocker与声明口径冻结.md` | 冻结 C5-M13 后的 Filling precise blocker 口径 |
| S1 | `工作步骤细分/6-23-19-20-【已实现】C5-M20-S1-FreeCAD源码候选矩阵.md` | FreeCAD helper / builder source 候选 |
| S2 | `工作步骤细分/6-23-19-21-【已实现】C5-M20-S2-scope准入与blocker矩阵.md` | scope / blocker / nonGoal / backendGap 分类 |
| S3 | `工作步骤细分/6-23-19-22-【已实现】C5-M20-S3-SurfaceSupportOrder专项复审.md` | Surface 与 support/order G1/G2 复审 |
| S4 | `工作步骤细分/6-23-19-23-【已实现】C5-M20-S4-Params专项复审.md` | 剩余 constructor params 与 all-params 复审 |
| S5 | `工作步骤细分/6-23-19-24-【已实现】C5-M20-S5-WrapperControl与requestLocal边界复审.md` | direct wrapper control 与 request-local 边界 |
| S6 | `工作步骤细分/6-23-19-25-【已实现】C5-M20-S6-Oracle实现与发布闸门.md` | 发布闸门和无代码落点结论 |
| probe 记录 | `docs/temp/6-23-19-17-C5-M20-fillingPreciseBlockerProbe记录.md` | FreeCADCmd 逐 case reclassification |
| source candidates | `矩阵/c5m20_filling_precise_blocker_source_candidates.tsv` | FreeCAD source authority |
| scope review | `矩阵/c5m20_filling_precise_blocker_scope_review_matrix.tsv` | scope 状态 |
| blocker queue | `矩阵/c5m20_filling_precise_blocker_blocker_queue.tsv` | blocker/delete condition |
| backend gap | `矩阵/c5m20_filling_precise_blocker_backend_gap_classification.tsv` | 是否进入代码落点 |
| non-goal | `矩阵/c5m20_filling_precise_blocker_non_goal_registry.tsv` | wrapper / full-family 等边界 |
| oracle | `矩阵/c5m20_filling_precise_blocker_fixture_oracle_matrix.tsv` | oracle / diagnostic 证据 |
| validation | `矩阵/c5m20_filling_precise_blocker_validation_matrix.tsv` | 验收命令 |

## 发布结论

- `Part.makeFilledFace` 的请求范围内没有可采 supported expected：Surface 为 SIGSEGV，support/order 产生 runtime error 或 timeout，剩余参数为 SIGSEGV / timeout / OCCT build crash，non-boundary support/order 为 OCCError 或 timeout。
- direct `Part.BRepOffsetAPI.MakeFilling` 的 `loadInitSurface` 和 support/order G1 control 能 build，但这只证明低层 builder 可用，不能替代 `Part.makeFilledFace` helper oracle，也不能引入 persistent wrapper lifecycle。
- 不新增 collector fixture、不改 expected、不改 `cad-core/src/part/part_filling.cpp` 主路径、不新增 C++ fallback。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
C5M20_PROBE_CASE=surface_only /home/user/.local/bin/FreeCADCmd -c "exec(compile(open('docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M20-PartWorkbenchFillingPreciseBlockerRecovery主线/docs/temp/6-23-19-17-c5m20-filling-precise-blocker-probe.py', encoding='utf-8').read(), 'c5m20_filling_probe.py', 'exec'))"
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M20-PartWorkbenchFillingPreciseBlockerRecovery主线 docs/CADCore5.0-PartDesign-高价值剩余语义/README.md docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
for f in docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M20-PartWorkbenchFillingPreciseBlockerRecovery主线/矩阵/*.tsv docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M20-PartWorkbenchFillingPreciseBlockerRecovery主线 --format markdown
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c5m13_part_filling_param_subsets_are_expected_backed tests.test_p8_features.CadCoreP8FeatureTest.test_c5m8_part_filling_non_default_params_are_constructor_batch tests.test_p8_features.CadCoreP8FeatureTest.test_c5m8_part_filling_support_order_sources_are_source_backed_known_gap
```

## 非目标

- 不重开 C5-M14 至 C5-M19。
- 不实现完整 Part surface family、Surface Workbench GUI/native DocumentObject 或 native `Part::FilledFace` DocumentObject。
- 不把 direct `Part.BRepOffsetAPI.MakeFilling` mutable builder 生命周期做成 cad-core API。
- 不从 cad-core 输出倒推 expected，不按 fixture 名、几何类型、bbox 或输出排序伪造 FreeCAD expected。
