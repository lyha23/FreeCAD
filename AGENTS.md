# Repository Guidelines（仓库指南）

## 仓库目标
- 本仓库是本地 FreeCAD 源码树，也是抽取 CAD Core 的语义来源；当前目标是基于 `/Users/li/Chili3DProject/重构Chili/FreeCAD` 中的 FreeCAD 实现，把几何建模核心逻辑抽到 `/Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core`。
- 抽取范围重点包括文档对象图、属性与链接、依赖分析、recompute、草图、PartDesign 特征链、几何建立、拓扑命名、拓扑元素追踪、几何结果映射与前端 CAD 运行时需要的几何内核能力。
- 做架构取舍、API 设计、实现拆分或文档整理时，优先服务上述抽取目标；若“通用后端框架”风格与 FreeCAD 几何库抽取需求冲突，以 FreeCAD 业务语义、可重建能力和 `cad-core` 清晰边界为准。
- 需要确认行为语义时，优先读取本仓库 `src/` 中对应 FreeCAD 实现，再决定 `cad-core` 中的 C++ API、拓扑命名模型、重建流程和 fixture 期望。

## 几何库前后端架构
- 当前几何库按前后端架构设计：前端负责保存和编辑完整的 FreeCAD 风格 `DocumentObject graph`，并在建模操作、参数修改或重算时把该 graph 作为请求数据发送给后端或 `cad-core` adapter。
- 后端 / CAD Core 是无状态几何计算服务：每次收到请求后，只根据请求里的 `DocumentObject graph`、`recompute` 目标和运行时参数重新计算目标 shape，不依赖上一次请求留下的文档、会话或几何缓存。
- `DocumentObject graph` 是唯一真实数据；shape、`NamedShape`、`ElementMap`、topomap、subshape map 和 mesh 都是单次请求中的计算产物，请求结束后不得作为前端或后端长期状态保存。已批准的唯一 BREP 例外是 `ReferenceShadow.brep`：它只能保存被引用单个 subshape 的旧几何快照，用作引用恢复证据，不能作为建模输入或完整对象 BREP。
- 后端只返回前端显示和拾取所需的 mesh、subshapes、完整 `subname`、引用更新建议与诊断信息；除 `ReferenceShadow.brep` 这个旧 subshape snapshot 例外外，不要在请求或响应中传递 BREP，也不要把 BREP 作为前端或后端长期几何状态保存。
- 前后端接口、拓扑命名、重算流程和阶段边界以 `docs/CADCore方案/00-CAD-Core抽取方案.md` 及其细化方案为准；实现中不得绕过这些文档定义的无状态 CAD Core 边界。

