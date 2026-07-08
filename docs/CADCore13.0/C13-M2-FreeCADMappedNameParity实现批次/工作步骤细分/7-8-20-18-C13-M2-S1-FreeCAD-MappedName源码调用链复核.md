# C13-M2 S1 FreeCAD MappedName 源码调用链复核

## 目标

明确 FreeCAD raw `MappedName`、`ElementMap`、child map key 和 mapper history 的真实调用链。

## 必读文件

- S0 输出
- `src/App/MappedName.cpp`
- `src/App/MappedName.h`
- `src/App/ElementNamingUtils.h`
- `src/App/ElementMap.cpp`
- `src/App/ElementMap.h`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `src/Mod/Part/App/TopoShapeMapper.cpp`
- `src/App/PropertyLinks.cpp`

## 操作

1. 记录 `MappedName::fromRawData()`、postfix、hash/delete encoding 的关键短句。
2. 记录 `ElementMap::encodeElementName()`、`hashElementName()`、`hashChildMaps()`、`getElementHistory()` 的调用关系。
3. 记录 `TopoShapeExpansion.cpp` 中 `ensureElementMap()->encodeElementName()` 的生成/修改/child map 调用点。
4. 更新 source / contract matrix，明确哪些是 C13-M2 必须复刻，哪些仍是后续。

## 关闭条件

- source matrix 对 raw mappedName、child key、mapper history source authority 标为 `authority_frozen`。
- 不再存在“从 expected 字符串复制 mappedName”的路线。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n "encodeElementName|hashElementName|hashChildMaps|getElementHistory|MappedName::fromRawData" src/App src/Mod/Part/App
git diff --check
```
