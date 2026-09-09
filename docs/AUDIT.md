# FrameCompare v1.3 static audit

审计范围：当前源码包的静态检查与工程一致性检查。当前环境没有 Windows/MSVC + ReShade 游戏运行环境，因此不宣称已经生成并实机验证 `.addon64`。

## 已检查

- CMake/Windows VERSIONINFO 均为 1.3.0。
- 目标仅 Windows x64，要求 ReShade Add-on API 20+。
- v1.2 `application_present` 主捕获模式已从实现中移除。
- 旧 GDI 文字纹理路径及 `gdi32` 链接依赖已移除。
- 标签/HUD 使用 ReShade ImGui overlay。
- 快捷键 UI 为直接按键捕获，默认 Full Before/After 不占用 Home/End。
- 精确截图对与实时处理链均存在独立路径。
- Before/After 尺寸与 typed pixel format 不兼容时拒绝合成。
- Shader semantic：`FRAMECOMPARE_BEFORE` / `FRAMECOMPARE_AFTER` / `FRAMECOMPARE_PARAMS` 与 C++ 绑定一致。
- `FrameCompareComposite` 由 Add-on 显式调用，并在正常 ReShade preset 中保持 technique state 关闭，避免普通效果链重复执行。
- 自定义 HUD 支持真实 ReShade Effects State，不依赖复制 END 等外部快捷键；同时支持快捷键切换、按住与单次提示。
- GitHub Actions 在 Windows 构建前执行 `tools/static_audit.py`。

## 尚未由当前环境验证

- MSVC 实际编译结果。
- ReShade 对具体游戏/DX11/DX12 的实机加载。
- DLSS5/RenoDX 不同版本下的 NGX Feature 18 Hook 行为。
- 各游戏 HDR/特殊 BackBuffer 格式的表现。
- ShaderToggler 与具体 HUD preset 的组合实测。

## 依赖可重复性

MinHook 固定为 v1.3.4。ReShade 默认跟踪 `main`，因此未来上游 API 变化可能影响构建；需要完全可重复构建时，应通过 `FRAMECOMPARE_RESHADE_ROOT` 指向经过验证的 ReShade 源码 checkout。

运行：

```text
python tools/static_audit.py
```

该脚本只做静态一致性检查，不替代编译器和游戏运行测试。
