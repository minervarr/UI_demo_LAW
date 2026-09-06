cmake_minimum_required(VERSION 3.22.1)
project("windows_ui_demo")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# ---------- vk_canvas: the engine (Renderer/Canvas/widgets/layout/input) ----------
# The vk_canvas submodule brings the whole rendering stack, including its own
# vulkan_font_engine + img_decode_kit under first_party/. This app is a pure
# consumer: no Vulkan bootstrap, batcher, text stack, or shaders of its own.
set(VK_CANVAS ${CMAKE_SOURCE_DIR}/libs/firstparty/vk_canvas)
add_subdirectory(${VK_CANVAS}/core vk_canvas_core_build)

# ---------- mathcore: standalone math AST/CAS/editor/typesetting ----------
add_subdirectory(libs/mathcore)

# ---------- shaders: all compiled from the library's sources ----------
# FileAssetReader reads <exe dir>/assets/, so shaders land in build/assets/.
# msdf_vert/frag are required because this app calls Renderer::initMsdf (the
# library's own windows demo doesn't compile them — consumers must, see the
# vk_canvas CLAUDE.md).
include(${VK_CANVAS}/cmake/VceShaders.cmake)
vce_compile_slang(compile_shared_shaders
    ${CMAKE_BINARY_DIR}/assets/shaders
    ${VK_CANVAS}/first_party/vulkan_font_engine/shaders_src
    composite_vert
    composite_frag
    tiling
    coverage
    msdf_vert
    msdf_frag
)
vce_compile_slang(compile_canvas_shaders
    ${CMAKE_BINARY_DIR}/assets/shaders
    ${VK_CANVAS}/shaders_src
    overlay_vert
    overlay_frag
    image_vert
    image_frag
    shape_vert
    shape_frag
)

# ---------- math font atlas: baked at build time, never committed ----------
# atlas_gen (an offline host tool from the font engine) bakes the
# NewComputerModern Roman/Bold/Italic + Math faces (glyphs AND OpenType MATH
# metrics) into one MTSDF atlas (font.msdf + atlas.rgba) — the single font
# the app loads at startup. Built as a fully separate nested CMake project:
# it vendors its own FreeType/msdfgen subdirectories, which would collide
# with vk_font_core's in one configure pass. Only re-runs when the tool's
# sources or the four source font files change.
set(ATLAS_GEN_SRC ${VK_CANVAS}/first_party/vulkan_font_engine/tools/atlas_gen)
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

set(MATH_FONT_DIR ${CMAKE_SOURCE_DIR}/assets/fonts/newcomputermodern)
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
    COMMENT "Baking font atlas (NewComputerModern Roman/Bold/Italic/Math, MTSDF)"
    VERBATIM
)
add_custom_target(generate_math_atlas DEPENDS ${MATH_ATLAS_OUT}/font.msdf ${MATH_ATLAS_OUT}/atlas.rgba)

# ---------- app executable ----------
add_executable(windows_ui_demo WIN32
    src/app/main.cpp
    src/app/page_text.cpp
    src/app/page_shapes.cpp
    src/app/page_widgets.cpp
    src/app/page_animation.cpp
    src/app/page_math.cpp
    src/math/msdf_font_metrics_adapter.cpp
    # The library's Win32 platform seam impls (FileAssetReader /
    # Win32SurfaceProvider / win32_translate_input) are consumer-compiled by
    # design — they belong to a host exe, not to vk_canvas_core.
    ${VK_CANVAS}/platform/windows/win32_platform.cc
)
target_include_directories(windows_ui_demo PRIVATE
    src
    ${VK_CANVAS}/core
    ${VK_CANVAS}/platform/windows
    ${VK_CANVAS}/first_party/vulkan_font_engine/core
)
target_link_libraries(windows_ui_demo PRIVATE vk_canvas_core mathcore user32)
# win32_platform.hh / asset_reader.hh define WIN32_LEAN_AND_MEAN/NOMINMAX
# themselves (unguarded), so defining them here would just redefine-warn.
target_compile_definitions(windows_ui_demo PRIVATE UNICODE _UNICODE)
if(MSVC)
    target_compile_options(windows_ui_demo PRIVATE /utf-8)
endif()
add_dependencies(windows_ui_demo compile_shared_shaders compile_canvas_shaders generate_math_atlas)

# FileAssetReader reads <exe dir>/assets/, and the shaders above compile into
# ${CMAKE_BINARY_DIR}/assets — so the exe must sit at the build root.
set_target_properties(windows_ui_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Copy bundled fonts next to the executable under assets/ — the CJK fallback
# faces the app bakes from at startup, plus the build-generated math atlas
# (which doesn't exist in source at all).
#
# Named faces, not copy_directory. assets/fonts is the shared `fonts` submodule
# and it is 127 MiB; src/app/main.cpp opens three faces out of it. The paths
# below must match what main.cpp asks the reader for — adding a fallback there
# means adding it here, or the row renders blank.
add_custom_command(TARGET windows_ui_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/fonts/fandol/FandolHei-Regular.otf
        $<TARGET_FILE_DIR:windows_ui_demo>/assets/fonts/fandol/FandolHei-Regular.otf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/fonts/haranoaji/HaranoAjiGothic-Regular.otf
        $<TARGET_FILE_DIR:windows_ui_demo>/assets/fonts/haranoaji/HaranoAjiGothic-Regular.otf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/fonts/unfonts-core/UnDotum.ttf
        $<TARGET_FILE_DIR:windows_ui_demo>/assets/fonts/unfonts-core/UnDotum.ttf
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${MATH_ATLAS_OUT}
        $<TARGET_FILE_DIR:windows_ui_demo>/assets/fonts/math)
