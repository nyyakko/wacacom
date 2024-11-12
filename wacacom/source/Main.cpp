#include "Tablet.hpp"
#include "Display.hpp"
#include "Math.hpp"

#include <fmt/format.h>
#include <GLFW/glfw3.h>
#include <fplus/fplus.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_internal.h"

#include "imgui/extensions/imgui_bezier_editor.hpp"

#include <span>

static auto constexpr GRAB_RADIUS = 6;
static auto constexpr GRAB_BORDER = 2;

static void draw_background_grid(ImDrawList* const drawList, ImVec2 const& dimensions, ImRect const& mappableRegion)
{
    static ImColor const COLOR = ImGui::GetColorU32(ImGuiCol_TextDisabled);

    for (auto i = 0; i <= static_cast<int>(dimensions.x); i += static_cast<int>(dimensions.x / 16))
    {
        drawList->AddLine(
            { mappableRegion.Min.x + static_cast<float>(i), mappableRegion.Min.y },
            { mappableRegion.Min.x + static_cast<float>(i), mappableRegion.Max.y },
            COLOR
        );
    }

    for (auto i = 0; i <= static_cast<int>(dimensions.y); i += static_cast<int>(dimensions.y / 9))
    {
        drawList->AddLine(
            { mappableRegion.Min.x, mappableRegion.Min.y + static_cast<float>(i) },
            { mappableRegion.Max.x, mappableRegion.Min.y + static_cast<float>(i) },
            COLOR
        );
    }
}

static bool draw_anchor_grabbers(std::string_view label, ImDrawList* const drawList, ImVec2 points[4], ImVec2 const& dimensions, ImVec2 const& rootCursorPosition, ImVec2 positionOut[4] = nullptr, bool const& forceProportions = false)
{
    // TODO: implement force proportions
    (void)forceProportions;

    static ImColor const BORDER_COLOR = ImGui::GetStyle().Colors[ImGuiCol_Text];
    static ImColor const COLOR = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];

    bool changed = false;

    static std::array<std::array<size_t, 2>, 4> constexpr ANCHOR_PAIRS {{
        { 1, 2 }, // BOTTOM_LEFT, TOP_RIGHT
        { 0, 3 }, // TOP_LEFT, BOTTOM_RIGHT
        { 3, 0 }, // BOTTOM_RIGHT, TOP_LEFT
        { 2, 1 }  // TOP_RIGHT, BOTTOM_LEFT
    }};

    for (auto i = 0zu; i < 4; i += 1)
    {
        ImVec2 const position {
            lmap<float>(points[i].x, 0.f, 1.f, 0.f, dimensions.x) + rootCursorPosition.x,
            lmap<float>(i % 2 == 0 ? 1 - points[i].y : points[i].y, 0.f, 1.f, 0.f, dimensions.y) + rootCursorPosition.y
        };

        if (positionOut != nullptr) positionOut[i] = position;

        ImGui::SetCursorScreenPos(position - ImVec2(GRAB_RADIUS, GRAB_RADIUS));
        ImGui::SetCursorPos(position - ImVec2(GRAB_RADIUS, GRAB_RADIUS));
        ImGui::InvisibleButton(fmt::format("##{}{}", label, i).data(), ImVec2(2 * GRAB_RADIUS, 2 * GRAB_RADIUS));
        drawList->AddCircleFilled(position, GRAB_RADIUS, ImColor(BORDER_COLOR));
        drawList->AddCircleFilled(position, GRAB_RADIUS - GRAB_BORDER, ImColor(COLOR));
        if (ImGui::IsItemActive() || ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            auto const posX = ImGui::GetIO().MouseDelta.x / dimensions.x;
            points[i].x += posX;
            points[i].x = points[i].x < 0.f ? 0.f : points[i].x > 1.f ? 1.f : points[i].x;
            points[ANCHOR_PAIRS[i][0]].x += posX; // horizontalPair
            points[ANCHOR_PAIRS[i][0]].x = points[ANCHOR_PAIRS[i][0]].x < 0.f ? 0.f : points[ANCHOR_PAIRS[i][0]].x > 1.f ? 1.f : points[ANCHOR_PAIRS[i][0]].x; // horizontalPair
            auto const posY = ImGui::GetIO().MouseDelta.y / dimensions.y;
            points[i].y = points[i].y + (i % 2 == 0 ? -1 * posY : +1 * posY);
            points[i].y = points[i].y < 0.f ? 0.f : points[i].y > 1.f ? 1.f : points[i].y;
            points[ANCHOR_PAIRS[i][1]].y = points[i].y + (i % 2 == 0 ? -1 * posY : +1 * posY); // verticalPair
            points[ANCHOR_PAIRS[i][1]].y = points[ANCHOR_PAIRS[i][1]].y < 0.f ? 0.f : points[ANCHOR_PAIRS[i][1]].y > 1.f ? 1.f : points[ANCHOR_PAIRS[i][1]].y; // verticalPair
            changed = true;
        }
    }

    return changed;
}

