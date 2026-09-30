// Shell: çizim yardımcıları
#include "shell/Panel.hpp"

#include <cmath>
#include <cstdlib>
#include <map>

#include "imgui.h"

namespace shell {

ImVec4 hexToColor(const std::string& hex, float alpha) {
  if (hex.size() < 7 || hex[0] != '#') return ImVec4(0.29f, 0.62f, 1.0f, alpha);
  const auto cv = [&](std::size_t i) {
    const std::string h = hex.substr(i, 2);
    return static_cast<float>(std::strtol(h.c_str(), nullptr, 16)) / 255.0f;
  };
  return ImVec4(cv(1), cv(3), cv(5), alpha);
}

ImVec4 withAlpha(const ImVec4& c, float a) { return ImVec4(c.x, c.y, c.z, a); }

ImVec4 categoryColor(const std::string& category) {
  static const std::map<std::string, const char*> kColors = {
      {"Değer", "#EF4444"},         {"Matematik", "#F59E0B"},
      {"Metin", "#22C55E"},         {"Dönüşüm", "#A855F7"},
      {"Mantık", "#EC4899"},        {"Karşılaştırma", "#06B6D4"},
      {"Koleksiyon", "#8B5CF6"},    {"Akış", "#3B82F6"},
      {"Giriş / Çıkış", "#EF4444"}, {"Değişken", "#10B981"},
      {"Tanım", "#64748B"},
  };
  const auto it = kColors.find(category);
  return hexToColor(it != kColors.end() ? it->second : "#4A9EFF");
}

void drawBadge(const char* text, const ImVec4& color) {
  ImGui::TextColored(color, "%s", text);
}

void drawEdgeCurve(const ImVec2& a, const ImVec2& b, const ImVec4& color, float thickness,
                   bool dashed) {
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImU32 c = ImGui::ColorConvertFloat4ToU32(color);
  const ImVec2 c1(a.x + 50, a.y);
  const ImVec2 c2(b.x - 50, b.y);
  if (!dashed) {
    dl->AddBezierCubic(a, c1, c2, b, c, thickness);
    return;
  }
  // kesikli: ufuk noktalar
  constexpr int kSteps = 16;
  for (int i = 0; i < kSteps; i += 2) {
    const float t0 = static_cast<float>(i) / kSteps;
    const float t1 = static_cast<float>(i + 1) / kSteps;
    const auto bez = [&](float t) {
      const float mt = 1.0f - t;
      const float a0 = mt * mt * mt, a1 = 3.0f * mt * mt * t, a2 = 3.0f * mt * t * t, a3 = t * t * t;
      return ImVec2(a0 * a.x + a1 * c1.x + a2 * c2.x + a3 * b.x,
                    a0 * a.y + a1 * c1.y + a2 * c2.y + a3 * b.y);
    };
    dl->AddLine(bez(t0), bez(t1), c, thickness);
  }
}

void drawNodePort(const gpi::PortDef& port, bool connected, bool required, float x, float y,
                  const ImVec4& accent) {
  (void)port;
  (void)connected;
  (void)required;
  (void)x;
  (void)y;
  (void)accent;
}

}  // namespace shell
