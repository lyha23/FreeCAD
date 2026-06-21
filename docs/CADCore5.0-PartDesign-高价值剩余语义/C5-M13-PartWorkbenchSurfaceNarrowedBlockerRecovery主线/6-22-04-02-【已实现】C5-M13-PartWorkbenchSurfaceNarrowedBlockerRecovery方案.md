# 【已实现】C5-M13 Part Workbench Surface Narrowed Blocker Recovery 方案

状态：`done_C5-M13`

## 当前基线

C5-M12 已完成 surface native oracle recovery 收口，当前 `docs/CADCore3.0/capabilities-gap对照表.md` 中 surface family 的 broad gap 已被压缩为少量 precise blockers：

- `part_workbench.sweep`：只剩 located profile / combined 的 FreeCADCmd wrapper build blocker，错误为 `OCCError: NCollection_Array1::Value`。
- `part_workbench.filling`：只剩 native helper expected blockers：surface/support/order、G2、non-default params、non-boundary edge support/order。
- `part_workbench.geomplate`：只剩 G1 curve-on-surface native-hidden diagnostic-only、ProjectedCurve2d RuntimeError blocker，以及 curve criteria setter / PlateSurface.Curves wrapper lifecycle 的 diagnostic / non-goal 边界。
- `part_workbench.loft`：C5-M12 已关闭 broad `complex_profile_family`；C5-M13 不再重做 Loft。

C5-M13 的目标不是把所有 surface 相关内容混成大包，而是处理同一类“已有 helper / wrapper 能力因 FreeCADCmd/native oracle blocker 无法晋级 expected-backed”的剩余问题。每个子项必须保留 source authority、DTO/API、collector path、expected schema、focused tests 和 capability/docs 收口。

## 最终收口

- Sweep located/combined：保留 `add(Profile, Location, WithContact, WithCorrection)` Location overload build-stage `NCollection_Array1::Value` blocker；combined 依赖 Location overload，no-location controls 已可 build。
- Filling：`Degree`、`NumIter`、`Tol2d+Tol3d`、`MaxDegree` 单字段 representatives 已 expected-backed；`Surface`、support/order G1/G2、`PtsOnCurve`、`Anisotropy`、`TolG1+TolG2`、`MaxSegments`、all-params、non-boundary support/order 保留 precise blocker。
- GeomPlate：`ProjectedCurve2d + InitialSurface` 已 expected-backed；无 `InitialSurface` ProjectedCurve2d 保留 `Geom_RectangularTrimmedSurface::V1==V2` blocker；G1、curve criteria setters、`Part.PlateSurface.Curves` 保留 native-hidden/NotImplemented/SIGSEGV 边界。
- Loft broad `complex_profile_family`、完整 Part surface family、GUI/native DocumentObject、persistent wrapper lifecycle 和 cad-core-output-derived expected 均未重开。

## FreeCAD 调用链

Sweep wrapper：

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakePipeShell.pyi`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::add(Profile, Location, WithContact, WithCorrection)`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::setAuxiliarySpine()`、`setTolerance()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` 只作为 native base property 边界，不声明 advanced direct properties。

Filling helper:

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp`

GeomPlate helper:

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/PointConstraintPyImp.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PlateSurfacePyImp.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp::makeSurface()`

## cad-core 落点

- `cad-core/tools/collect_freecad_expected.py`：主要落点。S1-S4 只允许从 FreeCADCmd helper / wrapper 采集 expected 或 blocker evidence，不允许从 cad-core result 反推。
- `cad-core/fixtures/c5m10`、`cad-core/fixtures/c5m8`、`cad-core/fixtures/c5m7`、`cad-core/fixtures/c5m12`、新增 `cad-core/fixtures/c5m13`：存放复用或新增代表场景。
- `cad-core/src/part/part_sweep.cpp`、`part_filling.cpp`、`part_geomplate.cpp`：仅当 DTO / helper 实现确实缺字段或错误解析时修改。
- `cad-core/tests/test_p8_features.py`、`tests/test_expected_fixtures.py`、`tests/test_adapters.py`：focused tests 保护 expected、diagnostics、capability remaining gaps 和 non-goals。
- `cad-core/src/adapters/c_api/c_api.cpp`：只同步 capability metadata，不承接几何业务逻辑。

## 代表场景

| owner | 场景 | C5-M13 判断 |
| --- | --- | --- |
| Sweep | located profile `SectionOptions[].Location/WithContact/WithCorrection` | 先查 location vertex 是否必须位于 spine/profile、wrapper Add overload 参数是否构造错；能修就 expected-backed，否则记录更窄 OCCT blocker |
| Sweep | combined auxiliary + located profile + tolerance | 依赖 located profile 修复；不能在 location blocker 未解除时伪造 combined expected |
| Filling | `Surface` + support/order + G2 | 同一 `Part.makeFilledFace` helper owner；检查 helper kwargs、support face shape 类型、order enum 与 G2 参数 |
| Filling | non-default constructor params | 检查 FreeCADCmd crash 是否由参数组合、默认值、shape 构造或 wrapper调用方式导致；能拆代表场景则批量 expected |
| Filling | non-boundary edge support/order | C5-M12 已支持 no-support/order；本轮只补 support/order variant |
| GeomPlate | G1 curve-on-surface | 优先找可稳定构造 `Adaptor3d_CurveOnSurface` 或等价 surface-bound curve 的 FreeCADCmd path |
| GeomPlate | ProjectedCurve2d | 复核 `Geom_RectangularTrimmedSurface::V1==V2` 是否由代表 surface/trim 参数导致；可修则 expected-backed |
| GeomPlate | curve criteria setter / PlateSurface.Curves | 只有 FreeCAD source 或 wrapper runtime 证明可用时才推进；否则继续 diagnostic / non-goal |

## 实施顺序

1. S0：冻结当前 narrowed blockers、C5-M12 evidence、capability rows、checked-in expected 和 non-goals。
2. S1：写 FreeCADCmd blocker root-cause matrix，所有代表场景至少有一次可复现 probe、错误文本和 collectability 分类。
3. S2：处理 Sweep location / combined；优先修 collector / representative 形态，不改 adapter 输出。
4. S3：处理 Filling native helper expected；按同一 helper API 一次覆盖 surface/support/order/G2/params/non-boundary support-order。
5. S4：处理 GeomPlate G1 / ProjectedCurve2d；能采 expected 就落 fixtures/tests，不能采就收窄 blocker。
6. S5：同步 capability/docs/root matrices，关闭或保留精确 blocker，队列为空后才把方案和步骤改名为 `【已实现】`。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/工作步骤细分 --format markdown
```

实现短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/collect_freecad_expected.py --phase c5m13 --check --skip-unsupported
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

阶段收口：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 tools/collect_freecad_expected.py --phase c5m13 --check --skip-unsupported
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 收口标准

- 每个 C5-M12 narrowed blocker 都被重新分类：expected-backed、diagnostic-only、native-hidden、FreeCADCmd/OCCT blocker 或 non-goal。
- 可 expected-backed 的场景有 FreeCADCmd expected、fixtures、focused tests、capability metadata 和 docs。
- 不可 expected-backed 的场景有复现命令、错误文本、delete condition 和下一 owner，不再停留在 broad wording。
- `part_workbench.loft` broad `complex_profile_family` 不被重开。
- docs / capability / root matrix / package matrix 状态一致，队列脚本只在 C5-M13 未实现步骤显示 pending。
