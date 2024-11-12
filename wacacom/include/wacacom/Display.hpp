#pragma once

#include <fplus/fplus.hpp>
#include <ctre.hpp>

#include <string>
#include <vector>

struct Display
{
    int id;
    bool primary;
    int width, height;
    std::string name;
};

std::vector<Display> list_active_displays();
Display get_primary_display();

