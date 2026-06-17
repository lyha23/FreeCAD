# P6 MakerHistory S2 范围准入与 blocker 矩阵

## 目标

把 S1 候选压成 scope review、blocker queue、nonGoal registry 和 backend gap classification。S2 只做分类和路由，不采 oracle，不写 C++。

## 执行结论

2026-06-17 已完成 S2 分类：`P6MH-SCOPE-001/002/003/004/006` 保持 `releaseGate`，`P6MH-SCOPE-005` 保持 `notCollected`，当前没有满足 FreeCAD authority + cad-core mismatch 双证据的 `backendGap`。`P6MH-BLOCK-001..005` 已分别路由到 S3、S4、S5 或 S6；`P6MH-NG-001..005` 保留完整非目标边界；`P6MH-BG-001..005` 只记录 backendGap 闸门，不作为 C++ bug 结论。

## 分类规则

| 状态 | 进入条件 | 下一步 |
| --- | --- | --- |
| `supported` | 当前源码、focused tests、expected/capability 和文档结论一致 | 保留回归 |
| `releaseGate` | 代码或 capability 显示已覆盖，但文档 / expected / 状态声明仍不一致 | S3-S6 发布闸门 |
| `notCollected` | 缺 FreeCAD oracle 或 checked-in expected | S3-S5 采集或明确 nonGoal |
| `backendGap` | FreeCAD authority + oracle/mismatch 证明 cad-core 不一致 | S6 C++ |
| `unsupported` | 语义可见但本主线不能安全实现 | diagnostic 或后续专项 |
| `nonGoal` | 明确排除 | 写入 nonGoal registry |

## blocker 路由

| blocker 类型 | 来源状态 | 处理步骤 |
| --- | --- | --- |
| live drift | `releaseGate` | S0/S2/S6 文档和 capability 发布 |
| ShapeFix producer | `releaseGate` / `notCollected` / `backendGap` | S3 复审，必要时 S6 C++ |
| DressUp / Refine propagation | `releaseGate` / `notCollected` / `backendGap` | S4 复审，必要时 S6 C++ |
| taper partial/full | `releaseGate` / `notCollected` / `backendGap` | S5 复审，必要时 S6 C++ |
| complex split recovery | `notCollected` / `backendGap` | 先 oracle，再 S6 |

## nonGoal 要求

每个 nonGoal 必须说明：

- 排除原因。
- 对用户或协议的实际表现。
- 重新打开条件。

## 必须回写的矩阵行

- `P6MH-SCOPE-001` 到 `P6MH-SCOPE-006`。
- `P6MH-BLOCK-001` 到 `P6MH-BLOCK-005`。
- `P6MH-NG-001` 到 `P6MH-NG-005`。
- `P6MH-BG-001` 到 `P6MH-BG-005`。

## 验收标准

- 每个 scope 都有合法 `current_status`。
- 每个 blocker 都引用一个 scope。
- 每个 backendGap 分类都必须解释是否需要产品决策和下一步。
- 不允许没有 FreeCAD authority / mismatch evidence 的 `backendGap`。
- 执行：

```bash
python3 - <<'PY'
from pathlib import Path
root = Path('docs/FreeCAD几何生态迁移工程-细分/P6-MakerHistory-ShapeFix-DressUp-Taper收敛主线/矩阵')
valid = {'supported', 'releaseGate', 'notCollected', 'backendGap', 'unsupported', 'nonGoal'}
rows = (root / 'p6_maker_history_scope_review_matrix.tsv').read_text().splitlines()[1:]
bad = []
for line in rows:
    fields = line.split('\t')
    if len(fields) >= 4 and fields[3] not in valid:
        bad.append((fields[0], fields[3]))
if bad:
    raise SystemExit(f'bad statuses: {bad}')
PY
for f in docs/FreeCAD几何生态迁移工程-细分/P6-MakerHistory-ShapeFix-DressUp-Taper收敛主线/矩阵/*.tsv; do
  awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"
done
git diff --check
```

## 非目标

- 不把 releaseGate 当成 C++ bug。
- 不把 notCollected 直接转 backendGap。
- 不重写 P6/P7 正式方案文档；S6 发布闸门才决定是否回写。
