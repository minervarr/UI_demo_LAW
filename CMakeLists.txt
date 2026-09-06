cmake_minimum_required(VERSION 3.22.1)
project("ui_demo")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# This file builds the DESKTOP (Linux/Wayland) target. The Android target is
# built by platform/android/CMakeLists.txt, which Gradle invokes and which
# compiles the SAME src/app sources against app_shell's Android host — the
# arrangement Matrix Player and ViewMage use. Nothing under src/ knows which
# one it is in.
set(REPO_ROOT     ${CMAKE_CURRENT_SOURCE_DIR})
set(VK_CANVAS     ${REPO_ROOT}/libs/firstparty/vk_canvas)
set(APP_SHELL_DIR ${REPO_ROOT}/libs/firstparty/app_shell)

if(WIN32)
    # The Win32 backend was removed with the app_shell port. app_shell has a
    # Win32 host (os/win32_host.cc) and the pages are already portable, so
    # restoring it is a build-system job and not a rewrite — but it is not
    # carried here unbuilt and untested.
    message(FATAL_ERROR
        "Windows is not currently a supported target for ui_demo. "
        "Build on Linux (platform/linux/build.sh) or Android "
        "(platform/android/gradlew assembleDebug).")
endif()

# ---------- vk_canvas: the engine (Renderer/Canvas/widgets/layout/input) -----
# The submodule brings the whole rendering stack, including its own
# vulkan_font_engine + img_decode_kit. This app is a pure consumer: no Vulkan
# bootstrap, batcher, text stack or shaders of its own.
add_subdirectory(${VK_CANVAS}/core          vk_canvas_core_build)
add_subdirectory(${VK_CANVAS}/platform/linux vk_canvas_wayland_build)

# ---------- app_shell: the window, the pump, the Host seam ------------------
set(APP_SHELL_APP_NAME     "ui_demo" CACHE STRING "" FORCE)
set(APP_SHELL_WINDOW_TITLE "vk_canvas UI demo" CACHE STRING "" FORCE)
set(APP_SHELL_APP_ID       "io.nava.uidemo" CACHE STRING "" FORCE)

# app_shell's ui_metrics.hh includes a generated header carrying the smallest
# size at which this app's own faces stay legible, and its Debug tests depend
# on the target that emits it. The demo draws with the baked NewComputerModern
# atlas, so those are the faces to measure.
include(${APP_SHELL_DIR}/cmake/AppShellMinTextSize.cmake)
set(VK_CANVAS_DIR ${VK_CANVAS} CACHE PATH "vk_canvas root")
app_shell_generate_min_text_size(
    ${REPO_ROOT}/assets/fonts/newcomputermodern/NewCM10-Regular.otf
    ${REPO_ROOT}/assets/fonts/newcomputermodern/NewCM10-Bold.otf
    ${REPO_ROOT}/assets/fonts/newcomputermodern/NewCM10-Italic.otf)

add_subdirectory(${APP_SHELL_DIR} app_shell_build)
# ui_metrics.cc includes the header that call just declared, so it has to wait
# for it. Stated here rather than inside app_shell's own CMakeLists because the
# target does not exist yet when CMake reads that file — the consumer owns the
# font list, so the consumer owns the ordering.
add_dependencies(app_shell generate_ui_min_text_size)

# ---------- mathcore: standalone math AST/CAS/editor/typesetting ------------
add_subdirectory(libs/mathcore)

# ---------- shaders: all compiled from the library's sources ----------------
# FileAssetReader reads <exe dir>/assets/, so shaders land in build/assets/.
# msdf_vert/frag are required because this app calls Renderer::initMsdf (the
# library's own demo doesn't compile them — consumers must).
include(${VK_CANVAS}/cmake/VceShaders.cmake)
vce_compile_slang(compile_shared_shaders
    ${CMAKE_BINARY_DIR}/assets/shaders
    ${VK_CANVAS}/first_party/vulkan_font_engine/shaders_src
    composite_vert composite_frag tiling coverage msdf_vert msdf_frag)
