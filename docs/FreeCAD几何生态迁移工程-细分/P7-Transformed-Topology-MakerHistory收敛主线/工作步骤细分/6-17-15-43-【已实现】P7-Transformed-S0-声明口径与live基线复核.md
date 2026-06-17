# 【已实现】P7 Transformed S0 声明口径与 live 基线复核

## 目标

冻结本主线的声明口径：P7 transformed family 已有几何执行和部分 topology / history evidence，但仍存在 bbox / volume-only native expected，不能直接宣称 topology / complete maker-history 已收敛。

## live 基线

- `pwd`：`/home/user/Chili3DProject/FreeCAD`
- `HEAD`：`5ebb3e2081`
- `git log -1 --oneline`：`5ebb3e2081 test: 补齐 ExternalGeometry 状态机原生 oracle`
- `git status --short -uall`：进入 S0 前工作区已有多处非本轮改动和 P7 seed 未跟踪文件；本轮只触碰 S0 相关文档 / TSV，不 reset、不 revert、不提交。
- `goal-step-runner` 队列复核：S0-S6 在执行前均为 pending；本轮只关闭 S0，S1-S6 继续待执行。

## 当前允许声明

- Mirrored、LinearPattern、PolarPattern、Scaled、MultiTransform 已有基础 executor、native expected 和 focused tests。
- 部分 fixture 已有 topology_counts，部分 fixture 只冻结 bbox / volume。
- transformed copy source alias、original stable alias、split / deleted terminal history 和 merge history 已有 P7 focused evidence。
- 当前主线目标是审计并收敛 topology oracle / maker-history，不是扩大 transformed feature 参数全集。

## 禁止声明

- 禁止说 P7 transformed family 的 topology 已全部发布完成。
- 禁止说 `transformed_pattern_full_history` 已覆盖所有复杂 pattern ownership。
- 禁止把 bbox / volume parity 当作 topology parity。
- 禁止从当前 cad-core 输出倒推 FreeCAD topology expected。
- 禁止用 fixture 名称、transform index、输出排序、面积/长度、bbox/volume 匹配补 ownership。

## 纳入范围

| 范围 | 纳入理由 | 当前状态 |
| --- | --- | --- |
| Mirrored Features / Whole shape topology | native expected 有 bbox / volume-only 行 | 待 S3 审计 |
| LinearPattern Features / Whole shape topology | 多个 fixture 只冻结 bbox / volume | 待 S3 审计 |
| PolarPattern Whole shape topology | Features 模式已有 topology 对照，Whole shape 仍需审计 | 待 S3 审计 |
| Scaled Whole shape topology | Features 模式已有 topology，对 Whole shape 只冻结 bbox / volume | 待 S3 审计 |
| MultiTransform linear+mirror / whole shape topology | native expected 只冻结 bbox / volume，Scaled diagonal 已有 topology 对照 | 待 S3/S5 审计 |
| AddSubShape slot ownership / transformed copy history | 当前已有 focused evidence，但 complete history 仍需发布闸门 | 待 S4/S6 审计 |

## 排除范围

| 范围 | 原因 | 用户 / 协议行为 |
| --- | --- | --- |
| 完整 PartDesign 参数全集 | 本主线只收敛 transformed topology / maker-history | 保持现有 unsupported diagnostics |
| 完整 Sketcher solver | P5/P6 已明确非目标 | 前端继续提交已解草图 |
| Assembly solver / Joint placement 解算 | 属于 P8 后续 | 当前 Assembly 输出 `solve=not_migrated` |
| 跨请求 shape cache / BREP state | 违反 CAD Core 无状态边界 | 只允许 `ReferenceShadow.brep` 单 subshape evidence |
| standalone geometry-equivalent native golden | 无 FreeCAD lifecycle / support 的 Whole shape 用例不能伪装成 native topology oracle | 保持 geometry-equivalent smoke test，或由 S3 替换成 support-backed native fixture |

## 状态词典

| 状态 | 含义 |
| --- | --- |
| `supported` | 有 FreeCAD source、cad-core 实现、checked-in expected 或 focused test 证据 |
| `notCollected` | 属于本主线范围，但缺 FreeCAD topology oracle 或 checked-in expected |
| `backendGap` | 有 FreeCAD 依据和 cad-core mismatch evidence，需要 C++ 实现 |
| `releaseGate` | 实现已有，但发布前需 topology / fallback / capability 审计 |
| `unsupported` | 本阶段不支持但协议可见，必须 diagnostic |
| `nonGoal` | 明确排除，必须有 reopen 条件 |

## 完成结果

- scope matrix 已明确：P7T-SCOPE-001 到 P7T-SCOPE-005 仍是 `notCollected`，bbox / volume-only topology 未收敛；只有采到 FreeCAD topology oracle 后出现 cad-core mismatch，才能升级为 `backendGap`。
- P7T-SCOPE-006 保持 `releaseGate`：已有 transformed history evidence 需要 S4/S5/S6 发布前审计，不能等同于 P7 topology 全完成。
- S3 已充分列出 Mirrored、LinearPattern、PolarPattern、Scaled、MultiTransform 的 bbox / volume-only topology oracle 审计清单；S0 只记录复核结论，不重复堆字。
- non-goal registry 已保留 Sketcher solver、Assembly solver、完整 transformed 参数全集、跨请求 BREP/cache、standalone geometry-equivalent native golden 的用户 / 协议行为和 reopen 条件。

## 验收

```bash
rg -n "bbox / volume|topology|transformed|notCollected|backendGap|releaseGate" \
  docs/CADCore方案/细化方案/10-P7-PartDesign常用生态.md \
  docs/FreeCAD几何生态迁移工程-细分/P7-Transformed-Topology-MakerHistory收敛主线
```

## 非目标

- S0 不采 FreeCAD oracle。
- S0 不修改 cad-core C++。
- S0 不重写 P7 既有 fixture expected。
