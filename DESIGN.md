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


---



  可直接用这个 goal prompt：

  目标：落地 CAD Core 2.0 C2-M2：FaceMaker / WireJoiner history producer。基于当前已完成但未提交的 C2-M1 MapperHistory core 继续推进，让 Sketch InternalShape 的 InternalFace / InternalEdge / InternalVertex 来源由
  FaceMaker / WireJoiner 的真实 history evidence 解释，而不是继续依赖 summary-only 或复杂几何匹配。

  工作目录：
  /Users/li/Chili3DProject/重构Chili/FreeCAD

  当前前提：

- C2-M1 已完成，工作区保留未提交改动。
- 已有 cad-core/include/cad_core/topo/mapper_history.h 与 cad-core/src/topo/mapper_history.cpp。
- NamedShape JSON 已新增 mapper_history，并保留旧 history / element_history_status。
- ElementMap 只写唯一 target；split / deleted / ambiguous 只进入 mapper_history / diagnostics，不猜唯一目标。
- capabilities 已有 topo_history.mapper_history_core。

  开始前必须执行：
  git status --short

  必须先阅读：

- docs/CADCore2.0/README.md
- docs/CADCore2.0/00-总览.md
- docs/CADCore2.0/01-P5P6-ExternalGeometry-TopoNaming主线.md
- docs/CADCore2.0/04-验收矩阵与交付规则.md
- cad-core/include/cad_core/topo/mapper_history.h
- cad-core/src/topo/mapper_history.cpp
- cad-core/include/cad_core/topo/named_shape.h
- cad-core/src/topo/named_shape.cpp
- cad-core/include/cad_core/geometry/face_maker.h
- cad-core/src/geometry/face_maker.cpp
- cad-core/include/cad_core/geometry/wire_joiner.h
- cad-core/src/geometry/wire_joiner.cpp
- cad-core/include/cad_core/geometry/sketch_internal_builder.h
- cad-core/src/geometry/sketch_internal_builder.cpp
- cad-core/src/features/sketch_object.cpp
- cad-core/src/topo/element_map.cpp
- cad-core/tests/test_p5_sketch.py
- cad-core/tests/test_p6_topology.py

  FreeCAD 语义依据必须复核并写入相邻 C++ 注释：

- src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildInternals()
- src/Mod/Part/App/FaceMaker.cpp::FaceMaker::postBuild()
- src/Mod/Part/App/FaceMakerBuildFace.cpp
- src/Mod/Part/App/WireJoiner.cpp::WireJoiner::getOpenWires()
- src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::build()
- src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::buildClosedWire()
- src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findTightBound()
- src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findTightBoundSplitWire()
- src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findTightBoundUpdateVertices()
- src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::exhaustTightBoundUpdateWire()
- src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap()

  实施要求：

1. 不重做 C2-M1，不重命名 mapper_history schema，保持现有测试兼容。
2. 将 FaceMakerHistorySummary 从 count/flag summary 推进到可消费 evidence：
   - 表达 pre_split / splitter stage；
   - 表达 source EdgeN 到 bounded InternalFaceN outer boundary 的 generated 关系；
   - 表达 source edge one-to-many split；
   - 表达 source edge one-to-zero deleted；
   - evidence 中保留 maker stage、source edge index、target internal element、bounded face index、splitter/pre-split 依据。
3. 将 WireJoinerHistorySummary / WireJoinerLedgerSummary 中已有 EdgeInfo / WireInfo / producer identity 信息转为 MapperHistory event：
   - open-wire carry-through 用 preserved 或 generated event 表达；
   - source edge split fragment 用 split event 表达；
   - noOriginal purge / deleted source 用 deleted event 表达；
   - helper open export override、producer blocker、source lineage、child wire identity 进入 evidence / diagnostic_status；
   - 不靠 bbox、面积、输出顺序、fixture 名称或几何类型排序猜 source ownership。