## 项目结构与模块组织
- `src/`：FreeCAD 上游源码，是行为语义、调用链和字段含义的主要依据。
- `src/App`：承接 `Document`、`DocumentObject*`、`PropertyLinks.cpp`、`PropertyGeo.cpp`、`GeoFeature.cpp` 等文档对象、属性、链接、引用更新和 placement 语义。
- `src/Mod/Part/App`：承接 `PartFeature.cpp`、`BodyBase.cpp`、`PropertyTopoShape.cpp`、`TopoShape.cpp`、`TopoShapeExpansion.cpp`、`TopoShapeMapper.cpp`、`FaceMaker*.cpp`、`WireJoiner.cpp` 和 Attachment 相关语义。
- `src/Mod/PartDesign/App`：承接 `Body.cpp`、`Feature.cpp`、`FeatureAddSub.cpp`、`FeatureExtrude.cpp`、`FeaturePad.cpp`、`FeaturePocket.cpp`、`FeatureTransformed.cpp`、Pattern、Mirror、MultiTransform、Datum、Hole、DressUp 等 PartDesign 特征链语义。
- `src/Mod/Sketcher/App`：承接 `Sketch.cpp`、`SketchObject.cpp`、`SketchObjectGeometry.cpp`、`SketchObjectConstraints.cpp`、`SketchObjectExternal.cpp`、`SketchObjectOperations.cpp` 等草图对象、几何、约束、外部引用和 solver-facing 状态。
- `src/Mod/Assembly/App`：后续装配体、Link、Joint 和求解语义来源。
- `cad-core/`：从 FreeCAD 抽出的独立 C++17/CMake Core；不依赖 Qt、`src/Gui`、Workbench、ViewProvider、TaskPanel 或 Web 会话。
- `cad-core/include/cad_core/document` 与 `cad-core/src/document`：中立 `Document`、`DocumentObject`、属性链接和 JSON 解析。
- `cad-core/include/cad_core/graph` 与 `cad-core/src/graph`：依赖分析、拓扑排序和 recompute plan。
- `cad-core/include/cad_core/runtime` 与 `cad-core/src/runtime`：diagnostics、`ComputeContext`、feature registry 和 recompute loop。
- `cad-core/include/cad_core/features` 与 `cad-core/src/features`：`SketchObject`、`Body`、`FeatureBase`、`FeatureExtrude`、`Pad`、`Pocket` 等 executor 和 FreeCAD 特征语义。
- `cad-core/include/cad_core/geometry` 与 `cad-core/src/geometry`：OCCT 几何构造、bbox、volume、mesh、kernel metadata 和导出逻辑。
- `cad-core/include/cad_core/topo` 与 `cad-core/src/topo`：subshape map、stable subname、后续 `NamedShape` / topo naming / ElementMap 落点。
- `cad-core/include/cad_core/adapters` 与 `cad-core/src/adapters`：CLI 与 C ABI adapter。adapter 只做协议转换，不承接 FreeCAD 业务语义。
- `cad-core/fixtures/mvp` 与 `cad-core/fixtures/p2`：当前 CAD Core 验收输入和 FreeCAD 期望输出。
- `docs/CADCore方案`：CAD Core 抽取方案、MVP/P2 设计和当前状态。
- `docs/建模过程说明`：面向实现理解的 FreeCAD 建模链路说明。

## CAD Core 模块框架
- `cad-core` 的长期结构按本地 FreeCAD module 语义对齐，而不是按一般后端分层随意重新命名；框架依据见 `docs/CADCore方案/00-CAD-Core抽取方案.md`。
- `document/` 对齐 `src/App` 的文档对象和属性系统，只做中立输入模型、对象索引、链接解析和诊断，不塞入 PartDesign 或 Sketcher 业务规则。
- `graph/` 只处理对象依赖、目标选择、循环诊断和 recompute 顺序；不要把特征几何逻辑写到 graph 层。
- `runtime/` 只负责 feature registry、compute context、diagnostics 和执行调度；`FeatureExecutor` 可以作为调度接口存在，但 executor 不是所有语义的归属地。
- `features/` 对齐 `src/Mod/Sketcher/App` 与 `src/Mod/PartDesign/App` 的类和编译单元；新增或迁移 FreeCAD 语义时，优先落到和 FreeCAD 类/编译单元同名或同层的 C++ 文件。
- `geometry/` 承接 OCCT shape 构造、mesh、bbox、volume 和低层几何能力；不要把高层 FreeCAD 属性语义散落在 geometry helper 中。
- `topo/` 承接 stable subname、subshape map、`NamedShape`、`ElementMap`、MapperHistory 等命名传播能力；不要在 feature executor 或 adapter 中靠输出修正替代 topo 账本。
- 每次结构迁移优先做文件边界调整和清晰调用关系，行为变更另按具体 FreeCAD 源文件做实现任务；不要为了“小文件化”拆散 `WireJoiner`、FaceMaker、ElementMap / MapperHistory 这类依赖内部账本的状态机。

## 构建与运行
- 上游 FreeCAD 的完整构建遵循仓库原有 `CMakeLists.txt`、`CMakePresets.json`、`pixi.toml` 与 `.github/workflows`；不要为了 `cad-core` 任务随意改动上游构建系统。
- `cad-core` 使用 CMake、C++17、OpenCASCADE CONFIG package 和 `nlohmann/json.hpp`：
  ```bash
  cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
  cmake -S . -B build
  cmake --build build
  ```
