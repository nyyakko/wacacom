#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/extensions/imgui_bezier_editor.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <fmt/format.h>

#include <array>
#include <vector>

static auto constexpr SMOOTHNESS = 64;
static auto constexpr CURVE_WIDTH = 4;
static auto constexpr LINE_WIDTH = 1;
static auto constexpr GRAB_RADIUS = 6;
static auto constexpr GRAB_BORDER = 2;

static std::vector<ImVec2> bezier_table(std::array<ImVec2, 4> const& points, int steps)
{
    std::vector<ImVec2> result {};

    for (auto step = 0; step <= steps; ++step)
    {
        auto const t = static_cast<float>(step)/static_cast<float>(steps);

        auto const pointA = (1-t)*(1-t)*(1-t);
        auto const pointB = 3 * (1-t)*(1-t) * t;
        auto const pointC = 3 * (1-t) * t*t;
        auto const pointD = t*t*t;

        result.push_back({
            pointA*points[0].x + pointB*points[1].x + pointC*points[2].x + pointD*points[3].x,
            pointA*points[0].y + pointB*points[1].y + pointC*points[2].y + pointD*points[3].y
        });
    }

    return result;
}

static void draw_background_grid(ImDrawList* const drawList, ImVec2 const& dimensions, ImRect const& editorFrame)
{
    static ImColor const COLOR = ImGui::GetColorU32(ImGuiCol_TextDisabled);

    for (auto i = 0; i <= static_cast<int>(dimensions.x); i += static_cast<int>(dimensions.x / 4))
    {
        drawList->AddLine(
            { editorFrame.Min.x + static_cast<float>(i), editorFrame.Min.y },
            { editorFrame.Min.x + static_cast<float>(i), editorFrame.Max.y },
            COLOR
        );
    }

    for (auto i = 0; i <= static_cast<int>(dimensions.y); i += static_cast<int>(dimensions.y / 4))
    {
        drawList->AddLine(
            { editorFrame.Min.x, editorFrame.Min.y + static_cast<float>(i) },
            { editorFrame.Max.x, editorFrame.Min.y + static_cast<float>(i) },
            COLOR
        );
    }
}

static void draw_bezier_curves(ImDrawList* const drawList, float points[4], ImRect const& editorFrame)
{
    static ImColor const COLOR = ImGui::GetStyle().Colors[ImGuiCol_PlotLines];

    auto const bezier = bezier_table({{{ 0, 0 }, { points[0], points[1] }, { points[2], points[3] }, { 1, 1 }}}, SMOOTHNESS);
    for (auto i = 0zu; i < SMOOTHNESS; i += 1)
    {
        ImVec2 const p(bezier[i + 0].x, 1 - bezier[i + 0].y);
        ImVec2 const q(bezier[i + 1].x, 1 - bezier[i + 1].y);
        ImVec2 const r(p.x * (editorFrame.Max.x - editorFrame.Min.x) + editorFrame.Min.x, p.y * (editorFrame.Max.y - editorFrame.Min.y) + editorFrame.Min.y);
        ImVec2 const s(q.x * (editorFrame.Max.x - editorFrame.Min.x) + editorFrame.Min.x, q.y * (editorFrame.Max.y - editorFrame.Min.y) + editorFrame.Min.y);
        drawList->AddLine(r, s, COLOR, CURVE_WIDTH);
    }
}