4. topo 层消费 producer evidence：
   - 优先在 named_shape.cpp 或新增 topo 辅助文件中完成转换；
   - InternalFaceN 来源于 FaceMaker outer boundary history；
   - InternalEdgeN / InternalVertexN 的 split / deleted 来源于 WireJoiner / FaceMaker evidence；
   - internal_element_map 只保留简单唯一 alias，不再承担复杂 split 判断。
5. features/sketch_object.cpp 只传递 FaceMaker / WireJoiner 产生的结构化 evidence，不在 executor 中合成 split history。
6. 删除或隔离 C2-M1 中 summary-only mapper_history 事件，只有当 producer evidence 不足时保留 diagnostic-only event，并写清 blocker。
7. 若某个 WireJoiner producer 还缺完整 FreeCAD 账本，不要补 fixture 特判；输出明确 diagnostic_status，例如 missing_producer_identity、source_shape_identity_not_ready、no_original_purge、ambiguous_owner。
8. 补 focused tests：
   - FaceMaker bounded InternalFaceN 的 generated mapper_history；
   - source edge one-to-many split；
   - source edge deleted 不写唯一 ElementMap；
   - WireJoiner open-wire carry-through；
   - noOriginal purge diagnostic；
   - internal_element_map 不承担复杂 split 判断。
9. 同步 docs/CADCore2.0，只更新 C2-M2 当前基线、已完成语义调整、剩余缺口、验收命令和下一步；不要写流水账。

  优先修改文件：

- cad-core/include/cad_core/geometry/face_maker.h
- cad-core/src/geometry/face_maker.cpp
- cad-core/include/cad_core/geometry/wire_joiner.h
- cad-core/src/geometry/wire_joiner.cpp
- cad-core/include/cad_core/geometry/sketch_internal_builder.h
- cad-core/src/geometry/sketch_internal_builder.cpp
- cad-core/include/cad_core/topo/named_shape.h
- cad-core/src/topo/named_shape.cpp
- cad-core/include/cad_core/topo/mapper_history.h
- cad-core/src/topo/mapper_history.cpp
- cad-core/src/features/sketch_object.cpp
- cad-core/tests/test_p5_sketch.py
- cad-core/tests/test_p6_topology.py
- docs/CADCore2.0/01-P5P6-ExternalGeometry-TopoNaming主线.md
- docs/CADCore2.0/04-验收矩阵与交付规则.md

  验证命令：
  cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
  cmake --build build
  python3 -m unittest tests.test_p5_sketch tests.test_p6_topology tests.test_adapters

  若以上通过，再运行：
  python3 -m unittest tests.test_mvp tests.test_diagnostics tests.test_feature_flows tests.test_adapters tests.test_p5_sketch tests.test_p6_topology tests.test_p7_features tests.test_p8_features
  tests.test_expected_fixtures

  最后运行：
  cd /Users/li/Chili3DProject/重构Chili/FreeCAD
  git diff --check

  非目标：

- 不做 C2-M3 ExternalGeometry Defining / Frozen / Detached / Missing / Sync 完整状态机。
- 不做 C2-M4 Sketch InternalShape 主路径最终切换和 fallback 全删除，除非 C2-M2 evidence 已自然满足。
- 不做 C2-M5+ Refine / taper / transformed / DressUp history 收敛。
- 不做 Link / Assembly / Web adapter 产品化。
- 不新增 fixture 名称分支、几何类型排序、bbox / 面积 / 输出顺序猜测。
- 不提交代码，除非用户明确要求。

  完成标准：

- FaceMaker / WireJoiner 不再只是 summary/count，能产出结构化 MapperHistory evidence。
- Sketch InternalShape 的 mapper_history 至少能解释 InternalFace generated、InternalEdge/InternalVertex split/deleted/open carry-through 的核心场景。
- ElementMap 仍只包含唯一 target。
- C2-M1 现有 mapper_history schema 和回归测试不倒退。
- focused tests 和相关回归命令通过。
- docs/CADCore2.0 同步 C2-M2 当前状态和剩余缺口。

这一步完成后，下一阶段才适合做 C2-M3：Reference resolver + ExternalGeometry 状态机。