- 运行 MVP recompute：
  ```bash
  cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
  mkdir -p out
  ./cad-core recompute fixtures/mvp/rect-pad.json --output out/rect-pad.result.json
  ```
- 运行当前 Python 验收测试：
  ```bash
  cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
  python3 -m unittest tests/test_mvp.py
  ```
- `cad-core-lib` 是核心库 target；`cad-core` 是 CLI adapter；`cad_core_ffi` 是 C ABI adapter。新增能力时先保证核心库边界成立，再暴露到 adapter。
- `cad-core/build/`、`cad-core/cad-core`、`__pycache__/` 属于生成物或本地构建产物；除非任务明确要求，不要把它们当作源码编辑。
- 本机已安装 FreeCAD.app，调试时直接调用 `FreeCADCmd`，指向 `/Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd`。
- 原生 FreeCAD 当前可用；但在 Codex sandbox 内直接运行 `FreeCADCmd` 可能报 `Incompatible processor. This Qt build requires the following features: neon`。这只说明 sandbox 执行环境不适合启动该 Qt/FreeCAD 进程，不代表本机 FreeCAD 不可用。
- 需要重新采集 FreeCAD fixture、运行 oracle collector、或执行原生 FreeCAD/WireJoiner probe 时，应在本机非 sandbox 环境运行 `FreeCADCmd`，再以该输出作为 oracle 判断依据。
- 临时 probe 不能假设普通 Python `__main__` 会被 FreeCAD CLI 执行；若 `FreeCADCmd` 启动成功但没有执行脚本主体，应改成和现有采集脚本一致的 FreeCAD CLI 触发方式后再重跑。

## OpenCascade / OCCT 使用规则
- `cad-core` 直接使用 OCCT C++ API；新增几何能力时先确认 FreeCAD 在 `src/Mod/Part/App`、`src/Mod/PartDesign/App` 或相关模块中的调用路径，再决定是否封装到 `geometry/`、`features/` 或 `topo/`。
- 新增 OCCT 模块依赖时，同步维护 `cad-core/CMakeLists.txt` 的 source list、include、link library 和必要的 Apple RPATH 设置。
- 低层 OCCT helper 放在 `geometry/` 或 `topo/`，不要让 adapter、JSON parser 或高层 feature 文件散落重复的 OCCT 细节。
- C++ API 边界要清楚：核心语义类型放在头文件中保持稳定；临时实现细节放 `.cpp`；不要把 GUI、Web 或测试 fixture 专用结构引入核心 public API。
- 修改 C++ 代码后遵循仓库 `.clang-format` 风格；不要为局部修复重排大段无关代码。

