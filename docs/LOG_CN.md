# FrameCompare v1.2 日志说明

FrameCompare 会把关键状态写入游戏目录中的 `ReShade.log`。日志行会带有 `[FrameCompare]`，因此排查时可以直接搜索：

```text
[FrameCompare]
```

## 默认会记录

- 插件初始化成功/失败。
- `FrameCompare.fx / FrameCompareComposite` 成功找到并就绪。
- 第一组有效 Before/After Pair 成功建立。
- NGX Hook 初始化失败。
- `FrameCompare.fx` 缺失或没有编译成功。
- 参数纹理上传失败。
- NGX 捕获返回新的错误。
- INI 保存失败。

## 详细日志

在插件 UI：

```text
诊断与日志 -> 详细日志
```

或 INI：

```ini
[Diagnostics]
VerboseLogging=1
```

开启后还会记录捕获来源变化、部分状态切换等信息。正常录制时可以保持关闭，避免日志过多。

## UI 内诊断

ReShade -> Add-ons -> FrameCompare -> `诊断与日志` 中会显示：

- 当前捕获模式。
- 当前捕获来源。
- Before/After 是否形成有效 Pair。
- Before/After 分辨率。
- Freeze / Sweep / 标签预览状态。
- `FrameCompare.fx` 是否就绪。
- 参数上传是否就绪。
- NGX Hook 模块。
- Feature 18 Create/Evaluate/Capture/Failure 计数。
- 最近状态、最近警告、最近错误。

## 最重要的两个状态

正常工作至少需要：

```text
FrameCompare.fx: 已就绪
参数上传: 已就绪
```

日志中正常情况下还会看到类似：

```text
[FrameCompare] FrameCompare: 初始化完成...
[FrameCompare] FrameCompare: 已找到 FrameCompare.fx / FrameCompareComposite，合成着色器已就绪。
[FrameCompare] FrameCompare: Before/After 画面对已就绪：1920x1080 / 1920x1080；捕获来源：...
```

最后一行表示真正的 Before/After Pair 已经成功形成，是判断插件实际进入工作状态最直接的日志之一。

启动对比后还应看到：

```text
画面对: 已就绪
```

如果 `FrameCompare.fx` 显示“缺失 / 未编译”，插件虽然已经加载，但不会得到最终分屏画面。

## v1.1 测试日志发现的问题

用户提供的测试日志中，FrameCompare Add-on 本体成功加载并注册了 ReShade API 20，但之后连续出现：

```text
FrameCompare.fx / FrameCompareComposite was not found
```

这说明当时不是 `.addon64` 没加载，而是配套 `FrameCompare.fx` 没有位于 ReShade 的 Shader 搜索路径。

v1.2 已修改 GitHub Actions 的成品目录结构，Artifact 中会直接生成：

```text
00-FrameCompare.addon64
reshade-shaders\
  Shaders\
    FrameCompare.fx
```

因此把 Artifact 的内容合并复制到游戏 ReShade 根目录即可，能减少漏装 `.fx` 的情况。

另外 v1.1 的“FX 缺失”警告存在每帧重复写日志的问题。v1.2 已修正为只在首次发现/重新加载效果后再次确认时记录，不再刷屏。
