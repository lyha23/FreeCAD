# 【已实现】C5-M18-S1 FreeCAD Folding 源码候选矩阵

状态：`done_s1_closed`

## Source candidates

| candidate | FreeCAD source | 语义 | C5-M18 路由 |
| --- | --- | --- | --- |
| `C5M18-SRC-101` | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1313,1947-2056` | `mmFolding` 四条 straight line support、共享顶点、direction sign、final placement axes | backendGap implemented |
| `C5M18-SRC-102` | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2309-2346` | `calculateFoldAngle()` parallel / invalid cosine diagnostics | backendGap implemented |
| `C5M18-SRC-501` | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/AttachExtension.cpp`、`/home/user/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp` | AttachmentSupport / MapMode / MapReversed 是 request-local graph 输入与 response suggestion | release gate |

## 关键短句

- `mmFolding` 注册为四个 `rtLine` supports。
- placement branch 的顺序是 `edgeA`、`axisA`、`axisB`、`edgeB`。
- `calculateFoldAngle` 对 parallel axes、axisA/edgeA parallel、`cos_unfold` close to one 抛错。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'mmFolding|calculateFoldAngle|Folding axes are parallel|cosine of folding angle' src/Mod/Part/App/Attacher.cpp src/Mod/Part/App/Attacher.h
```
