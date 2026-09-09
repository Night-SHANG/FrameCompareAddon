# Changelog

## v1.3.0

- 将主工作流重构为“精确截图对 / 实时处理链”，不再把四个底层捕获点当成主功能暴露。
- 精确截图对从实际可见 BackBuffer 分别捕获 Before/After；捕获时跳过 FrameCompare 自身合成与 OSD。
- 新增 Before/After 尺寸和 typed pixel format 兼容性检查，不兼容时拒绝合成并给出诊断。
- `FrameCompare.fx` 重写为普通擦除 + SplitScreenCR 风格中心偏移两种显示方式；0%/100% 端点为完整单侧画面。
- 分割线支持位置、宽度、透明度、隐藏、短按步进、长按连续移动、自动扫屏、Ping-Pong 与鼠标拖动。
- 文字从 GDI 位图纹理改为 ReShade ImGui OSD，统一字号、透明度、描边和安全边距，修复 ON/OFF 上下裁切。
- 快捷键改为点击后直接按键/组合键捕获；UI 快捷键提示显示当前实际绑定，不再硬编码默认值。
- 默认 Full Before/Full After 改为 Ctrl+Left/Ctrl+Right，避免占用常见 Home/End。
- 新增最多 32 个自定义 HUD 条目；支持快捷键切换、ReShade 实际 Effects State、按住快捷键、快捷键单次提示四种来源。
- 新建 HUD 条目默认显示 1 秒，可设置 0 秒常驻。
- 配置结构升级并保留 v1.2 INI 的迁移兜底。
- CMake/版本资源升级到 1.3.0，移除 GDI32 依赖。
- 新增 `tools/static_audit.py`，GitHub Actions 构建前执行静态一致性检查。

## v1.2.0

- 重构 Add-on UI、配置与诊断。
- 增加 NGX Feature 18 实验捕获路径及通用 ReShade 捕获路径。
- 增加分割线、冻结、标签等早期功能。
- 增加 Windows VERSIONINFO 1.2.0.0。
