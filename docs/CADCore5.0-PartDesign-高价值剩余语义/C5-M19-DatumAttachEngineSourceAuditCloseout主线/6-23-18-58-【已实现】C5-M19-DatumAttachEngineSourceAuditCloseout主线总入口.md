# 【已实现】C5-M19 DatumAttachEngineSourceAuditCloseout 主线

状态：`done_source_audited_capability_closed`

本包承接 C5-M18 后仍被列在 `part_design.datum_attachment.exact_blockers.datum_attach_engine_remaining_modes` 的 `TangentU`、`TangentV`、`IntersectionPoint`。C5-M19 不新增几何能力；它只做 FreeCAD `Attacher.cpp` / `Attacher.h` 源码审计和 capability 收口：证明三项都是 enum/name 表暴露但没有可选 `modeRefTypes` 和执行分支的不可执行模式，并把它们从 exact blocker 转为 source-audited non-goal。

## 目标

- 复核 `TangentU`、`TangentV`、`IntersectionPoint` 是否有 FreeCAD 可执行 route。
- 明确 `IntersectionLine` 已经是 DatumLine `mm1Intersection`，不得被误当成 `IntersectionPoint`。
- 只有证明存在可执行 FreeCAD route 时才补 DTO、fixtures、expected、focused tests 和 capability supported 项。
- 本轮审计证明三项没有可执行 route，因此不新增 DTO、fixtures、expected 或 C++ placement helper。
- 从 capability exact blocker 删除 `datum_attach_engine_remaining_modes`，把三项作为 source-audited non-goal 记录。
- 保持 CAD Core 无状态边界：不新增 backend attachment session，不传递或保存完整 BREP；`ReferenceShadow.brep` 仍只允许作为单 referenced subshape snapshot 证据。

## 当前基线

- C5-M14 至 C5-M18 已分别关闭 DatumPoint proximity、Datum3DPlane、curve-frame/curvature、conic landmarks 和 Folding。
- `cad-core` 已支持 DatumLine `IntersectionLine`，其 FreeCAD route 是 `AttachEngineLine::mm1Intersection`，需要两个 Face support 并产出 line placement。
- `TangentU/V` 与 `IntersectionPoint` 在 FreeCAD enum/name 表存在，但 C5-M19 源码审计没有找到 `modeRefTypes[...]` 注册或 `_calculateAttachedPlacement()` case。
- FreeCADCmd 1.2.0 revision 20260519 在本机可启动；Python property enum 只能证明名称暴露，不能证明 `EnableAllSupportedModes()` 可选性，因此本包以 C++ `modeRefTypes` / switch 源码为判定依据。

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| enum/name 表 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.h:83-95`、`/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:110-122` | `mm1TangentU`、`mm1TangentV`、`mm0Intersection` 和显示名存在 |
| 可选性闸门 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:555-561` | `EnableAllSupportedModes()` 只启用非空 `modeRefTypes[i]` |
| TangentU/V 审计 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1237-1240,1651-1663,2394-2434,2444-2798` | 只有 `mmTangentPlane` 使用 `TangentU/TangentV` 计算内部方向；`mm1TangentU/V` 没有 ref type 注册，也没有 line switch case |
| IntersectionPoint 审计 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2827-2857,2866-3018` | `AttachEnginePoint` 没有 `modeRefTypes[mm0Intersection]`，switch 也没有 `mm0Intersection` case |
| IntersectionLine 对照 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2432,2703-2752` | `mm1Intersection` 注册两个 Face support 并用 `GeomAPI_IntSS` 产出 straight line；这是已支持的 `IntersectionLine` |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| Datum AttachEngine | `cad-core/src/part_design/datum_attachment.h` | 保持已支持的 selected MapMode；`IntersectionLine` 仍是 DatumLine route，不新增 `IntersectionPoint` helper |
| capability | `cad-core/src/runtime/capability_contract.cpp` | 删除 `datum_attach_engine_remaining_modes` exact blocker，新增 source-audited enum-only non-goal 文案 |
| adapter test | `cad-core/tests/test_adapters.py` | 断言 `IntersectionLine` supported 仍存在，`datum_attach_engine_remaining_modes` 不再是 exact blocker |
| docs / matrices | 本包 `矩阵/*.tsv` 与根 `docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵` | 记录 source audit、non-goal、无 DTO/fixture/expected 的收口证据 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-23-18-59-【已实现】C5-M19工作步骤总入口.md` | S0-S6 队列索引 |
| S0 | `工作步骤细分/6-23-19-00-【已实现】C5-M19-S0-liveBlocker与声明口径冻结.md` | 冻结审计口径和 forbidden claims |
| S1 | `工作步骤细分/6-23-19-01-【已实现】C5-M19-S1-FreeCAD源码候选矩阵.md` | FreeCAD source candidates 和可选性闸门 |
| S2 | `工作步骤细分/6-23-19-02-【已实现】C5-M19-S2-scope准入与nonGoal矩阵.md` | scope / blocker / nonGoal 路由 |
| S3 | `工作步骤细分/6-23-19-03-【已实现】C5-M19-S3-TangentUV源码复审.md` | `TangentU/V` enum-only 复审 |
| S4 | `工作步骤细分/6-23-19-04-【已实现】C5-M19-S4-IntersectionPoint与IntersectionLine边界复审.md` | `IntersectionPoint` 与 `IntersectionLine` 分离 |
| S5 | `工作步骤细分/6-23-19-05-【已实现】C5-M19-S5-capability与requestLocal边界复审.md` | capability / no-session / no-BREP 边界 |
| S6 | `工作步骤细分/6-23-19-06-【已实现】C5-M19-S6-发布收口.md` | capability、tests、docs/root matrices closeout |

## S6 closeout

- `datum_attach_engine_remaining_modes` 已从 capability exact blocker 删除。
- `TangentU`、`TangentV`、`IntersectionPoint` 转为 source-audited non-goal，不作为 cad-core implementable backendGap。
- `IntersectionLine` supported claim 和 focused implementation 保持不变。
- C5 Datum AttachEngine 主线正式收口；后续优先转向 Part surface precise blockers，例如 Filling / GeomPlate / Sweep 的 FreeCADCmd/native blocker。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'mm1TangentU|mm1TangentV|mm0Intersection|modeRefTypes\\[mm0Intersection\\]|modeRefTypes\\[mm1TangentU\\]|modeRefTypes\\[mm1TangentV\\]|case mm0Intersection|case mm1TangentU|case mm1TangentV|case mm1Intersection' src/Mod/Part/App/Attacher.cpp src/Mod/Part/App/Attacher.h
git diff --check -- cad-core docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M19-DatumAttachEngineSourceAuditCloseout主线 docs/CADCore5.0-PartDesign-高价值剩余语义/README.md docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
for f in docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M19-DatumAttachEngineSourceAuditCloseout主线/矩阵/*.tsv docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

## 非目标

- 不重开 C5-M14 至 C5-M18 已关闭 modes。
- 不从 `IntersectionLine` 推导或伪造 `IntersectionPoint`。
- 不新增 DTO、fixture、expected 或 focused placement test，除非未来上游源码出现可执行 FreeCAD route。
- 不新增 GUI Attachment editor、TaskPanel、ViewProvider、visual resize、backend attachment session 或完整 BREP 状态。
