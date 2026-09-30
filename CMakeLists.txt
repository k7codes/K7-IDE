cmake_minimum_required(VERSION 3.22)
project(GPI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/utf-8 /W4 /permissive- /EHsc /bigobj /FS)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX UNICODE _UNICODE
                          WIN32_LEAN_AND_MEAN)
else()
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------------------
#  gpcore : IDE'nin saf C++ çekirdeği (arayüzden bağımsız)
# ---------------------------------------------------------------------------
add_library(gpcore STATIC
  native/src/Json.cpp
  native/src/Types.cpp
  native/src/Graph.cpp
  native/src/Emit.cpp
  native/src/Catalog.cpp
  native/src/BuiltinNodes.cpp
  native/src/BuiltinFlow.cpp
  native/src/Codegen.cpp
  native/src/ProjectGen.cpp
  native/src/Toolchain.cpp
  native/src/Build.cpp
  native/src/Api.cpp
  native/src/Serialization.cpp
)
target_include_directories(gpcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/native/include)

# ---------------------------------------------------------------------------
#  gpcli : uçtan uca doğrulama aracı (arayüz olmadan)
# ---------------------------------------------------------------------------
add_executable(gpcli native/tests/gp_cli.cpp)
target_link_libraries(gpcli PRIVATE gpcore)

# ---------------------------------------------------------------------------
#  Birim testleri
# ---------------------------------------------------------------------------
enable_testing()
add_executable(gp_tests native/tests/gp_tests.cpp)
target_link_libraries(gp_tests PRIVATE gpcore)
add_test(NAME gp_tests COMMAND gp_tests)

# ---------------------------------------------------------------------------
#  Gpi.Shell : ImGui + DirectX 11 tabanlı görsel programlama arayüzü
# ---------------------------------------------------------------------------
option(GPI_BUILD_SHELL "ImGui/DX11 arayüzünü derle" ON)

if(GPI_BUILD_SHELL AND WIN32)
  set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)

  add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_impl_win32.cpp
    ${IMGUI_DIR}/imgui_impl_dx11.cpp
  )
  target_include_directories(imgui PUBLIC ${IMGUI_DIR})
  target_link_libraries(imgui PUBLIC d3d11 dxgi dwmapi)
  target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)

  add_executable(Gpi.Shell WIN32
    shell/src/main.cpp
    shell/src/App.cpp
    shell/src/Platform.cpp
    shell/src/EditorState.cpp
shell/src/BuildService.cpp
shell/src/LiveAnalyzer.cpp
shell/src/FileDialog.cpp
shell/src/CrashHandler.cpp
    shell/src/Panel.cpp
    shell/src/Theme.cpp
    shell/src/panels/CatalogPanel.cpp
shell/src/panels/CanvasPanel.cpp
shell/src/panels/SolutionPanel.cpp
shell/src/panels/DetailsPanel.cpp
    shell/src/panels/CodePanel.cpp
    shell/src/panels/OutputPanel.cpp
    shell/src/panels/ToolchainPanel.cpp
  )
  target_include_directories(Gpi.Shell PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shell/include)
  target_link_libraries(Gpi.Shell PRIVATE gpcore imgui)

  # Kullanıcı düğümlerinin glifleri için Segoe MDL2
  add_custom_target(shell_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${IMGUI_DIR}/LICENSE.txt
            $<TARGET_FILE_DIR:Gpi.Shell>/LICENSE-imgui.txt
    COMMENT "ImGui lisansını kopyalıyor")
endif()