vce_compile_slang(compile_canvas_shaders
    ${CMAKE_BINARY_DIR}/assets/shaders
    ${VK_CANVAS}/shaders_src
    overlay_vert overlay_frag image_vert image_frag shape_vert shape_frag)

# ---------- math font atlas: baked at build time, never committed -----------
# atlas_gen (an offline host tool from the font engine) bakes the
# NewComputerModern Roman/Bold/Italic + Math faces (glyphs AND OpenType MATH
# metrics) into one MTSDF atlas — the single font the app loads at startup.
# Built as a fully separate nested CMake project: it vendors its own
# FreeType/msdfgen subdirectories, which would collide with vk_font_core's in
# one configure pass.
set(ATLAS_GEN_SRC   ${VK_CANVAS}/first_party/vulkan_font_engine/tools/atlas_gen)
set(ATLAS_GEN_BUILD ${CMAKE_BINARY_DIR}/atlas_gen_tool)
# No .exe. The name was hardcoded when this repo only ever built on Windows,
# which is precisely why the Linux build could not run its own atlas baker.
set(ATLAS_GEN_EXE ${ATLAS_GEN_BUILD}/atlas_gen${CMAKE_EXECUTABLE_SUFFIX})

add_custom_command(
    OUTPUT ${ATLAS_GEN_EXE}
    COMMAND ${CMAKE_COMMAND} -S ${ATLAS_GEN_SRC} -B ${ATLAS_GEN_BUILD}
            -G ${CMAKE_GENERATOR} -DCMAKE_BUILD_TYPE=Release
    COMMAND ${CMAKE_COMMAND} --build ${ATLAS_GEN_BUILD}
    DEPENDS ${ATLAS_GEN_SRC}/atlas_gen.cc ${ATLAS_GEN_SRC}/math_table.cc
            ${ATLAS_GEN_SRC}/math_table.hh
    COMMENT "Building atlas_gen host tool (nested build; vendors its own FreeType/msdfgen)"
    VERBATIM)
add_custom_target(atlas_gen_tool_build DEPENDS ${ATLAS_GEN_EXE})

set(MATH_FONT_DIR  ${REPO_ROOT}/assets/fonts/newcomputermodern)
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
    VERBATIM)
add_custom_target(generate_math_atlas
    DEPENDS ${MATH_ATLAS_OUT}/font.msdf ${MATH_ATLAS_OUT}/atlas.rgba)

# ---------- the application -------------------------------------------------
# The portable half. platform/android/CMakeLists.txt compiles this exact list.
set(UI_DEMO_APP_SRC
    ${REPO_ROOT}/src/app/demo_app.cc
    ${REPO_ROOT}/src/app/page_text.cpp
    ${REPO_ROOT}/src/app/page_shapes.cpp
    ${REPO_ROOT}/src/app/page_widgets.cpp
    ${REPO_ROOT}/src/app/page_animation.cpp
    ${REPO_ROOT}/src/app/page_math.cpp
    ${REPO_ROOT}/src/app/page_hdr.cc
    ${REPO_ROOT}/src/app/page_image.cc
    ${REPO_ROOT}/src/app/page_gestures.cc
    ${REPO_ROOT}/src/app/page_textedit.cc
    ${REPO_ROOT}/src/app/page_plot.cc
    ${REPO_ROOT}/src/math/msdf_font_metrics_adapter.cpp)

set(UI_DEMO_INCLUDE_DIRS
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/app
    ${APP_SHELL_DIR}
    ${VK_CANVAS}/core
    ${VK_CANVAS}/first_party/vulkan_font_engine/core
    ${CMAKE_BINARY_DIR}/generated)

add_executable(ui_demo
    ${UI_DEMO_APP_SRC}
    ${REPO_ROOT}/src/app/gui_main.cc)

target_include_directories(ui_demo PRIVATE
    ${UI_DEMO_INCLUDE_DIRS}
    ${VK_CANVAS}/platform/linux)

