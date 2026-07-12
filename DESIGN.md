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

  可直接用这个 goal prompt（4000 字以内）：

  /goal 目标：落地 CAD Core 2.0 C2-M2：FaceMaker / WireJoiner history producer。基于当前未提交的 C2-M1 MapperHistory core 继续，不重做 schema、不回退工作区；让 Sketch InternalShape 的 InternalFace / InternalEdge / InternalVertex 来源由 FaceMaker / WireJoiner 的真实 history evidence 解释，不再依赖 summary-only 或几何匹配。

  工作目录：/Users/li/Chili3DProject/重构Chili/FreeCAD

  开始先执行 git status --short。先读 docs/CADCore2.0/{README.md,00-总览.md,01-P5P6-ExternalGeometry-TopoNaming主线.md,04-验收矩阵与交付规则.md}，以及 cad-core 的 topo/{mapper_history,named_shape,element_map}、geometry/{face_maker,wire_joiner,sketch_internal_builder}、features/sketch_object.cpp、tests/test_p5_sketch.py、tests/test_p6_topology.py。

  FreeCAD 依据必须复核，并写入承载语义的相邻 C++ 注释：SketchObject::buildInternals；FaceMaker::postBuild；FaceMakerBuildFace；WireJoiner::getOpenWires；WireJoinerP::{build,buildClosedWire,findTightBound,findTightBoundSplitWire,findTightBoundUpdateVertices,exhaustTightBoundUpdateWire}；TopoShape::makeShapeWithElementMap。

  实施要求：

1. FaceMakerHistorySummary 从 count/flag 推进到可消费 evidence：表达 pre_split / splitter stage、source EdgeN 到 bounded InternalFaceN outer boundary 的 generated、source edge one-to-many split、one-to-zero deleted，并保留 maker stage、source edge index、target internal element、bounded face index、splitter/pre-split 依据。
2. WireJoinerHistorySummary / LedgerSummary 把 EdgeInfo、WireInfo、producer identity 转为 MapperHistory event：open-wire carry-through 用 preserved/generated；split fragment 用 split；noOriginal purge/deleted source 用 deleted；helper override、producer blocker、lineage、child wire identity 进入 evidence 或 diagnostic_status。
3. topo 层优先在 named_shape.cpp 或新增 topo helper 消费 producer evidence：InternalFaceN 来自 FaceMaker outer boundary history；InternalEdgeN/InternalVertexN 的 split/deleted 来自 WireJoiner/FaceMaker evidence；internal_element_map 只保留简单唯一 alias，不承担复杂 split 判断。
4. features/sketch_object.cpp 只传递结构化 evidence，不合成 split history。producer evidence 不足时只保留 diagnostic-only event，并写清 blocker，例如 missing_producer_identity、source_shape_identity_not_ready、no_original_purge、ambiguous_owner。
5. 禁止 fixture 名称分支、bbox、面积、输出顺序、几何类型排序、source ownership 猜测；ElementMap 仍只写唯一 target。
6. 补 focused tests：bounded InternalFace generated；source edge one-to-many split；deleted 不写唯一 ElementMap；open-wire carry-through；noOriginal purge diagnostic；internal_element_map 不承担复杂 split。
7. 同步 docs/CADCore2.0 的 C2-M2 当前基线、已完成语义、剩余缺口、验收命令和下一步，不写流水账。

  验证：
  cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
  cmake --build build
  python3 -m unittest tests.test_p5_sketch tests.test_p6_topology tests.test_adapters
  通过后可跑阶段回归：
  python3 -m unittest tests.test_mvp tests.test_diagnostics tests.test_feature_flows tests.test_adapters tests.test_p5_sketch tests.test_p6_topology tests.test_p7_features tests.test_p8_features tests.test_expected_fixtures
  cd /Users/li/Chili3DProject/重构Chili/FreeCAD
  git diff --check

  非目标：不做 C2-M3 ExternalGeometry 状态机；不做 C2-M4 主路径最终切换和 fallback 全删；不做 C2-M5+ Refine/taper/transformed/DressUp；不做 Link/Assembly/Web adapter 产品化；不提交代码。

  完成标准：FaceMaker/WireJoiner 能产出结构化 MapperHistory evidence；Sketch InternalShape 的 InternalFace generated、InternalEdge/InternalVertex split/deleted/open carry-through 可解释；C2-M1 schema 和回归不倒退；focused tests 通过；docs 同步 C2-M2 状态。完成后下一阶段再做 C2-M3。

