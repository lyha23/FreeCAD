# CADCore3 走近路后续真实语义收口 Goal Prompt

下面 prompt 可直接用于开启实现 goal，控制在 4000 字以内。

```text
请在本地仓库 `/Users/admin/Chili3DProject/重构Chili/FreeCAD` 中，按照
`docs/偏移处理/CADCore3走近路后续真实语义收口/` 的总控与子方案，实现 CADCore3 走近路后的真实语义收口。

目标不是继续写方案，也不是只改 capability 文档，而是把仍处于 representative / contract / bridge / partial 的实现推进到真实 FreeCAD 语义链路。

必须先阅读：

1. `AGENTS.md`
2. `docs/偏移处理/CADCore3走近路后续真实语义收口/00-总控.md`
3. `01-真实OndselSolver迁移方案.md`
4. `02-CopyOnChange完整DeepCopyLifecycle迁移方案.md`
5. `03-WireJoinerBridge彻底删除方案.md`
6. `docs/CADCore3.0/00-总览.md`
7. `docs/CADCore3.0/capabilities-gap对照表.md`
8. `docs/CADCore3.0/FreeCAD语义矩阵.md`

开始前执行：

```bash
git status --short
```

只改本 goal 相关文件，不要回退、覆盖或清理用户/其他任务已有改动。

## 实施顺序

三条线必须独立推进、独立验收、独立更新状态，不要混成一个半成品。

默认先做 `02-CopyOnChange完整DeepCopyLifecycle迁移方案.md`：

- 把 `copy_on_change_deep_copy_lifecycle.status=partial` 推进到真实 deep copy 生命周期。
- 实现 property tree deep copy、child/group copy、internal link relink、dependency graph rewrite、history preserve、touched 后 sync。
- Link executor 只能调用 App / document 层 CopyOnChange API，adapter 只做协议转换。
- deep copy、relink、history preserve、sync case 全通过后，才能改为 `covered_full`。

再做 `01-真实OndselSolver迁移方案.md`：

- 把 Assembly 从 representative solver / placement writeback contract 推进到真实 Ondsel solver adapter。
- 先复核 FreeCAD `AssemblyObject::solve()`、`fixGroundedParts()`、`makeMbdJointOfType()`、`setNewPlacements()`、`validateNewPlacements()`。
- 建立 `AssemblySolveRequest` / `AssemblySolveResult` / joint constraint DTO / validation diagnostics。
- solver result 必须来自真实 Ondsel/MBD 求解与 validation，不得用静态 representative placement 假装 full。
- 若真实 Ondsel 依赖或构建条件缺失，给出源码级/构建级 blocker 证据，不要改成 `covered_full`。

最后做 `03-WireJoinerBridge彻底删除方案.md`：

- 删除 generated open export / purge-as-original helper bridge 主路径。
- open wire ownership 必须来自 FreeCAD WireJoiner 账本：`sourceEdgeArray`、`aHistory`、`myShapesToReturn`、`openWireCompound`、`EdgeInfo`、`WireInfo`、`wireInfo2`、`iteration/iteration2`、`superEdge`。
- 不允许新增 fixture-specific pruning、几何类型分支、source/split 形态猜测、sketch executor 或 adapter 层补丁。
- 若几何已一致但 stable subname / InternalEdge / InternalVertex 仍不同，应补 `MapperHistory(aHistory)` 到 `NamedShape` / `ElementMap` 的传播，不要在输出端修。

如果当前 blocker 明确来自 WireJoiner internal face / open wire ownership，可以先做第 3 条，但仍必须只围绕 WireJoiner 账本闭环。

## FreeCAD 依据

每个实质语义迁移点，写代码前必须确认本地 FreeCAD 调用链，并在相邻 C++ 注释或方案增量中标注：

- FreeCAD 源文件绝对路径
- 类/函数名
- 关键字段或原文短句
- 调用顺序
- CAD Core 落点

不得只写“参考 FreeCAD”。`/Users/li/...` 与 `/Users/admin/...` 只要仓库相对路径和函数一致，就视为同一源码树依据。

## 禁止事项

- 不从 fixture 输出倒推业务逻辑。
- 不新增 fixture 名称分支、几何类型组合分支、source edge 猜测、split edge compound 注入、degenerate face 注入或导出顺序修正。
- 不把 FreeCAD 业务语义塞进 adapter、response serializer、graph 层或 sketch executor。
- 不把 representative / contract / bridge path 改名后继续当 full implementation。
- 不在 fallback 上叠加 helper reason。若短期保留 fallback，必须写清临时性、边界、FreeCAD 正确路径和删除条件。

## 文档同步

每完成一个独立子方案，同步更新：

- `docs/CADCore3.0/capabilities-gap对照表.md`
- `docs/CADCore3.0/FreeCAD语义矩阵.md`
- 必要时更新 `docs/CADCore3.0/oracle-fixture队列.md`

capability 状态必须准确：transport 只能是 `covered_contract`；representative 只能是 `covered_representative`；helper bridge 仍参与主路径就是 `bridge`；真实 FreeCAD 语义链路和验收矩阵都通过后，才能写 `covered_full`。

## 验收命令

代码修改后先做：

```bash
git diff --check -- cad-core docs/CADCore3.0 docs/偏移处理
```

CopyOnChange / Assembly 阶段回归：

```bash
cd /Users/admin/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest tests.test_adapters
python3 -m unittest tests.test_p8_features
```

WireJoiner 阶段回归：

```bash
cd /Users/admin/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest tests.test_p5_sketch
python3 -m unittest tests.test_p6_topology
```

只有阶段收口、runner/oracle 改动或用户明确要求时，才执行：

```bash
cd /Users/admin/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest
```

## 完成定义

本 goal 至少要完成一个独立子方案的真实语义闭环：

- 代码主路径切到真实 FreeCAD 语义。
- 对应 fixtures / tests 补齐并通过。
- capability / 语义矩阵 / oracle 队列同步。
- 未完成子方案仍保持准确 gap 状态，不得提前改成 full。

最终回复说明：完成了哪条子方案、关键文件、FreeCAD 依据链路、验收结果、剩余真实语义 gap。
```