static ImRect calculate_mapped_region(ImVec2 points[4], ImVec2 const& regionMapperSize, ImVec2 const& rootCursorPosition)
{
    return {
        {
            lmap<float>(points[0].x, 0.f, 1.f, 0.f, regionMapperSize.x) + rootCursorPosition.x,
            lmap<float>(1 - points[0].y, 0.f, 1.f, 0.f, regionMapperSize.y) + rootCursorPosition.y
        },
        {
            lmap<float>(points[3].x, 0.f, 1.f, 0.f, regionMapperSize.x) + rootCursorPosition.x,
            lmap<float>(points[3].y, 0.f, 1.f, 0.f, regionMapperSize.y) + rootCursorPosition.y
        }
    };
}

static void draw_mapped_region(ImRect const& mappedRegion)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyle().Colors[ImGuiCol_FrameBgActive]);
            ImGui::RenderFrame(mappedRegion.Min, mappedRegion.Max, ImGui::GetColorU32(ImGuiCol_Button));
            {
                auto const width = std::round(mappedRegion.Max.x - mappedRegion.Min.x);
                auto const height = std::round(mappedRegion.Max.y - mappedRegion.Min.y);
                auto const mappedRegionInfo = fmt::format("{}x{}", width, height);
                auto const currentCursorPosition = ImGui::GetCursorPos();
                ImGui::SetCursorPos(mappedRegion.Max - ImGui::CalcTextSize(mappedRegionInfo.data()));
                ImGui::Text("%s", mappedRegionInfo.data());
                ImGui::SetCursorPos(currentCursorPosition);
            }
        ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

static bool draw_monitor_region_mapper(ImGuiWindow* const window, ImDrawList* const drawList, Region& mappedAreaOut, ImVec2 positionOut[4] = nullptr)
{
    auto changed = false;

    ImVec2 const DIMENSIONS(20.f * 16, 20.f * 9);
    static auto constexpr LABEL = "Display";

    auto const availableRegionX = ImGui::GetContentRegionAvail().x;
    auto static const display = get_primary_display();
    ImGui::SetCursorPosX((availableRegionX - DIMENSIONS.x) / 2);
    ImGui::Text("Display (%s)", fmt::format("{} {}x{}", display.name, display.width, display.height).data());
    ImGui::SetCursorPosX((availableRegionX - DIMENSIONS.x) / 2);

    auto const rootCursorPosition = window->DC.CursorPos;
    ImRect const mappableRegion(rootCursorPosition, rootCursorPosition + DIMENSIONS);
    ImGui::ItemSize(mappableRegion);
    if (!ImGui::ItemAdd(mappableRegion, 0)) return changed;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyle().Colors[ImGuiCol_Border]);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_Text]);
                ImGui::RenderFrame(mappableRegion.Min, mappableRegion.Max, ImGui::GetColorU32(ImGuiCol_FrameBg));
                {
                    draw_background_grid(drawList, DIMENSIONS, mappableRegion);
                }
            ImGui::PopStyleColor();
        ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    auto const currentCursorPosition = ImGui::GetCursorPos();

    ImVec2 const mappedArea {
        lmap<float>(static_cast<float>(display.width), 0, static_cast<float>(display.width), 0, DIMENSIONS.x),
        lmap<float>(static_cast<float>(display.height), 0, static_cast<float>(display.height), 0, DIMENSIONS.y)
    };

    // normalized
    static ImVec2 mappedAreaPoints[4] {
        { normalize(0.f, 0.f, DIMENSIONS.x), normalize(DIMENSIONS.y, 0.f, DIMENSIONS.y) },
        { normalize(0.f, 0.f, DIMENSIONS.x), normalize(mappedArea.y, 0.f, DIMENSIONS.y) },
        { normalize(mappedArea.x, 0.f, DIMENSIONS.x), normalize(DIMENSIONS.y, 0.f, DIMENSIONS.y) },
        { normalize(mappedArea.x, 0.f, DIMENSIONS.x), normalize(mappedArea.y, 0.f, DIMENSIONS.y) },
    };

    changed = draw_anchor_grabbers(LABEL, drawList, mappedAreaPoints, DIMENSIONS, rootCursorPosition, positionOut);
    auto const mappedRegion = calculate_mapped_region(mappedAreaPoints, DIMENSIONS, rootCursorPosition);
    ImGui::SetCursorScreenPos(currentCursorPosition);
    draw_mapped_region(mappedRegion);

    auto const size = mappedRegion.Max - mappedRegion.Min;
    mappedAreaOut.width = lmap<int>(static_cast<int>(size.x), 0, static_cast<int>(DIMENSIONS.x), 0, static_cast<int>(display.width));
    mappedAreaOut.height = lmap<int>(static_cast<int>(size.y), 0, static_cast<int>(DIMENSIONS.y), 0, static_cast<int>(display.height));

    return changed;
}

