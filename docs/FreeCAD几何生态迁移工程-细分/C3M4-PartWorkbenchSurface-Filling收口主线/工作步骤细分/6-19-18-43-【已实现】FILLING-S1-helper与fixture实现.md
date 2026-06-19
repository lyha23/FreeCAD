# 【已实现】FILLING-S1 helper 与 fixture 实现

新增 filling helper / collector / fixtures / tests。第一批包含 closed boundary、boundary edges 成功 expected 和 invalid boundary diagnostics。

## 完成内容

- C++：新增 `Part::FilledFace` cad-core helper executor，注册到 runtime registry；新增 `topo_shape_expansion` filling helper，使用 `BRepOffsetAPI_MakeFilling`，覆盖默认参数、closed wire、connected boundary edges、wire normalization、maker history 和 boundary source evidence。
- Fixtures：新增 `cad-core/fixtures/c3m4/part-filling-closed-wire-default.json`、`part-filling-boundary-edges-default.json`、`part-filling-invalid-inputs.json`。
- Expected：collector 已把 helper object 翻译成 `Part.makeFilledFace(...)`，并 checked-in 三个 `fixtures/c3m4/expected/*.freecad.json`。
- Tests：`cad-core/tests/test_p8_features.py` 增加 filling focused tests，断言成功 shape、metadata、source evidence、diagnostics 和 expected 对齐。
- Docs/matrices：S1 required rows 已标为 implemented / expected-backed；support/order、surface、constraint、非默认参数仍 blocked/deferred，未发布 supported capability。

## 非目标保留

- 不改 C API capability 发布和 CADCore3.0 发布文案；留给 S2。
- 不支持 supports/orders/surface/non-default params。
- 不把 Filling 改成 FaceMaker 路径。
