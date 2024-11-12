#pragma once

#include "Region.hpp"

#include <ctre.hpp>
#include <fmt/format.h>
#include <fplus/filter.hpp>
#include <fplus/search.hpp>
#include <libenum/Enum.hpp>
#include <fplus/fplus.hpp>

#include <cctype>
#include <cassert>
#include <cstdio>
#include <vector>

ENUM_CLASS(DeviceType, STYLUS, PAD, ERASER, TOUCH);

struct Pressure
{
    float minX, minY;
    float maxX, maxY;
};

struct Device
{
    std::string name;
    int id;
    DeviceType type;
};

std::vector<Device> get_devices();
std::vector<Device> get_drawing_devices();
void set_device_output_from_display_name(Device const& device, std::string_view displayName);
void set_device_output_from_display_region(Device const& device, Region const& dimension);
Pressure get_device_pressure_curve(Device const& device);
void set_device_pressure_curve(Device const& device, Pressure const& pressure);
Region get_device_area(Device const& device);
Region get_device_entire_area(Device const& device);
void set_device_area(Device const& device, Region const& area);
void reset_device_area(Device const& device);

