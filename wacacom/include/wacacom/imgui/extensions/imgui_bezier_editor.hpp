// ImGui Bezier widget. @r-lyeh, public domain
// v1.02: add BezierValue(); comments; usage
// v1.01: out-of-bounds coord snapping; custom border width; spacing; cosmetics
// v1.00: initial version
//
// [ref] http://robnapier.net/faster-bezier
// [ref] http://easings.net/es#easeInSine

#pragma once

#include "wacacom/imgui/imgui.h"

#include <string_view>

namespace ImGui {

bool BezierEditor(std::string_view label, ImVec2 const& dimensions, float points[4]);

}

