# P8-LinkAssemblyRuntime S1 FreeCAD 源码候选矩阵

## 目标

把 S0 确认的 scope 精确到 FreeCAD source authority：文件、类、函数、关键字段、关键短句和 cad-core 对应落点。S1 是后续实现的准入闸门；没有 source authority 的 scope 不能进入 S2-S5 实现。

## 必读

- `src/App/Link.cpp`
- `src/App/Link.h`
- `src/App/PropertyLinks.cpp`
- `src/App/PropertyLinks.h`
- `src/App/DocumentObjectGroup.cpp`
- `src/Mod/Assembly/App/AssemblyLink.cpp`
- `src/Mod/Assembly/App/AssemblyObject.cpp`
- `src/Mod/Assembly/App/AssemblyUtils.cpp`
- `src/Mod/Assembly/App/JointGroup.cpp`
- `cad-core/src/app/link.cpp`
- `cad-core/src/app/property_links.cpp`
- `cad-core/src/assembly/`
- `cad-core/src/adapters/c_api/c_api.cpp`

## 产物

- 更新 `矩阵/p8_link_assembly_runtime_source_candidates.tsv`，把候选升级为可引用 source authority。
- 对每个后续实现 scope 写明 cad-core 层级：`app` / `runtime` / `topo` / `assembly` / `adapters`。
- 在 scope 矩阵中标记 `sourceReady`、`sourceBlocked` 或 `nonGoal`。

## 非目标

- 不写实现。
- 不扩大到 GUI、ViewProvider 或 Python command 行为。
- 不把未精确到函数和字段的“参考 FreeCAD”当作实现依据。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线
```
