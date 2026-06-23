# 【已实现】C5-M19-S0 liveBlocker 与声明口径冻结

状态：`done_claim_boundary_frozen`

## 目标

冻结 C5-M19 的审计口径：本包只判断 `TangentU`、`TangentV`、`IntersectionPoint` 是否有 FreeCAD 可执行 selected MapMode route，并根据源码证据决定是否转为 non-goal。

## 冻结声明

| 项 | 口径 |
| --- | --- |
| 审计对象 | `TangentU`、`TangentV`、`IntersectionPoint` |
| source authority | `src/Mod/Part/App/Attacher.h`、`src/Mod/Part/App/Attacher.cpp` |
| 可执行条件 | 同时存在非空 `modeRefTypes[...]` 和 `_calculateAttachedPlacement()` case / reuse route |
| 关闭条件 | 三项均没有可执行条件，capability exact blocker 删除，non-goal / source-audited rows 写入 |
| 不触发实现条件 | enum/name 表存在但无 `modeRefTypes` / branch |

## 禁止声明

- 禁止把 `TangentU/V` 发布为支持的 selected MapMode。
- 禁止把 `IntersectionLine` 当成 `IntersectionPoint` 的实现证据。
- 禁止为不可选模式创建 DTO、fixture、expected 或 focused placement test。
- 禁止借 C5-M19 重开 C5-M14~M18 已关闭 modes。
- 禁止新增 backend session、shape cache、完整 BREP 输入/输出或 GUI attachment 语义。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'TangentU|TangentV|IntersectionPoint|IntersectionLine|datum_attach_engine_remaining_modes' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py src/Mod/Part/App/Attacher.cpp src/Mod/Part/App/Attacher.h
```

S0 验收结论：审计对象、可执行条件、禁止声明和完成条件已写入总入口、工作步骤入口和矩阵。

## 非目标

- 不运行完整 FreeCAD build。
- 不采集新 native expected。
