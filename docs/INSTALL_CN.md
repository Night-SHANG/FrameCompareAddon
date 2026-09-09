# FrameCompare v1.1 安装与使用

## 1. 编译

最简单的方法是把整个源码目录上传到 GitHub：

1. 创建一个仓库并上传全部文件，包括 `.github/workflows/build.yml`。
2. 打开 GitHub 仓库的 **Actions**。
3. 选择 **Build FrameCompare**。
4. 点击 **Run workflow**。
5. 构建成功后下载 Artifact：`FrameCompare-Windows-x64`。

本地 Windows + VS2022 也可以直接运行 `build_vs2022.bat`。

## 2. 安装到游戏

需要 **ReShade 6.8 Add-on 版或兼容 API 20+ 的 Add-on 版本**。

把：

```text
00-FrameCompare.addon64
```

放到游戏的 ReShade Add-on 搜索目录。常见情况是游戏 EXE/ReShade DLL 所在目录；如果你已经在用 ShaderToggler、RenoDX 等 `.addon64`，与它们放在同一个实际加载目录即可。

把：

```text
FrameCompare.fx
```

放到当前 ReShade 的 Shader 搜索目录，例如：

```text
reshade-shaders\Shaders\
```

第一次可以把：

```text
FrameCompare.ini.example
```

复制为：

```text
FrameCompare.ini
```

并与 `00-FrameCompare.addon64` 放在一起。插件也可以自己生成/保存 `FrameCompare.ini`。

## 3. 默认快捷键

- `F9`：开启/关闭对比。
- `F10`：冻结/解除冻结 Before + After 两张画面。不是暂停游戏。
- `F11`：开启/关闭自动 Sweep。
- `← / →`：短按按 Step 移动；长按超过 HoldDelay 后连续移动。
- `Home`：完整 Before。
- `End`：完整 After。

## 4. 分割线拖动

开启 ReShade Overlay 后，可以在屏幕上的分割线附近按住鼠标左键拖动。为了避免正常游戏操作时抢鼠标，屏幕拖动默认只在 ReShade Overlay 打开时工作。

## 5. DLSS5 场景如何选择 CaptureMode

### A. RenoDX / 原生 Add-on 方式启动 DLSS5

优先：

```ini
[General]
CaptureMode=0
```

Auto 会优先使用新鲜的 NGX Feature 18 pre-Evaluate `Color`，没有可用原生捕获时退回 Before ReShade FX。

确认诊断页能稳定显示 `NGX Feature 18 ... (pre-Evaluate)` 后，如果你希望绝不使用退回画面，可以改：

```ini
CaptureMode=1
```

### B. DLSS5 Feeder / ReShade 效果链方式

推荐：

```ini
CaptureMode=2
```

这样 Before 就取 ReShade 效果链开始前的目标纹理，适合“Feeder + 后续 preset”的比较。

### C. 不确定 / 普通游戏

先用 Auto。`CaptureMode=3` 的 Application Present 只作为兼容/诊断模式，它的 Add-on 调用顺序不能保证一定早于所有插件。

## 6. OFF / ON 文字

在 Add-ons → FrameCompare 中可以修改：

- Before label / After label
- Windows font
- Before X / Y
- After X / Y
- Font size
- Opacity
- Outline

调文字位置时可以临时开启：

`Label placement preview (always show both)`

开启后两个标签会持续同时显示，X/Y、字号、透明度、描边可以实时调整；即使 Before/After 还没有成功捕获，也会在当前最终画面上显示两个标签。正式录制前关闭该预览，恢复正常的分割区域裁切。

INI 对应：

```ini
[Labels]
Preview=0
BeforeText=原版
AfterText=DLSS 5 ON
FontName=Microsoft YaHei UI
BeforeX=0.04
BeforeY=0.06
AfterX=0.82
AfterY=0.06
```

中文需要选择包含中文字形的 Windows 字体，例如 `Microsoft YaHei UI`。

## 7. SplitScreenCR 风格

```ini
[General]
DisplayMode=1
```

这个模式保留旧 `SplitScreenCR` 使用时最明显的“左右画面中心区域重新映射”观感，而不是普通 Splitscreen 的简单同 UV 左右裁切。运行时不需要旧 shader，也不需要 `sMask.png`。

## 8. ShaderToggler / HUD

FrameCompare 不会重新执行游戏 draw call，只复制已经存在的图像资源。所以由 ShaderToggler 阻止掉的 HUD shader/draw 不会被 FrameCompare主动重新画回来。

但 Before 捕获点本身决定能看到什么：例如 NGX Color 很可能本来就是 HUD 之前的场景纹理；Before ReShade FX 则是进入 ReShade 时实际已有的画面。

## 9. INI 跨游戏复用

调好后直接复制：

```text
FrameCompare.ini
```

到另一个游戏的 FrameCompare Add-on 目录，即可复用快捷键、分割线、速度、标签和位置参数。

CaptureMode 可能需要按游戏/DLSS5接入方式单独调整，这是最主要的游戏相关参数。

## 10. D3D12 NGX 出现异常时

默认：

```ini
[NGX]
D3D12ColorState=0x40
AllowUnsafeCrossDevice=0
```

`0x40` 对应 `D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE`。

如果游戏/Bridge 的 NGX Color 在调用时处于其他状态，错误状态转换可能导致 GPU validation/花屏/崩溃。只有明确知道真实状态时才改 `D3D12ColorState`。

`AllowUnsafeCrossDevice=1` 不建议常开。私有 D3D12 device/queue 与 ReShade device 之间只有共享 handle 并不能提供完整 GPU 同步，因此该功能只用于已知 Bridge 的诊断尝试。

## 11. 诊断时重点看

Add-ons → FrameCompare：

- `Current source`
- `Pair: ready / not ready`
- Before / After 分辨率
- `NGX hook module`
- NGX latest API / resolution / generation
- Feature18 creates / evaluates / captures / failures
- D3D11 / D3D12 Hook 状态

如果 GitHub 首次编译失败，保留完整 Actions 编译日志；如果游戏内失败，提供 ReShade.log 与 FrameCompare 诊断区截图即可定位。
