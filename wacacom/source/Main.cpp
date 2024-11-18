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

#define H_SPACING(COUNT) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + COUNT);

#define V_SPACING(COUNT) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + COUNT);

#define SEPARATOR(COUNT) \
    H_SPACING(COUNT);    \
    ImGui::Separator();  \
    H_SPACING(COUNT);

static auto constexpr INPUT_WIDGET_WIDTH = 150;
static auto constexpr GRAB_RADIUS = 6;
static auto constexpr GRAB_BORDER = 2;

static void draw_background_grid(ImDrawList* const drawList, ImVec2 const& dimensions, ImRect const& mappableRegion)
{
    static auto const COLOR = ImGui::GetColorU32(ImGuiCol_TextDisabled);

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

static bool draw_anchor_grabbers(std::string_view label, ImDrawList* const drawList, ImVec2 points[4], ImVec2 const& dimensions, ImVec2 const& rootCursorPosition, ImVec2 positionOut[4] = nullptr, [[maybe_unused]] bool const& forceProportions = false, bool enabled = true)
{
    static ImColor const BORDER_COLOR = ImGui::GetStyle().Colors[ImGuiCol_Text];
    static ImColor const COLOR_ENABLED = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
    static ImColor const COLOR_DISABLED = ImGui::GetStyle().Colors[ImGuiCol_Separator];

    bool changed = false;

    static std::array<std::array<size_t, 2>, 4> constexpr ANCHOR_PAIRS {{
        { 1, 2 }, // BOTTOM_LEFT, TOP_RIGHT
        { 0, 3 }, // TOP_LEFT, BOTTOM_RIGHT
        { 3, 0 }, // BOTTOM_RIGHT, TOP_LEFT
        { 2, 1 }  // TOP_RIGHT, BOTTOM_LEFT
    }};

    for (auto i = 0zu; i < 4; i += 1)
    {
        ImVec2 const position (
            lmap<float>(points[i].x, 0.f, 1.f, 0.f, dimensions.x) + rootCursorPosition.x,
            lmap<float>(i % 2 == 0 ? 1 - points[i].y : points[i].y, 0.f, 1.f, 0.f, dimensions.y) + rootCursorPosition.y
        );

        if (positionOut != nullptr) positionOut[i] = position;

        ImGui::SetCursorScreenPos(position - ImVec2(GRAB_RADIUS, GRAB_RADIUS));
        ImGui::SetCursorPos(position - ImVec2(GRAB_RADIUS, GRAB_RADIUS));
        ImGui::InvisibleButton(fmt::format("##{}{}", label, i).data(), ImVec2(2 * GRAB_RADIUS, 2 * GRAB_RADIUS));
        drawList->AddCircleFilled(position, GRAB_RADIUS, ImColor(BORDER_COLOR));
        drawList->AddCircleFilled(position, GRAB_RADIUS - GRAB_BORDER, COLOR_ENABLED);

        if (enabled && (ImGui::IsItemActive() || ImGui::IsItemHovered())) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        if (enabled && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            auto const posX = ImGui::GetIO().MouseDelta.x / dimensions.x;
            points[i].x += posX;
            points[i].x = points[i].x < 0.f ? 0.f : points[i].x > 1.f ? 1.f : points[i].x;
            auto const& pairX = ANCHOR_PAIRS[i][0];
            points[pairX].x += posX; // horizontalPair
            points[pairX].x = points[pairX].x < 0.f ? 0.f : points[pairX].x > 1.f ? 1.f : points[pairX].x; // horizontalPair

            auto const posY = ImGui::GetIO().MouseDelta.y / dimensions.y;
            points[i].y = points[i].y + (i % 2 == 0 ? -1 * posY : +1 * posY);
            points[i].y = points[i].y < 0.f ? 0.f : points[i].y > 1.f ? 1.f : points[i].y;
            auto const& pairY = ANCHOR_PAIRS[i][1];
            points[pairY].y = points[i].y + (i % 2 == 0 ? -1 * posY : +1 * posY); // verticalPair
            points[pairY].y = points[pairY].y < 0.f ? 0.f : points[pairY].y > 1.f ? 1.f : points[pairY].y; // verticalPair

            changed = true;
        }
    }

    return changed;
}

static ImRect mapped_points_to_rect(ImVec2 points[4], ImVec2 const& mapperSize, ImVec2 const& cursorPosition)
{
    ImVec2 const minArea (
        lmap(points[0].x, 0.f, 1.f, 0.f, mapperSize.x) + cursorPosition.x,
        lmap(1 - points[0].y, 0.f, 1.f, 0.f, mapperSize.y) + cursorPosition.y
    );

    ImVec2 const maxArea (
        lmap(points[3].x, 0.f, 1.f, 0.f, mapperSize.x) + cursorPosition.x,
        lmap(points[3].y, 0.f, 1.f, 0.f, mapperSize.y) + cursorPosition.y
    );

    return { minArea, maxArea };
}

bool MonitorRegionMapper(std::string_view label, ImVec2 const& dimensions, Display const& display, Region& mappedAreaOut, ImVec2 positionOut[4])
{
    auto changed = false;

    auto* window = ImGui::GetCurrentWindow();
    auto* drawList = ImGui::GetWindowDrawList();

    auto const currentPosition = ImGui::GetCursorPos();
    ImGui::BeginGroup();
    ImGui::Text("%s", label.data());
    ImGui::SetCursorPosX(currentPosition.x);
    auto const rootCursorPosition = window->DC.CursorPos;
    ImRect const mappableRegion(rootCursorPosition, rootCursorPosition + dimensions);
    ImGui::ItemSize(mappableRegion);
    if (!ImGui::ItemAdd(mappableRegion, 0)) return changed;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyle().Colors[ImGuiCol_Border]);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_Text]);
                ImGui::RenderFrame(mappableRegion.Min, mappableRegion.Max, ImGui::GetColorU32(ImGuiCol_FrameBg));
                {
                    draw_background_grid(drawList, dimensions, mappableRegion);
                }
            ImGui::PopStyleColor();
        ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    auto currentCursorPosition = ImGui::GetCursorPos();

    ImVec2 const mappedArea (
        lmap(static_cast<float>(display.width), 0.f, static_cast<float>(display.width), 0.f, dimensions.x),
        lmap(static_cast<float>(display.height), 0.f, static_cast<float>(display.height), 0.f, dimensions.y)
    );

    static ImVec2 mappedAreaPoints[4] {
        { normalize(0.f, 0.f, dimensions.x), normalize(dimensions.y, 0.f, dimensions.y) },
        { normalize(0.f, 0.f, dimensions.x), normalize(mappedArea.y, 0.f, dimensions.y) },
        { normalize(mappedArea.x, 0.f, dimensions.x), normalize(dimensions.y, 0.f, dimensions.y) },
        { normalize(mappedArea.x, 0.f, dimensions.x), normalize(mappedArea.y, 0.f, dimensions.y) },
    };

    changed = draw_anchor_grabbers(label, drawList, mappedAreaPoints, dimensions, rootCursorPosition, positionOut, false, false);
    auto const mappedRegion = mapped_points_to_rect(mappedAreaPoints, dimensions, rootCursorPosition);

    ImGui::SetCursorScreenPos(currentCursorPosition);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyle().Colors[ImGuiCol_FrameBgActive]);
            ImGui::RenderFrame(mappedRegion.Min, mappedRegion.Max, ImGui::GetColorU32(ImGuiCol_Button));
            {
                auto const text = fmt::format("{}x{}", std::round(mappedRegion.Max.x - mappedRegion.Min.x), std::round(mappedRegion.Max.y - mappedRegion.Min.y));
                currentCursorPosition = ImGui::GetCursorPos();
                ImGui::SetCursorPos(mappedRegion.Max - ImGui::CalcTextSize(text.data()));
                ImGui::Text("%s", text.data());
                ImGui::SetCursorPos(currentCursorPosition);
            }
        ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    auto const size = mappedRegion.Max - mappedRegion.Min;
    mappedAreaOut.width = lmap<int>(static_cast<int>(size.x), 0, static_cast<int>(dimensions.x), 0, static_cast<int>(display.width));
    mappedAreaOut.height = lmap<int>(static_cast<int>(size.y), 0, static_cast<int>(dimensions.y), 0, static_cast<int>(display.height));
    ImGui::EndGroup();

    return changed;
}

