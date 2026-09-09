# FrameCompare 插件完整功能规格

版本：v1.2 目标规格  
定位：ReShade Add-on 级 Before / After 对比、录制与展示工具

## 1. 项目目标

FrameCompare 的目标不是普通 `.fx` 分屏着色器，而是一个能够跨越 ReShade / DLSS5 / RenoDX 等增强链路的插件级对比工具。

最终要实现的核心视觉结果是：

```text
[ 原版 / Before / OFF ] | [ 最终增强 / After / ON ]
```

其中：

- **Before**：尽可能取得 DLSS5、RenoDX 画质增强、ReShade Preset 等比较对象介入之前的画面。
- **After**：取得 DLSS5 / RenoDX / 当前 ReShade Preset 等增强链路完成后的最终效果画面。
- 对比不通过“临时关闭 DLSS5 再重新开启”的方式实现，避免破坏 DLSS/神经渲染的 temporal history、资源状态或稳定性。
- 对于 ShaderToggler 一类辅助插件，FrameCompare 应尊重其当前状态，不主动恢复已经被阻止的 HUD draw call。

---

## 2. 插件级 Before / After 捕获

### 2.1 Before 捕获

插件需要提供多种 Before 捕获策略，以适配不同 DLSS5 接入方式。

#### Auto

自动模式优先选择真正的 NGX Feature 18 pre-Evaluate 输入；当前帧没有可用原生捕获时，退回 ReShade 效果链开始前的画面。

用途：

- RenoDX / 原生 DLSS5 Add-on。
- 不确定当前游戏 DLSS5 接入方式时的默认选择。

#### NGX Feature 18 Strict

针对 D3D11 / D3D12 的原生 NGX 路径，在目标 Feature 执行 `EvaluateFeature` 之前捕获其 `Color` 输入资源。

要求：

- 必须在原始 Evaluate 之前复制资源。
- 不允许为了获得 Before 而关闭 DLSS5。
- 没有新鲜、可确认的 Feature 18 捕获时不得用其他阶段的旧帧冒充。
- 对未知 NGX handle 不应随意猜测为 Feature 18。

用途：

- RenoDX / 原生 DLSS5 Neural Rendering 路径。

#### Before ReShade FX

在 ReShade 效果链正式执行前保存当前 effect target。

用途：

- DLSS5 Feeder 作为 ReShade 效果链一部分运行时。
- 普通 ReShade Preset 的前后比较。

#### Application Present

提供一个通用、依赖 Add-on 调用顺序的兼容模式。

用途：

- 非标准游戏或诊断。
- 不作为“必然是 DLSS5 前画面”的保证模式。

### 2.2 After 捕获

After 应在 ReShade 当前效果链执行完成后捕获，使其包含：

- 已经在此前完成的 DLSS5 / RenoDX 效果。
- 当前启用的 ReShade Preset / `.fx` 效果。
- 该捕获阶段之前已经应用的其他增强。

随后由 FrameCompare 自己在更晚阶段执行最终对比合成。

### 2.3 Before 的定义边界

FrameCompare 中的 “Before” 应理解为：

> 当前所选捕获点的真实前置图像，而不是通过卸载所有插件并重新渲染一遍游戏得到的第二条独立渲染管线。

对于 NGX Feature 18，捕获到的 `Color` 可能是内部渲染分辨率或中间资源。它是真实 pre-Evaluate 输入，但不保证在所有游戏中都与“完全卸载 DLSS5 后最终 Present 的画面”数学等价。

---

## 3. ShaderToggler / HUD 兼容

这是插件的硬性兼容要求。

FrameCompare：

- 不重新执行应用程序 draw call。
- 不重新播放被 ShaderToggler 阻止的 shader。
- 不为了获得 Before 而主动恢复 HUD。
- 只复制当前渲染链中已经存在的图像资源。

因此，ShaderToggler 当前已经关闭的 HUD 不应被 FrameCompare 主动重新显示。

需要注意：不同捕获点天然包含的内容不同。例如 NGX 的 Color 输入本身就可能处于 HUD 绘制之前，而 Before ReShade FX 则包含进入 ReShade 时已经存在的内容。

---

## 4. 对比显示模式

### 4.1 Normal Wipe

标准左右分屏：

- 分割线一侧显示 Before。
- 另一侧显示 After。
- 两边使用相同 UV，对同一个画面位置进行直接比较。

### 4.2 SplitScreenCR 风格

参考旧 `SplitScreenCR` 的视觉习惯：

