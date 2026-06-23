# 【已实现】C5-M18-S0 live Folding blocker 冻结

状态：`done_s0_closed`

## 结论

- C5-M17 后 live exact blocker 包含 `Folding`、`TangentU`、`TangentV`、`IntersectionPoint`。
- C5-M18 只允许删除 `Folding`，删除条件是 FreeCADCmd expected、diagnostics、focused tests 和 capability test 全部闭环。
- `TangentU/V` 与 `IntersectionPoint` 不随本包发布 supported。

## 发布真源

| 来源 | 用途 |
| --- | --- |
| `cad-core/src/runtime/capability_contract.cpp` | 当前 supported / fixtures / exact blocker 发布入口 |
| `cad-core/tests/test_adapters.py` | capability contract 回归断言 |
| `docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵` | root closeout 记录 |

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'Folding|TangentU|TangentV|IntersectionPoint|datum_attach_engine_remaining_modes' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py
```

## 非目标

- 不修改 GUI / ViewProvider / TaskPanel。
- 不引入 backend attachment session 或 complete BREP state。
