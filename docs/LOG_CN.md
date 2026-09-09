# FrameCompare v1.3 日志与排错

## 正常启动

ReShade 日志中应看到 FrameCompare v1.3.0 初始化，以及当前实际快捷键。`FrameCompare.fx / FrameCompareComposite` 被找到后会记录一次“合成着色器已就绪”。

## FrameCompare.fx 缺失

检查：

```text
reshade-shaders/Shaders/FrameCompare.fx
```

是否存在，并确认 ReShade 的 Effect Search Path 包含该目录。插件只在首次发现缺失或 ReShade 重新加载效果后重新确认，不应每帧刷同一警告。

## 精确截图对未就绪

诊断区会分别显示 Before/After 是否已捕获。

- 只捕获一侧：再捕获另一侧。
- 捕获请求一直待命：关闭 ReShade 设置面板。
- 复制失败：请求不会丢失，会在下一帧继续尝试。
- 尺寸/格式不一致：保持游戏输出分辨率和显示格式不变，重新捕获两侧。

## 实时模式无 Before

先用“自动”模式。若 NGX Feature 18 没有可用快照，会回退到 ReShade FX 前。严格 NGX 模式不会回退，诊断会保留最近的 NGX 错误与快照年龄。

跨设备 NGX 导入默认关闭，因为无法普遍保证同步。`AllowUnsafeCrossDevice` 只用于明确测试，不应当作通用稳定路径。

## 文字问题

v1.3 不使用 GDI 文字纹理。如果文字仍有边缘问题，先检查全局字号、描边与安全边距；X/Y 是锚点，不是绝对像素位置。

## 详细日志

`Diagnostics.VerboseLogging=1` 会增加捕获来源与 NGX 诊断。正常录制时可保持关闭。
