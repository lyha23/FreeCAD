# P6 MakerHistory S5 Taper Partial History 专项复审

## 目标

复核 taper history 是否仍是 partial / known_gap，还是当前代码、capabilities 和 tests 已经可以发布为 covered。S5 不直接改 expected，不从现有 cad-core 输出倒推 FreeCAD 行为。

## 执行结论

2026-06-17 已完成 S5 复审：`P6MH-SCOPE-004` 裁决为 `supported`。当前 cad-core object metadata、C ABI capability 和 focused tests 均使用 `maker_history:taper_thru_sections` / `covered_full`，没有对象级 `topo_naming=known_gap:taper_history` 或 checked-in expected 残留。

S5 执行时发现 `known_gap:taper_history` 与 `history_partial:taper` 只残留在旧 P3b 正式方案文档；S6 已完成发布回写，因此这不是 C++ backendGap。当前 FreeCAD 依据仍是 `ExtrusionHelper::makeElementDraft()`、`MapperThruSections::GeneratedFace(s)` / `FirstShape()` / `LastShape()` 和 `FeatureExtrude` 复用 taper helper；cad-core 落到 `makeTaperedExtrusion()`、`namedShapeForThruSectionsHistory()`、`namedShapeForTaperedExtrusionHistory()`、Pad / Pocket / Part::Extrusion metadata。

## FreeCAD 依据

| 入口 | 关键点 |
| --- | --- |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/ExtrusionHelper.cpp` | taper 通过 `makeElementDraft()` / offset section / ThruSections 建立几何 |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::MapperThruSections` | `GeneratedFace(s)`、`FirstShape()`、`LastShape()` 是 taper history 的关键 |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp` | Pad / Pocket taper 复用 Part taper helper |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureExtrusion.cpp` | Part::Extrusion taper 使用同一语义 |

## cad-core 复核点

| 文件 | 检查项 |
| --- | --- |
| `cad-core/src/part/extrusion_helper.cpp` | TaperedExtrusionResult historyComponents / historySources |
| `cad-core/src/part/topo_shape.cpp` | `namedShapeForThruSectionsHistory()`、`namedShapeForTaperedExtrusionHistory()`、inner-wire cut history |
| `cad-core/src/part_design/feature_extrude.cpp` | Pad / Pocket object metadata 是否仍输出 known_gap |
| `cad-core/src/part/part_extrusion.cpp` | Part::Extrusion taper metadata |
| `cad-core/src/adapters/c_api/c_api.cpp` | `object_metadata.local_history.taper_history.status` |
| `cad-core/tests/test_p6_topology.py` | taper history focused tests |

## 范围裁决

| scope | S5 需要裁决 |
| --- | --- |
| `P6MH-SCOPE-004` | taper 是 supported、releaseGate、notCollected 还是 backendGap |
| `P6MH-SCOPE-005` | taper split / generated / deleted 后续引用恢复是否需要 oracle |
| `P6MH-SCOPE-006` | P6 文档和 capability 是否需要发布回写 |

## 必须回写的矩阵行

- `P6MH-SCOPE-004`
- `P6MH-BLOCK-004`
- `P6MH-BG-004`

## 验收标准

- 必须直接回答当前 `known_gap:taper_history` 是否仍存在于 object metadata、expected 或 docs。
- 如果 capability 的 `covered_full` 被接受，S6 只能做发布回写和 focused regression，不写无必要 C++。
- 如果仍有 partial gap，必须列出具体 fixture、missing history relation 和 FreeCAD ThruSections 依据。
- 执行：

```bash
rg -n "known_gap:taper_history|history_partial:taper|maker_history:taper_thru_sections|taper_history|namedShapeForTaperedExtrusionHistory|namedShapeForThruSectionsHistory" docs/CADCore方案 cad-core/src cad-core/include cad-core/tests
python3 -m unittest tests.test_p6_topology.CadCoreP6TopologyTest.test_p6_taper_thru_sections_history_is_mapper_backed
git diff --check
```

## 非目标

- 不扩大到 UpToShape taper face-list 尚未支持路径。
- 不用当前输出重写 FreeCAD oracle。
- 不把所有 taper 几何参数全集纳入本主线。
