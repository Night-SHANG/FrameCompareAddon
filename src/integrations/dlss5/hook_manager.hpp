#pragma once

#include <cstdint>
#include <string>

namespace framecompare::dlss5
{
struct HookSnapshot
{
    bool scanner_running = false;
    bool module_loaded = false;
    bool d3d11_evaluate = false;
    bool d3d11_evaluate_c = false;
    bool d3d12_evaluate = false;
    bool d3d12_evaluate_c = false;
    std::uint64_t d3d11_calls = 0;
    std::uint64_t d3d12_calls = 0;
    std::uint64_t resource_pairs = 0;
    std::uint64_t incomplete_calls = 0;
    std::string last_observation;
    std::string last_error;
};

bool start_hooks();
void stop_hooks();
HookSnapshot hook_snapshot();
}
