cmake_minimum_required(VERSION 3.25)

project(wacacom LANGUAGES CXX)

include(cmake/static_analyzers.cmake)
include(cmake/get_cpm.cmake)
include(GNUInstallDirs)

# set(wacacom_CompilerOptions ${wacacom_CompilerOptions})
# set(wacacom_LinkerOptions ${wacacom_LinkerOptions})

CPMAddPackage("gh:hanickadot/compile-time-regular-expressions@3.9.0")
CPMAddPackage("gh:glfw/glfw#3.4")

CPMAddPackage(
    NAME glfw
    GITHUB_REPOSITORY glfw/glfw
    GIT_TAG 3.4
)

CPMAddPackage("gh:nyyakko/LibError#master")
CPMAddPackage("gh:nyyakko/LibEnum#master")
CPMAddPackage("gh:fmtlib/fmt#10.2.1")
CPMAddPackage("gh:Dobiasd/FunctionalPlus#v0.2.24")

set(wacacom_ExternalLibraries
    glfw
    GL
    fmt::fmt
    ctre::ctre
    LibError::LibError
    LibEnum::LibEnum
    FunctionalPlus::fplus
)

add_subdirectory(wacacom)