---




  目标：完整落地 /Users/li/Chili3DProject/重构Chili/FreeCAD/docs/CADCore2.0 中定义的 CAD Core 2.0 方案，把当前 cad-core 从已完成 C2-M0/C2-M1 的 MapperHistory core，推进到 C2-M8 验收冻结。不要按小分片停止，也不要每完成一个
  阶段就等待用户继续；内部可以按 C2-M2 -> C2-M8 顺序推进，但只有当 CAD Core 2.0 整体完成、验证通过、文档同步后才标记 goal complete。

  工作目录：
  /Users/li/Chili3DProject/重构Chili/FreeCAD

  当前前提：

- C2-M0/C2-M1 已完成但尚未提交。
- 工作区保留未提交改动，包括 mapper_history core、NamedShape mapper_history 输出、capabilities topo_history.mapper_history_core、相关测试和 CADCore2.0 文档同步。
- 继续基于当前工作区推进，不要回退、覆盖或重新规划已完成内容。
- 本地 FreeCAD src/ 是唯一语义来源，不使用 web。
- cad-core 仍必须保持无状态 CAD Core 边界：DocumentObject graph 是唯一持久源数据；shape、mesh、NamedShape、ElementMap、MapperHistory 都是单次 recompute 产物。

  必须先阅读：

- AGENTS.md
- docs/CADCore2.0/README.md
- docs/CADCore2.0/00-总览.md
- docs/CADCore2.0/01-P5P6-ExternalGeometry-TopoNaming主线.md
- docs/CADCore2.0/02-P6P7-History-PartDesign收敛.md
- docs/CADCore2.0/03-P8-Link-Assembly-Adapter产品化.md
- docs/CADCore2.0/04-验收矩阵与交付规则.md
- cad-core/include/cad_core/topo/mapper_history.h
- cad-core/src/topo/mapper_history.cpp
- cad-core/include/cad_core/topo/named_shape.h
- cad-core/src/topo/named_shape.cpp
- cad-core/include/cad_core/topo/reference_matcher.h
- cad-core/src/topo/reference_matcher.cpp
- cad-core/include/cad_core/geometry/face_maker.h
- cad-core/src/geometry/face_maker.cpp
- cad-core/include/cad_core/geometry/wire_joiner.h
- cad-core/src/geometry/wire_joiner.cpp
- cad-core/src/features/sketch_object.cpp
- cad-core/src/features/feature_extrude.cpp
- cad-core/src/features/pad.cpp
- cad-core/src/features/pocket.cpp
- cad-core/src/features/dress_up.cpp
- cad-core/src/features/transformed.cpp
- cad-core/src/features/link.cpp
- cad-core/src/runtime/recompute.cpp
- cad-core/src/adapters/c_api/c_api.cpp
- cad-core/tests/

  整体实施顺序：

1. 先确认当前工作区和测试基线，保留 C2-M1 已完成成果。
2. 落地 C2-M2：FaceMaker / WireJoiner history producer。
   - FaceMaker pre-split / splitter 产出可消费 MapperHistory evidence。
   - WireJoiner EdgeInfo / WireInfo / producer identity / noOriginal / splitWire / done lifecycle 转成 MapperHistory event。
   - InternalFaceN 来源由 FaceMaker outer boundary history 解释。
   - InternalEdgeN / InternalVertexN 的 split / deleted / carry-through 由 WireJoiner / FaceMaker evidence 解释。
   - internal_element_map 只保留简单唯一 alias，不承担复杂 split 判断。
3. 落地 C2-M3：Reference resolver + ExternalGeometry 状态机。
   - 在 runtime/topo 建立统一 reference resolver。
   - 统一处理 SubList、StableSubList、source-prefixed stable key、mapped postfix、ReferenceShadow fingerprint、ReferenceShadow.brep。
   - 在 document/features/sketch_object 中支持 ExternalGeometry Defining / Frozen / Detached / Missing / Sync。
   - 成功恢复输出 elementReferenceUpdates / documentObjectUpdates；失败输出稳定 diagnostics。
   - 一对多 split 不猜唯一目标。
