# FrameCompare v2 分页、本地化与自动发布实施计划

> **执行方式：** 当前任务内连续完成。每一阶段先补验证，再实现，再运行相关检查；最终只在全部本地检查和 GitHub 双架构 CI 通过后创建 `v2.0.0` 标签。

**目标：** 将长纵向面板改为六个顶部页签，加入默认简体中文且可持久化的中英文切换，同时交付 Win32/x64 构建和语义版本标签自动 Release。

**边界：** 保留当前捕获、合成、扫屏、标签裁切、快捷键、状态条目与配置行为。本次不加入 DLSS5 捕获代码。

## 阶段一：建立单一 UI 本地化状态

### 任务 1：新增纯 C++ 本地化模块和测试

**文件：**

- 新建 `src/ui/i18n/localization.hpp`
- 新建 `src/ui/i18n/localization.cpp`
- 新建 `tests/localization_tests.cpp`
- 修改 `CMakeLists.txt`

**步骤：**

1. 在测试中断言默认语言为 `zh_cn`、`zh-CN`/`en` 可解析、未知值回退中文、每个 `TextId` 的中英文文本均非空。
2. 运行本地化测试并确认在模块不存在时失败。
3. 定义 `UiLanguage { zh_cn, en }` 和完整 `TextId`，覆盖页签、对比、扫屏、快捷键、标签、状态、配置、按钮与运行状态消息。
4. 实现 `language()`、`set_language()`、`parse_language()`、`language_code()` 与 `text()`；语言表长度由编译期断言与 `TextId::count` 对齐。
5. 将模块加入 `framecompare_core`，将测试加入 CTest。
6. 运行新测试与现有六组测试。

## 阶段二：持久化语言并清除双语运行状态

### 任务 2：将语言加入现有 INI

**文件：**

- 修改 `src/config/settings_encode.cpp`
- 修改 `src/config/settings_decode.cpp`
- 修改 `FrameCompare.ini.example`
- 修改 `tests/ini_document_tests.cpp`

**步骤：**

1. 增加 `[UI] Language=zh-CN` 的 INI 往返测试。
2. 编码时使用本地化模块的 `language_code(language())` 写入 `[UI]`。
3. 解码时先读取 `UI/Language`，缺失、空值或未知值均调用 `parse_language` 回退中文。
4. 保持旧 INI 的其他字段读取行为不变。
5. 更新示例文件，确保复制示例即可看到语言设置。

### 任务 3：配置状态改为可本地化枚举

**文件：**

- 修改 `src/config/config_runtime.hpp`
- 修改 `src/config/config_runtime.cpp`

**步骤：**

1. 新增 `ConfigStatus { none, save_failed, saved, reload_failed, reloaded }`。
2. 用枚举替换当前双语 `std::string` 权威状态。
3. 暴露 `status()`；显示层根据当前语言选择文字。
4. 保留现有自动保存、防抖、手动保存和重读逻辑。

## 阶段三：将长面板拆成六个顶部页签

### 任务 4：稳定所有交互控件 ID

**文件：**

- 修改 `src/ui/parameter_widgets.hpp`
- 修改 `src/ui/parameter_widgets.cpp`
- 修改 `src/input/hotkeys.hpp`
- 修改 `src/input/hotkeys.cpp`

**步骤：**

1. `numeric_setting` 增加独立 `stable_id` 参数，滑块、输入框和重置按钮只用该 ID 定位。
2. 重置按钮文字通过 `TextId::reset` 获取，不再写死双语。
3. `draw_binding_editor` 增加独立 `stable_id` 参数，快捷键捕获状态保存该 ID。
4. “未设置”“清除”“请按键”通过本地化模块显示。
5. 确认运行时切换语言不会改变正在捕获的快捷键身份。

### 任务 5：新增页面绘制模块并精简协调器

**文件：**

- 新建 `src/ui/pages/panel_pages.hpp`
- 新建 `src/ui/pages/panel_pages.cpp`
- 修改 `src/ui/panel.cpp`
- 修改 `src/hud/indicator_panel.cpp`
- 修改 `CMakeLists.txt`