static bool draw_tablet_region_mapper(ImGuiWindow* const window, ImDrawList* const drawList, Device const& device, Region& mappedAreaOut, bool forceProportions, bool fullArea, ImVec2 positionOut[4] = nullptr)
{
    bool changed = false;

    ImVec2 const DIMENSIONS(15.f * 16, 15.f * 9);
    static auto constexpr LABEL = "Tablet";

    auto const availableRegionX = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((availableRegionX - DIMENSIONS.x) / 2);
    ImGui::Text(LABEL);
    ImGui::SetCursorPosX((availableRegionX - DIMENSIONS.x) / 2);

    auto const rootCursorPosition = window->DC.CursorPos;
    ImRect const mappableRegion(rootCursorPosition, rootCursorPosition + DIMENSIONS);
    ImGui::ItemSize(mappableRegion);
    if (!ImGui::ItemAdd(mappableRegion, 0)) return changed;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyle().Colors[ImGuiCol_Border]);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
                ImGui::RenderFrame(mappableRegion.Min, mappableRegion.Max, ImGui::GetColorU32(ImGuiCol_FrameBg));
                {
                    draw_background_grid(drawList, DIMENSIONS, mappableRegion);
                }
            ImGui::PopStyleColor();
        ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    auto currentCursorPosition = ImGui::GetCursorPos();

    static auto const availableArea = get_device_entire_area(device);
    static auto currentArea = get_device_area(device);

    ImVec2 const mappedArea {
        lmap<float>(static_cast<float>(currentArea.width), 0, static_cast<float>(availableArea.width), 0, DIMENSIONS.x),
        lmap<float>(static_cast<float>(currentArea.height), 0, static_cast<float>(availableArea.height), 0, DIMENSIONS.y)
    };

    ImVec2 const mappedAreaOffset {
        lmap<float>(static_cast<float>(currentArea.offsetX), 0, static_cast<float>(availableArea.width), 0, DIMENSIONS.x),
        lmap<float>(static_cast<float>(currentArea.offsetY), 0, static_cast<float>(availableArea.height), 0, DIMENSIONS.y)
    };

    // normalized
    static ImVec2 mappedAreaPoints[4] {
        { normalize(0.f + mappedAreaOffset.x, 0.f, DIMENSIONS.x), normalize(DIMENSIONS.y - mappedAreaOffset.y, 0.f, DIMENSIONS.y) },
        { normalize(0.f + mappedAreaOffset.x, 0.f, DIMENSIONS.x), normalize(mappedArea.y + mappedAreaOffset.y, 0.f, DIMENSIONS.y) },
        { normalize(mappedArea.x + mappedAreaOffset.x, 0.f, DIMENSIONS.x), normalize(DIMENSIONS.y - mappedAreaOffset.y, 0.f, DIMENSIONS.y) },
        { normalize(mappedArea.x + mappedAreaOffset.x, 0.f, DIMENSIONS.x), normalize(mappedArea.y + mappedAreaOffset.y, 0.f, DIMENSIONS.y) },
    };

    if (fullArea)
    {
        currentArea = availableArea;
        mappedAreaPoints[0] = { normalize(0.f, 0.f, DIMENSIONS.x), normalize(DIMENSIONS.y, 0.f, DIMENSIONS.y) };
        mappedAreaPoints[1] = { normalize(0.f, 0.f, DIMENSIONS.x), normalize(mappedArea.y, 0.f, DIMENSIONS.y) };
        mappedAreaPoints[2] = { normalize(mappedArea.x, 0.f, DIMENSIONS.x), normalize(DIMENSIONS.y, 0.f, DIMENSIONS.y) };
        mappedAreaPoints[3] = { normalize(mappedArea.x, 0.f, DIMENSIONS.x), normalize(mappedArea.y, 0.f, DIMENSIONS.y) };
    }

    changed = draw_anchor_grabbers(LABEL, drawList, mappedAreaPoints, DIMENSIONS, rootCursorPosition, positionOut, forceProportions);
    if (changed) fullArea = false;
    auto const mappedRegion = calculate_mapped_region(mappedAreaPoints, DIMENSIONS, rootCursorPosition);
    ImGui::SetCursorScreenPos(currentCursorPosition);
    draw_mapped_region(mappedRegion);

    auto const size = mappedRegion.Max - mappedRegion.Min;

    mappedAreaOut.width   = lmap<int>(static_cast<int>(size.x), 0, static_cast<int>(DIMENSIONS.x), 0, static_cast<int>(availableArea.width));
    mappedAreaOut.height  = lmap<int>(static_cast<int>(size.y), 0, static_cast<int>(DIMENSIONS.y), 0, static_cast<int>(availableArea.height));
    auto const offsetX    = (mappedAreaPoints[0].x > 0.f ? 1 : 0) * static_cast<int>(DIMENSIONS.x - size.x);
    mappedAreaOut.offsetX = lmap<int>(static_cast<int>(offsetX), 0, static_cast<int>(DIMENSIONS.x), 0, static_cast<int>(availableArea.width));
    auto const offsetY    = (mappedAreaPoints[2].y < 1.f ? 1 : 0) * static_cast<int>(DIMENSIONS.y - size.y);
    mappedAreaOut.offsetY = lmap<int>(static_cast<int>(offsetY), 0, static_cast<int>(DIMENSIONS.y), 0, static_cast<int>(availableArea.height));

    return changed;
}

