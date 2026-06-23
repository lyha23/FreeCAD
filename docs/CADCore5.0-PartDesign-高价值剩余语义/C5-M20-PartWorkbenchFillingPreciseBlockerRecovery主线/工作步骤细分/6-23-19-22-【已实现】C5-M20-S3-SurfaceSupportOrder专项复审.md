# 【已实现】C5-M20-S3 SurfaceSupportOrder 专项复审

状态：`done_surface_support_order_reprobe`

## 目标

复跑 `Surface`、boundary support/order G1/G2、non-boundary support/order G1/G2，并确认是否可稳定采集 `Part.makeFilledFace(...)` expected。

## 结论

| case | 结论 |
| --- | --- |
| `surface_only` | SIGSEGV，栈在 `Wrapped_ParseTupleAndKeywords` / `Part.so` |
| `boundary_support_only` | `CADKernelError: Failed to created face by filling edges` |
| `boundary_order_g1_only` | timeout 60s |
| `boundary_support_order_g1` | `CADKernelError: Failed to created face by filling edges` |
| `boundary_support_order_g2` | `OCCError: GeomPlate : the degree resolution must be upper of 2` |
| `nonboundary_support_order_g1` | 同上 OCCError |
| `nonboundary_support_order_g2` | timeout 60s |

## 必须回写的矩阵行

- `C5M20-BLK-101`
- `C5M20-BLK-102`
- `C5M20-BLK-105`
- `C5M20-ORC-201`

## 验收

- probe 记录中包含上述 case、错误类型和 delete condition。
- 不新增 `cad-core/fixtures/c5m20/expected`。
- `part_filling.cpp` 不新增 fixture-specific fallback。

## 非目标

- 不把 direct wrapper support/order control 当成 supported helper oracle。
