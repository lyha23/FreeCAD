# C5-M12-S1 FreeCAD Native Oracle Probe 矩阵记录

状态：`done_C5M12-S1_oracle_probe_matrix`

## 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`a565c71faa`
- `git log -1 --oneline`：`a565c71faa docs: 冻结 C5-M12 S0 live gap scope`
- 起始 `git -c core.quotepath=false status --short -uall`：干净
- `FreeCADCmd --version`：`FreeCAD 1.2.0 Revision: 20260519 (Git shallow)`

## FreeCAD 依据

- Sweep wrapper：`src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::setSpineSupport()`、`add(Profile, Location, WithContact, WithCorrection)`、`setAuxiliarySpine()`、`setTolerance()`。
- Loft native object：`src/Mod/Part/App/PartFeatures.cpp::Loft::execute()` 读取 `Sections/Solid/Ruled/Closed/Linearize/MaxDegree`，调用 `TopoShapeExpansion.cpp::TopoShape::makeElementLoft()`。
- Filling helper：`src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()` 到 `TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()`。
- GeomPlate helper：`src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp`、`CurveConstraintPyImp.cpp`、`PointConstraintPyImp.cpp`。

## Probe 入口

Probe 脚本：`docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线/docs/temp/6-22-02-29-c5m12-s1-native-oracle-probe.py`

直接执行脚本路径不会运行脚本主体：

```bash
C5M12_PROBE_CASE=sweep_support FreeCADCmd docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线/docs/temp/6-22-02-29-c5m12-s1-native-oracle-probe.py
```

实际只输出 FreeCAD banner，exit code 0，无 probe JSON。有效触发方式如下：

```bash
C5M12_PROBE_CASE=<case> FreeCADCmd -c "exec(compile(open('docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线/docs/temp/6-22-02-29-c5m12-s1-native-oracle-probe.py', encoding='utf-8').read(), 'c5m12_s1_probe.py', 'exec'))"
```

本轮实际执行的 `<case>`：`sweep_support`、`sweep_located`、`sweep_combined`、`loft_profiles`、`filling_default`、`filling_params`、`filling_nonboundary`、`filling_surface_only`、`filling_support_order`、`filling_nonboundary_support_order`、`geomplate_g1_curve_on_surface`、`geomplate_projected_curve2d`。

## 分流矩阵

| owner | representative | 结果 | 分流 | S2-S4 expected-backed | delete condition |
| --- | --- | --- | --- | --- | --- |
| Sweep | valid `SpineSupport/SupportMode` | `set_spine_support=true`，`shape_type=Solid`，faces=3，volume=78.539816 | collectable expected | S2 yes | S2 为 valid support representative 落 FreeCAD expected 和 focused assertions 后，删除 `support_mode` diagnostic-only blocker |
| Sweep | `add(Profile, Location, WithContact, WithCorrection)` | `Part.OCCError: NCollection_Array1::Value` at `finish_pipeshell()` | FreeCADCmd blocker | no | FreeCADCmd 对 Location overload 返回稳定 `shape_summary` 后替换 blocker |
| Sweep | auxiliary + tolerance + located profile | `Part.OCCError: NCollection_Array1::Value` at `finish_pipeshell()` | FreeCADCmd blocker | no | Location overload blocker 解除后再采 combined payload |
| Loft | wire / face / vertex / whole sketch object profiles | all return non-null `Part::Loft` Shell summaries; whole sketch object profile collectable | collectable expected | S3 yes | S3 为 wire/face/vertex/sketch object representatives 落 expected 或 diagnostic-backed fixtures 后，删除 `complex_profile_family` broad gap |
| Loft | sketch subelement assignment | `TypeError: Type must be App.DocumentObject or None, not tuple`; `Sections` is `PropertyLinkList` | native-hidden | diagnostic only | 只有 FreeCAD upstream `Part::Loft.Sections` 出现 subname storage path，或 C5 scope 批准 request-local DTO 映射时才能删除 |
| Filling | default boundary | non-null Face, edges=4 | collectable expected | already collectable | 不属于 C5-M12 blocker，只作为 FreeCADCmd helper smoke evidence |
| Filling | non-boundary edge without support/order | non-null Face, edges=4 | collectable expected | S4 yes for no-support/order slice | S4 可为 non-boundary edge no-support/order representative 落 expected；不能替代 support/order blockers |
| Filling | non-default params | FreeCADCmd process `exit 139`, no JSON emitted | FreeCADCmd blocker | no | `Part.makeFilledFace(... degree/ptsOnCurve/numIter/anisotropy/tol*/max*)` 不再终止进程并返回稳定 geometry expected 后删除 |
| Filling | initial surface only | FreeCADCmd process `exit 139`, no JSON emitted | FreeCADCmd blocker | no | `Part.makeFilledFace(..., surface=face)` 不再终止进程后删除 |
| Filling | support/order | 错误字符串不稳定：单独 probe 曾返回 `UnicodeDecodeError`，安全批量 probe 返回带控制字节的 `TypeError: argument 2 must be ..., not Part.Face` | FreeCADCmd blocker | no | `supports/orders` native helper path 返回可解码、结构化 Python/OCCT error 或稳定 expected 后删除 |
| Filling | non-boundary support/order | FreeCADCmd process `exit 139`, no JSON emitted | FreeCADCmd blocker | no | non-boundary constraint with support/order 不再终止进程后删除 |
| GeomPlate | G1 curve-on-surface | `setG1Criterion()` -> `NotImplementedError`; `setCurve2dOnSurf()` ok; `BuildPlateSurface.perform()` -> `Curve must be on a Surface` | native-hidden + diagnostic-only | no | FreeCADCmd 能稳定构造 `Adaptor3d_CurveOnSurface` G1 constraint，或 Python helper 暴露等价 surface-bound curve path 后删除 |
| GeomPlate | ProjectedCurve2d | `RuntimeError: Geom_RectangularTrimmedSurface::V1==V2` | FreeCADCmd blocker | no | `CurveConstraint.setProjectedCurve(...)` 代表场景返回稳定 helper expected 后删除 |

## 后续分工

- S2：只把 Sweep valid support 作为可 expected-backed 候选；located / combined 继续保留 `NCollection_Array1::Value` blocker。
- S3：Loft wire / face / vertex / whole sketch object profiles 可进入 expected-backed；sketch subelement 只能保留 native-hidden diagnostic。
- S4：Filling 只把 non-boundary edge without support/order 作为可采候选；support/order、surface、params 保留 FreeCADCmd blockers。GeomPlate G1 保留 native-hidden / diagnostic-only，ProjectedCurve2d 保留 FreeCADCmd blocker。
- 本记录不替换 expected、不做 capability promotion、不声明 C5-M12 完成。
