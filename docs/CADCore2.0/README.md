# CAD Core 2.0 方案入口

本目录定义 `cad-core` 从当前 P0-P8 基础子集继续推进到 CAD Core 2.0 的实施方案。2.0 的目标不是继续堆 feature 数量，而是把当前最深的 FreeCAD 语义缺口收敛成可维护主路径：完整引用更新、完整 MapperHistory、ExternalGeometry 状态机、FaceMaker / WireJoiner history 消费，以及 Link / Assembly / adapter 的产品化边界。

## 当前判断

当前 `cad-core` 已具备独立 C++17 / CMake Core、CLI adapter、薄 C ABI adapter、FreeCAD 风格 `DocumentObject graph` 输入、单次 recompute 输出、mesh / subshape / `NamedShape` / diagnostics、P0-P8 基础 feature 覆盖和 300+ 测试回归。它已经不是 MVP，但还不是完整 FreeCAD 几何内核抽取版。

按可用工程能力估算，当前约完成 65%-70%；按接近完整 FreeCAD 几何 / 拓扑语义 parity 估算，约完成 45%-50%。差距主要集中在深层引用和 history 生命周期，而不是单个几何 API。

## C2-M0 / C2-M8 当前基线

当前 C2-M8 冻结基线已锁定：`cad-core` 构建通过；阶段收口回归覆盖 `tests.test_mvp tests.test_diagnostics tests.test_feature_flows tests.test_adapters tests.test_p5_sketch tests.test_p6_topology tests.test_p7_features tests.test_p8_features`，结果 324 tests OK；`tests.test_expected_fixtures` 结果 `OK (skipped=9)`。capabilities 显式暴露 2.0 剩余缺口，包括完整 MapperHistory、transformed / pattern 完整 history、导入 shape ElementMap、Link 深层生命周期、document hash / rename recovery、Assembly 真求解和 Worker / WASM adapter。

C2-M1 已在 `cad-core/topo` 建立统一 `MapperHistory` core：新增统一 event schema，表达 source / target endpoint、shape kind、relation、maker stage、evidence、recoverability 和 diagnostic status；`NamedShape` JSON 保留旧 `history` / `element_history_status`，同时新增 `mapper_history` 字段。现有 maker history、preserved ElementMap alias、terminal split / deleted、merge、Link retag、transformed copy 和 Sketch InternalShape 的 FaceMaker / WireJoiner summary 子集已通过统一入口序列化或转换。

C2-M2 已把 FaceMaker / WireJoiner 的 Sketch InternalShape producer evidence 接入主路径：FaceMaker summary 现在携带 pre-split / splitter edge evidence、bounded face outer-boundary evidence，WireJoiner open export entry 携带 result-wire producer identity 与 target child shape；`topo/named_shape.cpp` 优先消费这些 evidence 生成 InternalFace generated、InternalEdge split/deleted/open-export 和 noOriginal purge diagnostics。`internal_element_map` 仍只保留简单唯一 alias，不承担复杂 split 判断。

C2-M3 已把 ExternalGeometry flag 输入、ReferenceShadow resolver 和请求内写回建议接入主路径：`document/model.*` 解析 Defining / Frozen / Detached / Missing / Sync，`features/sketch_object.cpp` 按状态决定是否刷新、跳过或让 Defining 外部几何进入 profile，`runtime/recompute.cpp` 保留 ExternalFlags 并继续通过 `elementReferenceUpdates` / `documentObjectUpdates` 返回 stateless 写回建议。split / deleted 仍不猜唯一目标。

C2-M4 已完成 Sketch InternalShape 主路径切换：`topo/named_shape.cpp` 只根据 FaceMaker / WireJoiner producer evidence 写入 InternalFace generated、InternalEdge split/deleted/open-export 和 noOriginal purge 的可恢复历史；旧 summary-only MapperHistory 事件只保留为 `summary_only:*` diagnostic，不能参与唯一引用恢复；`internal_element_map` 继续只表达 FreeCAD `SketchObject::getInternalElementMap()` 的 InternalEdge / InternalVertex 简单唯一 alias。

C2-M5 已有三个子路径进入正式 MapperHistory：taper ThruSections 已从 `known_gap:taper_history` 切到正式 MapperHistory，Pad / Pocket / Part::Extrusion 当前 Length、Two sides、Symmetric 和 inner-wire taper fixture 消费 `BRepOffsetAPI_ThruSections` maker / section source history；DressUp SupportTransform 的 AddSubShape slot 子集已在 Mirrored / chained Fillet-Chamfer fixture 中保留 source-prefixed alias、maker history、terminal split / deleted 和 merge；RefineModel 当前 P7 fixture 已锁定 generated / modified / terminal deleted / merge 的 mapper history 覆盖，覆盖 Pad、Pocket、Hole、DressUp 和 Transformed refined support。capabilities 暴露 `taper_thru_sections`、`dressup_addsubshape_slot` 和 `refine_modified_deleted_generated`。