bool TabletRegionMapper(Device const& device, Region& mappedTabletAreaOut)
{
    auto changed = false;

    auto* window = ImGui::GetCurrentWindow();
    auto* drawList = ImGui::GetWindowDrawList();

    static bool forceProportions = true;
    static bool fullArea = false;

    static Region mappedMonitorArea {};
    static ImVec2 mappedMonitorAreaPosition[4] {};
    draw_monitor_region_mapper(window, drawList, mappedMonitorArea, mappedMonitorAreaPosition);

    static ImVec2 mappedTabletAreaPosition[4] {};
    draw_tablet_region_mapper(window, drawList, device, mappedTabletAreaOut, forceProportions, fullArea, mappedTabletAreaPosition);

    for (auto const& [tabletArea, monitorArea] : fplus::zip(std::span<ImVec2>(mappedMonitorAreaPosition, 4), std::span<ImVec2>(mappedTabletAreaPosition, 4)))
    {
        drawList->AddLine(tabletArea, monitorArea, ImColor(1.f, 0.f, 0.f, 0.5f), 2.f);
    }

    // ImGui::Checkbox("Force Proportions", &forceProportions);
    // ImGui::SameLine();
    ImGui::Checkbox("Full Area", &fullArea);

    return changed;
}

void main_window()
{
    ImGui::Begin("Wacacom", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

    static int selectedDevice = 0;
    static auto devices = get_drawing_devices();

    static auto deviceNames = fplus::transform([] (Device const& device) { return device.name; }, devices);
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 308);
    ImGui::SetNextItemWidth(300);
    auto const changedDevice = ImGui::Combo(
        "##Devices",
        &selectedDevice,
        fplus::transform([] (std::string const& data) { return data.data(); }, deviceNames).data(),
        static_cast<int>(deviceNames.size())
    );

    static auto mappedTabletArea = get_device_entire_area(devices.at(static_cast<size_t>(selectedDevice)));
    TabletRegionMapper(devices.at(static_cast<size_t>(selectedDevice)), mappedTabletArea);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);

    static auto pressureCurve = get_device_pressure_curve(devices.at(static_cast<size_t>(selectedDevice)));
    if (changedDevice) pressureCurve = get_device_pressure_curve(devices.at(static_cast<size_t>(selectedDevice)));
    static std::vector pressureCurvePoints { pressureCurve.minX, pressureCurve.minY, pressureCurve.maxX, pressureCurve.maxY };
    ImGui::BezierEditor("Pressure Curve", { 250, 250 }, pressureCurvePoints.data());

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - (35 + 8));
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - (200 + 8));
    if (ImGui::Button("Apply", { 200, 35 }))
    {
        auto const& device = devices.at(static_cast<size_t>(selectedDevice));
        set_device_area(device, mappedTabletArea);
        set_device_pressure_curve(device, {
            pressureCurvePoints.at(0),
            pressureCurvePoints.at(1),
            pressureCurvePoints.at(2),
            pressureCurvePoints.at(3)
        });
    }

    ImGui::End();
}

int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    auto* window = glfwCreateWindow(1000, 900, "Wacacom", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    int displayWidth {};
    int displayHeight {};
    glfwGetFramebufferSize(window, &displayWidth, &displayHeight);

    auto io = ImGui::GetIO();
    auto* font = io.Fonts->AddFontFromFileTTF(HOME"/resources/fonts/iosevka.ttf", 20.f, nullptr, io.Fonts->GetGlyphRangesDefault());

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushFont(font);
        main_window();
        ImGui::PopFont();
        ImGui::PopStyleVar(1);
        ImGui::Render();

        glfwGetFramebufferSize(window, &displayWidth, &displayHeight);
        glViewport(0, 0, displayWidth, displayHeight);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}

