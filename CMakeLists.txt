cmake_minimum_required(VERSION 3.22.1)
project("windows_ui_demo")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

include(cmake/CompileShaders.cmake)

find_package(Vulkan REQUIRED)

# ---------- vulkan_font_engine's platform-agnostic core (MTSDF text) ----------
add_subdirectory(libs/firstparty/vulkan_font_engine/core vk_font_core_build)

# ---------- mathcore: standalone math AST/lexer/parser (+ more, later tasks) ----------
add_subdirectory(libs/mathcore)

# ---------- app executable ----------
add_executable(windows_ui_demo WIN32
    src/app/main.cpp
)
target_sources(windows_ui_demo PRIVATE
    src/platform/window.cpp
    src/platform/input_state.cpp
    src/platform/paths.cpp
    src/gfx/vk_core.cpp
    src/gfx/primitives.cpp
    src/gfx/shape_pipeline.cpp
    src/anim/animated_float.cpp
    src/text/text_renderer.cpp
    src/ui/widgets.cpp
    src/ui/layout.cpp
    src/app/nav.cpp
    src/app/page_text.cpp
    src/app/page_shapes.cpp
    src/app/page_widgets.cpp
    src/app/page_animation.cpp
    src/app/page_math.cpp
    src/math/math_canvas.cpp
    src/math/msdf_font_metrics_adapter.cpp
)
target_include_directories(windows_ui_demo PRIVATE src
    libs/firstparty/vulkan_font_engine/core
)
target_link_libraries(windows_ui_demo PRIVATE vk_font_core mathcore Vulkan::Vulkan)
target_compile_definitions(windows_ui_demo PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)

compile_glsl_shaders(compile_shape_shaders
    ${CMAKE_BINARY_DIR}/shaders
    ${CMAKE_SOURCE_DIR}/shaders
    shape.vert shape.frag
)
add_dependencies(windows_ui_demo compile_shape_shaders)

compile_slang_shaders(compile_msdf_shaders
    ${CMAKE_BINARY_DIR}/shaders
    ${CMAKE_SOURCE_DIR}/libs/firstparty/vulkan_font_engine/shaders_src
    msdf_vert msdf_frag
)
add_dependencies(windows_ui_demo compile_msdf_shaders)

# Copy bundled fonts next to the executable
add_custom_command(TARGET windows_ui_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/fonts
        $<TARGET_FILE_DIR:windows_ui_demo>/fonts)

# ---------- input_state unit test ----------
add_executable(input_state_test src/platform/input_state.cpp src/platform/input_state_test.cpp)

# ---------- primitives unit test ----------
add_executable(primitives_test src/gfx/primitives.cpp src/gfx/primitives_test.cpp)
target_include_directories(primitives_test PRIVATE src)

# ---------- animated_float unit test ----------
add_executable(animated_float_test src/anim/animated_float.cpp src/anim/animated_float_test.cpp)
target_include_directories(animated_float_test PRIVATE src)

# ---------- widgets unit test ----------
add_executable(widgets_test src/ui/widgets.cpp src/platform/input_state.cpp src/ui/widgets_test.cpp)
target_include_directories(widgets_test PRIVATE src
    libs/firstparty/vulkan_font_engine/core
)
target_link_libraries(widgets_test PRIVATE vk_font_core Vulkan::Vulkan)

# ---------- layout unit test ----------
add_executable(layout_test src/ui/layout.cpp src/ui/layout_test.cpp)
