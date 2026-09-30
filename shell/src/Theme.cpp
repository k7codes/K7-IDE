// Shell: koyu tema kurulumu
#include "shell/Theme.hpp"

#include "imgui.h"

namespace shell {
namespace theme {
namespace {
ImFont* g_uiFont = nullptr;
ImFont* g_codeFont = nullptr;
}  // namespace

ImFont* uiFont() { return g_uiFont; }
void setUiFont(ImFont* f) { g_uiFont = f; }
ImFont* codeFont() { return g_codeFont; }
void setCodeFont(ImFont* f) { g_codeFont = f; }

void applyDark() {
  ImGui::StyleColorsDark();

  ImGuiStyle& s = ImGui::GetStyle();
  s.WindowRounding = 6.0f;
  s.ChildRounding = 6.0f;
  s.FrameRounding = 4.0f;
  s.PopupRounding = 4.0f;
  s.ScrollbarRounding = 4.0f;
  s.GrabRounding = 4.0f;
  s.TabRounding = 4.0f;
  s.WindowBorderSize = 1.0f;
  s.FrameBorderSize = 0.0f;
  s.WindowPadding = ImVec2(10, 10);
  s.FramePadding = ImVec2(8, 4);
  s.ItemSpacing = ImVec2(8, 5);
  s.ScrollbarSize = 12.0f;
  s.GrabMinSize = 10.0f;

  auto& c = s.Colors;
  c[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.065f, 0.082f, 1.0f);
  c[ImGuiCol_ChildBg] = ImVec4(0.067f, 0.078f, 0.098f, 1.0f);
  c[ImGuiCol_PopupBg] = ImVec4(0.075f, 0.086f, 0.106f, 1.0f);
  c[ImGuiCol_Border] = ImVec4(0.18f, 0.20f, 0.25f, 1.0f);
  c[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.13f, 0.16f, 1.0f);
  c[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.18f, 0.22f, 1.0f);
  c[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.22f, 0.28f, 1.0f);
  c[ImGuiCol_TitleBg] = ImVec4(0.045f, 0.055f, 0.070f, 1.0f);
  c[ImGuiCol_TitleBgActive] = ImVec4(0.065f, 0.080f, 0.100f, 1.0f);
  c[ImGuiCol_MenuBarBg] = ImVec4(0.045f, 0.055f, 0.070f, 1.0f);
  c[ImGuiCol_Button] = ImVec4(0.16f, 0.29f, 0.48f, 1.0f);
  c[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.40f, 0.65f, 1.0f);
  c[ImGuiCol_ButtonActive] = ImVec4(0.26f, 0.48f, 0.76f, 1.0f);
  c[ImGuiCol_Header] = ImVec4(0.18f, 0.32f, 0.52f, 0.8f);
  c[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.42f, 0.68f, 0.9f);
  c[ImGuiCol_HeaderActive] = ImVec4(0.28f, 0.50f, 0.80f, 1.0f);
  c[ImGuiCol_Tab] = ImVec4(0.09f, 0.11f, 0.14f, 1.0f);
  c[ImGuiCol_TabHovered] = ImVec4(0.20f, 0.36f, 0.58f, 1.0f);
  c[ImGuiCol_TabActive] = ImVec4(0.14f, 0.26f, 0.44f, 1.0f);
  c[ImGuiCol_DockingPreview] = ImVec4(0.30f, 0.55f, 0.90f, 0.55f);
}

}  // namespace theme
}  // namespace shell
