cmake_minimum_required(VERSION 3.20)
project(PayloadDll LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# ImGui
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

# MinHook
set(MINHOOK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/minhook")
set(MINHOOK_SOURCES
    ${MINHOOK_DIR}/src/buffer.c
    ${MINHOOK_DIR}/src/hook.c
    ${MINHOOK_DIR}/src/trampoline.c
    ${MINHOOK_DIR}/src/hde/hde32.c
    ${MINHOOK_DIR}/src/hde/hde64.c
)

# Main payload sources
set(SOURCES
    src/dll_main.cpp
    src/hooks.cpp
    src/menu.cpp
    src/protection.cpp
    src/heartbeat.cpp
    src/integrity_check.cpp
    src/dynamic_imports.cpp
    ${IMGUI_SOURCES}
    ${MINHOOK_SOURCES}
)

add_library(payload SHARED ${SOURCES})

# Include directories
target_include_directories(payload PRIVATE 
    src 
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${MINHOOK_DIR}/include
    ${MINHOOK_DIR}/src
)

# Link Windows libraries needed for DirectX and networking
target_link_libraries(payload PRIVATE d3d11 dxgi d3dcompiler user32 winhttp)

# Define UNICODE
target_compile_definitions(payload PRIVATE UNICODE _UNICODE)