**步骤：**

1. 把现有设置按职责移动到 `draw_compare_page`、`draw_motion_page`、`draw_hotkeys_page`、`draw_labels_page`、`draw_status_page`、`draw_config_page`。
2. 保持每项设置的原权威对象与默认值，不复制设置状态。
3. 状态条目正文继续由 `hud::draw_indicator_panel()` 管理，但去掉其外部页面标题。
4. `panel.cpp` 顶部绘制紧凑语言组合框，并创建六个 `BeginTabItem`。
5. 页签和控件使用 `本地化文字##stable_id`，默认进入“对比”页，当前页不写入 INI。
6. 配置页将 `ConfigStatus` 映射到本地化文字，重读成功后继续重置捕获状态。
7. 将所有插件设置 UI 中的硬编码双语标签改为 `TextId`，用户自定义文本保持原样。

### 任务 6：扩展静态审计

**文件：**

- 修改 `tools/static_audit.py`

**步骤：**

1. 在实现前加入六页签、本地化入口、稳定 ID 和无双语拼接检查并确认失败。
2. 实现后确认审计通过。
3. 保持脚本不超过项目 Python 200 行限制；若接近限制，将新检查放入 `tools/audits/` 的单一职责模块。

## 阶段四：双架构构建与标签 Release

### 任务 7：让 CMake 按目标位数生成正确后缀

**文件：**

- 修改 `CMakeLists.txt`
- 修改 `README.md`

**步骤：**

1. 根据 `CMAKE_SIZEOF_VOID_P` 设置 `.addon64` 或 `.addon32`，未知指针宽度直接配置失败。
2. 打包时复制 `$<TARGET_FILE:FrameCompareV2>`，不再硬编码 64 位文件名。
3. README 写明两种架构、语言切换、六个页签和安装内容。
4. 保留 ReShade API v6.8.0 与当前静态运行库设置。

### 任务 8：GitHub Actions 矩阵和自动 Release

**文件：**

- 修改 `.github/workflows/build.yml`
- 修改 `tools/static_audit.py`

**步骤：**

1. 将 Windows job 改为 `x64`/`Win32` 矩阵，构建目录按架构隔离。
2. 每个矩阵项运行静态审计、全部 CTest 和 add-on 构建，上传对应 package。
3. `main`、PR、手动运行只产生 Actions 制品；`v*.*.*` 标签触发同一构建。
4. 新增仅标签运行的 Release job，声明 `contents: write` 并依赖整个构建矩阵。
5. 下载 x64/x86 package，分别压缩为 `FrameCompare-<tag>-windows-x64.zip` 与 `FrameCompare-<tag>-windows-x86.zip`。
6. 生成 `SHA256SUMS.txt`，通过 `gh release create --verify-tag` 创建 Release；已有同名 Release 时失败，不覆盖。
7. 静态审计确认双后缀、矩阵、标签条件、依赖关系与 Release 命令均存在。

## 阶段五：验证、推送与发布

### 任务 9：本地完整检查

**步骤：**

1. 运行 `python tools/static_audit.py`。
2. 使用可用的本地编译器运行所有纯 C++ 测试；若本机仍无 CMake，记录该限制并由 Actions 完成 MSVC 权威构建。
3. 运行 `git diff --check`、文件行数与 UTF-8 检查。
4. 审阅最终 diff，确认没有 DLSS5 代码和旧功能删除。
5. 提交实现提交。

### 任务 10：GitHub 双架构验证和 `v2.0.0` 发布

**步骤：**

1. 推送 `main`。
2. 等待对应 Actions 运行完成；x64 与 Win32 任一失败时读取日志、修复、重新推送。
3. 在双架构普通 CI 成功且远端不存在 `v2.0.0` 标签/Release 后，在该精确提交创建并推送带注释标签 `v2.0.0`。
4. 等待标签工作流完成并验证 Release 存在。
5. 核对 Release 含 x64 ZIP、x86 ZIP 与 SHA256 文件。
6. 最终说明本地检查、Actions 结果、Release 链接，以及仍需用户进行的游戏内 64 位/32 位界面运行验证。
