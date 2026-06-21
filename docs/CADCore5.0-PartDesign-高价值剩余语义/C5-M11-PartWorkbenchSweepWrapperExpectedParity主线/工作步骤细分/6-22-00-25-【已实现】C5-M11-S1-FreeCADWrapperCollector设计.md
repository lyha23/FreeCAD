# 【已实现】C5-M11-S1 FreeCAD wrapper collector 设计

状态：`done_C5M11-S1_wrapper_collector_design`

## live baseline

```text
pwd
/Users/li/Chili3DProject/FreeCAD

git rev-parse --short HEAD
ea3e47af6f

git log -1 --oneline
ea3e47af6f docs: 冻结C5-M11 Sweep wrapper expected gap

git -c core.quotepath=false status --short -uall
<clean>
```

## S1 结论

- `cad-core/tools/collect_freecad_expected.py` 已新增 request-local PipeShell wrapper collector 分支：识别 C5-M10 advanced `Part::Sweep` DTO，不创建 native `Part::Sweep` 目标对象，而是只创建 profile/spine/auxiliary/support/location 源 shape 后调用 wrapper。
- FreeCAD source canonical 名称来自 `BRepOffsetAPI_MakePipeShell.pyi` / `BRepOffsetAPI_MakePipeShellPyImp.cpp`；当前本机 FreeCADCmd runtime 实际暴露路径是 `Part.BRepOffsetAPI.MakePipeShell`，expected schema 仍固定 `object_fields.helper=Part.BRepOffsetAPI_MakePipeShell`，并额外记录 `object_fields.runtime_helper`。
- Collector 固定输出 `object_fields.advanced` 与 `object_fields.builder_status`：`advanced` 记录 auxiliary/binormal/support/sections/tolerance metadata，`builder_status` 记录 `transition_mode`、`is_ready`、`status_before_build`、`build_ok`、`status_after_build`、`shape_access_ok` 和具体 set-mode 标志。
- invalid support/mode/location/tolerance payload 不交给 FreeCAD wrapper 消费；它们进入 `diagnostic_split`，仍由 cad-core focused diagnostics 验收。
- 本轮不替换 `cad-core/fixtures/c5m10/expected/*.freecad.json`，S2 决定哪些 collectable payload 可替换，以及哪些 FreeCADCmd wrapper blocker 必须保留。

## Wrapper 调用路径

1. 从 fixture 源对象构造 shape：`Spine` 解析 `EdgeN` 后转 `Part.Wire`；`Sections` 取 profile wire 或 vertex；`AuxiliarySpine` 解析 wire；`SpineSupport` 解析 support shape；`SectionOptions[].Location` 解析 vertex。
2. 创建 builder：优先 `Part.BRepOffsetAPI_MakePipeShell(spine_wire)`，当前 runtime fallback 为 `Part.BRepOffsetAPI.MakePipeShell(spine_wire)`。
3. 按 DTO 字段调用 wrapper：`setTransitionMode()`、可选 `setTolerance(tol3d,boundTol,tolAngular)`、单一 mode 分支 `setAuxiliarySpine(...)` / `setSpineSupport(...)` / `setBiNormalMode(...)`，无 advanced mode 时调用 `setFrenetMode()`。
4. 按 Sections 顺序调用 `add(Profile, Location, WithContact, WithCorrection)` 或无 Location overload。
5. 记录 `isReady()` / `getStatus()`，然后 `build()`，可选 `makeSolid()`，最后 `shape()` 生成 `shape_summary`。

## Probe 证据

Probe 输出只写入 `/tmp/c5m11-*-wrapper-probe.freecad.json`，没有替换 expected。FreeCADCmd 输出的 open sketch face warning 来自 spine/auxiliary open wire，不作为失败。

| fixture | probe result |
| --- | --- |
| `part-sweep-auxiliary-spine-contract` | collectable：`advanced.mode=Auxiliary`，`builder_status.build_ok=true` |
| `part-sweep-binormal-contract` | collectable：`advanced.mode=Binormal`，`builder_status.build_ok=true` |
| `part-sweep-tolerance-contract` | collectable valid `ToleranceSweep`；invalid tolerance / legacy scalar 进入 `diagnostic_split` |
| `part-sweep-support-mode-diagnostics` | 当前 fixture 全是 invalid/diagnostic objects；collector 输出 `diagnostic_only`，valid `setSpineSupport` 路径已实现但本 fixture 没有可替换 geometry expected |
| `part-sweep-located-profile-contract` | wrapper 调到 valid `add(Profile, Location, WithContact, WithCorrection)`，`build()` 返回 `OCCError: NCollection_Array1::Value`，S2 必须保留或记录明确 FreeCADCmd blocker |
| `part-sweep-advanced-combined-contract` | `CombinedSweep` wrapper build 同样返回 `OCCError: NCollection_Array1::Value`；invalid siblings 进入 `diagnostic_split` |

## S2 边界

- 可直接进入 expected batch 的候选：auxiliary、binormal、tolerance 的 valid payload。
- support 当前没有 valid representative expected；S2 不能把 diagnostic-only support fixture 硬替换成 geometry expected，除非先补有 FreeCAD source 依据的 valid support representative。
- located-profile 和 combined 当前暴露 FreeCADCmd wrapper build blocker；S2 必须记录 blocker、缩窄 known_gap，或在有 FreeCAD 依据时调整 fixture / 调用路径后再采集。
- native `Part::Sweep` direct property 边界仍是 `Sections`、`Spine`、`Solid`、`Frenet`、`Linearize`、`Transition`；本 helper 不发布 upstream native advanced properties。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 -m py_compile cad-core/tools/collect_freecad_expected.py
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M11-PartWorkbenchSweepWrapperExpectedParity主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 cad-core/tools/collect_freecad_expected.py
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M11-PartWorkbenchSweepWrapperExpectedParity主线/工作步骤细分 --format markdown
```
