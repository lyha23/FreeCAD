# C5-M13-S1 blocker 根因分类与 probe 矩阵记录

状态：`done_C5M13-S1_blocker_probe_matrix`

## 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`ff3955d7d0`
- `git log -1 --oneline`：`ff3955d7d0 docs: 冻结 C5-M13 S0 live blocker`
- 起始 `git -c core.quotepath=false status --short -uall`：存在 unrelated `cad-core/*`、`docs/CADCore5.0-PartDesign-高价值剩余语义/README.md`、`docs/BUG修改/*` 脏改动，本轮不触碰。
- `FreeCADCmd --version`：`FreeCAD 1.2.0 Revision: 20260519 (Git shallow)`
- S0 后队列确认：`step_goal_queue.py .../C5-M13.../工作步骤细分 --format markdown` 从 `6-22-04-04-C5-M13-S1-blocker根因分类与probe矩阵.md` 开始。

## FreeCAD 依据

- Sweep wrapper：`src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::add(Profile, Location, WithContact, WithCorrection)`、`setAuxiliarySpine()`、`setTolerance()`；location overload 直接调用 `BRepOffsetAPI_MakePipeShell::Add(s, v, ...)`。
- Filling helper：`src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()` 到 `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()`；当前 Python wrapper 对 `surface/supports/orders` 的 runtime 表现仍是 oracle blocker。
- GeomPlate helper：`src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::setG0Criterion/setG1Criterion/setG2Criterion()` 仍返回 `NotImplementedError`；`setProjectedCurve()` 调用 native projected adaptor；`BuildPlateSurfacePyImp.cpp::perform()` 捕获 OCCT runtime error；`PlateSurfacePyImp.cpp` 对 `Curves` 分支仍是 `TODO`。

## Probe 入口

Probe 脚本：

`docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/docs/temp/6-22-04-20-c5m13-s1-blocker-probe.py`

命令模板：

```bash
cd /Users/li/Chili3DProject/FreeCAD
C5M13_PROBE_CASE=<case> FreeCADCmd -c "exec(compile(open('docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/docs/temp/6-22-04-20-c5m13-s1-blocker-probe.py', encoding='utf-8').read(), 'c5m13_s1_probe.py', 'exec'))"
```

本轮每个 case 均用上面的命令模板运行；`<case>` 见下表。`returncode -11` 等价于 FreeCADCmd 被 signal 11 终止，shell 语义为 exit 139。

## 分流矩阵

