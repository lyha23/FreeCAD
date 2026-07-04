# C12-M14 helper lifecycle request-local 产品契约

## 状态

S3 发布 `product_contract_only`，只覆盖 `C12M14-ORACLE-105`：`add(profile) -> remove(profile) -> add(profile_alt) -> simulate(2) -> build() -> shape()`。

## 契约声明

这是 CAD Core request-local product contract，不是 FreeCAD native parity。

FreeCAD `1.2.0 revision 20260519` / OCCT `7.8.1` 的 S2 artifact 显示：该组合里 `simulate(2)` 返回 list payload，但随后的 `build()` 与 `shape()` 均抛 `NCollection_Sequence::ChangeValue`。该 native instability 只能作为产品契约输入，不能写成 checked-in FreeCAD expected 或 native parity。

## CAD Core request-local 语义

- Helper lifecycle request 是单次 recompute 请求内的显式 operation replay，不建立跨请求 Python mutable object。
- `add/remove/add` 只更新 request-local section list；`simulate(2)` 是观察性操作，不能污染同一请求后续 `build/shape`。
- 若 S4 实现该组合，允许用隔离的 request-local builder 分别执行 simulate 与 build/shape，避免继承 FreeCAD native helper 的 `NCollection_Sequence::ChangeValue` 不稳定状态。
- Response 必须保留 operation 顺序、每步 payload/diagnostic、S2 artifact 引用、`native_parity=false`，以及 `contract_provenance=cad_core_product_contract_non_parity` 或等价字段。
- 该契约不适用于 plain `Part::Sweep` wrapper、PartDesign Pipe、其他 Part Workbench Python helpers，且不能用 final mesh output 代替 helper lifecycle response。

## 删除或重审条件

- 同一 FreeCAD / LibPack / OCCT oracle baseline 产出 stable native expected，且不再触发 `NCollection_Sequence::ChangeValue`。
- S4 实现尝试把该组合写成 FreeCAD native parity。
- helper lifecycle DTO 边界变化，导致 request-local operation replay 不再成立。