- 两侧画面会进行中心区域重新映射。
- 在约 50/50 分割时形成旧 SplitScreenCR 特有的左右中心偏移观感。
- 当分割位置达到 0% / 100% 时恢复完整单帧画面。
- 运行时不依赖旧 `SplitScreenCR.fx`。
- 运行时不依赖 `sMask.png`。

### 4.3 Before / After 左右反转

允许设置：

- Before 在左，After 在右。
- After 在左，Before 在右。

标签、分割逻辑和快捷操作必须同步适配。

---

## 5. 分割线系统

### 5.1 分割位置

- 范围：0% ～ 100%。
- 0% / 100% 可用于完整显示某一侧。
- 默认 50%。

### 5.2 分割线外观

可调参数：

- 是否显示分割线。
- 分割线宽度。
- 分割线透明度。

### 5.3 鼠标拖动

ReShade Overlay 打开时：

- 可以直接用鼠标抓住分割线并左右拖动。
- 可设置抓取判定范围（像素）。
- 正常游戏状态下不应抢占鼠标输入。

---

## 6. 分割线快捷键控制

### 6.1 左右移动

分别提供：

- 向左移动快捷键。
- 向右移动快捷键。

默认：

- `Left Arrow`：向左。
- `Right Arrow`：向右。

### 6.2 短按

短按时按固定 Step 移动。

可调：

- `Divider.Step`

用途：精确微调分割位置。

### 6.3 长按连续移动

按住左/右键超过 Hold Delay 后：

- 分割线持续平滑移动。
- 松开立即停止。

可调：

- 长按触发延迟。
- 连续移动速度（屏幕比例 / 秒）。

这样可以直接录制“原版逐渐切换到最终效果”的动画，而不需要在 Premiere 等软件中另外做遮罩动画。

### 6.4 完整 Before / After

提供一键到边缘快捷键：

- 默认 `Home`：完整 Before。
- 默认 `End`：完整 After。

---

## 7. 自动 Sweep 动画

插件应能够自动驱动分割线移动，用于录制演示。

### 7.1 开关

- 默认 `F11`：开始 / 停止自动 Sweep。

### 7.2 Sweep 速度

可调自动移动速度。

### 7.3 Sweep 方向

支持：

- 左 → 右。
- 右 → 左。

### 7.4 起始行为

可设置启动 Sweep 时：

- 自动从完整 Before 开始。
- 或从当前分割位置继续。

### 7.5 到边缘行为

支持：

- 到边缘停止。
- Ping-Pong 往返循环。

---

## 8. Freeze 静止画面

默认快捷键：`F10`。

Freeze 的定义不是暂停游戏，而是：

1. 保存一对有效的 Before + After。
2. 锁定这两张捕获纹理。
3. 后续游戏仍可以继续运行。
4. FrameCompare 继续显示被冻结的两帧。
5. 分割线、手动移动、自动 Sweep 仍然可以在冻结画面上运行。

如果按下 Freeze 时当前还没有一对有效的 Before / After，应进入 Armed 状态，在下一对有效画面出现时锁定。

用途：

- 固定完全相同的画面进行画质比较。
- 在冻结帧上录制左右扫屏动画。
- 避免动态场景中的人物、镜头、粒子变化干扰对比。

---

## 9. 对比总开关

默认快捷键：`F9`。

功能：

- 开启 FrameCompare 对比显示。
- 关闭后恢复正常最终游戏画面。
- 不应通过关闭 DLSS5 / RenoDX / ReShade preset 本身来实现。

---

## 10. ON / OFF 标签系统

### 10.1 默认文字

- Before：`OFF`
- After：`ON`

### 10.2 自定义文字

两边分别独立设置，例如：

```text
原版
DLSS 5 ON
```

或：

```text
Vanilla
Enhanced
```

### 10.3 独立位置

Before / After 标签分别提供：

- X 坐标。
- Y 坐标。

坐标使用屏幕归一化比例，便于跨分辨率复用。

### 10.4 字体

支持指定 Windows 字体名称。

例如中文：

```text
Microsoft YaHei UI
```

### 10.5 字体参数

可调：

- 字号。
- 透明度。
- 描边宽度。

### 10.6 标签区域归属

标签必须只显示在它所描述的区域中。

例如分割线移动时：

- `OFF` 不应漂到 ON 区域。
- `ON` 不应漂到 OFF 区域。

标签应跟随 Before / After 区域裁切。

### 10.7 标签总开关

