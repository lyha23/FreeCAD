# C13-M3 S1 producer ledger 接口设计

## 目标

设计 `NamedShape` / codec helper 可承载的 FreeCAD-equivalent tag、source tag、operation postfix、raw/canonical mapped-name provenance。

## 必读文件

- S0 输出
- `src/App/MappedName.h`
- `src/App/MappedName.cpp`
- `src/App/ElementMap.cpp`
- `src/App/ElementNamingUtils.h`
- `cad-core/include/cad_core/part/topo_shape.h`
- `cad-core/src/part/topo_shape.cpp`

## 操作

1. 明确 ledger 字段：current element、source element、element type、producer tag、source tag、operation postfix、raw/canonical、provenance status。
2. 决定字段落在 `NamedShape`、`NamedElement`、独立 map，还是专用 `MappedNameProvenance`。
3. 写 FreeCAD source 注释要求。
4. 更新 source / implementation matrix。

## 关闭条件

- `C13M3-BLOCKER-101` 关闭。
- 代码接口可以支持 S2 实现，不要求本步填满所有 producer。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check
```
