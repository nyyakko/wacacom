#include "Display.hpp"

std::vector<Display> list_active_displays()
{
    std::vector<Display> monitors {};

    auto const command = "xrandr --listactivemonitors";
    auto const fd = popen(command, "r");
    assert(fd && "FILE DESCRIPTOR WAS NULL");
    auto const matcher = ctre::search<R"((\d+):\s*\+(\*?)([A-Za-z0-9\-]+)\s(\d+)\/\d+x(\d+))">;

    while (true)
    {
        std::array<char, 512> buffer {};
        if (fgets(buffer.data(), buffer.size(), fd) == nullptr) break;
        std::string data(buffer.data());
        data.pop_back();
        auto [expression, id, primary, name, width, height] = matcher(data);
        if (expression)
        {
            monitors.push_back({ id.to_number(), primary ? true : false, width.to_number(), height.to_number(), name.to_string() });
        }
    }

    pclose(fd);

    return monitors;
}

Display get_primary_display()
{
    auto const monitor = fplus::find_first_by([] (auto&& monitor) { return monitor.primary; }, list_active_displays());
    assert(monitor.is_just() && "COULD NOT FIND PRIMARY MONITOR");
    return monitor.unsafe_get_just();
}