4. 落地 C2-M4：Sketch InternalShape 主路径切换。
   - Sketch.InternalShape 的 InternalFace / InternalEdge / InternalVertex 命名全部通过 MapperHistory / ElementMap 或 diagnostics 解释。
   - 删除或隔离 summary-only、geometry-match、fixture-specific fallback。
   - features/sketch_object.cpp 只表达 FreeCAD SketchObject 调用顺序，不承担 split history 合成。
5. 落地 C2-M5：ShapeFix / Refine / taper / DressUp history 收敛。
   - RefineModel Modified / IsDeleted / generated face history 转成 MapperHistory event。
   - taper BRepOffsetAPI_ThruSections / section source history 从 known_gap:taper_history 收敛到正式 history，或收敛为更窄的 explicit known gap。
   - ShapeFix history 覆盖会改变 edge / wire / face 身份的主路径。
   - Fillet / Chamfer / DressUp AddSubShape slot 级 NamedShape 持续传播 source alias、terminal split / deleted、merge history。
6. 落地 C2-M6：PartDesign transformed / pattern 复杂 ownership。
   - Mirrored / LinearPattern / PolarPattern / Scaled / MultiTransform 的 Features / Whole shape 模式消费统一 AddSubShape slot history。
   - chain support、SupportTransform、refined prefix support、multi-original Add/Sub replay 的 source ownership 明确。
   - transformed copy 按 FreeCAD copyElementMap(tmp, op) 等价语义传播 source-prefixed alias，不从 result 几何倒推。
   - pattern 后进入 Body boolean、DressUp、Link retag 时继续传播 terminal history。
7. 落地 C2-M7：Link / Assembly / adapter 产品化。
   - ShowElement create / claim / sync / delete 建议形成稳定 transaction schema。
   - ElementList / ElementCount / PlacementList / ScaleList / VisibilityList 写回优先级明确。
   - 补完整 _ChildCache、copy-on-change、linked-owner、plain group nested child 生命周期。
   - PropertyXLink、FullSubList、mapped postfix、文档哈希、source-prefixed stable key 生命周期与 resolver 统一。
   - Assembly Joint / GroundedJoint 至少有 solver adapter interface、基础 placement update 成功路径和失败 diagnostics。
   - CLI / C ABI / Web adapter 共用 cad-core-lib recompute 入口，adapter 不承载建模语义。
8. 落地 C2-M8：验收冻结。
   - oracle、fixture、capabilities、diagnostics、接口字段、CADCore2.0 文档全部同步。
   - remaining gaps 必须以 capabilities、diagnostics 或 known-gap expected 显式暴露。
   - 不允许还有静默 fallback 或无法解释的 fixture pass。

  FreeCAD 依据纪律：

- 每个承载 FreeCAD 语义的新类型、新字段、新函数、新 mapper 规则、新 resolver 规则，都必须在相邻 C++ 注释中标注 FreeCAD 依据。
- 注释必须包含 FreeCAD 源文件路径、类/函数名、关键字段或短句。
- 必须优先读取本地 src/ 中对应实现，不从 fixture 输出倒推业务逻辑。

  关键 FreeCAD 文件：

