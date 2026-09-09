# FrameCompare v1.2 安装与使用

## 1. GitHub 编译

把源码上传到 GitHub 仓库的 `main` 或 `master` 分支后，`Build FrameCompare` 会自动运行。

编译成功后：

```text
Actions -> Build FrameCompare -> 对应成功任务 -> Artifacts
```

下载：

```text
FrameCompare-Windows-x64
```

## 2. 安装到游戏

v1.2 的 Artifact 已整理成正确目录：

```text
00-FrameCompare.addon64
FrameCompare.ini.example
reshade-shaders\
  Shaders\
    FrameCompare.fx
```

最简单的方法是把 Artifact 内全部内容直接合并复制到游戏 ReShade 根目录。

其中：

- `00-FrameCompare.addon64` 必须位于 ReShade 能扫描到 Add-on 的目录。通常与 `dxgi.dll`、ShaderToggler、RenoDX `.addon64` 在同一目录。
- `FrameCompare.fx` 必须位于 ReShade Shader 搜索路径。默认最常见是：

```text
reshade-shaders\Shaders\FrameCompare.fx
```

如果只复制 `.addon64` 而漏掉 `.fx`，插件会注册成功，但无法显示最终对比画面。

第一次可以把：

```text
FrameCompare.ini.example
```

复制/重命名为：

```text
FrameCompare.ini
```

并与 `.addon64` 放在一起。也可以直接进游戏调参数，插件会保存自己的 INI。

## 3. 第一次进游戏怎么判断安装正常

打开：

```text
ReShade -> Add-ons -> FrameCompare
```

v1.2 默认是中文 UI。

先看顶部：

```text
合成着色器：已就绪
```

如果看到红色：

```text
错误：未找到 FrameCompare.fx
```

说明 `.addon64` 已加载，但 `.fx` 安装位置不对。

展开：

```text
诊断与日志
```

至少应看到：

```text
FrameCompare.fx: 已就绪
参数上传: 已就绪
```

开始对比后：

```text
画面对: 已就绪
```

## 4. 最基础的使用

默认快捷键：

```text
F9    开关对比
F10   冻结 / 解除冻结 Before + After
F11   自动扫屏
← →   移动分割线；短按步进，长按连续移动
Home  完整显示 Before
End   完整显示 After
```

推荐第一次这样测试：

1. 进入一个静止场景。
2. 按 F9 开启对比。
3. 看中央是否出现 Before / After 分割。
4. 按左右方向键移动分割线。
5. 按 F10 冻结当前两张图。
6. 按 F11 做一次自动扫屏。

## 5. DLSS5 应该用哪个捕获模式

### A. RenoDX / 原生 Add-on 方式 DLSS5

先选：

```text
自动：NGX Feature 18 -> ReShade FX 前回退
```

插件会优先尝试 DLSS5 Neural Rendering Feature 18 Evaluate 前的 Color。

### B. DLSS5 Feeder / ReShade 效果链方式

选：

```text
ReShade FX 前
```

这样 Before 在 ReShade 效果链开始处保存，随后 Feeder / 其他效果继续运行。

### C. 只想验证 NGX 真正工作

选：

```text
严格 NGX Feature 18
```

如果没有新鲜有效的 Feature 18 snapshot，插件不会偷偷回退到另一阶段。

## 6. OFF / ON 文字

展开：

```text
文字标签
```

默认：

```text
Before = OFF
After  = ON
```

支持：

- 自定义文字。
- Before / After 独立 X/Y。
- Windows 字体。
- 字号。
- 透明度。
- 黑色描边。

### 调位置

开启：

```text
文字位置调试预览（始终显示两边）
```

即使 Before / After 还没捕获成功，两个标签也会持续显示。可以一边看游戏，一边实时调整 X/Y、字号和描边。

正式录制前建议关闭预览，让标签恢复只属于对应 Before / After 区域的正常裁切逻辑。

中文标签建议字体：

```text
Microsoft YaHei UI
```

## 7. 分割线与自动动画

展开：

```text
分割线与动画
```

可以调：

- 分割位置。
- 短按步长。
- 长按触发延迟。
- 长按移动速度。
- 自动扫屏速度。
- Ping-Pong 往返。
- 分割线宽度 / 透明度。
- 鼠标拖动抓取范围。

ReShade Overlay 打开时，可以直接抓住分割线拖动。

## 8. SplitScreenCR 风格

`显示模式` 有：

```text
普通擦除
SplitScreenCR 风格中央重映射
```

普通擦除不会压缩或拉伸两张图，两边使用同一完整屏幕坐标，更适合画质对比和视频录制。

SplitScreenCR 模式参考旧 `SplitScreenCR.fx` 的中央区域重映射显示逻辑，作为可选展示模式保留。

## 9. ShaderToggler / HUD

FrameCompare 不会重新执行游戏 draw call。ShaderToggler 已经阻止的 HUD shader 不会因为 FrameCompare 保存 Before 而主动恢复。

目标是：

```text
Before：原版画质，但继续保留 ST 的 HUD 隐藏状态
After ：DLSS5 / RenoDX / ReShade 最终画面，同样保留 HUD 隐藏状态
```

## 10. 中文 / English

UI 顶部：

```text
界面语言 / UI Language
```

可选：

```text
中文
English
```

选择会写入 `FrameCompare.ini`：

```ini
[General]
Language=0   ; 中文
```

或：

```ini
Language=1   ; English
```

默认中文。

## 11. 折叠 UI

v1.2 不再把所有参数一次性铺满页面。

分组为：

```text
快速教程 / 使用说明
基础设置
分割线与动画
文字标签
快捷键
DLSS5 / NGX 捕获高级设置
诊断与日志
配置文件
```

点击标题即可展开/收起。

## 12. 日志

FrameCompare 会把关键问题写入：

```text
ReShade.log
```

搜索：

```text
[FrameCompare]
```

插件 UI 的：

```text
诊断与日志
```

还会显示最近状态、最近警告、最近错误。

如果需要更多运行信息，开启：

```text
详细日志
```

更多说明见 `LOG_CN.md`。

## 13. INI 跨游戏复用

调好后把：

```text
FrameCompare.ini
```

复制到其他游戏的 `.addon64` 同目录即可复用：

- 中文/English。
- 快捷键。
- 标签文字和位置。
- 字号/描边。
- 分割线参数。
- Sweep 速度。
- 捕获模式等。

## 14. D3D12 NGX 高级参数

默认：

```ini
D3D12ColorState=0x40
AllowUnsafeCrossDevice=0
```

除非诊断某个特殊 Bridge，否则不要打开跨设备 NGX 导入。共享 handle 本身没有跨队列同步保证，可能得到旧帧或未定义数据。
