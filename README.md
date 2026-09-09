# FrameCompare v1.3

FrameCompare 是 ReShade Add-on 级 Before/After 画质对比工具，目标是直接录制“原版 → 完整增强”的对比视频，不依赖 PR 后期拼接。

## v1.3 核心变化

- 默认工作流改为“精确截图对”：分别捕获真正 OFF 状态的 Before 与完整增强 ON 状态的 After。
- 保留“实时处理链”作为高级模式，用于动态画面；Before 是否等于绝对原版取决于游戏、DLSS/RenoDX 与 ReShade 的注入位置。
- 分屏合成重写为两种显示：普通同坐标擦除、SplitScreenCR 风格中心偏移分屏。
- 分割线可移动、长按连续移动、自动扫屏、往返、隐藏、调宽度/透明度，并可在 ReShade 面板打开时拖动。
- 标签改为 ReShade ImGui OSD 直接绘制，统一字号/透明度/描边/安全边距，解决旧版 ON/OFF 上下裁切。
- 快捷键改为点击后直接按键/组合键捕获，不再输入 VK 数字。
- 新增多条自定义 HUD：快捷键切换状态、读取 ReShade 实际 Effects State、按住快捷键、快捷键单次提示。
- FrameCompare.ini 保存全部布局、快捷键与 HUD 条目，可跨游戏复制。

## 默认快捷键

- F7：捕获 Before（精确截图对）
- F8：捕获 After（精确截图对）
- F9：对比开/关
- F10：冻结/解冻（实时模式）
- F11：自动扫屏
- Left / Right：移动分割线；短按按步长移动，长按连续移动
- Ctrl+Left / Ctrl+Right：完整 Before / 完整 After

默认不占用 Home/End，避免和 ReShade 常用按键冲突。所有快捷键都可在插件 UI 中直接重新绑定。

## 最推荐的录制方式

1. 选择“精确截图对”。
2. 把 DLSS5、RenoDX、ReShade 预设等切到你认为的真正 OFF 状态，按“捕获 Before”。
3. 打开完整增强状态，按“捕获 After”。
4. 开启对比，用分割线、长按移动或自动扫屏直接录像。

捕获请求会在 ReShade 设置面板关闭后抓下一帧，并暂时跳过 FrameCompare 自身的合成和 OSD，避免把插件界面录进截图对。

## 安装

实际运行需要 Windows x64、支持 Add-on 的 ReShade，以及编译得到的 `00-FrameCompare.addon64`。将文件放成：

```text
<游戏目录>/00-FrameCompare.addon64
<游戏目录>/FrameCompare.ini               （首次可不放）
<游戏目录>/reshade-shaders/Shaders/FrameCompare.fx
```

更具体的安装、录制、HUD 配置见 `docs/INSTALL_CN.md` 与 `docs/FUNCTIONS_CN.md`。

## 构建

需要 Visual Studio 2022 x64 C++ 工具链、CMake 3.24+ 与 Git：

```bat
build_vs2022.bat
```

或者：

```bat
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

CMake 默认获取 ReShade `main`（含匹配的 ImGui 子模块）与 MinHook v1.3.4。也可以通过 `FRAMECOMPARE_RESHADE_ROOT` / `FRAMECOMPARE_MINHOOK_ROOT` 指向本地源码。

源码静态检查：

```bat
python tools\static_audit.py
```

本源码包只声明已经完成的静态审计；没有在当前环境伪装成已通过 Windows/MSVC 编译或游戏实机验证。见 `docs/AUDIT.md`。
