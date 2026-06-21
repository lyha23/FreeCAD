# C5-M8-S1a helper oracle 恢复

状态：`blocked_runtime_verify_after_source_fix`

## 当前结论

- 已修复 `src/Mod/Part/App/AppPartPy.cpp` 中 `Part.makeFilledFace(...)` / `Part.makeFilledSurface(...)` kwargs parser：`surface` 现在写入 `params.surface`，`supports` / `orders` 现在会从 kwargs 解析，`parseSequence()` 现在把 tuple 第二项传给 callback。
- 已用既有 Ninja compile command 加 `FastSignals` include 手动编译 `src/Mod/Part/App/CMakeFiles/Part.dir/AppPartPy.cpp.o`，编译通过。
- 尚未完成 runtime helper oracle 验证：`ninja -C build/debug Mod/Part/Part.so` 会先触发 CMake reconfigure，并因当前 Python 环境缺少 `pivy` 失败。
- 手动 relink `Mod/Part/Part.so` 被本机 Homebrew / ABI 漂移阻塞：旧 build 命令引用 OCCT 7.9.1、Boost 1.89、fmt 11 等库；映射到当前库后仍出现 `fmt::v11`、`App::Document::addObject(std::basic_string_view...)`、`Base::pyThrowWrapped*` 等未定义符号，说明不能只重链 Part.so 验证，需要恢复一致的 FreeCAD build 环境后重建。
- 因此 S1 仍不能解封：不得用安装版 `FreeCADCmd` 或 direct wrapper probe 证明 patched helper oracle。

## 目标

修复并验证 `Part.makeFilledFace(...)` / `Part.makeFilledSurface(...)` 的 native helper kwargs 入口，让 S1 能重新采集 `Surface` / `Supports` / `Orders` expected，而不是依赖 direct `Part.BRepOffsetAPI.MakeFilling` wrapper 绕路。

## 必读

- `src/Mod/Part/App/AppPartPy.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()`
- `cad-core/tools/probe_filling_s1_contract.py`
- 本包 S1 step 和局部矩阵

## 产物

- 修复 `AppPartPy.cpp` 中 `surface` / `supports` / `orders` parser 传参与 `parseSequence()` value 传递。
- 用本源码构建出的 `FreeCADCmd` 运行 `probe_filling_s1_contract.py surface`、`supports`、`orders`、`support_order`。
- 若本机构建环境阻塞，必须记录构建阻塞与已完成的源码修复，不得把安装版 `FreeCADCmd` 的失败当成修复后行为。
- 恢复 S1 的进入条件：只有 helper probe 可返回，才能继续采集 S1 expected。

## 非目标

- 不实现 cad-core Filling DTO。
- 不采集或提交 S1 expected。
- 不把 direct wrapper probe 当成 helper oracle。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- src/Mod/Part/App/AppPartPy.cpp docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线 cad-core/tools/probe_filling_s1_contract.py
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线/工作步骤细分 --format markdown

# 需要使用当前源码构建出的 FreeCADCmd，而不是安装版 App：
/opt/homebrew/bin/timeout 30 build/debug/bin/FreeCADCmd cad-core/tools/probe_filling_s1_contract.py surface
/opt/homebrew/bin/timeout 30 build/debug/bin/FreeCADCmd cad-core/tools/probe_filling_s1_contract.py supports
/opt/homebrew/bin/timeout 30 build/debug/bin/FreeCADCmd cad-core/tools/probe_filling_s1_contract.py orders
/opt/homebrew/bin/timeout 30 build/debug/bin/FreeCADCmd cad-core/tools/probe_filling_s1_contract.py support_order
```
