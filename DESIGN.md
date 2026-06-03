'/Users/li/Chili3DProject/重构Chili/FreeCAD/docs/CADCore方案/00-CAD-Core抽取方案.md' 我希望你把这套方案按步骤细化一下, 整理到'/Users/li/Chili3DProject/重构
Chili/FreeCAD/docs/CADCore方案/细化方案', 要把每一个步骤干什么说清楚, 是一个可以用于实操的方案, 但是内容也不要太复杂, 现在需要搭的是一个 mvp

 /goal '/Users/li/Chili3DProject/重构Chili/FreeCAD/docs/CADCore方案/细化方案/00-CAD-Core完整抽取执行总览.md' 按照该方案继续为我实现, 细节查看'/Users/admin/Chili3DProject/重构Chili/FreeCAD/docs/CADCore方案/细化方案' , 完成每个部分的时候, 就更新'/Users/admin/Chili3DProject/重构Chili/FreeCAD/docs/CADCore方案/细化方案', 只写重要的最大的节点, 不要写流水账

 /goal 按照'UsersadminChili3DProject重构ChiliFreeCADdocs5-30-04-33-CADCore-fixture-expected迁移方案.md'为我实现, 完成每个部分的时候, 就更新'/Users/admin/Chili3DProject/重构Chili/FreeCAD/docs/5-30-04-33-CADCore-fixture-expected迁移方案.md', 只写重要的最大的节点, 不要写流水账

/goal  按照'/Users/admin/Chili3DProject/重构Chili/FreeCAD/docs/偏移处理/06-02-02-37-cad-core临时诊断主路径偏移整改方案.md' 为我实现, 完成每个部分的时候,就更新 '/Users/admin/Chili3DProject/重构Chili/FreeCAD/docs/偏移处理/06-02-02-37-cad-core临时诊断主路径偏移整改方案.md' 只写重要的最大的节点, 不要写流水账

---

  目标：在 /Users/li/Chili3DProject/重构Chili/FreeCAD 仓库中，完成 CAD Core 临时诊断主路径偏移整改的第一轮 goal：先完成 M0 fallback 闸门审计，再推进 M1
  WireJoiner EdgeInfo/WireInfo 生命周期中的下一个最小可验收切片。

  必须先读：

- AGENTS.md
- docs/偏移处理/CADCore临时诊断主路径偏移整改方案-细分/00-总入口.md
- docs/偏移处理/CADCore临时诊断主路径偏移整改方案-细分/M0-fallback闸门.md
- docs/偏移处理/CADCore临时诊断主路径偏移整改方案-细分/M1-WireJoiner-EdgeInfo-WireInfo生命周期.md
- 必要时读 docs/偏移处理/06-02-02-37-cad-core临时诊断主路径偏移整改方案.md 和 docs/偏移处理/06-02-04-01-WireJoiner完整账本迁移方案.md

  执行要求：

1. 从当前 worktree 真实状态开始，先执行 git status --short，不能回退或覆盖用户已有改动。
2. M0 只做 fallback 闸门审计：确认当前主路径里是否还有未登记的 midpoint、endpoint-touch、boundary-touch、same-coordinate、face count、partial
   overlap、fixture 名称分支、raw/internal geometry sampling 等偏移规则。
3. 如果 M0 发现未登记偏移，先收敛或登记到 M0 文档，不要直接扩展新规则。
4. M0 完成后，推进 M1 的一个最小可验收切片，优先补 FreeCAD WireJoiner 的真实 EdgeInfo/WireInfo 生命周期，而不是改输出端结果。
5. M1 实现前必须写清 FreeCAD 依据：本地 src/ 中的文件、类/函数、关键字段、调用顺序，以及 cad-core 对应落点。
6. 不要上网查 FreeCAD 行为；语义来源只用本地 /Users/li/Chili3DProject/重构Chili/FreeCAD/src。
7. 禁止新增 fixture 名称分支、midpoint、endpoint-touch、boundary-touch、same-coordinate、face count、partial overlap 等输出端形态规则。
8. 禁止继续扩展 generatedOpenExportShapeForSketchInternals() 或 purgeAsOriginalOpenEdge 作为新主路径规则；如果当前切片碰到它们，必须归因到 M1/M2/M3 的
   边界并写入文档。
