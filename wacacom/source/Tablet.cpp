#include "Tablet.hpp"

static std::string trim(auto value) requires std::is_convertible_v<decltype(value), std::string>
{
    auto result = value;
    result.erase(result.begin(), std::ranges::find_if(result, [] (auto character) { return !std::isspace(character); }));
    result.erase(std::find_if(result.rbegin(), result.rend(), [] (auto character) { return !std::isspace(character); }).base(), result.end());
    return result;
}

std::vector<Device> get_devices()
{
    std::vector<Device> devices {};

    auto const command = "xsetwacom --list devices";
    auto const fd = popen(command, "r");
    assert(fd && "FILE DESCRIPTOR WAS NULL");
    auto const matcher = ctre::search<R"((.+)\s+id: (\d+)\s+type: (\w+))">;

    while (true)
    {
        std::array<char, 512> buffer {};
        if (fgets(buffer.data(), buffer.size(), fd) == nullptr) break;
        std::string data(buffer.data());
        data.pop_back();
        if (auto match = matcher(data))
        {
            auto const name = trim(match.get<1>().to_string());
            auto const id = match.get<2>().to_number();
            auto const type = DeviceType::from_string(trim(match.get<3>().to_string()));
            devices.push_back({ name, id, type });
        }
    }

    pclose(fd);

    return devices;
}

std::vector<Device> get_drawing_devices()
{
    return fplus::keep_if([] (auto&& device) { return device.type == DeviceType::STYLUS; }, get_devices());
}

Region get_device_area(Device const& device)
{
    Region area {};

    auto const command = fmt::format("xsetwacom --get {} Area", device.id);
    auto const fd = popen(command.data(), "r");
    assert(fd && "FILE DESCRIPTOR WAS NULL");

    std::array<char, 512> buffer {};
    fgets(buffer.data(), buffer.size(), fd);
    std::stringstream sstream(buffer.data());
    sstream >> area.offsetX;
    sstream >> area.offsetY;
    sstream >> area.width;
    sstream >> area.height;

    pclose(fd);

    return area;
}

Region get_device_entire_area(Device const& device)
{
    Region area {};

    auto const currentArea = get_device_area(device);
    reset_device_area(device);

    auto const command = fmt::format("xsetwacom --get {} Area", device.id);
    auto const fd = popen(command.data(), "r");
    assert(fd && "FILE DESCRIPTOR WAS NULL");

    std::array<char, 512> buffer {};
    fgets(buffer.data(), buffer.size(), fd);
    std::stringstream sstream(buffer.data());
    sstream >> area.offsetX;
    sstream >> area.offsetY;
    sstream >> area.width;
    sstream >> area.height;

    pclose(fd);

    set_device_area(device, currentArea);

    return area;
}

void set_device_output_from_display_name(Device const& device, std::string_view displayName)
{
    auto const command = fmt::format("xsetwacom --set \"{}\" MapToOutput {}", device.id, displayName);
    auto const fd = popen(command.data(), "r");
    assert(fd && "FILE DESCRIPTOR WAS NULL");
    pclose(fd);
}

void set_device_output_from_display_region(Device const& device, Region const& dimension)
{
    auto const command = fmt::format("xsetwacom --set \"{}\" MapToOutput {}x{}+0+0", device.id, dimension.width, dimension.height, dimension.offsetX, dimension.offsetY);
    auto const fd = popen(command.data(), "r");
    assert(fd && "FILE DESCRIPTOR WAS NULL");
    pclose(fd);
}

void set_device_area(Device const& device, Region const& area)
{
    auto const command = fmt::format("xsetwacom --set {} Area {} {} {} {}", device.id, area.offsetX, area.offsetY, area.width, area.height);
    auto const fd = popen(command.data(), "r");
    assert(fd && "FILE DESCRIPTOR WAS NULL");
    pclose(fd);
}

void reset_device_area(Device const& device)
{
    auto const command = fmt::format("xsetwacom --set {} ResetArea", device.id);
    auto const fd = popen(command.data(), "r");
    assert(fd && "FILE DESCRIPTOR WAS NULL");
    pclose(fd);
}

Pressure get_device_pressure_curve(Device const& device)
{
    Pressure pressure {};

    auto const command = fmt::format("xsetwacom --get {} PressureCurve", device.id);
    auto const fd = popen(command.data(), "r");
    assert(fd && "FILE DESCRIPTOR WAS NULL");

    std::array<char, 512> buffer {};
    fgets(buffer.data(), buffer.size(), fd);
    std::stringstream sstream(buffer.data());

    sstream >> pressure.minX;
    pressure.minX /= 100.f;

    sstream >> pressure.minY;
    pressure.minY /= 100.f;

    sstream >> pressure.maxX;
    pressure.maxX /= 100.f;

    sstream >> pressure.maxY;
    pressure.maxY /= 100.f;

    pclose(fd);

    return pressure;
}

void set_device_pressure_curve(Device const& device, Pressure const& pressure)
{
    auto const minX = std::round(pressure.minX * 100.f);
    auto const minY = std::round(pressure.minY * 100.f);
    auto const maxX = std::round(pressure.maxX * 100.f);
    auto const maxY = std::round(pressure.maxY * 100.f);
    auto const command = fmt::format("xsetwacom --set {} PressureCurve {} {} {} {}", device.id, minX, minY, maxX, maxY);
    auto const fd = popen(command.data(), "r");
    assert(fd && "FILE DESCRIPTOR WAS NULL");
    pclose(fd);
}

