# Repository Guidelines（仓库指南）

## 仓库定位
- 这个 checkout 是本地 FreeCAD 源码树，同时也是抽取 CAD Core 语义的源码依据；不要把它当成普通上游同步仓库或旧的 Rust `opencascade-rs` workspace。
- 当前目标是基于本仓库的 FreeCAD 实现，把建模核心逻辑抽到 `/Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core`。
- `src/` 下的 FreeCAD C++ 源码是语义 oracle；`cad-core/` 是正在抽出的无 UI、无 Qt、可被 CLI/FFI/Web/桌面外壳调用的几何核心。
- 做架构、接口、fixture、诊断或文档取舍时，优先服务“从 FreeCAD 抽核心逻辑到 `cad-core`”这个目标，而不是泛化成新的完整 CAD 框架。

## CAD Core 边界
- CAD Core 应包含：FreeCAD 风格 `DocumentObject graph`、属性与链接解析、依赖分析、recompute 调度、特征执行、OCCT 几何构造、topo/subshape 映射、mesh/诊断导出。
- CAD Core 不应依赖：Qt event loop、`src/Gui`、ViewProvider、TaskPanel、Workbench、Selection、TreeView、PropertyEditor、Web route、Session、JWT、数据库或用户系统。
- `DocumentObject graph` 是输入真相；shape、mesh、subshape map、diagnostics 都是单次 recompute 的计算结果。不要让 `cad-core` 依赖 GUI 文档会话或跨请求隐藏状态。
- 输入模型保持 FreeCAD 风格：`Objects[]`、`Name`、`ID`、`TypeId`、`Properties`、`PropertyLink*`。不要新增并行的 `featureType`、`operation`、`params` 等替代 schema。
- 外壳层只通过 adapter 调用 CAD Core。`cad-core-lib` 承载核心逻辑；`cad-core` 是 CLI adapter；`cad_core_ffi` 是 C ABI adapter。

## 项目结构
- `src/`：上游 FreeCAD 源码，是行为语义和调用链的主要依据。
  - `src/App`：`Document`、`DocumentObject`、属性、链接和 recompute 基础。
  - `src/Mod/Sketcher/App`：`SketchObject`、草图几何、约束、外部引用和草图输出。
  - `src/Mod/Part/App`：`TopoShape`、布尔、prism、FaceMaker、WireJoiner、ShapeFix、topo 相关语义。
  - `src/Mod/PartDesign/App`：`Body`、`FeatureBase`、`FeatureAddSub`、`FeatureExtrude`、`Pad`、`Pocket` 等特征链。
  - `src/Mod/Assembly/App`：后续装配体、Link、Joint 和求解语义。
- `cad-core/`：抽取出来的 C++17/CMake 核心实现。
  - `include/cad_core/document`、`src/document`：中立 Document/Object/Link 模型和 JSON 解析。
  - `include/cad_core/graph`、`src/graph`：依赖分析和 recompute plan。
  - `include/cad_core/runtime`、`src/runtime`：diagnostics、compute context、feature registry、recompute loop。
  - `include/cad_core/features`、`src/features`：`SketchObject`、`Body`、`FeatureBase`、`FeatureExtrude`、`Pad`、`Pocket` 等 executor。
  - `include/cad_core/geometry`、`src/geometry`：OCCT shape 导出、mesh、bbox、volume、kernel metadata。
  - `include/cad_core/topo`、`src/topo`：subshape map 和后续 stable subname/topo naming 落点。
  - `include/cad_core/adapters`、`src/adapters`：CLI 与 C ABI 边界。
- `cad-core/fixtures/mvp` 与 `cad-core/fixtures/p2`：当前 CAD Core 验收输入和 FreeCAD 期望输出。
- `docs/CADCore方案`：CAD Core 抽取方案、MVP/P2 设计和状态文档。
- `docs/建模过程说明`：面向实现理解的 FreeCAD 建模链路说明。

## FreeCAD 到 cad-core 的映射纪律
- 涉及 CAD 语义时，先读本仓库 FreeCAD 源码，再改 `cad-core`。最少明确 FreeCAD 源文件、类/函数、关键属性、调用顺序和对应 `cad-core` 落点。
- 不要从 fixture 输出倒推业务逻辑。fixture 用来验收和定位偏差，不能替代 FreeCAD 调用链。
- 新增特征时优先落到 FreeCAD 同名或同层语义的文件：例如 `FeatureExtrude` 相关通用逻辑放 `feature_extrude.*`，不要塞进 `pad.*` 或 CLI adapter。
- `SketchObject` 只表达草图对象语义；`Body` 只表达 Body/Tip/Group/BaseFeature 语义；Pad/Pocket 的共用拉伸路径应收敛到 `FeatureExtrude` 和 Add/Sub 通道。
- 如果 FreeCAD 语义依赖内部账本或历史映射（如 FaceMaker、WireJoiner、TopoShape history、ElementMap/topo naming），优先补对应核心模型，不要在输出端靠几何猜测、fixture 名称或结果修剪绕过。
- 短期 fallback 必须标注适用边界、FreeCAD 正确路径和删除条件；不要在临时 fallback 上继续叠加特判。