---

  可直接用这个 goal prompt（4000 字以内）：

  /goal 目标：完整落地 /Users/li/Chili3DProject/重构Chili/FreeCAD/docs/CADCore2.0 定义的 CAD Core 2.0，从当前未提交的 C2-M0/C2-M1 MapperHistory core 推进到 C2-M8 验收冻结。不要阶段性停下等待；只有整体完成、验证通过、文档同步后才 goal complete。

  工作目录：/Users/li/Chili3DProject/重构Chili/FreeCAD。先执行 git status --short，保留当前未提交成果，不回退、不重做、不覆盖。本地 FreeCAD src 是唯一语义来源，不上网。cad-core 保持无状态边界：DocumentObject graph 是唯一持久源；shape、mesh、NamedShape、ElementMap、MapperHistory 都是单次 recompute 产物；BREP 长期状态只允许 ReferenceShadow.brep 的旧单 subshape snapshot。

  先读 AGENTS.md、docs/CADCore2.0/*.md、cad-core 相关 topo/geometry/features/runtime/adapters/tests。新增承载 FreeCAD 语义的类型、字段、函数、mapper/resolver 规则，必须在相邻 C++ 注释标注 FreeCAD 源文件、类/函数、关键字段或短句；不能从 fixture 输出倒推。

  实施顺序：

1. C2-M2：FaceMaker/WireJoiner producer evidence。FaceMaker pre-split/splitter、bounded InternalFace outer boundary 产出 MapperHistory；WireJoiner EdgeInfo/WireInfo/producer identity/noOriginal/splitWire/done 转成 preserved/generated/split/deleted；InternalFace/Edge/Vertex 来源由 evidence 或 diagnostics 解释；internal_element_map 只保留简单唯一 alias。
2. C2-M3：Reference resolver + ExternalGeometry 状态机。统一 SubList、StableSubList、source-prefixed key、mapped postfix、ReferenceShadow fingerprint/brep；支持 Defining/Frozen/Detached/Missing/Sync；成功返回 elementReferenceUpdates/documentObjectUpdates，失败返回稳定 diagnostics；split 不猜唯一目标。
3. C2-M4：Sketch InternalShape 主路径切换。InternalFace/Edge/Vertex 通过 MapperHistory/ElementMap/diagnostics 解释；删除或隔离 summary-only、geometry-match、fixture fallback；sketch_object.cpp 只表达 FreeCAD 调用顺序。
4. C2-M5：ShapeFix/Refine/taper/DressUp history 收敛。Refine Modified/IsDeleted/generated face、ShapeFix identity change、taper section source、Fillet/Chamfer/DressUp AddSubShape slot history 进入统一 MapperHistory；不能收敛的保留 explicit known gap。
5. C2-M6：PartDesign transformed/pattern ownership。Mirrored/LinearPattern/PolarPattern/Scaled/MultiTransform 消费 AddSubShape slot history；SupportTransform、refined prefix、multi-original replay、copyElementMap 等价语义传播 source-prefixed alias，不从结果几何倒推。
6. C2-M7：Link/Assembly/adapter 产品化。ShowElement transaction、Element/Placement/Scale/Visibility 写回、_ChildCache/copy-on-change/linked-owner、PropertyXLink/FullSubList/resolver 生命周期、Assembly solver adapter 成功路径和失败 diagnostics；CLI/C ABI/Web adapter 共用 cad-core-lib recompute。
7. C2-M8：验收冻结。oracle、fixture、capabilities、diagnostics、接口字段和 docs/CADCore2.0 同步；remaining gaps 通过 capabilities、diagnostics 或 known-gap expected 显式暴露；不允许静默 fallback 或无法解释的 fixture pass。

  禁止：fixture 名称分支；bbox/面积/输出顺序/几何类型/source edge 猜测；在 adapter/runtime 输出层/document parser/sketch executor 塞 topo 规则；Link/Assembly 绕过 MapperHistory/resolver；完整 FreeCAD 全仓库构建；未经要求提交代码；partial complete。

  验证分层执行。每轮代码修改只跑相关粗 filter：C2-M2/M4 跑 tests.test_p5_sketch tests.test_p6_topology；C2-M3 跑 tests.test_diagnostics tests.test_feature_flows tests.test_p6_topology；C2-M5/M6 跑 tests.test_p7_features tests.test_p8_features；C2-M7 跑 tests.test_adapters tests.test_feature_flows tests.test_p8_features。随后仓库根跑 git diff --check。

  阶段收口或改 oracle/runner 时运行：
  cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
  cmake --build build
  python3 -m unittest tests.test_mvp tests.test_diagnostics tests.test_feature_flows tests.test_adapters tests.test_p5_sketch tests.test_p6_topology tests.test_p7_features tests.test_p8_features

  C2-M8 冻结前再加 tests.test_expected_fixtures，并在仓库根运行 git diff --check。

  文档同步 docs/CADCore2.0/{README.md,00-总览.md,01-P5P6-ExternalGeometry-TopoNaming主线.md,02-P6P7-History-PartDesign收敛.md,03-P8-Link-Assembly-Adapter产品化.md,04-验收矩阵与交付规则.md}，只写当前基线、完成语义、FreeCAD 依据、cad-core 落点、缺口、验收命令、下一步、非目标，不写流水账。

  最终标准：P5/P6 ExternalGeometry、InternalShape、stable subname 共用 MapperHistory/ElementMap/resolver；FaceMaker/WireJoiner ownership 可被消费；旧引用恢复统一走 resolver；PartDesign history、transformed/pattern/DressUp/Link retag 后 terminal history 可传播；Link transaction 与 Assembly solver 有基础路径和 diagnostics；adapter 复用 core；remaining gaps 显式暴露；重型收口通过；工作区无 build、__pycache__ 等生成物。

 '/Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core/fixtures' 现在我这个里面的测试都通过了吗？
那这个测试的覆盖的量相比于整个FreeCAD的几何库的功能来说，能占到多少呢？

---

 目标：在 /Users/li/Chili3DProject/重构Chili/FreeCAD 继续 CAD Core 3.0，从当前 C3-M1~C3-M5 first slice 基线推进到 C3-M8 freeze。以本地 FreeCAD src/ 为唯一语
  义权威；不查网页；不从 fixture 输出倒推业务逻辑。

  开始先复核 live 状态：
  cd /Users/li/Chili3DProject/重构Chili/FreeCAD
  git rev-parse --short HEAD
  git log -1 --oneline
  git status --short -uall

  必读：AGENTS.md；docs/CADCore3.0/README.md、00-总览.md、02-TopoNaming与History完全体.md、03-Sketcher-Part-PartDesign几何能力复刻.md、04-Link-Assembly-运行时
  产品化.md、05-验收矩阵与交付规则.md、FreeCAD语义矩阵.md、capabilities-gap对照表.md、oracle-fixture队列.md；cad-core/src/adapters/c_api/c_api.cpp；cad-core/
  tests/test_adapters.py。以 live docs/capabilities/tests 领取 remaining gaps，不重复实现已完成切片。

  执行原则：每轮只领取一个最小可验收切片，先读 FreeCAD 调用链并写清 cad-core 落点，再改代码。新增/修改核心语义 API、字段、enum、executor 主路径、mapper/
  history 规则时，在相邻 C++ 注释标明 FreeCAD 绝对路径、类/函数、关键短句或字段名。语义分层：document 管 graph/属性/链接/writeback；graph 管依赖/recompute
  plan；runtime 管 registry/diagnostics/调度；features 管 Sketcher/Part/PartDesign/Link/Assembly；geometry 管 OCCT/FaceMaker/WireJoiner/ShapeFix/mesh；topo 管
  NamedShape/ElementMap/MapperHistory/stable subname/reference recovery；adapters 只做 CLI/C ABI/Worker/WASM 协议转换。

  禁止：不把 BREP 作为长期状态，唯一例外是 ReferenceShadow.brep 单 subshape snapshot；不在 adapter/runtime 输出层修 subname；不把 FreeCAD 业务语义塞进
  adapter；不为单个 fixture 特判；不靠 fixture 名、bbox、面积、几何类型、输出顺序或 sketch executor 中的几何形态猜 source ownership；不维护后端跨请求
  session。不要自动提交，除非用户另行要求。

  优先队列：

1. C3-M1 TopoNaming 收口：继续收敛 complete_mapper_history producer matrix，明确 ShapeFix/import/boolean/section/general_fuse/refine/part_offset/
   transformed/dressup/sketch_internalshape 等 producer 是 covered、partial、review 还是 diagnostic-only。
2. C3-M3 Sketcher：full_solver_dof、underconstrained_state、solver_geometry_update、malformed_constraint_diagnostics、partial_redundancy_diagnostics，以及复
   杂 InternalShape/FaceMaker/WireJoiner oracle。
3. C3-M4 Part：Part Section stable history、GeneralFuse/Boolean history、Part Offset Fill/solid source makeElementSolid/Offset2D、Part Workbench Thickness、
   Sweep/Loft/import-export history。语义优先落 features/part*.cpp、geometry、topo。
4. C3-M5 PartDesign：hole_cut_history_full_element_map_freeze、更多 point profile/head cut 组合。Hole 必须按 FeatureHole.cpp::Hole::execute()/findHoles() 与
   TopoShapeExpansion.cpp::makeShapeWithElementMap() 语义实现；已完成的 Body/DressUp/transformed 只做必要收敛。
5. C3-M6 Link+Assembly：full_child_cache_lifecycle、copy_on_change_deep_copy_lifecycle、full_ondsel_solver、solver_placement_updates；placement write-back
   通过 documentObjectUpdates 表达。
   cmake --build build
   python3 -m unittest <本轮相关测试>
   按范围补：topo/adapter 跑 tests.test_p6_topology tests.test_adapters；sketch 跑 tests.test_p5_sketch tests.test_adapters；PartDesign 跑
   tests.test_p7_features tests.test_adapters；Part/Link/Assembly 跑 tests.test_p8_features tests.test_adapters；expected fixture 改动跑
   tests.test_expected_fixtures。
   收口再执行：
   cd /Users/li/Chili3DProject/重构Chili/FreeCAD
   git diff --check
   M8 更新 05。只写当前基线、已完成语义、剩余缺口、验收命令、下一步和非目标，不写流水账。

---

1. P5/P6 联合主线
   实现完整 MapperHistory / ElementMap 生命周期、FaceMaker / WireJoiner history producer、统一 reference
   resolver，以及 ExternalGeometryExtension 的 Defining / Frozen / Detached / Missing / Sync 状态机。这个是
   下一阶段主线，落点在 topo、part、runtime、document、sketcher，见 docs/CADCore方案/细化方案/13-
   ExternalGeometry-TopoNaming下一阶段主线.md:79。
2. Topo Naming 收敛
   补完整 MapperHistory 消费、复杂 split 旧引用恢复、ExternalGeometry 旧引用恢复、ShapeFix / Refine /
   transformed / DressUp 的命名传播。当前只是主路径骨架和若干 producer 子集，见 docs/CADCore方案/00-CAD-Core
   抽取方案.md:207。
3. Sketcher 深水区
   补完整约束求解、BSpline solver / control-point 语义、复杂 getInternalElementMap()、FaceMakerBuildFace /
   WireJoiner history 到 InternalShape / internal element 的正式消费，见 docs/CADCore方案/细化方案/00-CAD-
   Core完整抽取执行总览.md:89。
4. FeatureExtrude / PartDesign 补强
   FeatureExtrude 还缺多 face / shell UpToShape、非平面终止、完整 attachment/support/subname 恢复。
   PartDesign 还缺 Hole ModelThread、标准件表驱动头部尺寸、复杂 Fillet/Chamfer、链式 DressUp
   SupportTransform ownership、复杂 transformed/pattern ownership，见 docs/CADCore方案/00-CAD-Core抽取方
   案.md:205。
5. P8 后置能力
   Assembly solver、完整 Joint placement / constraint、Worker / WASM / Web adapter、导入 shape 完整
   ElementMap、完整 FreeCAD Link 账本、ShowElement=true LinkElement / LinkGroup 持久写回事务、cross-document
   文档哈希 / postfix 生命周期和复杂多层 LinkSub 链还没完整迁移，见 docs/CADCore方案/细化方案/00-CAD-Core完
   整抽取执行总览.md:92。

下一步应该做 P5P6-SCOPE-008 / P5P6-BLOCK-002：ExternalGeometry 的 Frozen / Sync / Detached / Missing
  原生 FreeCAD 状态机 oracle。

  理由很直接：当前矩阵里 SCOPE-007 已经是 supported，BLOCK-001 已关闭；剩下唯一明确的 notCollected
  blocker 是 BLOCK-002，落在 docs/FreeCAD几何生态迁移工程-细分/P5P6-ExternalGeometry-TopoNaming联合主
  线/矩阵/p5p6_blocker_queue.tsv:3。

  建议按这个顺序做：

1. 先修 native oracle harness / collector，不先写业务 C++。

   - Frozen 源对象变化不刷新
   - Frozen + Sync 本次刷新并建议清除 Sync
   - Detached 不再追随源对象
   - Missing object/subshape、deleted target、snapshot present/missing
     为。
2. 只有 native expected 证明不一致时，再做最小 C++ 修正。
3. 回写 SCOPE-008 和 BLOCK-002 为 supported/backendGap/unsupported。

  不要先碰 FaceMaker/WireJoiner/MapperHistory 主线；这些在当前矩阵里已经是 supported/closed 基线。

---

› 你给我出一个 /goal prompt , 他的任务是根据 '/Users/li/Chili3DProject/FreeCAD/docs/CADCore4.0/6-19-23-52-CADCore4.0总览方案.md', 挨个用 $goal-step-runner 执行'/Users/
  li/Chili3DProject/FreeCAD/docs/CADCore4.0' 里面的各个主线的工作步奏细分

---

我希望你给我出一个 /goal prompt , 把  3c5ccff1fe2b1dea8a143a15755069ee33151913 这次提交以来, '/Users/li/Chili3DProject/FreeCAD/cad-core' 的所有改动, merge到 /Users/li/Chili3DProject/cad-web-background/cad-core

---


 将 /Users/li/Chili3DProject/FreeCAD 中从 3c5ccff1fe2b1dea8a143a15755069ee33151913 到当前 HEAD 的
  cad-core/** 全部改动，迁移/merge 到 /Users/li/Chili3DProject/cad-web-background/cad-core，并完成验证与中
  文提交。

  背景：本 prompt 生成时源仓 HEAD 是 5c54f3c5a2903596ab45973a09627d91dfd688bb，目标仓 HEAD 是
  a95648e9d8b116992265950975d7fdedb8f3fbbf。执行时先重新确认两边 git status/HEAD。源仓当前只有 DESIGN.md 非
  cad-core 修改；若执行时源侧 cad-core/** 有未提交改动，也要纳入迁移范围并说明。目标仓当前有无关未跟踪文件
  docs/接口规定/7-4-21-04-Body-Tip-dress-up引用接口规定.md，保护它，不修改、不提交，除非它被本轮真实需要且
  先说明原因。

  范围：

- 只迁移源仓 /Users/li/Chili3DProject/FreeCAD/cad-core/** 自 3c5ccff1fe2b1dea8a143a15755069ee33151913 以
  来的改动。
- 包括新增 fixtures、expected、headers、src、tests、CMake/source list/capability/adapter 相关 cad-core 改
  动。
- 不迁移 FreeCAD 仓 cad-core 外的文档或源码。
- 不改目标仓无关 Rust/HTTP 代码，除非 cad-core FFI/build/test 必须同步；如必须改 crates/cad-core-sys、
  crates/cad-core、crates/cad-server 等适配层，保持 JSON/HTTP adapter 边界，不把几何语义搬到 Rust Web 层。

  建议步骤：

1. 在源仓执行：
   git -C /Users/li/Chili3DProject/FreeCAD diff --name-status
   3c5ccff1fe2b1dea8a143a15755069ee33151913..HEAD -- cad-core
   git -C /Users/li/Chili3DProject/FreeCAD diff --binary 3c5ccff1fe2b1dea8a143a15755069ee33151913..HEAD
   -- cad-core
2. 在目标仓检查同名文件当前状态，优先用 patch/三方思路迁移；如果目录结构已分叉，逐文件手工移植，不要直接
   整目录覆盖。
3. 特别确认这些源侧新增/变更族完整落入目标 cad-core：c12m12/c12m13/c12m14/c12m16/c5m1 fixtures，
   part_sweep/topo_shape_expansion/shape_exporter/body/feature_pipe/shape_binder，runtime recompute/
   reference/element update/capability，sketch edge identity/internal/external，以及相关 Python tests。
4. 处理冲突时以目标仓当前后端产品语义为运行环境，以 FreeCAD/cad-core 源侧 diff 为语义来源；不要新增
   fixture 名称特判或输出端修剪。
5. 不提交 build/、__pycache__/、临时 patch 文件、日志或本地生成物。

  验证：

- git -C /Users/li/Chili3DProject/cad-web-background diff --check
- cd /Users/li/Chili3DProject/cad-web-background/cad-core && cmake -S . -B build && cmake --build build
- 在目标仓运行本轮相关 Python 测试，至少覆盖变更触达的 test_adapters.py、test_p5_sketch.py、
  test_p7_features.py、test_p8_features.py、test_c8_shapebinder.py；若测试命令或 build 目录约定不同，先读取
  目标仓现有测试说明后调整。
- 如改动影响 FFI/HTTP 输出，从 /Users/li/Chili3DProject/cad-web-background 根目录补跑 cargo check 或相关
  cargo test，并说明是否需要重启 cad-server 才能验证 HTTP 运行态。

  完成标准：

- 目标 cad-core 包含源侧自 3c5ccff1...以来所有 cad-core 改动，无遗漏新增/删除/重命名。
- 目标无关未跟踪文件仍保持未被纳入。
- 验证命令结果清楚记录；若某项因环境失败，给出具体失败原因和后续最小命令。
- 按中文提交工作流提交本轮相关变更，提交前展示变更边界，提交后证明工作区只剩用户原有无关改动或完全干净。

---

那删除'/Users/li/Chili3DProject/cad-web-background/docs/BUG处理/7-7-14-09-【已实现】Pad3Body重复稳定拓扑名后端修复方案.md', 并回退'/Users/li/Chili3DProject/cad-web-background/cad-core/src/runtime/recompute.cpp', 改为 不同 Edge 发布同一个 stableSubname 的时候, 直接返回 结构化 diagnostics

---

 '/Users/li/Chili3DProject/cad-web-background/docs/BUG处理/7-7-15-59-【待实现】Revolution过度发布Body作用域Face导致下游引用失效.md' 你再看一下这个问题, 我希望按 FreeCAD 的代码实现和
  FreeCADCmd结果, 作为基准, 来实现我的'/Users/li/Chili3DProject/cad-web-background/cad-core', 该怎么实现呢

---

 用法：

  tools/build_freecad_macos_homebrew.sh

  只编 FreeCADCmd：

  tools/build_freecad_macos_homebrew.sh --target FreeCADMainCmd

  只重新配置，不编译：

  tools/build_freecad_macos_homebrew.sh --configure-only

  缓存乱了就这样：

  tools/build_freecad_macos_homebrew.sh --clean-cache

  脚本默认做这些事：

  - 使用 build/relwithdebinfo
  - 使用 Homebrew Python：/opt/homebrew/bin/python3
  - 固定 ICU 到 icu4c@78
  - 固定 MEDFile 到 med-file@4.1.1_py312
  - 修复 Homebrew PySide/Shiboken 的兼容 symlink
  - 默认 BUILD_BIM=OFF，绕过 lark 依赖

---

可以，那就把它明确成：服务端无会话，但协议有状态。

  也就是：

  - 后端不保存 session。
  - 每次 recompute 返回完整 topoNamingState。
  - 前端下次请求把上一次的 topoNamingState 原样带回来。
  - 后端用“旧 state + 当前 graph”做拓扑引用续接。

  关键规则要定死：

  1. topoNamingState 不是建模几何输入
     建模仍然只看 DocumentObject graph。state 只用于解析旧引用、判断 split/deleted/renamed。

  2. stableSubname 可以引用 state 里的 token
     比如 Face1;:H551,F;CSD... 这种 raw mapped name，只有在配套旧 state 一起传回时才有意义。

  3. state 必须 versioned
     要包含 schemaVersion、FreeCAD/OCCT 版本、collector/cad-core topo state 版本、object revision/hash。版本不匹配就降级为不可恢复或重新建
     立引用。

  4. 客户端携带的 state 不能完全信任
     后端要校验它和当前 graph/object 是否匹配，不能让前端伪造 state 影响几何建模结果。

  建议协议先长这样：

  {
    "graph": { "...": "DocumentObject graph" },
    "topoNamingState": {
      "schemaVersion": "cad-core.topo-state.v1",
      "kernelVersion": "FreeCAD 1.2.0 / OCCT ...",
      "documentHash": "...",
      "objects": {
        "BooleanFragments": {
          "objectHash": "...",
          "elementMap": {},
          "childElementMaps": [],
          "rawMappedNames": {},
          "canonicalMappedNames": {},
          "history": []
        }
      }
    }
  }

  响应里返回新的：

  {
    "results": [...],
    "topoNamingState": { "...new full snapshot..." },
    "elementReferenceUpdates": [...]
  然后引用字段可以变成：

  {
  大白话说：可以把旧账本随请求带回来，但要把它当“旧账本快照”，不是裸字符串。 裸 rawFreecadMappedName 下次不可靠；rawFreecadMappedName +
  topoNamingState 才有恢复意义。

  下一步应该先改接口方案文档，把当前“纯无状态、ElementMap 不长期保存”的规则改成“客户端携带 topoNamingState 的协议有状态模式”，再动
  collector / recompute DTO / response schema。

---

 '/Users/li/Chili3DProject/FreeCAD/cad-core/fixtures' 检查一下这里面的测试用例, 是否满足'/Users/li/Chili3DProject/FreeCAD/docs/接口规定/7-8-11-08-topoNamingState客户端携带状态接口方案.md' , 我是让你检查测试用例 和 输出结果的, 不是用来测试 cad-core 的. 这个是用做权威基线的, 我之后要实现 cad-core 的

---

 我希望让它建立多个子Agent 并行跑输出, 在跑输出之前, 先把原有的 expected 里的内容全删了, 给我出一个 /goal prompt

----

根据'/Users/li/Chili3DProject/FreeCAD/docs/接口规定/7-8-11-08-topoNamingState客户端携带状态接口方案.md' 挨个查阅一下, '/Users/li/Chili3DProject/
  FreeCAD/cad-core/fixtures'里面的 `fixtures/<phase>/*.json` 是根输入 fixture。 这些输入 fixture 有不满足要求的吗

----

根据'/Users/li/Chili3DProject/FreeCAD/docs/接口规定/7-8-11-08-topoNamingState客户端携带状态接口方案.md' 挨个查阅一下, '/Users/li/Chili3DProject/
  FreeCAD/cad-core/fixtures'里面的 `fixtures/<phase>/expected/*.freecad.json` 是 FreeCADCmd / native oracle 结果。 这些输出 fixture 有不满足要求的吗


 '/Users/li/Chili3DProject/opencascade-rs/docs/框架/7-8-11-08-topoNamingState客户端携带状态接口方案.md'要实现这个方案, 并对其'/Users/li/
  Chili3DProject/FreeCAD/cad-core/fixtures'中 FreeCAD 的expeted, 下一步该做什么

---

我觉得先不用考虑  cad-core 的 , FreeCADCmd 在生成 recomputed 的输入和输出的时候, 应该先考虑如何保证这个账本的完整性, 这样才能设置权威

---

要让'/Users/li/Chili3DProject/FreeCAD/cad-core' 的输出对齐 `fixtures/<phase>/expected/*.freecad.json` , 并实现这个方案 '/Users/li/Chili3DProject/FreeCAD/docs/框架/7-9-15-53-FreeCADCmd权威账本与topoNamingState裁剪原则.md' 下一步该做什么

---

我是希望'/Users/li/Chili3DProject/FreeCAD/cad-core' 针对'/Users/li/Chili3DProject/FreeCAD/cad-core/fixtures' 里面  "`fixtures/<phase>/*.json` 是根输入 fixture。" 的输入 fixture, 能够产生与
  FreeCADCmd 同样的输出(FreeCADCmd 是使用 /Users/li/Chili3DProject/FreeCAD/cad-core/tools/collect_freecad_expected.py 采集的)—— - `fixtures/<phase>/expected/*.freecad.json` 是 FreeCADCmd /
  native oracle 结果。, 你把这个目标给我落方案到'/Users/li/Chili3DProject/FreeCAD/docs/框架

---

查阅'/Users/li/Chili3DProject/FreeCAD/docs/框架/7-10-15-53-CADCore根输入Fixture到FreeCADCmd输出等价框架方案.md''/Users/li/
  Chili3DProject/FreeCAD/docs/框架/检查流程.md', 排查一下'/Users/li/Chili3DProject/FreeCAD/cad-core/fixtures/c3m2' 中 cad-core 的输出是否
  满足要求

---

我能不能 新建一个 比较的  python 脚本 在 /Users/li/Chili3DProject/FreeCAD/cad-core/tools， 用于比较  expected/<case>.freecad.producer-trace.json  与  cad-core 的过程输出， 从而在第一处分叉的位置就报错？

---
查阅/Users/li/Chili3DProject/FreeCAD/docs/框架/CADCore建模可用性与ProducerTrace门禁边界.md  和 /Users/li/Chili3DProject/FreeCAD/docs/框架/7-12-00-46-FreeCADCmd-ElementMap生产者Trace驱动CADCore实现指南.md ， 排查一下  /Users/li/Chili3DProject/FreeCAD/cad-core/fixtures/c3m5 有哪些缺口？
那么对比一下  cad-core 与 expected 的输出， 查询第一个缺口是什么
