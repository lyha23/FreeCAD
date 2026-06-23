# 【已实现】C5-M20-S2 scope 准入与 blocker 矩阵

状态：`done_scope_blocker_matrix`

## 目标

把 probe 结果路由为 `precise_blocker`、`direct_wrapper_control`、`non_goal` 或 `no_backend_gap`。只有 `Part.makeFilledFace(...)` helper 稳定返回 shape summary 的 row 才能进入 implementable backendGap。

## 分类规则

| 状态 | 条件 | 处理 |
| --- | --- | --- |
| `supported` | helper 稳定返回 geometry expected | 补 collector / fixture / expected / tests |
| `precise_blocker` | helper SIGSEGV、timeout、OCCError、CADKernelError 或不可解码错误 | 保留 known_gap 与 delete condition |
| `direct_wrapper_control` | direct wrapper 稳定 build | 只作为 source-audited evidence |
| `non_goal` | GUI/native DocumentObject/persistent wrapper/full family | 不进代码落点 |

## 必须回写的矩阵行

- `C5M20-SCOPE-101/201/301/401`
- `C5M20-BLK-101~105`
- `C5M20-GAP-001~004`

## 验收

- 每个 scope row 都有对应 blocker 或 nonGoal。
- `backend_gap_classification.tsv` 明确无 C++ code landing。
- 不存在 `supported` 状态的 C5-M20 helper row。

## 非目标

- 不因 direct wrapper control 成功而替换 helper expected。