bool TabletRegionMapper(std::string_view label, ImVec2 const& dimensions, Device const& device, Region& mappedAreaOut, ImVec2 positionOut[4], bool& forceProportions, bool& fullArea)
{
    auto changed = false;

    auto* window = ImGui::GetCurrentWindow();
    auto* drawList = ImGui::GetWindowDrawList();

    auto currentPosition = ImGui::GetCursorPos();
    ImGui::BeginGroup();
    ImGui::Text("%s", label.data());
    ImGui::SetCursorPosX(currentPosition.x);
    auto const rootCursorPosition = window->DC.CursorPos;
    ImRect const mappableRegion(rootCursorPosition, rootCursorPosition + dimensions);
    ImGui::ItemSize(mappableRegion);
    if (!ImGui::ItemAdd(mappableRegion, 0)) return changed;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyle().Colors[ImGuiCol_Border]);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
                ImGui::RenderFrame(mappableRegion.Min, mappableRegion.Max, ImGui::GetColorU32(ImGuiCol_FrameBg));
                {
                    draw_background_grid(drawList, dimensions, mappableRegion);
                }
            ImGui::PopStyleColor();
        ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    auto currentCursorPosition = ImGui::GetCursorPos();

    static auto const availableArea = get_device_entire_area(device);
    static auto currentArea = get_device_area(device);

    ImVec2 const mappedArea (
        lmap(static_cast<float>(currentArea.width), 0.f, static_cast<float>(availableArea.width), 0.f, dimensions.x),
        lmap(static_cast<float>(currentArea.height), 0.f, static_cast<float>(availableArea.height), 0.f, dimensions.y)
    );

    ImVec2 const mappedAreaOffset (
        lmap(static_cast<float>(currentArea.offsetX), 0.f, static_cast<float>(availableArea.width), 0.f, dimensions.x),
        lmap(static_cast<float>(currentArea.offsetY), 0.f, static_cast<float>(availableArea.height), 0.f, dimensions.y)
    );

    // normalized
    static ImVec2 mappedAreaPoints[4] {
        { normalize(0.f + mappedAreaOffset.x, 0.f, dimensions.x), normalize(dimensions.y - mappedAreaOffset.y, 0.f, dimensions.y) },
        { normalize(0.f + mappedAreaOffset.x, 0.f, dimensions.x), normalize(mappedArea.y + mappedAreaOffset.y, 0.f, dimensions.y) },
        { normalize(mappedArea.x + mappedAreaOffset.x, 0.f, dimensions.x), normalize(dimensions.y - mappedAreaOffset.y, 0.f, dimensions.y) },
        { normalize(mappedArea.x + mappedAreaOffset.x, 0.f, dimensions.x), normalize(mappedArea.y + mappedAreaOffset.y, 0.f, dimensions.y) },
    };

    if (fullArea)
    {
        currentArea = availableArea;
        mappedAreaPoints[0] = { normalize(0.f, 0.f, dimensions.x), normalize(dimensions.y, 0.f, dimensions.y) };
        mappedAreaPoints[1] = { normalize(0.f, 0.f, dimensions.x), normalize(mappedArea.y, 0.f, dimensions.y) };
        mappedAreaPoints[2] = { normalize(mappedArea.x, 0.f, dimensions.x), normalize(dimensions.y, 0.f, dimensions.y) };
        mappedAreaPoints[3] = { normalize(mappedArea.x, 0.f, dimensions.x), normalize(mappedArea.y, 0.f, dimensions.y) };
    }

    changed = draw_anchor_grabbers(label, drawList, mappedAreaPoints, dimensions, rootCursorPosition, positionOut, forceProportions);
    if (changed) fullArea = false;
    auto const mappedRegion = mapped_points_to_rect(mappedAreaPoints, dimensions, rootCursorPosition);

    ImGui::SetCursorScreenPos(currentCursorPosition);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyle().Colors[ImGuiCol_FrameBgActive]);
            ImGui::RenderFrame(mappedRegion.Min, mappedRegion.Max, ImGui::GetColorU32(ImGuiCol_Button));
            {
                auto const text = fmt::format("{}x{}", std::round(mappedRegion.Max.x - mappedRegion.Min.x), std::round(mappedRegion.Max.y - mappedRegion.Min.y));
                currentCursorPosition = ImGui::GetCursorPos();
                ImGui::SetCursorPos(mappedRegion.Max - ImGui::CalcTextSize(text.data()));
                ImGui::Text("%s", text.data());
                ImGui::SetCursorPos(currentCursorPosition);
            }
        ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    auto const size = mappedRegion.Max - mappedRegion.Min;

    mappedAreaOut.width = lmap(static_cast<int>(size.x), 0, static_cast<int>(dimensions.x), 0, static_cast<int>(availableArea.width));
    mappedAreaOut.height = lmap(static_cast<int>(size.y), 0, static_cast<int>(dimensions.y), 0, static_cast<int>(availableArea.height));
    auto const offsetX = (mappedAreaPoints[0].x > 0.f ? 1 : 0) * static_cast<int>(dimensions.x - size.x);
    mappedAreaOut.offsetX = lmap(static_cast<int>(offsetX), 0, static_cast<int>(dimensions.x), 0, static_cast<int>(availableArea.width));
    auto const offsetY = (mappedAreaPoints[2].y < 1.f ? 1 : 0) * static_cast<int>(dimensions.y - size.y);
    mappedAreaOut.offsetY = lmap(static_cast<int>(offsetY), 0, static_cast<int>(dimensions.y), 0, static_cast<int>(availableArea.height));
    ImGui::EndGroup();

    return changed;
}

