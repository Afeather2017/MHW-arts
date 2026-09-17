cmake_minimum_required(VERSION 3.21)
project(ValorGauge LANGUAGES CXX)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/kiero")
file(READ "${ROOT}/external/kiero/kiero.h" KIERO_HEADER)
string(REPLACE "#define KIERO_INCLUDE_D3D11  0" "#define KIERO_INCLUDE_D3D11  1" KIERO_HEADER "${KIERO_HEADER}")
string(REPLACE "#define KIERO_USE_MINHOOK    0" "#define KIERO_USE_MINHOOK    1" KIERO_HEADER "${KIERO_HEADER}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/kiero/kiero.h" "${KIERO_HEADER}")
file(COPY "${ROOT}/external/kiero/kiero.cpp" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/generated/kiero")

add_subdirectory("${ROOT}/MHW-QuestLoader/external/minhook" "${CMAKE_BINARY_DIR}/minhook")

add_library(ValorGauge SHARED
    src/dllmain.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/generated/kiero/kiero.cpp"
    "${ROOT}/external/imgui/imgui.cpp"
    "${ROOT}/external/imgui/imgui_draw.cpp"
    "${ROOT}/external/imgui/imgui_tables.cpp"
    "${ROOT}/external/imgui/imgui_widgets.cpp"
    "${ROOT}/external/imgui/backends/imgui_impl_dx11.cpp"
)

target_include_directories(ValorGauge PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/kiero"
    "${ROOT}/external/kiero/minhook/include"
    "${ROOT}/MHW-QuestLoader/external"
    "${ROOT}/MHW-QuestLoader/external/minhook/include"
    "${ROOT}/external/imgui"
    "${ROOT}/external/imgui/backends"
)

target_compile_definitions(ValorGauge PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(ValorGauge PRIVATE minhook d3d11 dxgi psapi)
set_target_properties(ValorGauge PROPERTIES OUTPUT_NAME "ValorGauge")

add_custom_command(TARGET ValorGauge POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "E:/SteamLibrary/steamapps/common/Monster Hunter World/nativePC/plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:ValorGauge>"
        "E:/SteamLibrary/steamapps/common/Monster Hunter World/nativePC/plugins/ValorGauge.dll"
)

# ValorProbe = research/diagnostics build of the same source. It observes and
# logs (Wwise, damage/hit dispatch, F5 miner, F11 property histogram, F7 state
# logger, F10 diff capture, RXN/EPROBE watchers) and performs NO gameplay
# actions: no gauge, no clang triggers, no force, no audio hooks in the mod.
# The probe is NEVER auto-installed: drop it into nativePC/plugins manually
# (and remove ValorGauge.dll) only while researching. Both DLLs refuse to
# co-install.
add_library(ValorProbe SHARED
    src/dllmain.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/generated/kiero/kiero.cpp"
    "${ROOT}/external/imgui/imgui.cpp"
    "${ROOT}/external/imgui/imgui_draw.cpp"
    "${ROOT}/external/imgui/imgui_tables.cpp"
    "${ROOT}/external/imgui/imgui_widgets.cpp"
    "${ROOT}/external/imgui/backends/imgui_impl_dx11.cpp"
)

target_include_directories(ValorProbe PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/kiero"
    "${ROOT}/external/kiero/minhook/include"
    "${ROOT}/MHW-QuestLoader/external"
    "${ROOT}/MHW-QuestLoader/external/minhook/include"
    "${ROOT}/external/imgui"
    "${ROOT}/external/imgui/backends"
)

target_compile_definitions(ValorProbe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX VALOR_PROBE=1)
target_link_libraries(ValorProbe PRIVATE minhook d3d11 dxgi psapi)
set_target_properties(ValorProbe PROPERTIES OUTPUT_NAME "ValorProbe")

add_custom_command(TARGET ValorProbe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/dist"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:ValorProbe>"
        "${CMAKE_CURRENT_SOURCE_DIR}/dist/ValorProbe.dll"
)