## 构建与运行
- 上游 FreeCAD 的完整构建遵循仓库原有 `CMakeLists.txt`、`CMakePresets.json`、`pixi.toml` 和 `.github/workflows`；不要为了 `cad-core` 任务随意改动上游构建系统。
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
- `cad-core/build/`、`cad-core/cad-core`、`__pycache__/` 属于生成物或本地构建产物；除非任务明确要求，不要把它们当作源码编辑。
- 本机 FreeCAD.app 的命令入口是 `/Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd`。需要采集 FreeCAD oracle 或跑临时 probe 时，优先使用本机 FreeCAD 运行态；如果 sandbox 内出现 Qt/processor 相关错误，不代表本机 FreeCAD 行为不可用。

## 编码风格
- 上游 `src/` 保持 FreeCAD 既有 C++/Python/CMake 风格和模块边界，不做无关重构。
- `cad-core` 使用 C++17；公共头文件放 `include/cad_core/...`，实现放 `src/...`，命名空间保持 `cad_core::<module>`。
- 核心逻辑放 `cad-core-lib`；CLI 和 C ABI 只做参数解析、协议转换和错误封装，不承载建模语义。
- 错误和不支持能力应通过稳定 diagnostics 返回；不要静默吞掉解析失败、链接缺失、OCCT 构造失败或未支持属性。
- 修改 OCCT 几何路径时，保持 CMake target 依赖清楚；新增 OCCT 模块后同步维护 `target_link_libraries`。
- 修改上游 C++ 或 `cad-core` C++ 时，优先使用仓库现有 `.clang-format` 风格；不要为局部改动重排大段无关代码。

## 文档规则
- 写到 `docs/` 的内容应面向当前状态和关键结论：当前基线、FreeCAD/OCCT 依据、已完成语义、剩余风险、验收命令和下一步。
- 不记录流水账，不追加“执行了某命令、格式化通过”这类过程日志；验证结果重要时只保留最终结论或关键失败输出。
- `docs/CADCore方案/00-CAD-Core抽取方案.md` 是 CAD Core 抽取边界的主说明；涉及 MVP/P2 细化时优先更新该目录下已有方案文档。
- `docs/建模过程说明` 用来解释 FreeCAD 建模链路；新增说明保持编号/主题风格，不套用旧的时间戳命名规则。
- 用户指定文档落点时，结果写入对应仓库文件，不只在聊天里总结。

## 测试指南
- 文档类修改通常不构建、不跑测试。
- 代码修改后只跑与本次改动相关的最小范围测试；不要默认跑完整 FreeCAD CI 或全量上游构建。
- `cad-core` 功能变更优先跑：
  ```bash
  cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
  cmake --build build
  python3 -m unittest tests/test_mvp.py
  ```
- 新增或调整 fixture 时，说明它验证的是 FreeCAD 语义、CAD Core 解析/调度、OCCT 几何结果、topo/subshape map，还是 diagnostics。
- 若输出顺序与 FreeCAD 不同但几何等价且顺序稳定，应单独归类为“命名/顺序差异”；几何数量、体积、bbox、引用语义或 diagnostics 不一致才算实现偏差。

## Git 与工作区
- 当前工作区可能有用户或其它任务留下的未提交改动；开始编辑前看 `git status --short`，只改本次任务相关文件。
- 不要回退、覆盖或清理自己没有改的文件。特别是 `cad-core/` 下已有源码、fixture、docs 改动，除非用户明确要求，否则只作为上下文读取。
- 不使用 `git reset --hard`、`git checkout --` 等破坏性命令，除非用户明确要求。

## Web 内容获取策略
- 获取具体网页正文时，优先使用 `smart-web-fetch` skill，不直接抓取原始 HTML。
- `smart-web-fetch` 通过 Jina Reader、`markdown.new`、`defuddle.md` 获取更清晰的 Markdown 输出，并带多级降级策略。
- 如果平台规则要求必须先用内置浏览/检索能力确认最新信息，则先满足该要求；需要读取正文时再配合 `smart-web-fetch`。
