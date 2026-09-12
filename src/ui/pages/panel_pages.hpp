#pragma once

namespace reshade::api
{
class effect_runtime;
}

namespace framecompare::ui::pages
{
void draw_language_selector(const char *stable_id);
void draw_compare_page(reshade::api::effect_runtime *runtime);
void draw_motion_page();
void draw_hotkeys_page();
void draw_labels_page();
void draw_status_page();
void draw_config_page(reshade::api::effect_runtime *runtime);
}