target_link_libraries(ui_demo PRIVATE
    app_shell
    app_shell_wayland
    vk_canvas_core
    vk_canvas_wayland
    mathcore)

target_compile_options(ui_demo PRIVATE -Wall)
add_dependencies(ui_demo compile_shared_shaders compile_canvas_shaders
                        generate_math_atlas generate_ui_min_text_size)

# FileAssetReader reads <exe dir>/assets/, and the shaders compile into
# ${CMAKE_BINARY_DIR}/assets — so the exe sits at the build root.
set_target_properties(ui_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ---------- tests (desktop, Debug) ------------------------------------------
# ScrollArea and UiUnits are pure logic with no Vulkan and no OS in them, which
# is exactly why they can be tested: "can every control be reached on a screen
# too small for the page" is arithmetic. The interesting case is also the one
# hardest to produce by hand — a tiling compositor does not hand out a window
# short enough on request — so it is worth having as a test rather than a
# screenshot.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    enable_testing()
    add_executable(scroll_area_test ${REPO_ROOT}/tests/scroll_area_test.cc)
    target_include_directories(scroll_area_test PRIVATE ${UI_DEMO_INCLUDE_DIRS})
    target_link_libraries(scroll_area_test PRIVATE vk_canvas_core)
    add_test(NAME scroll_area_test COMMAND scroll_area_test)
endif()

# Named faces, not copy_directory. assets/fonts is the shared `fonts` submodule
# and it is 127 MiB; this app opens the nine faces below. They must match what
# demo_app.cc registers — adding a fallback there means adding it here, or that
# script renders blank.
#
# Song / Mincho / Batang are the serif cuts, chosen to sit with New Computer
# Modern's serif Latin; the sans and calligraphic cuts bundled beside them are
# deliberately not shipped.
add_custom_command(TARGET ui_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${REPO_ROOT}/assets/fonts/newcomputermodern/NewCM10-Regular.otf
        $<TARGET_FILE_DIR:ui_demo>/assets/fonts/newcomputermodern/NewCM10-Regular.otf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${REPO_ROOT}/assets/fonts/newcomputermodern/NewCM10-Bold.otf
        $<TARGET_FILE_DIR:ui_demo>/assets/fonts/newcomputermodern/NewCM10-Bold.otf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${REPO_ROOT}/assets/fonts/newcomputermodern/NewCM10-Italic.otf
        $<TARGET_FILE_DIR:ui_demo>/assets/fonts/newcomputermodern/NewCM10-Italic.otf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${REPO_ROOT}/assets/fonts/fandol/FandolSong-Regular.otf
        $<TARGET_FILE_DIR:ui_demo>/assets/fonts/fandol/FandolSong-Regular.otf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${REPO_ROOT}/assets/fonts/fandol/FandolSong-Bold.otf
        $<TARGET_FILE_DIR:ui_demo>/assets/fonts/fandol/FandolSong-Bold.otf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${REPO_ROOT}/assets/fonts/haranoaji/HaranoAjiMincho-Regular.otf
        $<TARGET_FILE_DIR:ui_demo>/assets/fonts/haranoaji/HaranoAjiMincho-Regular.otf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${REPO_ROOT}/assets/fonts/haranoaji/HaranoAjiMincho-Bold.otf
        $<TARGET_FILE_DIR:ui_demo>/assets/fonts/haranoaji/HaranoAjiMincho-Bold.otf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${REPO_ROOT}/assets/fonts/unfonts-core/UnBatang.ttf
        $<TARGET_FILE_DIR:ui_demo>/assets/fonts/unfonts-core/UnBatang.ttf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${REPO_ROOT}/assets/fonts/unfonts-core/UnBatangBold.ttf
        $<TARGET_FILE_DIR:ui_demo>/assets/fonts/unfonts-core/UnBatangBold.ttf
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${MATH_ATLAS_OUT}
        $<TARGET_FILE_DIR:ui_demo>/assets/fonts/math)
