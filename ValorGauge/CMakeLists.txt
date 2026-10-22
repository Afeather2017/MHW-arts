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

# /utf-8: the source (and its Chinese/Japanese comments) is UTF-8; without
# this MSVC decodes it as the ANSI codepage (936 here) and a mangled trailing
# multibyte can swallow the next newline, hiding the following declaration
# (phantom C2065) - the C4819 warnings were that hazard's smoke alarm.
target_compile_options(ValorGauge PRIVATE /utf-8)
target_compile_definitions(ValorGauge PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(ValorGauge PRIVATE minhook d3d11 dxgi psapi winmm)
set_target_properties(ValorGauge PROPERTIES OUTPUT_NAME "ValorGauge")
# No POST_BUILD auto-install: with two targets sharing one source, an automatic
# copy re-installs the sibling DLL behind research swaps and the two builds
# then stand each other down. Deploy explicitly with deploy.sh (mod|probe|off),
# which also enforces the one-DLL rule.

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

target_compile_options(ValorProbe PRIVATE /utf-8)
target_compile_definitions(ValorProbe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX VALOR_PROBE=1)
target_link_libraries(ValorProbe PRIVATE minhook d3d11 dxgi psapi winmm)
set_target_properties(ValorProbe PROPERTIES OUTPUT_NAME "ValorProbe")