## 编码风格与命名约定
- 上游 `src/` 保持 FreeCAD 既有 C++/Python/CMake 风格和模块边界，不做无关重构。
- `cad-core` 使用 C++17；公共头文件放 `include/cad_core/...`，实现放 `src/...`，命名空间保持 `cad_core::<module>`。
- 文件和类型命名优先跟 FreeCAD 语义对齐：`FeatureExtrude` 相关通用逻辑放 `feature_extrude.*`，Pad/Pocket 只保留各自特化语义。
- 错误处理优先显式返回 diagnostics 或结构化结果，不要静默忽略 JSON 解析失败、链接缺失、OCCT 构造失败、文件导入失败或未支持属性。
- 核心逻辑放 `cad-core-lib`；CLI 和 C ABI 只做参数解析、协议转换和错误封装，不承载建模语义。
- 拓扑命名相关改动优先查看 FreeCAD 的 `TopoShape*`、`TopoShapeMapper*`、`PropertyTopoShape*`、FaceMaker、WireJoiner 和 `cad-core/src/topo/`，保持命名、映射与索引语义一致。
- 修复 FreeCAD parity、拓扑命名、内部面、几何排序或 fixture 偏差时，起初就必须按完整通用语义设计，优先补齐 FreeCAD / OpenCascade 对应的通用流程、历史映射与排序规则；不得用只覆盖单一 fixture 形态的窄路径或特异化处理替代通用实现。若短期不得不落窄路径，必须在相邻代码注释和方案文档中说明临时性、适用边界、FreeCAD 依据与后续通用化路径，并避免继续扩大 fixture 特判。
- 公开 API、核心语义类型、executor、mapper/history 规则等承载 FreeCAD 几何库抽取语义的新增函数、结构体、枚举或字段，必须在相邻 C++ 注释或实现注释中标注 FreeCAD 依据：写明 FreeCAD 源文件绝对路径、类/函数名，并摘录能支撑当前语义的 FreeCAD 原文短句或字段名；不要只写“参考 FreeCAD”。普通 helper、内部实现细节、测试辅助结构若不承载 FreeCAD 语义，不强制标注。示例：`// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePad.cpp::Pad::execute(), calls "buildExtrusion(ExtrudeOption::MakeFace | ExtrudeOption::MakeFuse)".`
- 审计 FreeCAD 依据路径时，`/Users/li/...` 与 `/Users/admin/...` 只代表不同机器上的本地用户目录；只要后续仓库相对路径、源码文件、类/函数和关键短句一致，不得仅因 `li` / `admin` 用户目录不同判定依据路径不可追溯。若需要在当前机器复核，可把这两个前缀视为同一 FreeCAD 源码树根的等价用户目录前缀。