可以完全关闭正式对比中的标签显示。

### 10.8 标签位置调试预览

提供独立的 `Label placement preview` 调试模式，用于正式录制前调文字布局。

要求：

- 开启后 Before / After 两个标签必须同时持续显示。
- 调整 X/Y、字号、透明度、描边时，游戏画面应实时更新。
- 调试预览不依赖已经取得有效 Before / After pair。
- 当没有有效 pair 时，以当前 ReShade 效果完成后的画面作为中性背景，只叠加两个标签。
- 当已有有效 pair 时，可以继续显示当前对比画面，但两个标签暂时忽略分割区域裁切，确保两者都能看见。
- 调试预览应独立于正式 `Show labels` 开关；即使正式标签关闭，也允许用 Preview 检查布局。
- 关闭 Preview 后，恢复正常录制逻辑：标签重新只显示在各自 Before / After 区域。
- Preview 状态写入 `FrameCompare.ini`，默认关闭。

该模式借鉴了稳定 OSD 中“常驻显示/调试位置”的交互思路，但 FrameCompare 正式标签仍使用自身的 GPU 字形纹理 + compositor 路径，以便录制、分割区域裁切和最终合成保持一致。

---

## 11. INI 配置系统

FrameCompare 使用独立：

```text
FrameCompare.ini
```

目标类似 ShaderToggler 的使用体验。

### 11.1 自动保存 / 读取

插件需要：

- 启动时读取 INI。
- ReShade Add-ons 页面修改参数后可保存。
- 支持手动 Reload。

### 11.2 跨游戏复用

用户调好后可以直接把 `FrameCompare.ini` 复制到其他游戏，复用：

- 快捷键。
- 分割位置。
- Step。
- 长按延迟。
- 手动移动速度。
- Sweep 参数。
- 分割线外观。
- ON / OFF 文字。
- 标签位置。
- 字体参数。

不同游戏最可能需要单独调整的是 CaptureMode / NGX 兼容参数。

---

## 12. ReShade Add-ons 设置页面

插件应在 ReShade `Add-ons` 页面提供完整 GUI，而不是要求用户只能手写 INI。

设置内容至少包括：

### General

- Enable comparison。
- Freeze Before + After。
- Capture mode。
- Before image on left。
- Display mode。

### Divider

- Divider position。
- Short press step。
- Hold delay。
- Hold movement speed。
- Auto sweep active。
- Auto sweep speed。
- Ping-Pong。
- Auto reset from full Before。
- Sweep direction。
- Show divider。
- Divider width。
- Divider opacity。
- Mouse drag。
- Drag grab radius。

### Labels

- Show labels。
- Label placement preview。
- Before label。
- After label。
- Windows font。
- Before X/Y。
- After X/Y。
- Font size。
- Opacity。
- Outline。

### Hotkeys

所有主要快捷键均允许配置，而不是硬编码。

### NGX / DLSS5

- Feature 18 snapshot 最大有效时间。
- D3D12 Color resource state。
- Unsafe cross-device import 开关。

### INI 操作

- Save FrameCompare.ini。
- Reload FrameCompare.ini。

---

## 13. 诊断功能

插件需要提供足够诊断信息，以便某个游戏无法取得真正 Before 时可以定位原因。

诊断至少包括：

- 当前配置的 CaptureMode。
- 当前实际使用的 Capture Source。
- Capture note / fallback 原因。
- Before / After 是否已经形成有效 pair。
- Before 分辨率。
- After 分辨率。
- Freeze 当前状态：Live / Armed / Frozen。
- Sweep 当前状态。
- `FrameCompare.fx` 是否加载成功。
- 参数纹理上传是否正常。
- 当前 NGX Hook 模块。
- 最近一次 NGX API：D3D11 / D3D12。
- 最近 NGX Color 分辨率。
- NGX capture generation。
- Feature 18 Create 次数。
- Feature 18 Evaluate 次数。
- 成功捕获次数。
- 捕获失败次数。
- D3D11 Create/Evaluate/Release Hook 状态。
- D3D12 Create/Evaluate/Release Hook 状态。
- 最近 Hook 错误信息。

---

## 14. D3D11 / D3D12 NGX 支持

### D3D11

- Hook NGX CreateFeature。
- Hook NGX EvaluateFeature。
- Hook NGX ReleaseFeature。
- 在目标 Evaluate 之前复制 Color resource。

### D3D12

