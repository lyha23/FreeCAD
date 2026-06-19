# 【已实现】LOFT-S0 source 与 oracle 矩阵

复核 `Part::Loft` / `TopoShape::makeElementLoft()` / `MapperThruSections`，设计 fixture：two-section surface、solid loft、ruled loft、closed loft、invalid sections。只写 docs/矩阵，不写 C++。

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`a675b143f1`。
- `git log -1 --oneline`：`a675b143f1 feat: 发布PARTSURF S5 RuledSurface能力`。
- `git -c core.quotepath=false status --short -uall`：既有未提交 `src/Mod/Sketcher/App/SketchObject.h`、`src/Mod/Sketcher/App/SketchObjectPyImp.cpp`，以及其它 C3M4 Surface / CADCore 主线 docs 未跟踪文件；S0 只修改 Loft 专题包 docs/矩阵。

## FreeCAD 源码裁决

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.h:70` 定义 `Part::Loft`，公开属性为 `Sections`、`Solid`、`Ruled`、`Closed`、`Linearize`、`MaxDegree`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp:179` 初始化属性：`Sections` 是 `App::PropertyLinkList`，`Solid` 默认 `true`，`Ruled` / `Closed` / `Linearize` 默认 `false`，`MaxDegree` 默认 `5` 且约束下限为 `2`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp:222` 的 `Loft::execute()` 先要求 `Sections.getSize() != 0`，再用 `getTopoShape(... ResolveLink | Transform)` 收集 profile，映射 `Solid/Ruled/Closed/MaxDegree` 后调用 `result.makeElementLoft(shapes, isSolid, isRuled, isClosed, degMax)`；`Linearize=true` 只在 loft shape 已生成后调用 `result.linearize(...)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp:2404` 的 `prepareProfiles()` 接受 single face、single wire/edge 转 wire、single vertex；空 shape、无 profile、非单一 vertex/edge/wire/face 进入 kernel error。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp:4463` 的 `TopoShape::makeElementLoft()` 使用 `BRepOffsetAPI_ThruSections(isSolid, isRuled)`、`SetMaxDegree()`、`AddVertex()` / `AddWire()`、`CheckCompatibility(Standard_True)` 和 `Build()`；`Closed=true` 通过复制第一 profile 闭合，但最后一个 profile 是 vertex 时会忽略 closed。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp:3631` 的 `MapperThruSections` 在通用 maker history 为空时补 `GeneratedFace(s)`，并把 first / last profile 内元素映射到 `FirstShape()` / `LastShape()`；S1 不能只比较 final shape bbox。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp:2168` 的 `Part.makeLoft()` 也走 `TopoShape().makeElementLoft(...)`，可作为低层 oracle 辅助；发布口径仍以 `Part::Loft` DocumentObject 为准。

## S0 矩阵结论

- 新增 `矩阵/part_surface_loft_fixture_oracle_matrix.tsv`，首批 required fixture 固定为 `part-loft-two-section-surface`、`part-loft-solid`、`part-loft-ruled`、`part-loft-closed`、`part-loft-invalid-sections`。
- fixture JSON schema 必须表达 source-backed `DocumentObject`：`TypeId` 为 `Part::Loft`，`Properties.Sections` 使用 `App::PropertyLinkList` 指向源 profile 对象；`Solid` / `Ruled` / `Closed` / `Linearize` 用 `App::PropertyBool` 或普通 bool，`MaxDegree` 用 `App::PropertyInteger` 或普通 integer。禁止 adapter 直接输出 loft shape。
- FreeCAD expected collector 路径优先创建 native `Part::Loft` object。当前 `cad-core/tools/collect_freecad_expected.py` 已能设置 `App::PropertyLinkList`，但 `NATIVE_TYPE_IDS` 还没有 `Part::Loft`；S1 需要先补 native type 和必要的 payload/diagnostic 采集，再新增 fixture/expected。
- 几何实现应落在 Part executor + `part/topo_shape` / ThruSections helper + `topo` history，不放到 adapter；历史断言至少覆盖 `GeneratedFace()`、first-section、last-section 三类 `MapperThruSections` 行为。
- `Linearize=true` 不进首批 required fixture：它是 `makeElementLoft()` 之后的后处理，S1 只保留 deferred matrix row，等 expected 和输出差异单独裁决后再发布。

## 非目标

- 不实现 C++ executor。
- 不新增实际 fixture / expected 文件。
- 不采集 FreeCAD expected。
- 不修改 CADCore3.0 capability supported 文案。
- 不要求全量 FreeCAD build。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-Loft收口主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-Loft收口主线/工作步骤细分 --format markdown
```

完成状态：本文件已按完成规则命名为 `6-19-18-36-【已实现】LOFT-S0-source与oracle矩阵.md`。
