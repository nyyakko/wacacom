add_subdirectory(extensions)

set(DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(wacacom_SourceFiles ${wacacom_SourceFiles}
    "${DIR}/imgui_impl_opengl3.cpp"
    "${DIR}/imgui_impl_glfw.cpp"
    "${DIR}/imgui_widgets.cpp"
    "${DIR}/imgui_tables.cpp"
    "${DIR}/imgui_draw.cpp"
    "${DIR}/imgui.cpp"

    PARENT_SCOPE
)

