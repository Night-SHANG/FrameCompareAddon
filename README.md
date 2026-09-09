# FrameCompare v1.2

FrameCompare 是一个 ReShade Add-on 级别的 Before / After 对比工具，目标是直接录制“原版画面 → 最终增强画面”的分割对比，不再依赖后期 PR 遮罩合成。

## 主要功能

- 插件级 Before / After 双画面捕获。
- 支持 NGX Feature 18 / DLSS5 前 Color 捕获。
- 支持 `Before ReShade FX`，适合 DLSS5 Feeder / ReShade 效果链注入。
- 支持 Application Present 通用捕获模式。
- F9 开关对比。
- F10 冻结 / 解除冻结 Before + After。
- F11 自动扫屏。
- 左右方向键短按步进、长按连续移动。
- Home / End 完整显示 Before / After。
- 鼠标拖动分割线。
- 普通 Wipe 与 SplitScreenCR 风格中央重映射。
- OFF / ON 自定义文字、独立 X/Y、字号、透明度、描边。
- 文字位置调试预览：即使 Before/After 未就绪，也能持续显示两个标签用于排版。
- ShaderToggler 兼容：不重放被 ST 阻止的 HUD draw call。
- 独立 `FrameCompare.ini`，可跨游戏复用。
- 默认中文 UI，可实时切换 English。
- 折叠式 UI 与插件内快速教程。
- ReShade.log 关键日志、可选详细日志、UI 内最近状态/警告/错误。

## GitHub Actions 自动编译

把源码提交到 `main` 或 `master` 分支后，GitHub Actions 会自动执行 `Build FrameCompare`。

成功后下载：

```text
FrameCompare-Windows-x64
```

v1.2 的 Artifact 已按可直接合并到游戏目录的结构整理：

```text
00-FrameCompare.addon64
FrameCompare.ini.example
reshade-shaders/
  Shaders/
    FrameCompare.fx
INSTALL_CN.md
FUNCTIONS_CN.md
LOG_CN.md
...
```

不要只复制 `.addon64`。`FrameCompare.fx` 也是运行所必需的组件。

## 安装

最简单的方法：把 GitHub Artifact 解压后，直接将里面的内容合并复制到游戏的 ReShade 根目录。

要求：

- ReShade 6.8 Add-on 版或兼容 API 20+。
- 64-bit 游戏 / ReShade。

详细中文安装与使用：`docs/INSTALL_CN.md`。

## 默认快捷键

| 功能 | 默认键 |
|---|---|
| 开关对比 | F9 |
| 冻结 / 解除冻结 | F10 |
| 自动扫屏 | F11 |
| 分割线向左 / 向右 | ← / → |
| 完整 Before | Home |
| 完整 After | End |

## 捕获模式

1. `Auto`：优先使用 NGX Feature 18，失败时回退到 `Before ReShade FX`。RenoDX / 原生 DLSS5 建议先用这个。
2. `NGX Feature 18 strict`：只接受 DLSS5 Neural Rendering Evaluate 前的 Color 捕获，不回退。
3. `Before ReShade FX`：推荐 DLSS5 Feeder / 效果链内注入。
4. `Application Present`：通用、依赖 Add-on/Present 顺序。

## “Before”的含义

FrameCompare 的目标不是“关闭全部插件然后重新渲染一帧”，而是在合适的处理阶段保存一份 Before，再让 DLSS5 / RenoDX / ReShade 正常继续。因此它不会为了对比去关闭 DLSS5 temporal history，也不会主动恢复 ShaderToggler 已阻止的 HUD。

NGX Feature 18 的 Color 是 Neural Rendering 的真实输入，但个别游戏/Bridge 中它可能仍是内部中间资源，不保证数学上完全等价于“彻底卸载 DLSS5 后的最终 Present”。插件诊断会显示实际捕获来源。

## UI

v1.2 默认中文，并提供：

- `中文 / English` 切换。
- 快速教程。
- 基础设置。
- 分割线与动画。
- 文字标签。
- 快捷键。
- DLSS5 / NGX 高级设置。
- 诊断与日志。
- 配置文件。

除快速教程外，其余大部分区域默认折叠，避免 Add-ons 页面过长。

## 日志

关键问题会写入 `ReShade.log`，搜索：

```text
[FrameCompare]
```

正常时日志会明确记录“初始化完成”“合成着色器已就绪”，首次取得有效 Before/After 后还会记录“画面对已就绪”，这样不需要靠猜测判断插件是否工作。

详细说明：`docs/LOG_CN.md`。

用户提供的 v1.1 测试日志已确认 Add-on 本体成功加载，但 `FrameCompare.fx / FrameCompareComposite` 没有被找到。v1.2 一方面修正 Artifact 安装目录，另一方面修复该警告每帧刷屏的问题，并在 UI 顶部直接给出中文红色错误提示。

## 架构

```text
Game rendering / ShaderToggler blocking
        │
        ├─ Native path: NGX Feature 18 Color ──> Before
        │
        └─ Generic path: ReShade begin effects ─> Before fallback
                                             │
                                      DLSS/RenoDX/ReShade
                                             │
                                  ReShade finish effects
                                             │
                                          After
                                             │
                      FrameCompareComposite (explicit technique)
                                             │
                                          Present
```

`FrameCompare.fx` 不作为普通 preset technique 长期开启；Add-on 绑定 Before/After/参数/标签纹理，并在正确阶段显式调用 `FrameCompareComposite`。

## 构建

Windows + Visual Studio 2022：

```bat
build_vs2022.bat
```

或：

```bat
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

CMake 会自动获取 ReShade headers 与 MinHook，也可通过 cache path 指向已有源码。

## 文档

- `docs/INSTALL_CN.md`：安装和基础使用。
- `docs/FUNCTIONS_CN.md`：完整功能规格。
- `docs/LOG_CN.md`：日志与故障排查。
- `docs/AUDIT.md`：源码审查与已知边界。
- `THIRD_PARTY.md`：第三方依赖与参考说明。
