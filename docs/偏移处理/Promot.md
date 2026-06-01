  请在本地仓库 `/Users/admin/Chili3DProject/重构Chili/FreeCAD` 中，严格按照方案文档：

  `docs/偏移处理/06-02-04-01-WireJoiner完整账本迁移方案.md`

  实施 `cad-core` 的 WireJoiner 完整账本迁移。

  目标不是继续修 fixture，而是按 FreeCAD 语义迁移完整 `WireJoinerP::aHistory`、`EdgeInfo/WireInfo`、
  `findTightBound()` / `exhaustTightBound()` 生命周期，并最终替换当前 partial result-wire evidence
  collector。

  必须先阅读：
  1. `AGENTS.md`
  2. `docs/偏移处理/06-02-04-01-WireJoiner完整账本迁移方案.md`
  3. `docs/偏移处理/06-02-02-37-cad-core临时诊断主路径偏移整改方案.md`
  4. FreeCAD 依据源码：
     - `src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildInternals()`
     - `src/Mod/Part/App/WireJoiner.cpp::WireJoinerP`
     - `WireJoinerP::build()`
     - `splitEdges()`
     - `buildAdjacentList()`
     - `findClosedWires()`
     - `findTightBound()`
     - `exhaustTightBound()`
     - `getOpenWires()`

  实施要求：
  - 严格按方案阶段推进，不要跳过账本直接改输出。
  - 不允许新增 fixture 名称分支、T/cross/overlap/dangling 形态特判。
  - 不允许在 `SketchObject`、`SketchInternalBuilder`、adapter、response 层补 result-wire 规则。
  - 不允许靠 raw/internal 几何采样发明 WireJoiner split/generated/deleted history。
  - `topo` 只能消费 WireJoiner / FaceMaker 产出的 history，不能自己猜 history。
  - 当前 `resultWireEvidence_`、graph-cycle owner、`ownerContributesToLedger`、partial evidence collector 只
  能作为过渡路径，最终要删除或降级为 diagnostic。
  - 每完成一个大的阶段节点，更新方案文档，只记录关键结论、FreeCAD 依据、落点、剩余风险和最终验证结果，不写流
  水账。

  工作流程：
  1. 先 `git status --short`，确认工作区边界，不要回退用户已有改动。
  2. 从阶段 0 开始建立 trace / ledger 基线。
  3. 逐步迁移：
     - EdgeInfo / WireInfo / VertexInfo 原始账本
     - splitEdges() 与 aHistory 初始能力
     - buildAdjacentList() / findClosedWires()
     - findTightBound()
     - exhaustTightBound()
     - compound / openWireCompound 导出
     - topo history consumer
     - SketchInternalBuilder 主路径切换
     - 删除临时 partial evidence
  4. 每一步都要能说明 FreeCAD 对应文件、函数、字段和 cad-core 落点。
  5. 如果某阶段发现现有方案不够，不要继续叠 fallback；先回到 FreeCAD 源码确认账本语义，再更新方案。

  验收命令：
  ```bash
  cd /Users/admin/Chili3DProject/重构Chili/FreeCAD/cad-core
  cmake --build build
  python3 -m unittest tests/test_mvp.py
  python3 -m unittest tests/test_p5_sketch.py tests/test_p6_topology.py

  完成定义：

  - WireJoiner::getOpenWires() 输出只来自真实 final EdgeInfo lifecycle。
  - WireJoiner generated / modified / deleted history 进入 topo 消费路径。
  - SketchInternalBuilder 不再包含 result-wire 复制或形态判断。
  - 当前 partial evidence collector 被删除，或只剩完全不影响输出的 diagnostic。
  - MVP、P5 Sketch、P6 Topology 全部通过。
  - 更新 docs/偏移处理/06-02-04-01-WireJoiner完整账本迁移方案.md 和必要的整改状态文档。
