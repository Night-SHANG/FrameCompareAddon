# Changelog

## v1.2.0

- 插件 UI 默认改为中文。
- 新增 `中文 / English` 实时切换，保存到 `FrameCompare.ini`。
- UI 重构为可折叠分组：快速教程、基础设置、分割线与动画、文字标签、快捷键、DLSS5/NGX 高级设置、诊断与日志、配置文件。
- 插件内部加入基础使用教程，不再要求先阅读外部文档才能知道如何使用。
- 增加明显的 `FrameCompare.fx` 缺失红色提示。
- 增加“最近状态 / 最近警告 / 最近错误”诊断显示。
- 增加关键日志与可选 `VerboseLogging` 详细日志。
- 成功找到合成着色器、首次形成有效 Before/After Pair 时写入明确成功日志，便于判断插件是否真的工作。
- 增加 Windows VERSIONINFO 1.2.0.0，避免 ReShade 日志继续显示 v0.0.0.0。
- NGX 新错误会写入 `ReShade.log`；详细日志可记录捕获来源变化。
- 修复 `FrameCompare.fx` 缺失时每帧重复写 WARN 导致日志刷屏的问题。
- GitHub Actions/本地 Build Package 改为正确的即拷即用结构：`reshade-shaders/Shaders/FrameCompare.fx`。
- 新增 `docs/LOG_CN.md`。
- 保留 v1.1 的文字位置调试预览、GPU 文字合成、Freeze、Sweep、SplitScreenCR 模式、NGX Feature 18 捕获等功能。

## v1.1.0

- Added label placement preview that can keep both labels visible before a valid Before/After pair exists.
- Preview uses the same GPU glyph-texture compositor as normal recording labels.

## v1.0.0

- Initial complete source release.