- Hook NGX CreateFeature。
- Hook NGX EvaluateFeature。
- Hook NGX ReleaseFeature。
- 在原 Evaluate 之前，在同 command list 上执行源资源状态转换、snapshot copy、源状态恢复。
- D3D12 Color 输入状态允许通过 INI 指定。

### 跨设备 / Bridge

某些 Bridge 可能建立私有 D3D12 device / queue。

要求：

- 默认关闭不安全的 cross-device import。
- 只有用户明确开启时才尝试。
- UI / 文档必须提示：共享 handle 本身不能提供完整 GPU queue 同步，因此这不是可靠保证。

---

## 15. ReShade 效果链集成

FrameCompare 配套 `FrameCompare.fx` 不是普通 preset shader。

要求：

- 不要求用户手动把 FrameCompareComposite 加入普通 TechniqueSorting。
- Add-on 自己绑定 Before / After / 参数 / 标签纹理。
- Add-on 在最终对比阶段显式调用 FrameCompareComposite。
- 分割位置等动态参数应通过 GPU 参数纹理传递，避免依赖可能在 Performance Mode 下固化的普通 uniform。

---

## 16. 录制用途

FrameCompare 的主要应用是直接生成可录制的画面对比，而不是后期制作。

典型录制流程：

```text
进入需要展示的场景
→ F10 Freeze
→ Home 显示完整 Before
→ F11 自动 Sweep
→ 分割线从原版扫到最终效果
→ OBS / Game Capture 直接录制
```

也可以：

```text
实时游戏
→ F9 打开对比
→ 鼠标拖动分割线
→ 或长按左右键实时展示效果差异
```

外部录屏工具（例如 OBS / 游戏捕获）是推荐方式。

ReShade 自带截图的捕获时序在部分配置下可能早于 FrameCompare 的最终合成，因此不保证一定能截到最终分屏结果。

---

## 17. 便携与游戏兼容目标

插件需要做到：

- 单个 `00-FrameCompare.addon64`。
- 单个 `FrameCompare.fx`。
- 一个可复制的 `FrameCompare.ini`。
- 不依赖旧 `SplitScreenCR.fx`。
- 不依赖 `sMask.png`。
- 尽量允许同一份配置跨多个游戏使用。
- 针对游戏差异只调整 CaptureMode / NGX 参数，而不是每个游戏重新设置所有 UI 与快捷键。

---

## 18. 当前平台范围

v1.2 目标：

- Windows x64。
- ReShade Add-on API 20+。
- 目标 ReShade 6.8.x。
- 原生 NGX Feature 18：D3D11 + D3D12。
- Feeder / 普通效果链：使用 ReShade Before/After 捕获。

Vulkan / OpenGL / D3D9 / D3D10 可以尝试通用 ReShade 捕获路径，但 v1.2 不包含原生 Vulkan NGX Feature 18 Hook。

---

## 19. 默认快捷键

| 功能 | 默认快捷键 |
|---|---|
| 开启 / 关闭对比 | F9 |
| Freeze / Unfreeze | F10 |
| 自动 Sweep | F11 |
| 分割线向左 | Left |
| 分割线向右 | Right |
| 完整 Before | Home |
| 完整 After | End |

所有快捷键均应允许在 INI / Add-ons UI 中修改。

---

## 20. 默认参数建议

```ini
[General]
Enabled=1
CaptureMode=0
BeforeOnLeft=1
DisplayMode=0

[Divider]
Position=0.500000
Step=0.020000
HoldDelay=0.250000
MoveSpeed=0.250000
AutoSweepSpeed=0.200000
AutoSweepPingPong=0
AutoResetFromBefore=1
AutoSweepDirection=1
ShowBorder=1
BorderWidth=0.002000
BorderOpacity=1.000000
ScreenDrag=1
DragGrabPx=14.000000

[Labels]
Show=1
Preview=0
BeforeText=OFF
AfterText=ON
FontName=Segoe UI
BeforeX=0.040000
BeforeY=0.060000
AfterX=0.880000
AfterY=0.060000
FontSizePx=42
Opacity=1.000000
OutlinePx=1.500000
```

---

## 21. 完整验收标准

一个版本只有在以下行为全部满足时，才应视为 FrameCompare 的完整功能版本：