| owner | case / 命令变量 | 输入形态 | 输出 / 错误文本 | 分类 | S2-S4 expected-backed | delete condition |
| --- | --- | --- | --- | --- | --- | --- |
| Sweep | `sweep_located_free_vertex` | circle profile，独立 `Part.Vertex(0,0,0)` location | `Part.OCCError: NCollection_Array1::Value` at `finish_pipeshell()` | native-runtime blocker | S2 no | `add(Profile, Location, ...)` 对独立 location 返回稳定 `shape_summary` 后删除 |
| Sweep | `sweep_located_profile_vertex` | rectangle profile，location 取 profile 自身 vertex | `Part.OCCError: NCollection_Array1::Value` at `finish_pipeshell()` | native-runtime blocker，不是简单 representative-shape-fixable | S2 no | profile-owned vertex 也能返回稳定 `shape_summary` 后删除 |
| Sweep | `sweep_combined_profile_vertex` | AuxiliarySpine + Tolerance + located rectangle profile vertex | `Part.OCCError: NCollection_Array1::Value` at `finish_pipeshell()` | native-runtime blocker，依赖 location blocker | S2 no | location overload 解除后，再以 combined payload 返回稳定 expected |
| Filling | `filling_default` | closed boundary wire control | Face，edges=4，bbox `[-4.8,12.8]` | collectable control，不是 S1 blocker | already | 已有 default helper expected；不作为 C5-M13 新能力 |
| Filling | `filling_surface_only` | `Part.makeFilledFace([wire], surface=Part.Face(wire))` | `TypeError: argument 2 must be , not Part.Face` | native-runtime wrapper parse blocker | S3 no | `surface=` helper path 不再报该 TypeError 并返回 stable expected |
| Filling | `filling_support_order_g1` | surface + boundary edge support face + order 1 | `UnicodeDecodeError: 'utf-8' codec can't decode byte ...` | native-runtime wrapper parse / error encoding blocker | S3 no | support/order path 返回可解码结构化错误或 stable expected |
| Filling | `filling_support_order_g2` | surface + boundary edge support face + order 2 | `UnicodeDecodeError: 'utf-8' codec can't decode byte ...` | native-runtime blocker，G2 不可 expected-backed | S3 no | G2 support/order helper 返回 stable expected 后删除 |
| Filling | `filling_params_degree_only` | `degree=4` | Face，edges=4 | collector-fixable | S3 yes | S3 为 degree-only representative 落 FreeCAD expected 后删除此子 blocker |
| Filling | `filling_params_num_iter_only` | `numIter=4` | Face，edges=4 | collector-fixable | S3 yes | S3 为 numIter representative 落 FreeCAD expected 后删除此子 blocker |
| Filling | `filling_params_tolerance_only` | `tol2d=0.00001,tol3d=0.0001` | Face，edges=4 | collector-fixable | S3 yes | S3 为 tol2d/tol3d representative 落 FreeCAD expected 后删除此子 blocker |
| Filling | `filling_params_max_degree_only` | `maxDegree=9` | Face，edges=4 | collector-fixable | S3 yes | S3 为 maxDegree representative 落 FreeCAD expected 后删除此子 blocker |
| Filling | `filling_params_pts_on_curve_only` | `ptsOnCurve=16` | `returncode=-11`，无 JSON | native-runtime blocker | S3 no | `ptsOnCurve` 不再终止 FreeCADCmd 后删除 |
| Filling | `filling_params_anisotropy_only` | `anisotropy=True` | `returncode=-11`，无 JSON | native-runtime blocker | S3 no | `anisotropy` 不再终止 FreeCADCmd 后删除 |
| Filling | `filling_params_g1_g2_tol_only` | `tolG1=0.02,tolG2=0.2` | `returncode=-11`，无 JSON | native-runtime blocker | S3 no | `tolG1/tolG2` 不再终止 FreeCADCmd 后删除 |
| Filling | `filling_params_max_segments_only` | `maxSegments=10` | `returncode=-11`，无 JSON | native-runtime blocker | S3 no | `maxSegments` 不再终止 FreeCADCmd 后删除 |
| Filling | `filling_params_all` | degree/ptsOnCurve/numIter/anisotropy/tol*/max* 全量显式参数 | `returncode=-11`，无 JSON | native-runtime blocker，由 crash 参数子集触发 | S3 partial only | 所有显式参数子集都返回稳定 expected 后再删除 all-params blocker |
| Filling | `filling_nonboundary_no_support_order` | boundary wire + non-boundary edge control | Face，edges=4，bbox z range `[-0.647862,1.424028]` | collectable control，C5-M12 已覆盖 | already | 不替代 support/order blocker |
| Filling | `filling_nonboundary_support_order_g1` | non-boundary edge + support face + order 1 | `returncode=-11`，无 JSON | native-runtime blocker | S3 no | non-boundary support/order helper 不再终止 FreeCADCmd 后删除 |
| GeomPlate | `geomplate_g1_curve_on_surface` | `CurveConstraint(line, order=1)` + `setCurve2dOnSurf(line2d)` + initial surface | `setG1Criterion -> NotImplementedError`; `perform -> RuntimeError: Curve must be on a Surface` | native-hidden diagnostic-only + FreeCAD NotImplemented boundary | S4 no | Python/native helper 能稳定构造 `Adaptor3d_CurveOnSurface` G1 expected 后删除 |
| GeomPlate | `geomplate_projected_curve2d` | `CurveConstraint(line, order=0).setProjectedCurve(line2d,0.001,0.001)` | `RuntimeError: Geom_RectangularTrimmedSurface::V1==V2` | native-runtime blocker | S4 no | projected representative 返回 stable helper expected 后删除 |
| GeomPlate | `geomplate_curve_criteria_boundary` | `setG0Criterion/setG1Criterion/setG2Criterion` | 全部 `NotImplementedError: Not yet implemented` | FreeCAD NotImplemented diagnostic-only | S4 no | FreeCAD source 实现 curve criteria setters 后删除 |
| GeomPlate | `geomplate_plate_surface_curves_boundary` | `Part.PlateSurface(Curves=[line_curve()])` | `returncode=-11`，`PlateSurfacePyImp.cpp` 对 `Curves` 分支为 `TODO` | non-goal / wrapper lifecycle boundary with native-runtime evidence | S4 no | 产品批准 request-local PlateSurface.Curves 且 FreeCAD runtime 不再崩溃后删除 |

## S2-S4 分工

- S2 Sweep：`sweep_located_profile_vertex` 已排除“只换 profile-owned vertex 就能恢复”的代表形态修复；S2 只能继续尝试 wrapper call-order / builder lifecycle 变体，若仍为 `NCollection_Array1::Value`，保留更窄 native-runtime blocker。`sweep_combined_profile_vertex` 不可在 location blocker 未解除时伪造 expected。
- S3 Filling：可 expected-backed 子集是 `degree`、`numIter`、`tol2d/tol3d`、`maxDegree` 的单字段 representatives；`surface`、`support/order`、`G2`、`ptsOnCurve`、`anisotropy`、`tolG1/tolG2`、`maxSegments`、`non-boundary support/order` 继续保留精确 FreeCADCmd/native blocker。
- S4 GeomPlate：G1 保持 native-hidden diagnostic-only；ProjectedCurve2d 保持 RuntimeError blocker；curve criteria 是 FreeCAD NotImplemented；PlateSurface.Curves 是 wrapper lifecycle non-goal，并有 signal 11 runtime 证据。

## S1 结论

S1 完成 root-cause 分类，但不替换 expected、不改 collector、不提升 capability。下一步从 S2 开始，优先尝试 Sweep location / combined wrapper 变体；S3 可把 Filling 参数的可采单字段子集纳入 expected-backed 批次，同时必须保留 crash 参数子集的 delete condition。
