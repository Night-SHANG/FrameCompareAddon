#include "config/config_runtime.hpp"

#include "config/settings_codec.hpp"

#include <Windows.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace framecompare::config
{
namespace
{
using Clock = std::chrono::steady_clock;

std::filesystem::path g_path;
std::string g_saved;
std::string g_pending;
ConfigStatus g_status = ConfigStatus::none;
Clock::time_point g_pending_since = Clock::now();
bool g_initialized = false;

std::string read_file(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return {};
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

bool write_file(const std::filesystem::path &path,
                const std::string &content)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        return false;
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    output.flush();
    return output.good();
}
}

void initialize(void *addon_module)
{
    std::array<wchar_t, 32768> module_path{};
    const DWORD length = GetModuleFileNameW(
        static_cast<HMODULE>(addon_module), module_path.data(),
        static_cast<DWORD>(module_path.size()));
    g_path = length > 0 && length < module_path.size()
        ? std::filesystem::path(module_path.data()).parent_path() /
            L"FrameCompare.ini"
        : std::filesystem::path(L"FrameCompare.ini");
    g_initialized = true;

    std::error_code error;
    if (std::filesystem::exists(g_path, error))
        reload_now();
    else
        save_now();
}

void shutdown()
{
    if (g_initialized && encode_current_settings() != g_saved)
        save_now();
    g_initialized = false;
}

void tick()
{
    if (!g_initialized)
        return;
    const std::string current = encode_current_settings();
    if (current == g_saved)
    {
        g_pending.clear();
        return;
    }
    if (current != g_pending)
    {
        g_pending = current;
        g_pending_since = Clock::now();
        return;
    }
    if (std::chrono::duration<float>(
            Clock::now() - g_pending_since).count() >= 0.75f)
        save_now();
}

bool save_now()
{
    if (!g_initialized || g_path.empty())
        return false;
    const std::string current = encode_current_settings();
    if (!write_file(g_path, current))
    {
        g_status = ConfigStatus::save_failed;
        return false;
    }
    g_saved = current;
    g_pending.clear();
    g_status = ConfigStatus::saved;
    return true;
}

bool reload_now()
{
    if (!g_initialized || g_path.empty())
        return false;
    const std::string content = read_file(g_path);
    if (content.empty())
    {
        g_status = ConfigStatus::reload_failed;
        return false;
    }
    decode_current_settings(content);
    g_saved = encode_current_settings();
    g_pending.clear();
    g_status = ConfigStatus::reloaded;
    return true;
}

ConfigStatus status() noexcept
{
    return g_status;
}
}
