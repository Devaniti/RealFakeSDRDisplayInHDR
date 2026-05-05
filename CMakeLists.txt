cmake_minimum_required(VERSION 3.25)

include_guard(GLOBAL)

project(RealFakeSDRDisplayInHDR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(external/DirectXMath)

find_program(FXC_EXECUTABLE fxc.exe
        NO_DEFAULT_PATH
        PATHS "[HKLM/SOFTWARE/Microsoft/Windows Kits/Installed Roots;KitsRoot10]bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
        REGISTRY_VIEW HOST
        DOC "FXC shader compiler")
if(NOT FXC_EXECUTABLE)
message(FATAL_ERROR "Cannot find FXC.")
endif()

message(STATUS "Using FXC - ${FXC_EXECUTABLE}")

make_directory(${CMAKE_CURRENT_BINARY_DIR}/generated)

# Shader Build
add_custom_command(
        OUTPUT
                ${CMAKE_CURRENT_BINARY_DIR}/generated/tonemap_vs.dxbc
                ${CMAKE_CURRENT_BINARY_DIR}/generated/tonemap_vs.dxbc.h
                ${CMAKE_CURRENT_BINARY_DIR}/generated/tonemap_ps.dxbc
                ${CMAKE_CURRENT_BINARY_DIR}/generated/tonemap_ps.dxbc.h
        COMMAND ${FXC_EXECUTABLE} /nologo /T vs_5_0 /E VSMain /WX $<$<NOT:$<CONFIG:Release>>:/O3> $<$<CONFIG:Debug>:/Zi> /Fo ${CMAKE_CURRENT_BINARY_DIR}/generated/tonemap_vs.dxbc ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/tonemap.hlsl
        COMMAND ${CMAKE_COMMAND} -DINPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/generated/tonemap_vs.dxbc" -DOUTPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/generated/tonemap_vs.dxbc.h" -DVARIABLE_NAME=tonemap_vs_bytecode -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_file.cmake"
        COMMAND ${FXC_EXECUTABLE} /nologo /T ps_5_0 /E PSMain /WX $<$<NOT:$<CONFIG:Release>>:/O3> $<$<CONFIG:Debug>:/Zi> /Fo ${CMAKE_CURRENT_BINARY_DIR}/generated/tonemap_ps.dxbc ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/tonemap.hlsl
        COMMAND ${CMAKE_COMMAND} -DINPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/generated/tonemap_ps.dxbc" -DOUTPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/generated/tonemap_ps.dxbc.h" -DVARIABLE_NAME=tonemap_ps_bytecode -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_file.cmake"
        DEPENDS 
                cmake/embed_file.cmake
                src/shaders/cpp_shared.hlsli
                src/shaders/tonemap.hlsl
)

# Embed Resources
add_custom_command(
        OUTPUT
                ${CMAKE_CURRENT_BINARY_DIR}/generated/open_source_licenses.h
                ${CMAKE_CURRENT_BINARY_DIR}/generated/image_0.h
                ${CMAKE_CURRENT_BINARY_DIR}/generated/image_1.h
                ${CMAKE_CURRENT_BINARY_DIR}/generated/image_2.h
                ${CMAKE_CURRENT_BINARY_DIR}/generated/image_3.h
                ${CMAKE_CURRENT_BINARY_DIR}/generated/image_4.h
                ${CMAKE_CURRENT_BINARY_DIR}/generated/image_5.h
        COMMAND ${CMAKE_COMMAND} -DINPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/resources/open_source_licenses.txt" -DOUTPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/generated/open_source_licenses.h" -DVARIABLE_NAME=open_source_licenses_data -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_file.cmake"
        COMMAND ${CMAKE_COMMAND} -DINPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/resources/0.png" -DOUTPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/generated/image_0.h" -DVARIABLE_NAME=image_0 -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_file.cmake"
        COMMAND ${CMAKE_COMMAND} -DINPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/resources/1.png" -DOUTPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/generated/image_1.h" -DVARIABLE_NAME=image_1 -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_file.cmake"
        COMMAND ${CMAKE_COMMAND} -DINPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/resources/2.png" -DOUTPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/generated/image_2.h" -DVARIABLE_NAME=image_2 -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_file.cmake"
        COMMAND ${CMAKE_COMMAND} -DINPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/resources/3.png" -DOUTPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/generated/image_3.h" -DVARIABLE_NAME=image_3 -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_file.cmake"
        COMMAND ${CMAKE_COMMAND} -DINPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/resources/4.png" -DOUTPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/generated/image_4.h" -DVARIABLE_NAME=image_4 -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_file.cmake"
        COMMAND ${CMAKE_COMMAND} -DINPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/resources/5.png" -DOUTPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/generated/image_5.h" -DVARIABLE_NAME=image_5 -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_file.cmake"
        DEPENDS 
                cmake/embed_file.cmake
                resources/open_source_licenses.txt
                resources/0.png
                resources/1.png
                resources/2.png
                resources/3.png
                resources/4.png
                resources/5.png
)

add_executable(RealFakeSDRDisplayInHDR)

set_target_properties(RealFakeSDRDisplayInHDR PROPERTIES
    WIN32_EXECUTABLE TRUE
    VS_DPI_AWARE "PerMonitor"
)

target_link_libraries(RealFakeSDRDisplayInHDR
        PRIVATE
        d3d11.lib
        dxgi.lib
        DirectXMath
)

target_sources(RealFakeSDRDisplayInHDR
        PRIVATE
        FILE_SET shader_files TYPE HEADERS
        BASE_DIRS src
        FILES
        src/shaders/cpp_shared.hlsli
        src/shaders/tonemap.hlsl

        PRIVATE
        FILE_SET header_files TYPE HEADERS
        BASE_DIRS src
        FILES
        src/precompiled_header.h
        src/embeds.h
        src/resource.h

        PRIVATE
        FILE_SET generated_files TYPE HEADERS
        BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/generated
        FILES
        ${CMAKE_CURRENT_BINARY_DIR}/generated/tonemap_vs.dxbc.h
        ${CMAKE_CURRENT_BINARY_DIR}/generated/tonemap_ps.dxbc.h
        ${CMAKE_CURRENT_BINARY_DIR}/generated/image_0.h
        ${CMAKE_CURRENT_BINARY_DIR}/generated/image_1.h
        ${CMAKE_CURRENT_BINARY_DIR}/generated/image_2.h
        ${CMAKE_CURRENT_BINARY_DIR}/generated/image_3.h
        ${CMAKE_CURRENT_BINARY_DIR}/generated/image_4.h
        ${CMAKE_CURRENT_BINARY_DIR}/generated/image_5.h

        # ImGUI sources
        PRIVATE
        external/imgui/imgui.cpp
        external/imgui/imgui_demo.cpp
        external/imgui/imgui_draw.cpp
        external/imgui/imgui_tables.cpp
        external/imgui/imgui_widgets.cpp
        external/imgui/backends/imgui_impl_dx11.cpp
        external/imgui/backends/imgui_impl_win32.cpp

        # Windows .rc resources
        PRIVATE
        src/resource.rc

        # RealFakeSDRDisplayInHDR's source code
        PRIVATE
        src/main.cpp
        src/embeds.cpp
        src/header_impl.cpp
)

set_source_files_properties(src/header_impl.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

target_precompile_headers(RealFakeSDRDisplayInHDR PRIVATE src/precompiled_header.h)

target_include_directories(RealFakeSDRDisplayInHDR PRIVATE
        src
        external
        external/imgui
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT RealFakeSDRDisplayInHDR)