# C3M4 Part Workbench Surface ProjectOnSurface 独立主线草案

## 当前基线

- 来源主线：`docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/`。
- S4 裁决：RuledProjection 主线不实现 `Part::ProjectOnSurface`，只把它发布为 source-audited / planned；`Part::RuledSurface` edge/edge 第一批保持 supported。
- S0 live 基线（2026-06-20）：`pwd=/Users/li/Chili3DProject/FreeCAD`；`HEAD=34fae62fc8`；`git log -1 --oneline=34fae62fc8 docs: 添加 ProjectOnSurface 第二批工作队列`；开始编辑前 `git -c core.quotepath=false status --short -uall` 为空。
- 已完成第一批：`cad-core` 已注册 `Part::ProjectOnSurface` executor，native expected collector 已能覆盖 `Part::ProjectOnSurface`、`App::PropertyLinkSubList` 和 `App::PropertyDirection`；当前发布口径为 `supported_expected_backed_first_slice`。
- 第一批 fixtures：`cad-core/fixtures/c4m1/part-project-on-surface-edge-plane.json` 与 `cad-core/fixtures/c4m1/part-project-on-surface-deferred-boundaries.json`。
- 第一批边界：只支持 `Mode=Edges`、`Height=0`、`Offset=0`、单 `Projection` edge/wire 到单 `SupportFace`，并发布普通 indexed `NamedShape`，不宣称 projected edge provenance mapper。
- S1 live 基线（2026-06-20）：`pwd=/Users/li/Chili3DProject/FreeCAD`；`HEAD=8a1a905f6c`；`git log -1 --oneline=8a1a905f6c docs: 冻结PROJSURF S0基线与第二批范围`；开始编辑前 `git -c core.quotepath=false status --short -uall` 为空。
- S1 已完成：`Mode=Faces` / `Mode=All` 在 `Height=0`、`Offset=0`、单 face projection 到单 support face 的范围内已 expected-backed；face with hole 通过 projected wires 的 parametric-space rebuild 保留 inner wire；`Mode=Edges` 遇到 face input 会按 FreeCAD `filterShapes()` 拆成 wire 输出。
- S1 fixtures：`part-project-on-surface-face-plane`、`part-project-on-surface-face-hole-plane`、`part-project-on-surface-face-edges-mode`、`part-project-on-surface-face-all-plane`。
- S2 live 基线（2026-06-20）：`pwd=/Users/li/Chili3DProject/FreeCAD`；`HEAD=a47261c764`；`git log -1 --oneline=a47261c764 feat: 实现PROJSURF S1 face投影重建`；开始编辑前 `git -c core.quotepath=false status --short -uall` 为空。
- S2 已完成：`Mode=All` 且 `Height >= Precision::Confusion()` 时，rebuilt face 会沿反向 `Direction * Height` 经 `BRepPrimAPI_MakePrism` 生成 solid；`Mode=Faces` 即使 `Height>0` 仍保持 face 输出，`Height < Precision::Confusion()` 仍走 face 路径。
- S2 fixture：`part-project-on-surface-height-boundaries` 同时覆盖 `Mode=All, Height=1.5` solid 和 `Mode=Faces, Height=1.5` face 边界。
- S3 live 基线（2026-06-20）：`pwd=/Users/li/Chili3DProject/FreeCAD`；`HEAD=07f924301b`；`git log -1 --oneline=07f924301b feat: 实现PROJSURF S2 Height实体路径`；开始编辑前 `git -c core.quotepath=false status --short -uall` 为空。
- S3 已完成：`Offset != 0` 时按 FreeCAD `getOffsetPlacement()` normalize `Direction` 后乘 `Offset`，并在 `createCompound()` 阶段移动每个 child shape；投影、face rebuild、Height solid 和 Mode filter 均先于 Offset placement 执行。
- S3 fixtures：`part-project-on-surface-edge-offset`、`part-project-on-surface-face-offset`、`part-project-on-surface-height-offset-boundary` 分别覆盖 edge offset、face offset 和 Height+Offset 组合边界。
- 当前缺口：多 `Projection` compound 顺序和 projected edge provenance mapper/history 仍未发布；full `ProjectOnSurface` 仍不能写成 supported。

## FreeCAD 依据

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.h::Part::ProjectOnSurface`：声明 `Mode`、`Height`、`Offset`、`Direction`、`SupportFace`、`Projection`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::tryExecute()`：`getSupportFace()` -> `getProjectionShapes()` -> `createProjectedWire()` -> `filterShapes()` -> `createCompound()` -> restore `Placement`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::projectWire()`：`BRepProj_Projection(wire, supportFace, dir)`，取最近 projected wire，再拆成 edges。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::projectFace()`、`createFaceFromParametricWire()`、`fixWire()`：S1 已迁移为 `cad-core/src/part/part_project_on_surface.cpp` 中的 projected face wires、parametric-space edge/wire rebuild 和 face fix/reverse retry helper。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::createSolidIfHeight()`：S2 已迁移 `Mode=All` + Height solid 路径。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::getOffsetPlacement()`、`createCompound()`：S3 已迁移 Offset placement 路径；Offset 不参与投影方向，也不修改 source graph，只移动本次 compound child shape。

## 已实现边界

当前只允许 `Height=0`、`Offset=0`、单 projection item、单 support face：

