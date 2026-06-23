# 【已实现】C5-M20-S5 WrapperControl 与 requestLocal 边界复审

状态：`done_wrapper_boundary_review`

## 目标

确认 direct `Part.BRepOffsetAPI.MakeFilling` control 的含义：它可以证明低层 owner 行为，但不能作为 cad-core persistent wrapper lifecycle 或 `Part.makeFilledFace` helper expected 替代。

## 结论

- `wrapper_surface_control`：direct wrapper `loadInitSurface(face)` 可 build Face。
- `wrapper_support_order_g1_control`：direct wrapper `add(edge, face, 1, True)` 可 build Face。
- 这两个结果只说明 `BRepOffsetAPI_MakeFilling` 低层 builder 在这些输入下可用；C5-M20 不把它们写成 `Part::FilledFace` expected，因为 product contract 是 request-local `Part.makeFilledFace` helper DTO。

## 必须回写的矩阵行

- `C5M20-SCOPE-401`
- `C5M20-NG-001`
- `C5M20-ORC-401`

## 验收

- direct wrapper control 只出现在 `direct_wrapper_control` / `non_goal`，不出现在 supported fixture list。
- S6 明确无 persistent builder state、无完整 BREP 状态。

## 非目标

- 不新增 `Part.BRepOffsetAPI.MakeFilling` cad-core public API。
- 不保存 direct wrapper shape 或 session。