## FreeCAD 迁移实现纪律
- 涉及 FreeCAD parity、草图内部面、拓扑命名、WireJoiner、FaceMaker、ShapeFix、特征重建或历史映射的实现，必须先给出 FreeCAD 调用链和 `cad-core` 分层映射，再写代码。最少要明确：FreeCAD 源文件绝对路径、类/函数、关键字段/短句、调用顺序、对应的 `document` / `graph` / `runtime` / `features` / `geometry` / `topo` / `adapters` 落点。
- 禁止从 fixture 输出倒推业务逻辑。不得在 `cad-core/src/features/sketch_object.cpp`、`cad-core/src/runtime`、`cad-core/src/graph` 或 adapter 层中新增 `ellipse && bspline`、几何类型排序、source edge 猜测、split edge compound 注入、degenerate face 注入、按 fixture 名称分支等补丁式逻辑。
- 出现 `InternalEdgeN`、`InternalVertexN`、open wire 或 split fragment 映射差异时，优先按四层矩阵定位：1）`FaceMakerBuildFace` 的 face / edge / vertex 几何结果是否与 FreeCAD 一致；2）`WireJoiner::getOpenWires()` 的 open wire 几何结果是否与 FreeCAD 一致；3）raw compound / child shape identity 是否在组合时被重建或复制；4）`NamedShape` / `ElementMap` 是否完整消费 `MapperHistory(aHistory)`。如果前 1-3 层一致，只在第 4 层出现 stable subname、internal element 或 source trace 差异，应归类为 history 到 `ElementMap` 的传播缺口，不得在 sketch executor 中按几何类型、fixture 名、split 顺序或 source index 继续补猜测逻辑。
- 如果 FreeCAD 语义依赖底层能力，例如 `myPreSplitHistory`、`mySplitter`、`MapperMaker`、`MapperHistory`、`myShapesToReturn`、`FaceMaker::postBuild()`、`WireJoiner` history 或 `ShapeFix` history，而 `cad-core` 还没有对应能力，必须优先补 `geometry` / `topo` / 高层 API；不得在 sketch executor、document executor、adapter 或前端导出层用几何猜测绕过。
- 如果 FreeCAD 语义依赖某个内部账本或状态机，例如 `WireJoinerP::EdgeInfo`、`WireInfo`、`wireInfo/wireInfo2`、`iteration/iteration2`、`superEdge`、`MapperHistory` 或 `ElementMap` 生命周期，而 `cad-core` 里没有等价结构，不得先做结果 pruning、后处理过滤或输出修正当主路径；必须先补等价账本/状态机，或者把现有实现明确标成临时 fallback，并在相邻代码注释和方案文档中写清适用边界、FreeCAD 正确路径、删除条件和后续替换步骤。
- 只要出现“输出端修修剪剪越来越多”、同一方向反复修改仍无法正确实现、fallback/pruning 规则继续叠加、或需要靠 source/split 几何形态推断 FreeCAD ownership 的迹象，必须立即视为流程告警：暂停继续加规则，重新按 FreeCAD 内部账本/状态机定位问题。
- `cad-core/src/features/sketch_object.cpp` 只能表达 FreeCAD `SketchObject` 的业务调用顺序和属性语义，不承担几何内核推理、拓扑命名传播、split history 合成或 face 排序职责。FaceMaker / WireJoiner 账本放在 `geometry` 或 `features` 的对应正式模块，命名传播放在 `topo`。
- 若短期确实必须引入窄路径 fallback，必须同时满足：相邻代码注释标明“临时 fallback”；写清适用 fixture/边界；写明 FreeCAD 正确路径和对应源码依据；写明删除条件；不得继续在该 fallback 上叠加新的 fixture 特判。
- 涉及实质 FreeCAD 语义迁移或 executor 主路径切换时，实施顺序固定为：1）读 FreeCAD 并记录调用链；2）补 OCCT/geometry 运行态；3）补 `topo` mapper/history；4）补正式 `NamedShape`/高层 API；5）切换 executor 主路径；6）用 fixture 和语义单测验证；7）删除旧 fallback 和 synthetic name。不得跳过前四步直接在 executor 中凑输出。
- 验证不能只看现有 fixture parity。凡是修复内部面、split、open wire 或拓扑命名，至少补充能约束通用语义的单测或方案验收项，例如 self-intersection 与 inter-edge intersection 同时存在、bounded faces 与 open wires 同时存在、source edge 一对多 fragment 映射、splitter 失败继续使用原 edges、`ElementMapPolicy::Drop` 早退语义等。
- fixture 评估中，`InternalFaceN`、`InternalEdgeN`、`InternalVertexN` 等命名顺序与 FreeCAD 不一致时，只要几何等价且本仓库输出顺序稳定，不得算作硬失败；应单独归类为“命名顺序差异”或类似非失败项。face/edge/vertex 数量不同、几何内容不同、稳定 subname 丢失或引用语义不稳定仍然算失败。

## 仓库工作偏好与排查入口
- 处理 FreeCAD parity、fixture 或 oracle 问题时，先明确当前问题属于 oracle 采集、`cad-core` 实现、命名顺序差异、pending/known-mismatch 分组还是文档状态；结论必须直接回答 expected 与当前 `cad-core` 表现是否一致，并列出剩余不一致项。
- 用户指定文档落点时，结果要写入对应仓库目录，而不是只在聊天里总结：CAD Core 抽取方案、fixture 偏差和排查方案优先放 `docs/CADCore方案`；建模链路和已经接受的业务语义优先放 `docs/建模过程说明`；后续计划或暂不实现方案放到对应主题目录或新增清晰命名的方案文件。
- 写方案、排查记录、实现状态或回归文档时，不要记录流水账，只记录值得关注的内容：当前基线、关键结论、FreeCAD / OpenCascade 依据、已完成的语义性调整、剩余风险、验收命令和下一步。不要逐条追加“修改某处后执行构建、结果通过、格式化 warning”这类过程日志；若验证结果重要，只保留最终验证结论或对判断有影响的失败输出。
- 用户说“不需要太详细，只需要把框架说清楚”时，文档保持框架级和短结论；用户说“大白话解释一下”时，先解释具体流程和对象关系，再进入源码、实现或文档更新。
- 解释草图内部面时必须区分 sketch 的原始 `Shape` 和辅助结果 `InternalShape`：`FaceMakerBuildFace` 失败后得到空 `InternalShape` 是 FreeCAD parity，不代表原始 sketch 边丢失；open profile/open wire 语义应单独表达，不要强行混回 FreeCAD 风格 `InternalShape`。
- 扩展 Pad/Pocket fixture 或 executor 前，先盘点 `cad-core/fixtures/{mvp,p2}`、`cad-core/src/features/feature_extrude.cpp`、`cad-core/src/features/pad.cpp`、`cad-core/src/features/pocket.cpp` 和对应 FreeCAD 源码的当前覆盖和缺口；优先新增或补齐 oracle case，只有证明 collector 或 expected 本身错误时才先改采集脚本/期望数据。

