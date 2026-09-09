# FrameCompare v1.3 安装与使用

## 1. 文件位置

编译成功后的运行文件结构：

```text
游戏目录/
├─ 00-FrameCompare.addon64
├─ FrameCompare.ini                 可选，插件会保存/生成
└─ reshade-shaders/
   └─ Shaders/
      └─ FrameCompare.fx
```

`FrameCompare.fx` 必须能被当前 ReShade 的 Effect Search Path 找到。

## 2. 精确截图对：推荐录制流程

这是录制静态机位“真正 OFF → 完整 ON”最可靠的方式。

1. 打开 FrameCompare，工作流选择“精确截图对”。
2. 把需要对比的增强全部切到 OFF，例如 ReShade 预设、RenoDX/DLSS5 的相应效果状态。
3. 按“捕获 Before”或默认 F7。
4. 关闭 ReShade 设置面板；插件会抓取下一帧实际可见画面。
5. 把增强全部切到 ON。
6. 按“捕获 After”或默认 F8，再关闭设置面板完成捕获。
7. 画面对显示“已就绪”后，按默认 F9 开启/关闭对比。
8. 用 Left/Right、自动扫屏或面板打开时的鼠标拖动移动分割线。

精确截图对保存的是两张已经发生过的实际画面。移动分割线不会重新运行 DLSS/RenoDX/ReShade，也不会改变两侧内容。

如果 Before/After 的输出尺寸或像素格式发生变化，v1.3 会拒绝合成并提示重新捕获，避免把不兼容的纹理当作有效画面对。

## 3. 实时处理链

用于镜头或角色继续运动时的实时对比。Before 来源有：

- 自动：优先 NGX Feature 18，失败回退到 ReShade FX 前。
- 严格 NGX Feature 18。
- ReShade FX 前。

实时模式并不承诺所有游戏都能从同一动态帧得到“绝对原版”。DLSS5/RenoDX/游戏自身的注入阶段可能早于 ReShade 可观察的位置。

## 4. 分屏

“SplitScreenCR 分屏”是默认显示方式，使用中心偏移采样形成更像传统 SplitScreenCR 的左右对照；“普通同坐标擦除”保持原坐标采样。

可调：

- 分割位置
- 分割线显示/隐藏
- 宽度、透明度
- Before 左/右
- 短按移动步长
- 长按延迟、长按移动速度
- 自动扫屏速度、往返
- 自动扫屏是否从完整 Before 开始
- ReShade 面板打开时鼠标拖动

## 5. ON/OFF 与自定义 HUD

固定 Before/After 标签可以改文字和 X/Y。字号、透明度、描边、安全边距由全局文字设置统一控制。

自定义 HUD 可添加多个条目，状态来源：

1. 快捷键切换状态：每次按键在 ON/OFF 之间切换。
2. ReShade 实际效果状态：直接读取 ReShade Effects State，不需要把 END 再绑定一次。例如 ReShade 自己用 END 开关效果，条目会根据真实状态显示 ON 或 OFF。
3. 按住快捷键：按住为 ON，松开为 OFF。
4. 快捷键单次提示：每次按键显示一段自定义文字并重新开始显示计时，适合“某功能已触发”这类 HUD。

`状态变化后显示秒数=0` 表示常驻；大于 0 表示只在初始化/状态变化后显示指定时间。新建条目默认 1 秒。

## 6. 快捷键绑定

点击快捷键输入框，直接按目标键或 Ctrl/Shift/Alt 组合键，再点“确定”。支持清除和取消，不需要输入 Windows VK 数字。

默认快捷键只是初始值，界面中的按钮提示会显示当前实际绑定，而不是硬编码 F7/F8/F11。

## 7. INI

配置文件为 `FrameCompare.ini`，位于 Add-on 同目录。可以复制到其他游戏继续使用布局、文字、快捷键和 HUD 条目。

`FrameCompare.ini.example` 提供完整字段示例。
