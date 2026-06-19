# 【已实现】FILLING-S2 capability 发布

已更新 CADCore3.0 docs、oracle 队列、C API capability metadata 和 adapter tests。发布范围区分 simple boundary filling、support/order blocked / deferred 和 diagnostic-only 分支。

发布口径：`part_workbench.filling` 只表示 FreeCAD `Part.makeFilledFace()` source-backed helper parity，不是原生 FreeCAD DocumentObject；只覆盖 `Boundary` / `App::PropertyLinkSubList`、closed wire default、connected boundary edges default、`BRepOffsetAPI_MakeFilling`、`maker_history:filling`、source-backed helper metadata、invalid diagnostics 和三条 `c3m4/part-filling-*` fixtures。

非目标保留：`surface` / `supports` / `orders`、non-default params、constraints、compound optional case、Surface Workbench `Surface::Filling`、GeomPlate 和 full Part surface family 仍不发布 supported。