- `part-project-on-surface-edge-plane`：`Part::Line.Edge1` 沿 `Direction=(0,0,1)` 投影到 `Part::Plane.Face1`。
- `part-project-on-surface-face-plane`：`Mode=Faces` 的普通 face projection，输出 1 face / 1 wire / 0 inner wire。
- `part-project-on-surface-face-hole-plane`：`Mode=Faces` 的 face-with-hole projection，输出 1 face / 2 wires / 1 inner wire。
- `part-project-on-surface-face-edges-mode`：face input 在 `Mode=Edges` 下经 `filterShapes()` 拆成 wire，输出 0 face / 1 wire。
- `part-project-on-surface-face-all-plane`：`Mode=All`、`Height=0`、`Offset=0` 的 face projection，仍只输出 face，不触发 solid。
- `part-project-on-surface-height-boundaries`：`Mode=All`、`Height=1.5` 输出 1 solid / 6 faces / volume 9.0；同 fixture 内 `Mode=Faces`、`Height=1.5` 仍输出 1 face / volume 0.0。
- `part-project-on-surface-edge-offset`：`Mode=Edges`、非单位 `Direction=(0,0,2)`、`Offset=0.75`，输出 edge compound bbox 沿归一化方向移动到 `z=0.75`。
- `part-project-on-surface-face-offset`：`Mode=Faces`、非单位 `Direction=(0,0,2)`、`Offset=0.5`，输出 face compound bbox 沿归一化方向移动到 `z=0.5`，indexed subshape 保持稳定。
- `part-project-on-surface-height-offset-boundary`：`Mode=All`、`Height=1.5` 先生成 solid，再 `Offset=0.25` 移动 compound child；bbox 从 `z=-1.5..0` 移到 `z=-1.25..0.25`，volume 保持 9.0。
- `part-project-on-surface-deferred-boundaries`：继续拒绝多个 `Projection` item、缺失 support；`Mode=Faces` + edge input 目前记录为无 face 结果的边界诊断。
- capability 的 full 发布仍留到 S5；当前只允许 adapter capability 宣称 `supported_expected_backed_offset_slice`，不得把多 `Projection`、full `ProjectOnSurface` 或 projected edge provenance mapper/history 写成 supported。

## 第二批冻结范围

- `PROJSURF-S1`：已完成 `Mode=Faces` / `Mode=All` 在 `Height=0`、`Offset=0`、单 face projection 到单 support face 的 first slice，并覆盖 hole inner wire 与 `Mode=Edges` face input 拆 wire 边界。
- `PROJSURF-S2`：已完成 `Mode=All` + `Height` solid。FreeCAD 依据是 `createSolidIfHeight()` 只在 `Height >= Precision::Confusion()` 且 `Mode == All` 时沿反向 `Direction` prism；非 All 的 height 分支保持 FreeCAD face / wire 过滤语义，不生成 solid。
- `PROJSURF-S3`：已完成 `Offset` placement。FreeCAD 依据是 `getOffsetPlacement()` normalize `Direction` 后 scale `Offset`，再由 `createCompound()` 对每个 child `Moved(loc)`；已覆盖 edge/face 输出及 Height/Offset 交互。
- `PROJSURF-S4`：多 `Projection` ordering。FreeCAD 依据是 `getProjectionShapes()` 保留 `PropertyLinkSubList` 对象 / sub-name 顺序，`tryExecute()` 按 projection item 追加结果，`createCompound()` 按结果顺序 Add。
- `PROJSURF-S5`：capability 发布。只发布 S1-S4 已有 native expected、focused tests 和 adapter tests 证明的分支；若没有 ProjectOnSurface projected ownership mapper/history 账本，继续保留普通 indexed NamedShape 边界。

## 第二批工作步骤细分

队列目录：`docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线/工作步骤细分/`。

执行顺序：

1. `PROJSURF-S0`：刷新 live 基线、第二批 scope 和矩阵，不写 C++。
2. `PROJSURF-S1`：已实现 `Mode=Faces` / `Mode=All` 的 Height=0 face input rebuild / hole wires 第一批。
3. `PROJSURF-S2`：已实现 `Mode=All` + `Height` solid 路径。
4. `PROJSURF-S3`：已实现 `Offset` / `getOffsetPlacement()` 位移语义。
5. `PROJSURF-S4`：实现多 `Projection` item 和 compound 顺序。
6. `PROJSURF-S5`：发布 capability、文档和队列收口，不 overclaim full ProjectOnSurface。

## 非目标

- 不把 full Part surface family 或 full ProjectOnSurface 写成 supported。
- 不实现 GUI `TaskProjectOnSurface`、ViewProvider 或交互式 projection task panel。
- 不引入跨请求几何缓存；所有结果仍由请求内 DocumentObject graph 计算。
- 不把 projected edge provenance / mapper history 当作已完成，除非后续步骤补出可验证账本和 tests。

## 验收命令

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线/工作步骤细分 --format markdown
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线
```

第一批实现回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
```

发布闸门：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 晋升 / 删除条件

- 第二批晋升条件：multi-projection 有 native expected、cad-core focused tests、capability tests 和文档矩阵；S5 只发布已验证分支。Offset 已在 S3 完成 expected-backed 实现，但 full ProjectOnSurface 仍等待 S4/S5。
- 删除条件：若某一高级分支在当前 FreeCAD / OCCT expected 基线不可稳定采集，则该分支保持 deferred / blocked；不得在 executor 中按 fixture 名、输出顺序或几何形态补猜。
