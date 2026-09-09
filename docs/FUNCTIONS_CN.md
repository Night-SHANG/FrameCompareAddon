# FrameCompare v1.3 功能说明

## 工作流

### 精确截图对

分别从最终可见 BackBuffer 捕获 Before 与 After。捕获时跳过 FrameCompare 自身的分屏合成和 OSD；ReShade 设置面板未关闭时保持待命。两张纹理均捕获完成且尺寸/格式兼容后才允许合成。

用途：固定机位、冻结感画面、录制动态分割线，重点保证两边确实来自用户实际切换过的 OFF/ON 状态。

### 实时处理链

每个 ReShade effect cycle 获取 Before，再在 effect 结束时获取 After，并立即合成。支持 NGX Feature 18 或 ReShade FX 前路径。用于动态镜头，但“Before=绝对 Vanilla”依赖目标游戏的处理链位置。

## 合成器

`FrameCompare.fx` 只负责画面对，不再承担文字。

- 普通擦除：Before/After 使用相同 UV。
- SplitScreenCR 风格：在 50/50 时形成典型的左右 `+0.25 / -0.25` 中心偏移视觉；实现为独立重写，并处理 0%/100% 为真正完整单侧，便于自动扫屏干净收尾。
- 分割线独立控制显示、宽度和透明度。

Before/After/参数分别通过 `FRAMECOMPARE_BEFORE`、`FRAMECOMPARE_AFTER`、`FRAMECOMPARE_PARAMS` semantic 绑定。

## 分割线运动

- 单击移动键：按 `Step` 离散移动。
- 长按：超过 `HoldDelay` 后按 `MoveSpeed` 连续移动。
- 自动扫屏：`AutoSweepSpeed` 控制速度，可单向停止或 Ping-Pong 往返。
- 完整 Before/After：跳到逻辑端点，自动考虑 Before 位于左侧还是右侧。
- 鼠标拖动：只在 ReShade 面板打开时启用，避免正常游戏时抢鼠标输入。

## 冻结

实时模式中可以冻结首个有效画面对。冻结后停止更新 Before/After，只移动合成分割线；解冻恢复实时捕获。

精确截图对本身就是固定的两张捕获画面，不需要重复冻结。

## OSD 文字

文字通过 ReShade 的 ImGui overlay 绘制，不再使用 v1.2 的 GDI 位图纹理路径。

- 全局字号
- 全局透明度
- 全局描边
- 屏幕安全边距
- Before/After 各自文本、X/Y
- 多个自定义 HUD 条目共用全局样式

位置使用 0~1 锚点，并根据完整文字尺寸限制在安全区内，避免 ON/OFF 顶部或底部被旧纹理边界裁切。

## 自定义 HUD 状态来源

### 快捷键切换状态

用于普通自定义开关。每次按快捷键翻转 ON/OFF，并显示对应文字。

### ReShade 实际效果状态

每帧读取 `effect_runtime::get_effects_state()`。例如 ReShade 全局效果键本身是 END 时，HUD 不需要再重复绑定 END；只要状态真的变化，显示内容就跟真实 Effects State 变化。

### 按住快捷键

按住时 ON，松开时 OFF。

### 快捷键单次提示

每次按键都显示 `TextOn`，并重新开始 `ShowSeconds` 计时；不维护 ON/OFF 开关状态。用于只需要“按下就弹一次文字”的自定义 HUD。

## 快捷键捕获

所有插件快捷键使用统一的按键捕获 UI：点击输入框，直接按键/组合键，确认即可。INI 内部仍以 `VK,Ctrl,Shift,Alt` 持久化，但用户不需要手工输入数字。

默认 Full Before/Full After 使用 Ctrl+Left/Ctrl+Right，不占用常见的 Home/End。

## 配置持久化

`FrameCompare.ini` 保存：

- 工作流与实时 Before 来源
- 分屏方式和方向
- 分割线参数与运动参数
- 全局文字样式
- Before/After 标签
- 全部快捷键
- 0~32 个自定义 HUD 条目（四种状态/触发来源）
- NGX 高级参数与日志选项

旧 v1.2 INI 没有 `General.Strategy` 时，v1.3 自动使用精确截图对 + SplitScreenCR 风格作为迁移默认值。

## ShaderToggler 兼容原则

FrameCompare 不重新构造游戏 HUD，而是复制已经形成的画面。若 ShaderToggler 在捕获时已经隐藏 HUD，FrameCompare 不会主动把它恢复出来。

## 关闭对比

`Enabled=0` 或对比快捷键关闭后，FrameCompare 不执行合成；游戏继续显示正常最终效果画面。