static bool draw_anchor_grabbers(ImDrawList* const drawList, std::string_view label, float points[4], ImRect const& editorFrame, ImVec2 const& dimensions)
{
    static ImColor const BORDER_COLOR = ImGui::GetStyle().Colors[ImGuiCol_Text];
    static ImColor const COLOR = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];

    auto changed = false;

    static auto const tooltip = fmt::format("##{}", label);
    for (auto i = 0; i < 2; ++i)
    {
        auto const pos = ImVec2(points[i * 2 + 0], 1 - points[i * 2 + 1]) * (editorFrame.Max - editorFrame.Min) + editorFrame.Min;

        ImGui::SetCursorScreenPos(pos - ImVec2(GRAB_RADIUS, GRAB_RADIUS));
        ImGui::InvisibleButton(fmt::format("{}{}", i, tooltip.data()).data(), ImVec2(2 * GRAB_RADIUS, 2 * GRAB_RADIUS));

        if (ImGui::IsItemActive() || ImGui::IsItemHovered())
        {
            auto const x = static_cast<double>(points[i * 2 + 0]);
            auto const y = static_cast<double>(points[i * 2 + 1]);
            ImGui::SetTooltip("(%4.3f, %4.3f)", x, y);
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            auto const posX = ImGui::GetIO().MouseDelta.x / dimensions.x;
            points[i * 2 + 0] += posX;
            points[i * 2 + 0] = points[i * 2 + 0] < 0.f ? 0.f : points[i * 2 + 0] > 1.f ? 1.f : points[i * 2 + 0];
            auto const posY = ImGui::GetIO().MouseDelta.y / dimensions.y;
            points[i * 2 + 1] -= posY;
            points[i * 2 + 1] = points[i * 2 + 1] < 0.f ? 0.f : points[i * 2 + 1] > 1.f ? 1.f : points[i * 2 + 1];
            changed = true;
        }
    }

    auto const p1 = ImVec2(points[0], 1 - points[1]) * (editorFrame.Max - editorFrame.Min) + editorFrame.Min;
    auto const p2 = ImVec2(points[2], 1 - points[3]) * (editorFrame.Max - editorFrame.Min) + editorFrame.Min;

    drawList->AddLine(ImVec2(editorFrame.Min.x, editorFrame.Max.y), p1, ImColor(BORDER_COLOR), LINE_WIDTH);
    drawList->AddLine(ImVec2(editorFrame.Max.x, editorFrame.Min.y), p2, ImColor(BORDER_COLOR), LINE_WIDTH);

    drawList->AddCircleFilled(p1, GRAB_RADIUS, ImColor(BORDER_COLOR));
    drawList->AddCircleFilled(p1, GRAB_RADIUS - GRAB_BORDER, ImColor(COLOR));
    drawList->AddCircleFilled(p2, GRAB_RADIUS, ImColor(BORDER_COLOR));
    drawList->AddCircleFilled(p2, GRAB_RADIUS - GRAB_BORDER, ImColor(COLOR));

    ImGui::SetCursorScreenPos(ImVec2(editorFrame.Min.x, editorFrame.Max.y + GRAB_RADIUS));

    return changed;
}

bool ImGui::BezierEditor(std::string_view label, ImVec2 const& dimensions, float points[4])
{
    auto const& style = GetStyle();
    auto* drawList = GetWindowDrawList();
    auto* window = GetCurrentWindow();

    if (window->SkipItems) return false;

    ImGui::BeginGroup();

    ImGui::Text("%s", label.data());
    static auto changed = false;
    auto hovered = IsItemActive() || IsItemHovered();
    // Dummy(ImVec2(0, 3));

    ImRect const editorFrame(window->DC.CursorPos, window->DC.CursorPos + dimensions);
    ItemSize(editorFrame);
    if (!ItemAdd(editorFrame, 0))
    {
        ImGui::EndGroup();
        return changed;
    }

    hovered |= IsItemHovered();

    RenderFrame(editorFrame.Min, editorFrame.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

    draw_background_grid(drawList, dimensions, editorFrame);
    if (hovered || changed) drawList->PushClipRectFullScreen();
    draw_bezier_curves(drawList, points, editorFrame);
    if (hovered || changed) drawList->PopClipRect();
    draw_anchor_grabbers(drawList, label, points, editorFrame, dimensions);

    ImGui::PushItemWidth(dimensions.x / 2 - 3);
        ImGui::BeginGroup();
            changed = ImGui::SliderFloat(fmt::format("##0{}", label.data()).data(), &points[0], 0, 1);
            changed = ImGui::SliderFloat(fmt::format("##1{}", label.data()).data(), &points[1], 0, 1);
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
            changed = ImGui::SliderFloat(fmt::format("##2{}", label.data()).data(), &points[2], 0, 1);
            changed = ImGui::SliderFloat(fmt::format("##3{}", label.data()).data(), &points[3], 0, 1);
        ImGui::EndGroup();
    ImGui::PopItemWidth();

    ImGui::EndGroup();

    return changed;
}

