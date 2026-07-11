# C3M2 ReferenceShadow 恢复与 topoNamingState 取证（2026-07-11）

本记录只说明本轮 C3M2 的源码依据与运行时边界；它不把 fixture 输出当作 FreeCAD 语义来源。

## 1. 实际 FreeCAD 恢复链

| 位置 | 实际类型 / 函数 / 字段 | 本轮结论 |
| --- | --- | --- |
| `src/Mod/Part/App/PartFeature.h:256-259` | `Part::Feature::_elementCache`、`_elementCachePrefixMap` | 旧几何是 Feature 私有缓存，不是 DocumentObject graph 或 topoNamingState 几何。 |
| `src/Mod/Part/App/PartFeature.cpp:1416-1421` | `Feature::ElementCache { TopoShape shape; vector<string> names; bool searched; }` | 缓存的是旧的单个 `TopoShape` 与检索结果。 |
| `PartFeature.cpp:1437-1526` | `Feature::onBeforeChange()` | 在 Shape 改变前用 `getSubTopoShape()` 收集仍被引用的旧 subshape。 |
| `PartFeature.cpp:1567-1608` | `Feature::searchElementCache()` | 从缓存 shape 用 `findSubShapesWithSharedVertex()` 找恢复候选。 |
| `src/App/GeoFeature.cpp:248-280` | `GeoFeature::updateElementReference()` | Shape 变化时委托 `PropertyLinkBase::updateElementReferences()` 更新真实属性引用。 |
| `src/App/PropertyLinks.cpp:470-527` | `PropertyLinkBase` element-reference 更新 | 缺失元素时尝试 `searchElementCache()`；恢复成功后更新 property-local shadow/subname。 |
| `src/Mod/Sketcher/App/SketchObjectExternal.cpp:2297-2350,2683-2700` | `SketchObject::rebuildExternalGeometry()`、`ExternalGeo`、`ExternalGeometryFacade::Missing` | `Missing` 是持久 `ExternalGeo` facade 标志；`ExternalGeometry` link 列表本身不是历史几何池。 |
| `src/App/PropertyLinks.cpp:189-242,5690-5715` | `updateLabelReference()`、`PropertyXLinkContainer::afterRestore()`、`_DocMap` | 标签/XLink 恢复依赖真实 object hierarchy 和 restore-time document map。 |

因此 CAD Core 的 `ReferenceShadow.brep` 只能是 item-local 单 subshape 的请求携带恢复证据。它不得进入建模输入、results 或 `topoNamingState` 的 objects。

## 2. BREP 崩溃定位与恢复入口

基线使用 `/Users/li/.cargo/bin/FreeCADCmd`（FreeCAD 1.2.0 revision 20260519）。当前 CAD Core 头文件和 FreeCAD LibPack runtime 都报告 OCCT 7.8.1；这不是单纯 Homebrew OCCT 版本漂移。

默认 CLI 复现中，`part::readBrepTextSnapshot()` 的 `BRepTools::Read(shape, std::istream, builder)` 走到 `TopTools_ShapeSet::Read`，在 FreeCAD bundle libc++ 与 process system libc++ 混载时于 locale 析构 `SIGABRT`。异常捕获不能拦住该 abort。

FreeCAD 自己的 `PropertyPartShape::loadFromFile()`（`src/Mod/Part/App/PropertyTopoShape.cpp:640-687`）先将字节写到临时文件，再调用 bool-returning `BRepTools::Read(shape, filename, builder)`；`loadFromStream()`（689-710）则明确处理 stream locale/异常风险。本轮 CAD Core 按前者的 parser 入口恢复：

1. 先严格验证 `byteLength` 与 SHA-256；
2. 将已验证的原始 BREP bytes 写进私有临时目录；
3. 使用 filename overload；`false`、空 shape、文件系统错误和 C++/OCCT 异常都变为结构化诊断；
4. 临时文件只传输给 parser，绝不成为文档模型或 topoNamingState 载荷。

同一 face BREP 已在 C3M2 frozen/missing reuse 和 C4M3 frozen regression 连续运行验证，不再使 CLI abort。

## 3. topoNamingState 严格边界

`cad-core/src/runtime/topo_naming_state.cpp` 保持请求级 hard fail：schema、producer、documentHash、objectHash 或 element-map encoding 不兼容时返回 diagnostics-only（无 results、无 element updates、无新 state）。`cad-core-runtime-v1` 是 C++ `producerIsCompatible()` 明确允许的首轮 runtime producer；collector 与 ledger validator 只同步这一有限集合，未知 producer 仍 hard fail。

source-object rename 的首轮没有 state，由 runtime 正式产生 state；随后原样携带该 state 的第二轮仍生成 `ProbePad.UpToFace -> RenamedBody`、`StableSubList=[Pad.Face6]` 和 item-local `ReferenceShadow`。生成 state 的 top-level owners 只来自当前 request graph，不包含旧 `Body`：旧 target 留在属性局部 recovery evidence，不能被提升为 state owner。篡改 documentHash 后仍是 diagnostics-only `topo_state_document_hash_mismatch`。

该 recovery 输入的 `Pad.Face6` 是 display path，不能作为成功 recovery 的 FreeCADCmd public-root stable identity。静态 fixture 保留旧 state 的 documentHash mismatch，并由同次 collector 生成 native diagnostics-only `.freecad.json + ledger`；真正的成功路径仍由动态正式生成 state 的 runtime test 保护，不手填 hash 或伪造 stable name。

## 4. C3M2 artifact 裁决

本轮对 12 个输入逐一用同一次 FreeCADCmd collector 尝试。没有任何输入能在不补写历史几何、外部 FCStd lifecycle、restore-time label state 或 synthetic stable identity 的前提下形成 exact public-root native oracle：

- BREP / missing snapshot：缺失目标和 ElementCache-only old geometry；
- `missing-fix`：DTO `ExternalFlags` 与 upstream persisted `ExternalGeo` pool 不同；
- label / nested / cross-document：restore-time hierarchy、label reference 与 `_DocMap`；
- xlink status：缺失、pending 或 unloaded external FCStd；
- source rename：静态 stale-state hard fail 有 collector-owned native pair；动态合法 state 能驱动 CAD Core recovery，但无 public-root stable identity，因此由 focused runtime test 单独保护。

故角色是 `native=1, protocol_only=11, unsupported=0`。11 个 protocol-only input 都有 source-backed `.expeted.json`，由 `tests/test_c3m2_protocol_contract.py` 读取实时 CAD Core CLI 输出验证；source-object rename 的合法-state recovery 由 `tests/test_p6_topology.py` 的动态 round-trip 覆盖。native hard-fail pair 进入同 phase 的 collector/ledger/parity gate；protocol-only 不冒充 FreeCAD native parity。
