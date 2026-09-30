// Shell: paneller için ortak yardımcılar
#pragma once

#include <string>
#include <vector>

#include "imgui.h"

#include "gpi/Graph.hpp"

namespace shell {

class App;
class EditorState;

// --- Renkler ---
ImVec4 hexToColor(const std::string& hex, float alpha = 1.0f);
ImVec4 withAlpha(const ImVec4& c, float a);

// --- Yerleşim ---
// "A|B|C" biçimindeki port adını sütun/satır olarak yorumlar (ileride).
void drawNodePort(const gpi::PortDef& port, bool connected, bool required, float x, float y,
                  const ImVec4& accent);
void drawEdgeCurve(const ImVec2& a, const ImVec2& b, const ImVec4& color, float thickness,
                   bool selected);

// Düğüm başlığı rengini kategoriye göre üretir.
ImVec4 categoryColor(const std::string& category);

// Küçük yardımcı etiket
void drawBadge(const char* text, const ImVec4& color);

// Panel arayüzü
class Panel {
 public:
  virtual ~Panel() = default;
  virtual void draw(App& app) = 0;
};

}  // namespace shell
