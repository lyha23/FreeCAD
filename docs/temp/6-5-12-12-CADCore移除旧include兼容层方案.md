# CAD Core 移除旧 include 兼容层方案

## 当前基线

`cad-core` 主结构已经切到 FreeCAD 同构目录：

- `include/cad_core/app`
- `include/cad_core/base`
- `include/cad_core/sketcher`
- `include/cad_core/part`
- `include/cad_core/part_design`
- `include/cad_core/mesh`
- `include/cad_core/assembly`
- `include/cad_core/runtime`
- `include/cad_core/graph`
- `include/cad_core/adapters`

本轮收口后，旧 include facade 已删除：

- `include/cad_core/document`
- `include/cad_core/features`
- `include/cad_core/geometry`
- `include/cad_core/topo`
- `include/cad_core/compatibility/legacy_paths.h`

当前仓库内部源码、测试和 fixture 没有旧 include 路径依赖；旧 namespace alias 也已从 public headers 清除。由于目标是不保留向后兼容，最终状态必须持续保持旧 include 目录和旧 namespace alias 都不存在。

## 目标状态

1. `cad-core` public include 只暴露 FreeCAD 同构模块路径。
2. 删除 `document/features/geometry/topo` 旧 include facade。
3. 删除 `compatibility/legacy_paths.h` 及空的 `compatibility` 目录。
4. 删除旧 namespace alias，例如 `cad_core::document`、`cad_core::features`、`cad_core::geometry`、`cad_core::topo`。
5. 新增边界测试，防止旧 include 路径或旧 namespace alias 被重新引入。
6. `README.md` 不再描述任何 compatibility facade。

## 非目标

- 不保留宏开关式兼容路径。
- 不迁就外部旧调用方。
- 不借本次结构收口调整 FreeCAD 业务语义、拓扑命名、WireJoiner、FaceMaker 或 executor 行为。
- 不为了小文件化拆分 `WireJoiner`、FaceMaker 或其它内部账本状态机。

## 实施顺序

### 第一步：硬删除旧 include facade

删除：

```text
cad-core/include/cad_core/document/
cad-core/include/cad_core/features/
cad-core/include/cad_core/geometry/
cad-core/include/cad_core/topo/
cad-core/include/cad_core/compatibility/legacy_paths.h
```

如果 `cad-core/include/cad_core/compatibility/` 删除文件后为空，也删除该目录。

同步更新：

- `cad-core/README.md`：移除 `compatibility/`、`document/`、`features/`、`geometry/`、`topo/` 的 legacy facade 描述。
- 若有任何源码、测试或 fixture 仍 include 旧路径，改到真实归属路径。

推荐先跑：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD
rg -n '#include [<"]cad_core/(document|features|geometry|topo)/' cad-core/include cad-core/src cad-core/tests cad-core/fixtures
```

预期结果：无命中。

### 第二步：加 include 边界测试

新增 `cad-core/tests/test_include_boundaries.py`，至少检查：

1. 下列目录不存在：
   - `cad-core/include/cad_core/document`
   - `cad-core/include/cad_core/features`
   - `cad-core/include/cad_core/geometry`
   - `cad-core/include/cad_core/topo`
   - `cad-core/include/cad_core/compatibility`
2. 仓库内部不再出现旧 include：
   - `cad_core/document/`
   - `cad_core/features/`
   - `cad_core/geometry/`
   - `cad_core/topo/`

测试应扫描：

- `cad-core/include`
- `cad-core/src`
- `cad-core/tests`

不扫描 build、graphify 输出和缓存目录。

### 第三步：删除旧 namespace alias

旧 include 目录删除后，再单独删除旧 namespace alias。先用下面命令定位：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD
rg -n 'namespace cad_core::(document|features|geometry|topo)|cad_core::(document|features|geometry|topo)|using namespace cad_core::part' \
  cad-core/include/cad_core cad-core/src cad-core/tests
```

处理规则：

- `Document`、`DocumentObject`、属性和链接语义只保留 `cad_core::app`。
- `FeatureExecutor` 只保留 `cad_core::runtime`。
- `SketchObject` 只保留 `cad_core::sketcher`。
- `Body`、Pad、Pocket、DressUp、Pattern、Transform、Datum 等只保留 `cad_core::part_design`。
- FaceMaker、WireJoiner、TopoShape、PropertyTopoShape、ShapeFix、RefineModel 等只保留 `cad_core::part`。
- Placement 等基础值类型只保留 `cad_core::base`。
- Mesh import 只保留 `cad_core::mesh`。

这一刀可能触发较多编译错误，应单独实施、单独验证；修复只能指向真实模块，不能恢复兼容 alias。

### 第四步：扩展边界测试覆盖 namespace

在 `test_include_boundaries.py` 中追加禁止项：

- `namespace cad_core::document`
- `namespace cad_core::features`
- `namespace cad_core::geometry`
- `namespace cad_core::topo`
- `cad_core::document::`
- `cad_core::features::`
- `cad_core::geometry::`
- `cad_core::topo::`
- `using namespace cad_core::part`

允许项仅限历史文档中的普通文字说明；源码和测试不允许。

## 验收命令

### 本轮短跑

适用于第一步和第二步：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest tests/test_include_boundaries.py
cd ..
git diff --check -- cad-core docs/temp/6-5-12-12-CADCore移除旧include兼容层方案.md
cd cad-core
graphify update .
```

### 阶段回归

适用于删除 namespace alias 后：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests/test_include_boundaries.py tests/test_mvp.py tests/test_adapters.py tests/test_feature_flows.py
```

### 重型收口

仅在准备冻结结构边界或发现 adapter / C ABI 受影响时执行：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest discover tests
```

## 风险与处理

- 删除旧 include facade 可能破坏仓库外旧调用方；这是符合目标的行为，不回补兼容。
- 删除 namespace alias 可能暴露真实命名空间归属不清的问题；按 FreeCAD 源文件归属修正 include 和限定名，不新增中间兼容 namespace。
- 如果 `cad_core::geometry` 或 `cad_core::topo` 仍被业务代码直接依赖，应视为迁移未完成，而不是恢复兼容层。
- 若构建失败来自旧 namespace alias 删除，优先修调用点到真实模块；不要用 `using namespace` 或新 alias 绕回旧结构。

## 完成标准

1. 旧 include facade 目录不存在。
2. `compatibility/legacy_paths.h` 不存在。
3. `README.md` 只描述当前 FreeCAD 同构模块。
4. 边界测试能阻止旧 include 路径重新出现。
5. 删除 namespace alias 的后续阶段完成后，源码和测试中不再出现旧 `document/features/geometry/topo` namespace。

## 当前验收结论

- `cad-core/include/cad_core` 只保留 `app/base/sketcher/part/part_design/mesh/assembly/runtime/graph/adapters` 等真实模块目录。
- `cad-core/tests/test_include_boundaries.py` 覆盖 public include 根目录只含模块目录、旧 include facade 目录缺失、旧 include 路径禁用、源码/测试中旧 namespace 归属表述禁用、README 旧 facade 描述禁用和精确 `using namespace cad_core::part;` 禁用。
- 源码边界以本方案的本轮短跑和阶段回归命令为准；普通历史文档文字不参与边界测试失败判定。