## 文档命名规范
- 当前仓库已有两类文档风格：`docs/建模过程说明` 使用编号式系列文档，`docs/CADCore方案` 使用主题式方案文档；新增文档优先沿用所在目录的现有命名风格。
- 若新增临时排查、修复方案、重构方案且所在目录没有既有编号规范，可沿用 `M-D-HH-mm-主题名称.md` 格式。
- 修复方案、重构方案、实施方案等方案类文档在代码实现完成并验证后，可将文件重命名为 `M-D-HH-mm-【已实现】主题名称.md`，保持原时间前缀不变；已有编号系列不强制套用该规则。
- 已存在的上游说明文档（如 `README.md`、`CONTRIBUTING.md`）不因该规则强制改名，除非用户明确要求统一整理。

## 测试指南
- 代码检查默认只覆盖本次修改涉及的文件、target 或 fixture 范围；仅在用户明确要求时执行全量 FreeCAD 构建或全量 CI 等重操作。
- 文档类修改通常不构建、不跑测试。
- 代码修改后，只有在必要且用户未禁止时，最多执行一次相关范围的构建或测试；不要默认跑完整 FreeCAD CI。
- 不要运行全仓库格式化、全量 lint 或全量 FreeCAD build，除非用户在当前任务中明确要求。
- 排查、检查和验证时只看本次任务相关文件与目录，不要扫描无关目录。
- `cad-core` 功能变更优先使用：
  ```bash
  cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
  cmake --build build
  python3 -m unittest tests/test_mvp.py
  ```
- 如果 build 目录不存在或 CMake 配置过期，先运行：
  ```bash
  cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
  cmake -S . -B build
  ```
- 涉及 OCCT、FreeCAD 原生 runtime、oracle 采集或 GUI/Qt 的验证可能依赖本机环境；运行前先确认是否确实需要，sandbox 中的 FreeCADCmd/Qt 错误不能直接当作实现失败。

## Git 与工作区
- 当前工作区可能有用户或其它任务留下的未提交改动；开始编辑前看 `git status --short`，只改本次任务相关文件。
- 不要回退、覆盖或清理自己没有改的文件。特别是 `cad-core/` 下已有源码、fixture、docs 改动，除非用户明确要求，否则只作为上下文读取。
- 不使用 `git reset --hard`、`git checkout --` 等破坏性命令，除非用户明确要求。
- 如果需要提交，先展示当前变更边界，确认只包含本次任务相关内容；不要把本地生成物、build 目录或 `__pycache__` 混进提交。

## Web 内容获取策略
- 获取具体网页正文时，优先使用 `smart-web-fetch` skill，不直接抓取原始 HTML。
- `smart-web-fetch` skill 通过 Jina Reader、`markdown.new`、`defuddle.md` 获取更清晰的 Markdown 输出，并带多级降级策略。
- 对于“读取某个 URL 的正文并总结/抽取信息”这类任务，优先触发 `smart-web-fetch`，不要默认走内置网页抓取流程。
- 若平台级系统规则要求必须使用内置网络浏览/检索能力校验最新信息，则先满足该校验要求；在需要读取页面正文时，再配合 `smart-web-fetch` 获取清洗后的内容。
