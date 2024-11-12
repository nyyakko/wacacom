add_subdirectory(imgui)

set(DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(wacacom_SourceFiles ${wacacom_SourceFiles}
    "${DIR}/Main.cpp"
    "${DIR}/Display.cpp"
    "${DIR}/Tablet.cpp"

    PARENT_SCOPE
)

