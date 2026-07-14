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

# ---------- math font atlas: baked at build time, never committed ----------
# atlas_gen (an offline host tool) bakes fonts/newcomputermodern's math face
# into an MTSDF atlas (font.msdf + atlas.rgba). It's built as a fully separate
# nested CMake project rather than add_subdirectory'd here directly: its
# CMakeLists.txt independently add_subdirectory's the same third_party/
# freetype and msdfgen submodules vk_font_core (above) already builds, and
# pulling both into one configure pass would collide on duplicate target
# names (freetype, msdfgen-core, ...). Nesting a nested cmake -S/-B + build
# keeps the two fully isolated at the cost of building FreeType/msdfgen a
# second time on a clean build; the atlas itself lands in the build tree
# (${CMAKE_BINARY_DIR}), so it's covered by the same .gitignore rule as
# build_debug/build and is never checked in — only re-baked when the atlas
# tool's sources or the source font files actually change.
set(ATLAS_GEN_SRC ${CMAKE_SOURCE_DIR}/libs/firstparty/vulkan_font_engine/tools/atlas_gen)
set(ATLAS_GEN_BUILD ${CMAKE_BINARY_DIR}/atlas_gen_tool)
set(ATLAS_GEN_EXE ${ATLAS_GEN_BUILD}/atlas_gen.exe)

add_custom_command(
    OUTPUT ${ATLAS_GEN_EXE}
    COMMAND ${CMAKE_COMMAND} -S ${ATLAS_GEN_SRC} -B ${ATLAS_GEN_BUILD} -G Ninja -DCMAKE_BUILD_TYPE=Release
    COMMAND ${CMAKE_COMMAND} --build ${ATLAS_GEN_BUILD}
    DEPENDS ${ATLAS_GEN_SRC}/atlas_gen.cc ${ATLAS_GEN_SRC}/math_table.cc ${ATLAS_GEN_SRC}/math_table.hh
    COMMENT "Building atlas_gen host tool (nested build; vendors its own FreeType/msdfgen)"
    VERBATIM
)
add_custom_target(atlas_gen_tool_build DEPENDS ${ATLAS_GEN_EXE})

set(MATH_FONT_DIR ${CMAKE_SOURCE_DIR}/fonts/newcomputermodern)
set(MATH_ATLAS_OUT ${CMAKE_BINARY_DIR}/generated_fonts/math)
add_custom_command(
    OUTPUT ${MATH_ATLAS_OUT}/font.msdf ${MATH_ATLAS_OUT}/atlas.rgba
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MATH_ATLAS_OUT}
    COMMAND ${ATLAS_GEN_EXE}
        ${MATH_FONT_DIR}/NewCM10-Regular.otf
        ${MATH_FONT_DIR}/NewCM10-Bold.otf
        ${MATH_FONT_DIR}/NewCM10-Italic.otf
        ${MATH_FONT_DIR}/NewCMMath-Regular.otf
        ${MATH_ATLAS_OUT}/font.msdf
        ${MATH_ATLAS_OUT}/atlas.rgba
    DEPENDS ${ATLAS_GEN_EXE}
        ${MATH_FONT_DIR}/NewCM10-Regular.otf
        ${MATH_FONT_DIR}/NewCM10-Bold.otf
        ${MATH_FONT_DIR}/NewCM10-Italic.otf
        ${MATH_FONT_DIR}/NewCMMath-Regular.otf
    COMMENT "Baking math font atlas (NewComputerModern, MTSDF)"
    VERBATIM
)
add_custom_target(generate_math_atlas DEPENDS ${MATH_ATLAS_OUT}/font.msdf ${MATH_ATLAS_OUT}/atlas.rgba)

# ---------- app executable ----------
add_executable(windows_ui_demo WIN32
    src/app/main.cpp
)
target_sources(windows_ui_demo PRIVATE
    src/platform/windows/window.cpp
    src/platform/windows/paths.cpp
    src/platform/windows/fatal.cpp
    src/platform/windows/vk_surface_windows.cpp
    src/platform/input_state.cpp
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
add_dependencies(windows_ui_demo generate_math_atlas)

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

# Copy bundled fonts next to the executable — the committed fonts/ tree (OTF
# files) plus the build-generated math atlas (see generate_math_atlas above),
# which doesn't exist in source at all.
add_custom_command(TARGET windows_ui_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/fonts
        $<TARGET_FILE_DIR:windows_ui_demo>/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${MATH_ATLAS_OUT}
        $<TARGET_FILE_DIR:windows_ui_demo>/fonts/math)

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
