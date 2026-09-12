#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace framecompare::ui::i18n
{
enum class UiLanguage
{
    zh_cn,
    en
};

enum class TextId : std::size_t
{
    language, language_chinese, language_english,
    tab_compare, tab_motion, tab_hotkeys, tab_labels, tab_status, tab_config,
    enable_comparison, split_position, before_on_left, display_mode,
    mode_same_coordinate, mode_center_remap, center_focus, center_sweep_warning,
    show_border, border_width, border_opacity, reset_comparison,
    freeze_pair, manual_speed, auto_speed, auto_mode,
    sweep_left_to_right, sweep_right_to_left, sweep_ping_pong,
    stop_and_center, start_autosweep, reset_motion,
    move_left, move_right, focus_left, focus_right, toggle_comparison,
    toggle_autosweep, toggle_freeze, toggle_display_mode, toggle_border,
    reset_hotkeys, unbound, clear, press_key,
    show_labels, before_text, after_text, left_x, left_y, right_x, right_y,
    mirror_left_to_right, mirror_right_to_left, font_size, text_opacity,
    outline_width, outline_opacity, reset_labels, reset,
    status_help, add_status_item, remove, enabled, name, state_source,
    source_tracked_hotkey, source_reshade, hotkey, initial_state_on,
    reshade_source_help, on_text, off_text, x, y, show_seconds,
    current_state,
    language_help, config_help, save_now, reload,
    config_save_failed, config_saved, config_reload_failed, config_reloaded,
    capture_ready, capture_waiting, vanilla_warning,
    count
};

UiLanguage language() noexcept;
void set_language(UiLanguage value) noexcept;
UiLanguage parse_language(std::string_view value) noexcept;
const char *language_code(UiLanguage value) noexcept;
const char *text(TextId id) noexcept;
const char *text(TextId id, UiLanguage value) noexcept;
std::string label(TextId id, std::string_view stable_id);
}