9. 每完成一个切片，只更新对应 milestone 文档，记录关键基线、FreeCAD 依据、已完成语义、剩余风险和验收命令；不要写流水账。
10. 如果 fixture 失败，按 M7 的归因顺序判断，不要直接在 executor、SketchInternalBuilder、response 或测试里补特判。

  建议验证：
  cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
  cmake --build build
  python3 -m unittest tests/test_mvp.py
  python3 -m unittest tests/test_p5_sketch.py tests/test_p6_topology.py
  git diff --check

  如果本轮只改文档，可以不跑构建，但必须说明原因并执行 git diff --check。

  本轮完成条件：

- M0 fallback 闸门审计完成，并更新 M0 文档中的当前基线或风险。
- M1 至少推进一个可验收切片，或明确证明当前阻塞属于 M2/M3/M4 并更新 M1 文档。
- 相关代码或文档已更新。
- 已执行必要验证。
- final 回复说明：改了哪些文件、验证结果、M1 还剩什么、下一轮最合理目标是什么。

  这个 prompt 的关键是把 goal 的范围锁死在 M0 + M1 一个切片，不要让它一口气跳到 M2/M3/M4。

---

我的FreeCAD 仓库中docs/偏移处理/CADCore临时诊断主路径偏移整改方案-细分/M3-generated-result-wire-identity.md' 这个里程碑执行过程始终无法收敛，无法结束。 我希望你给我出一个 用于修改  CADCore临时诊断主路径偏移整改方案-细分 中内容 的 独立的 .patch 文件，不要尝试修改github中的内容，你没有权限


---




  继续在 /Users/li/Chili3DProject/重构Chili/FreeCAD 里实现 CADCore 偏移处理 / WireJoiner 临时诊断主路径偏移整改，不要从头规划，直
  接接当前未提交工作区继续。

  先做：

1. 运行 git status --short，确认当前 dirty files，不要回退任何现有改动。
2. 读取 docs/偏移处理/CADCore临时诊断主路径偏移整改方案-细分/README.md、03-实施切片与文件落点.md、04-验收矩阵与回归命令.md、05-
   旧计数器映射表.md。
3. 对照 cad-core/include/cad_core/geometry/wire_joiner.h、cad-core/src/geometry/wire_joiner.cpp、cad-core/src/features/
   sketch_object.cpp、cad-core/src/topo/named_shape.cpp、cad-core/tests/test_p5_sketch.py 当前 diff 继续实现。

  当前已完成并通过验证：

- P0 / P1 / M2S 主账本落地：ResultWireProducerKind / State / Blocker / Identity / LedgerEntry 已接入 EdgeInfo、
  OpenWireCompoundWireInfo、open-export history 和 JSON。
- P3 unowned-removal + member-suppressed current-member ready 子集已切到 ExportedWithoutHelper。
- P5 strict-source sidecar 子步已完成。
- P5 full-evidence source-shape 子步已完成。
- P5 live final-gate open edge 子步已完成。
- P4 primary-removal current-member 子步已完成；secondary branch 还没开放。
- 当前 build 和 tests 已通过：cmake --build build、python3 -m unittest tests/test_p5_sketch.py、tests/test_mvp.py、tests/
  test_p6_topology.py，git diff --check 也通过。

  下一步重点：

- 继续处理 P5 missing/foreign evidence。
- 继续处理 P4 secondary branch。
- 继续压缩剩余 SourceShapeIdentityNotReady。
- 等 open_wire_compound_legacy_helper_shape_wire_info_count == 0 后，才进入 P6 删除 legacy helper shape /
  generatedOpenExportShapeForSketchInternals 最终重命名或删除。

  硬性约束：

- 不要新增 helper_open_export_override_* 这类继续扩散的细分字段；确需扩展必须走 ResultWireBlocker enum。
- 不要通过 fixture 名称、输出数量、几何猜测、getOpenWires() 输出端裁剪、sibling member pruning 来凑结果。
- 保持 FreeCAD 语义来源：优先读本仓库 src/Mod/Part/App/WireJoiner.cpp，并在新增承载语义的 public API / enum / struct / 主路径注
  释里写 FreeCAD 依据。
- 文档只在大节点更新，不写流水账。
- 完成一个实质切片后运行：cmake --build build；python3 -m unittest tests/test_p5_sketch.py；必要时补 tests/test_mvp.py 和
  tests/test_p6_topology.py；最后 git diff --check。
