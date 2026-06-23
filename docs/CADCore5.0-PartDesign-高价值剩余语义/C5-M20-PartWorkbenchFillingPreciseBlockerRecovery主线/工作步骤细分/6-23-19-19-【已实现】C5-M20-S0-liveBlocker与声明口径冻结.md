# 【已实现】C5-M20-S0 liveBlocker 与声明口径冻结

状态：`done_live_blocker_freeze`

## 目标

冻结 C5-M20 只承接 C5-M13 留下的 Filling precise blockers，不重开已收口的 Datum AttachEngine C5-M14~M19，也不把 direct wrapper 成功当成 `Part.makeFilledFace` expected。

## 输入

- C5-M13 `C5M13-SCOPE-301`、`C5M13-FLD-301~303`、`C5M13-BLK-301`。
- c5m8 `part-filling-*` known_gap / expected。
- c5m12 `part-filling-non-boundary-edge-no-support-order` expected。
- c5m13 `part-filling-param-{degree,num-iter,tol2d-tol3d,max-degree}-only` expected。

## 声明口径

- `supported` 只用于 FreeCADCmd `Part.makeFilledFace(...)` helper 稳定返回 geometry expected 的 case。
- `direct_wrapper_control` 只证明 `BRepOffsetAPI_MakeFilling` 低层 owner 可 build，不替代 helper expected。
- `native_blocker` 包括 SIGSEGV、timeout、Unicode/control-byte、TypeError、OCCError、CADKernelError 等不可稳定采集 expected 的情况。

## 验收

- 根入口、工作步骤总入口、矩阵中明确 C5-M20 不新增 expected-backed support。
- `docs/temp/6-23-19-17-C5-M20-fillingPreciseBlockerProbe记录.md` 存在逐 case 结果。
- `c5m20_filling_precise_blocker_scope_review_matrix.tsv` 中 request-local helper rows 不得是 `supported`。

## 非目标

- 不修 FreeCAD 上游 `AppPartPy.cpp`。
- 不落 cad-core fallback。
- 不新增 GUI/native DocumentObject 或 persistent wrapper lifecycle。
