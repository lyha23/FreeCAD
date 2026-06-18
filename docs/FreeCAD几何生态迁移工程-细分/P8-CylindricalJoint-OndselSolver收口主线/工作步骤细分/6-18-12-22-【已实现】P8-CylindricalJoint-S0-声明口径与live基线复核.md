# P8 CylindricalJoint S0 声明口径与 live 基线复核

## 目标

冻结本包的支持声明、禁止声明、状态字典和 live 基线，避免把旧 P8 文档或未验收证据直接解释成已经发布。

## 输入

- `git status --short`
- `git submodule status --recursive`
- `src/3rdParty/OndselSolver`
- `cad-core/src/assembly/joint_solver.cpp`
- `cad-core/src/adapters/c_api/c_api.cpp`
- 既有 P8 AssemblySolver 主线和矩阵

## 声明口径

| 项 | 本包允许的声明 | 禁止声明 |
| --- | --- | --- |
| Cylindrical JointType | 只声明 FreeCAD 源码有 `ASMTCylindricalJoint` 映射，cad-core 可在 real Ondsel build 下实现 request-local placement writeback | 不声明完整 FreeCAD Joint 生命周期、limit 语义或 GUI drag/postDrag |
| Ondsel solver | 只支持 hard-linked `CAD_CORE_HAS_ONDSEL_SOLVER=1` | 不恢复 unlinked fallback、representative solver 或 silent no-op |
| placement writeback | `documentObjectUpdates.action=assembly_set_placement` 是前端 graph 更新建议 | 不把 CAD Core 作为跨请求 solver session 或 placement state store |
| 本轮变更证据 | 只有 build、focused tests、expected parity、capability/docs 同步通过后才发布 | 不把单个文件存在视为已发布结论 |

## 纳入 / 排除

| 分类 | 内容 | 状态 |
| --- | --- | --- |
| 纳入 | `Cylindrical -> ASMTCylindricalJoint` C++ 映射 | supported |
| 纳入 | c3m6 `assembly-grounded-cylindrical-joint-real-solver` fixture / expected | supported |
| 纳入 | C ABI supported / unsupported matrix | supported |
| 纳入 | Ondsel 子模块存在与构建通过 | supported |
| 排除 | RackPinion / Screw / Gears / Belt | unsupported |
| 排除 | Parallel / Perpendicular | unsupported |
| 排除 | Cylindrical length / angle limit 完整 lifecycle | notCollected |
| 排除 | GUI drag / postDrag / persistent solver session | nonGoal |

## 状态字典

| 状态 | 含义 |
| --- | --- |
| `supported` | build、focused tests、expected parity、capability/docs 同步全部通过 |
| `unsupported` | 有 FreeCAD 语义但本包不实现，必须稳定 diagnostic |
| `notCollected` | 还缺 native oracle 或细化语义证据 |
| `nonGoal` | 与无状态 CAD Core 边界冲突或明确不属于本包 |

## 验收标准

- `p8_cylindrical_joint_scope_review_matrix.tsv` 中不得出现无验收证据的 `supported`。
- `p8_cylindrical_joint_non_goal_registry.tsv` 必须包含 GUI drag / persistent session / extra JointType 三类边界。
- 必须记录当前构建基线：`src/3rdParty/OndselSolver/CMakeLists.txt` 存在，hard-linked Ondsel build 通过。
- 检查命令：

```bash
git status --short
git submodule status --recursive | rg 'src/3rdParty/OndselSolver'
test -f src/3rdParty/OndselSolver/CMakeLists.txt || true
rg -n "CYL-SCOPE-.*supported" docs/FreeCAD几何生态迁移工程-细分/P8-CylindricalJoint-OndselSolver收口主线/矩阵/p8_cylindrical_joint_scope_review_matrix.tsv
```

## 非目标

- S0 不修改 C++。
- S0 不采集 native oracle。
- S0 不把现有 P8 主线改名为已实现或未实现。