1. 能在支持路径下同时取得 Before 与 After。
2. Native DLSS5 路径不通过关闭 DLSS5 获得 Before。
3. Feeder 路径能使用 Before ReShade FX。
4. 能 F9 开关对比。
5. 能 F10 Freeze 一对画面，且游戏本身继续运行。
6. 分割位置可以 0% ～ 100% 调整。
7. 分割线可以鼠标拖动。
8. 左右键短按可以步进。
9. 左右键长按可以连续移动。
10. 移动速度和长按延迟可调。
11. F11 可以自动 Sweep。
12. Sweep 速度、方向、Ping-Pong 可调。
13. 支持完整 Before / 完整 After 快捷键。
14. 支持普通 Wipe。
15. 支持 SplitScreenCR 风格显示。
16. Before / After 可以左右反转。
17. 有可自定义 OFF / ON 标签。
18. 两边标签 X/Y 独立可调。
19. 标签支持字体、字号、透明度、描边。
20. 正常录制时标签不会跨越到错误的对比区域。
21. 有独立标签位置调试预览，可在没有有效 Before/After pair 时持续显示两个标签并实时调位置。
22. 所有主要参数能写入 / 读取 FrameCompare.ini。
23. INI 可以复制到其他游戏继续使用。
24. Add-ons 页面可以修改核心参数。
25. 有足够的 NGX / capture 诊断信息。
26. 不主动恢复 ShaderToggler 已隐藏的 HUD。
27. 不依赖旧 SplitScreenCR 或 sMask.png 才能运行。
28. 可以直接用 OBS / 游戏捕获录制 Sweep 对比动画。
29. D3D11 / D3D12 原生 NGX Hook 具备失败保护和诊断。
30. 在无法保证真正 Vanilla 时必须明确显示实际 Capture Source，而不是把 fallback 冒充成真正 DLSS5 前画面。
31. 工程可通过 Windows VS2022 / GitHub Actions 构建为 `.addon64`。

---

## 22. 设计参考

FrameCompare 的交互与视觉设计参考了旧 ReShade/SweetFX 生态中的 Before/After 分屏思路，尤其是：

- `Splitscreen.fx` 的 Before / After 分屏方式。
- `SplitScreenCR` 的中心重映射观感、动态 transition、分隔线思路。

FrameCompare 不是对这些 shader 的运行时依赖或源码复制，而是把其成熟交互思路提升到 ReShade Add-on 层，并加入 DLSS5/NGX 捕获、Freeze、Sweep、标签、INI、诊断等插件级功能。

---

## v1.2 UI、中文与日志补充

### 中文 / English

- 插件 UI 默认中文。
- 顶部提供 `界面语言 / UI Language`。
- 可选 `中文` / `English`。
- 语言选择写入 `FrameCompare.ini`：`General.Language`。
- 语言切换即时生效，不需要重启游戏。

### 折叠 UI

Add-ons 页面不再一次铺开全部参数，分组为：

1. 快速教程 / 使用说明。
2. 基础设置。
3. 分割线与动画。
4. 文字标签。
5. 快捷键。
6. DLSS5 / NGX 捕获高级设置。
7. 诊断与日志。
8. 配置文件。

除快速教程外，其余分组默认折叠，减少页面长度。

### 插件内教程

快速教程必须直接说明：

- `.addon64` 和 `FrameCompare.fx` 的安装位置。
- RenoDX / 原生 DLSS5 与 Feeder 应选择哪个 CaptureMode。
- F9/F10/F11/方向键/Home/End 的用途。
- 如何开启文字位置调试预览。
- 如何判断 `FrameCompare.fx`、参数上传和 Before/After Pair 是否就绪。
- 错误应查看 `ReShade.log`。

### 日志

默认关键日志：

- 初始化结果。
- NGX Hook 初始化失败。
- `FrameCompare.fx` 缺失。
- 参数上传失败。
- NGX 新错误。
- INI 保存失败。

可选 `Diagnostics.VerboseLogging=1`：

- 记录捕获来源变化。
- 记录额外运行状态。

UI `诊断与日志` 还显示：

- 最近状态。
- 最近警告。
- 最近错误。

### FX 缺失处理

如果插件本体已加载但 `FrameCompare.fx` 不在 Shader 搜索路径：

- UI 顶部必须以明显红色文字提示。
- ReShade.log 必须写入一次警告。
- 不允许像 v1.1 一样每帧重复写同一条警告刷爆日志。
- GitHub Artifact 必须直接包含 `reshade-shaders/Shaders/FrameCompare.fx` 的正确目录结构。

### 版本显示

v1.2 增加 Windows VERSIONINFO，目标是在 ReShade 加载日志中显示 `v1.2.0.0`，而不是旧版本的 `v0.0.0.0`。