struct ApplicationContext
{
    Display display;
    Region mappedMonitorArea;
    ImVec2 mappedMonitorAreaPosition[4];

    std::vector<Device> devices;

    Device device;
    Region mappedTabletArea;
    ImVec2 mappedTabletAreaPosition[4];
    Pressure pressureCurve;
    std::array<float, 4> pressureCurvePoints;

    bool forceProportions = true;
    bool fullArea = false;
};

void update_device_settings(ApplicationContext& ctx)
{
    ctx.mappedTabletArea = get_device_entire_area(ctx.device);
    ctx.pressureCurve = get_device_pressure_curve(ctx.device);
    ctx.pressureCurvePoints = { ctx.pressureCurve.minX, ctx.pressureCurve.minY, ctx.pressureCurve.maxX, ctx.pressureCurve.maxY };
}

void main_window()
{
    ImGui::Begin("Wacacom", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

    auto* drawList = ImGui::GetWindowDrawList();

    static ApplicationContext ctx {};

    if (ctx.devices.empty()) ctx.devices = get_drawing_devices();
    if (ctx.display.name.empty()) ctx.display = get_primary_display();
    if (!ctx.devices.empty() && ctx.device.name.empty()) ctx.device = ctx.devices.front(), update_device_settings(ctx);

    static auto selectedDeviceIndex = 0;
    static auto hasChangedDevice = false;

    if ((hasChangedDevice || ctx.device.name.empty()) && !ctx.devices.empty()) update_device_settings(ctx);

    ImGui::BeginGroup();
        static ImVec2 constexpr MONITOR_MAPPER_DIM(20.f * 16, 20.f * 9);
        static auto MONITOR_MAPPER_LABEL = fmt::format("Display ({} {}x{})", ctx.display.name, ctx.display.width, ctx.display.height);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - MONITOR_MAPPER_DIM.x) / 2);
        MonitorRegionMapper(MONITOR_MAPPER_LABEL, MONITOR_MAPPER_DIM, ctx.display, ctx.mappedMonitorArea, ctx.mappedMonitorAreaPosition);
        ImVec2 const TABLET_MAPPER_DIM(15.f * 16, 15.f * 9);
        static auto constexpr TABLET_MAPPER_LABEL = "Tablet";
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - TABLET_MAPPER_DIM.x) / 2);
        TabletRegionMapper(TABLET_MAPPER_LABEL, TABLET_MAPPER_DIM, ctx.device, ctx.mappedTabletArea, ctx.mappedTabletAreaPosition, ctx.forceProportions, ctx.fullArea);
    ImGui::EndGroup();

    SEPARATOR(10);

    ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 8);
    ImGui::BeginGroup();
        ImGui::BeginGroup();
            ImGui::Text("Device");
            ImGui::SetNextItemWidth(8 + 300);
            hasChangedDevice = ImGui::Combo(
                "##Device",
                &selectedDeviceIndex,
                fplus::transform([] (auto const& data) { return data.data(); },
                    fplus::transform([] (auto const& device) { return device.name; }, ctx.devices)
                ).data(),
                static_cast<int>(ctx.devices.size())
            );
            ImGui::BeginGroup();
                ImGui::Text("Width");
                ImGui::SetNextItemWidth(INPUT_WIDGET_WIDTH);
                ImGui::InputInt("##TabletAreaWidth", &ctx.mappedTabletArea.width);
                ImGui::Text("Offset X");
                ImGui::SetNextItemWidth(INPUT_WIDGET_WIDTH);
                ImGui::InputInt("##TabletOffsetX", &ctx.mappedTabletArea.offsetX);
            ImGui::EndGroup();
            ImGui::SameLine();
            ImGui::BeginGroup();
                ImGui::Text("Height");
                ImGui::SetNextItemWidth(INPUT_WIDGET_WIDTH);
                ImGui::InputInt("##TabletAreaHeight", &ctx.mappedTabletArea.height);
                ImGui::Text("Offset Y");
                ImGui::SetNextItemWidth(INPUT_WIDGET_WIDTH);
                ImGui::InputInt("##TabletOffsetY", &ctx.mappedTabletArea.offsetY);
            ImGui::EndGroup();
            ImGui::BeginGroup();
                ImGui::Checkbox("Full Area", &ctx.fullArea);
                ImGui::Checkbox("Force Proportions", &ctx.fullArea);
            ImGui::EndGroup();
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BezierEditor("Pressure Curve", { 300, 300 }, ctx.pressureCurvePoints);
    ImGui::EndGroup();

    for (auto const& [tabletArea, monitorArea] :
            fplus::zip(std::span<ImVec2>(ctx.mappedMonitorAreaPosition, 4),
                       std::span<ImVec2>(ctx.mappedTabletAreaPosition, 4)))
    {
        drawList->AddLine(tabletArea, monitorArea, ImColor(1.f, 0.f, 0.f, 0.5f), 2.f);
    }

    ImGui::SetCursorPos({ ImGui::GetWindowWidth() - (ImGui::GetCursorPosX() + 200), ImGui::GetWindowHeight() - (ImGui::GetCursorPosX() + 35) });
    if (ImGui::Button("Apply", { 200, 35 }))
    {
        set_device_area(ctx.device, ctx.mappedTabletArea);
        set_device_pressure_curve(ctx.device, {
            ctx.pressureCurvePoints.at(0),
            ctx.pressureCurvePoints.at(1),
            ctx.pressureCurvePoints.at(2),
            ctx.pressureCurvePoints.at(3)
        });
    }

    ImGui::End();
}

int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    auto* window = glfwCreateWindow(800, 900, "Wacacom", nullptr, nullptr);
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

