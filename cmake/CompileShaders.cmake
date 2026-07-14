# Compiles GLSL shaders with glslc (new grayscale primitive pipeline) into SPIR-V.
# Usage: compile_glsl_shaders(<target_name> <out_dir> <src_dir> <name> [<name> ...])
# For each <name>, expects <src_dir>/<name> to exist (e.g. "shape.vert") and
# produces <out_dir>/<name>.spv.
function(compile_glsl_shaders TARGET_NAME OUT_DIR SRC_DIR)
    find_program(GLSLC_EXE glslc HINTS "$ENV{VULKAN_SDK}/Bin" "C:/VulkanSDK/1.4.341.1/Bin")
    if(NOT GLSLC_EXE)
        message(FATAL_ERROR "glslc not found — install the Vulkan SDK or set VULKAN_SDK")
    endif()
    file(MAKE_DIRECTORY ${OUT_DIR})
    set(_outputs "")
    foreach(_name ${ARGN})
        set(_src "${SRC_DIR}/${_name}")
        set(_out "${OUT_DIR}/${_name}.spv")
        add_custom_command(
            OUTPUT ${_out}
            COMMAND ${GLSLC_EXE} ${_src} -o ${_out}
            DEPENDS ${_src}
            COMMENT "Compiling GLSL shader ${_name}")
        list(APPEND _outputs ${_out})
    endforeach()
    add_custom_target(${TARGET_NAME} ALL DEPENDS ${_outputs})
endfunction()

# Compiles Slang shaders with slangc (vulkan_font_engine's pre-existing MSDF
# shaders) into SPIR-V. Usage identical to compile_glsl_shaders, but <name>
# has no extension (e.g. "msdf_vert") and the source is "<name>.slang".
function(compile_slang_shaders TARGET_NAME OUT_DIR SRC_DIR)
    find_program(SLANGC_EXE slangc HINTS "$ENV{VULKAN_SDK}/Bin" "C:/VulkanSDK/1.4.341.1/Bin")
    if(NOT SLANGC_EXE)
        message(FATAL_ERROR "slangc not found — install the Vulkan SDK or set VULKAN_SDK")
    endif()
    file(MAKE_DIRECTORY ${OUT_DIR})
    set(_outputs "")
    foreach(_name ${ARGN})
        set(_src "${SRC_DIR}/${_name}.slang")
        set(_out "${OUT_DIR}/${_name}.spv")
        add_custom_command(
            OUTPUT ${_out}
            COMMAND ${SLANGC_EXE} ${_src} -target spirv -o ${_out}
            DEPENDS ${_src}
            COMMENT "Compiling Slang shader ${_name}")
        list(APPEND _outputs ${_out})
    endforeach()
    add_custom_target(${TARGET_NAME} ALL DEPENDS ${_outputs})
endfunction()