C2-M6 当前 Add/Sub ownership 子路径已进入正式 MapperHistory：LinearPattern、PolarPattern、Scaled、MultiTransform 的 Features / WholeShape 代表 fixture 保留 transformed copy alias、source object history、multi-original Add/Sub replay、refined prefix support 和 terminal split / deleted / merge；capabilities 暴露 `transformed_pattern_addsub_ownership`，但仍保留 `transformed_pattern_full_history` 作为完整 pattern ownership 缺口。

C2-M7a 当前 Link transaction 子路径已落地：`documentObjectUpdates` 稳定表达 ShowElement create / claim / sync / delete / toggle-off、ElementCount owner list sync、ElementList owner / child sync 和 CopyOnChangeOwned child sync；写回字段覆盖 `ElementList`、`ElementCount`、`PlacementList`、`ScaleList`、`VisibilityList`、`LinkedObject`、`_LinkOwner`、`LinkTransform`。capabilities 暴露 `link_transaction`，同时保留 `full_child_cache_lifecycle` 与 `copy_on_change_deep_copy_lifecycle` 缺口。

C2-M7b 当前 Link reference lifecycle 子路径已落地：`PropertyXLink*` / `FullSubList` / external tag mapped postfix / source-prefixed stable key 的代表 fixture 已进入 Link retag 和 `elementReferenceUpdates` 保留路径；多层 object / label LinkSub、`PropertyXLinkList` subset compound 和 external `FullSubList` alias 均由 P8 回归锁定。capabilities 暴露 `link_reference_lifecycle`，同时保留 document hash 生命周期、source object rename recovery 和 label rename recovery 缺口。

C2-M7c 当前 Assembly solver adapter 子路径已落地：AssemblyObject 不再统一输出 `solve=not_migrated`，而是区分 `skipped_no_joints`、`solved_noop` 和 `unsupported`；GroundedJoint-only fixture 走无状态 no-op success，普通 `JointType` 通过 `unsupported_assembly_solver` warning 暴露未迁移求解器。capabilities 暴露 `assembly.solver_adapter`，同时保留 `full_ondsel_solver` 和 `solver_placement_updates` 缺口。

C2-M7d 当前 adapter contract 子路径已落地：C ABI 暴露 recompute / capabilities / export buffer，CLI 暴露 recompute + file export，二者共用 `cad-core-lib`，返回同一 stateless result channel；C API export 拒绝 server file path，只返回 buffer + metadata diagnostics。capabilities 暴露 `adapters`，同时保留 Worker adapter、WASM adapter、streaming mesh limits 和 binary mesh protocol 缺口。

C2-M8 当前验收冻结已完成到“显式能力 + 显式 gap”状态：oracle / fixture / diagnostics / capabilities / docs 均能解释当前通过和未实现边界；未实现项不再作为静默 fallback 隐藏，而是通过 `topo_history.remaining_gaps`、`link_transaction.remaining_gaps`、`link_reference_lifecycle.remaining_gaps`、`assembly.remaining_gaps`、`adapters.remaining_gaps` 或 expected fixture 的 skipped / diagnostic policy 暴露。

当前仍非目标：不收敛 C2-M5 ShapeFix 主路径 history、RefineModel 更复杂 identity-change case 和 DressUp 非 SupportTransform 复杂参数余量，不完成完整 `_ChildCache` / copy-on-change deep copy / document hash 和 rename 恢复 / Assembly 真求解 / Worker-WASM 产品化；ShapeFix 主路径缺口已通过 `topo_history.remaining_gaps.shapefix_history` 暴露；Frozen / Detached 的旧 `ExternalGeo` 几何持久复用仍不属于当前无状态请求子集。

## 文档索引

| 文档 | 用途 |
| --- | --- |
| `00-总览.md` | 2.0 目标、边界、阶段拆分和推进顺序 |
| `01-P5P6-ExternalGeometry-TopoNaming主线.md` | ExternalGeometry、MapperHistory、FaceMaker / WireJoiner、旧引用恢复主线 |
| `02-P6P7-History-PartDesign收敛.md` | ShapeFix / Refine / taper / transformed / DressUp / PartDesign ownership 收敛 |
| `03-P8-Link-Assembly-Adapter产品化.md` | Link 账本、ShowElement 写回生命周期、Assembly solver、Worker / WASM / Web adapter |
| `04-验收矩阵与交付规则.md` | fixture / oracle / diagnostics / 回归命令 / 完成判定 |

## 执行原则

- 本地 FreeCAD `src/` 是语义来源；不从 fixture 输出倒推业务逻辑。
- `DocumentObject graph` 是唯一持久源数据；shape、mesh、`NamedShape`、`ElementMap` 是单次 recompute 产物。
- adapter 只做协议转换，不承载建模语义或引用恢复。
- P5/P6 的 MapperHistory 与引用恢复是 2.0 的前置主线；在它完成前，不继续扩大高层 executor 的特判。
- 每个新增能力必须有 FreeCAD 调用链、cad-core 落点、fixture / oracle 或明确 diagnostics。