- src/App/ElementMap.cpp
- src/App/MappedName.cpp
- src/App/GeoFeature.cpp
- src/App/PropertyLinks.cpp
- src/App/Link.cpp
- src/App/Link.h
- src/Mod/Sketcher/App/SketchObject.cpp
- src/Mod/Sketcher/App/SketchObjectExternal.cpp
- src/Mod/Sketcher/App/ExternalGeometryExtension.cpp
- src/Mod/Part/App/TopoShape.cpp
- src/Mod/Part/App/TopoShapeExpansion.cpp
- src/Mod/Part/App/TopoShapeMapper.cpp
- src/Mod/Part/App/PropertyTopoShape.cpp
- src/Mod/Part/App/FaceMaker.cpp
- src/Mod/Part/App/FaceMakerBuildFace.cpp
- src/Mod/Part/App/WireJoiner.cpp
- src/Mod/Part/App/modelRefine.cpp
- src/Mod/PartDesign/App/Feature.cpp
- src/Mod/PartDesign/App/FeatureAddSub.cpp
- src/Mod/PartDesign/App/FeatureExtrude.cpp
- src/Mod/PartDesign/App/FeaturePad.cpp
- src/Mod/PartDesign/App/FeaturePocket.cpp
- src/Mod/PartDesign/App/FeatureRefine.cpp
- src/Mod/PartDesign/App/FeatureDressUp.cpp
- src/Mod/PartDesign/App/FeatureFillet.cpp
- src/Mod/PartDesign/App/FeatureChamfer.cpp
- src/Mod/PartDesign/App/FeatureTransformed.cpp
- src/Mod/PartDesign/App/FeatureMirrored.cpp
- src/Mod/PartDesign/App/FeatureLinearPattern.cpp
- src/Mod/PartDesign/App/FeaturePolarPattern.cpp
- src/Mod/PartDesign/App/FeatureScaled.cpp
- src/Mod/PartDesign/App/FeatureMultiTransform.cpp
- src/Mod/Assembly/App/AssemblyObject.cpp
- src/Mod/Assembly/App/JointGroup.cpp
- src/Mod/Assembly/App/JointObject.cpp

  禁止事项：

- 不按 fixture 名称写分支。
- 不靠 bbox、面积、输出顺序、几何类型排序、source edge 猜测修正输出。
- 不在 adapter、runtime 输出层、document parser 或 sketch executor 中塞拓扑命名业务规则。
- 不把 BREP 作为长期状态；唯一例外仍是 ReferenceShadow.brep 的旧单 subshape snapshot。
- 不把 Link / Assembly 做成绕过 MapperHistory / resolver 的旁路。
- 不运行完整 FreeCAD 全仓库构建。
- 不提交代码，除非用户明确要求。
- 不因为某阶段工作量大就标记 partial complete；goal complete 只能在 CAD Core 2.0 整体冻结后执行。

  测试与验收：
  每次较大代码变更后运行相关 focused tests；最终必须运行：

  cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
  cmake --build build
  python3 -m unittest tests.test_mvp tests.test_diagnostics tests.test_feature_flows tests.test_adapters tests.test_p5_sketch tests.test_p6_topology tests.test_p7_features tests.test_p8_features
  tests.test_expected_fixtures

  最后运行：

  cd /Users/li/Chili3DProject/重构Chili/FreeCAD
  git diff --check

  文档同步：

- 同步 docs/CADCore2.0/README.md
- 同步 docs/CADCore2.0/00-总览.md
- 同步 docs/CADCore2.0/01-P5P6-ExternalGeometry-TopoNaming主线.md
- 同步 docs/CADCore2.0/02-P6P7-History-PartDesign收敛.md
- 同步 docs/CADCore2.0/03-P8-Link-Assembly-Adapter产品化.md
- 同步 docs/CADCore2.0/04-验收矩阵与交付规则.md
- 只写当前基线、已完成语义调整、FreeCAD 依据、cad-core 落点、剩余缺口、验收命令、下一步、非目标；不要写流水账。

  最终完成标准：

- P5/P6 的 ExternalGeometry、InternalShape、stable subname 共用同一个 MapperHistory / ElementMap / resolver 账本。
- FaceMaker / WireJoiner 的关键 ownership 能被 NamedShape / ElementMap 或 diagnostics 消费。
- 旧引用恢复统一走 resolver，成功时返回当前 subname 和写回建议，失败时返回稳定 diagnostics。
- ShapeFix / Refine / taper / transformed / DressUp history 不再各自散落。
- PartDesign transformed / pattern / DressUp / Link retag 后 terminal history 可持续传播。
- Link transaction 能让前端更新 DocumentObject graph 后稳定重算。
- Assembly solver 有明确接口、基础成功路径和失败 diagnostics。
- CLI / C ABI / Web adapter 复用同一 core recompute，不承载额外业务语义。
- 全部 remaining gaps 通过 capabilities、diagnostics 或 known-gap expected 显式暴露。
- 最终 build/test/git diff --check 通过。
- 工作区改动边界清楚，未混入 build、__pycache__ 或无关生成物。
