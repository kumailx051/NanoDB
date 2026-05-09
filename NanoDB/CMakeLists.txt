cmake_minimum_required(VERSION 3.20)
project(NanoDB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)

if(TARGET glfw)
    set(GLFW_TARGET glfw)
elseif(TARGET glfw3)
    set(GLFW_TARGET glfw3)
else()
    message(FATAL_ERROR "GLFW target not found. Install GLFW or provide glfw3Config.cmake.")
endif()

file(GLOB_RECURSE NANODB_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_executable(nanodb ${NANODB_SOURCES} ${IMGUI_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
target_include_directories(nanodb PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(nanodb PRIVATE OpenGL::GL ${GLFW_TARGET})

add_executable(test_runner
    ${NANODB_SOURCES}
    ${IMGUI_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_runner.cpp
)
target_include_directories(test_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(test_runner PRIVATE OpenGL::GL ${GLFW_TARGET})
