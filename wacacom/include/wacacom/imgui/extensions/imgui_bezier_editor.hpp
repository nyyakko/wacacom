// ImGui Bezier widget. @r-lyeh, public domain
//
// [ref] http://robnapier.net/faster-bezier
// [ref] http://easings.net/es#easeInSine

#pragma once

#include "wacacom/imgui/imgui.h"

#include <array>
#include <string_view>

namespace ImGui {

bool BezierEditor(std::string_view label, ImVec2 const& dimensions, std::array<float, 4>& points);

}

