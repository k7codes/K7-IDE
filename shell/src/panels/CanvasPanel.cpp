// Shell: grafik tuval - düğümler, portlar, kablolar, sürükleme, pan/zoom
#include "shell/panels/CanvasPanel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "imgui.h"

#include "shell/App.hpp"
#include "shell/LiveAnalyzer.hpp"
#include "shell/Panel.hpp"

namespace shell {

namespace {
constexpr float kHeaderH = 26.0f;
constexpr float kRowH = 19.0f;
constexpr float kNodeW = 190.0f;
constexpr float kPortDot = 6.0f;
constexpr float kGridStep = 26.0f;

// Anlık denetim sonucunu düğüm/port/kablo düzeyinde sorgulamak için kısa
// yardımcılar. Sonuç App tarafından kare başına bir kez kopyalanır, bu yüzden
// burada mutex'e hiç dokunulmaz.
// Bir düğümün ekran dikdörtgenini bulur. Yalnızca bu dosyada kullanılır,
// bu yüzden anonim ad alanında tutulur (isim çakışması olmasın).
const NodeRect* findRect(const std::vector<NodeRect>& rs, const std::string& id) {
  for (const auto& r : rs) {
    if (r.nodeId == id) return &r;
  }
  return nullptr;
}

gpi::ProblemLevel levelOfNode(const App& app, const std::string& nodeId) {
  gpi::ProblemLevel worst = gpi::ProblemLevel::Note;
  for (const auto& d : app.analysis().diagnostics) {
    if (d.nodeId != nodeId && d.fromNode != nodeId) continue;
    if (static_cast<int>(d.level) > static_cast<int>(worst)) worst = d.level;
  }
  return worst;
}

gpi::ProblemLevel levelOfPort(const App& app, const gpi::PortKey& key) {
  gpi::ProblemLevel worst = gpi::ProblemLevel::Note;
  for (const auto& d : app.analysis().diagnostics) {
    const bool hit = (d.nodeId == key.node && d.portName == key.port) ||
                     (d.fromNode == key.node && d.fromPort == key.port);
    if (!hit) continue;
    if (static_cast<int>(d.level) > static_cast<int>(worst)) worst = d.level;
  }
  return worst;
}

ImU32 problemColor(gpi::ProblemLevel lv) {
  switch (lv) {
    case gpi::ProblemLevel::Error:
      return IM_COL32(255, 86, 86, 255);
    case gpi::ProblemLevel::Warning:
      return IM_COL32(255, 196, 64, 255);
    default:
      return 0;
  }
}
}  // namespace

void CanvasPanel::draw(App& app) {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  if (!ImGui::Begin("Tuval", &app.showCanvas(),
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
    ImGui::End();
    ImGui::PopStyleVar();
    return;
  }
  ImGui::PopStyleVar();

  EditorState& s = app.state();
  layout(s);

  // ImGui çizim listesi EKRAN koordinatı bekler. Graf koordinatları ise
  // tuvalin içeriğine göre tanımlıdır; bu yüzden pan, içeriğin sol üst
  // köşesine sabitlenmelidir (aksi halde düğümler görünmez alana düşer).
  contentOrigin_ = ImGui::GetCursorScreenPos();
  contentSize_ = ImGui::GetContentRegionAvail();
  const ImVec2 origin = contentOrigin_;
  const ImVec2 size = contentSize_;
  viewCenter_ = ImVec2(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f);
  if (!viewAnchored_) anchorView(s);
  consumeReveal(app);

  ImDrawList* dl = ImGui::GetWindowDrawList();
  drawGrid(s, dl, origin, size);
  drawEdges(app);
  drawNodes(app);
  drawOverlay(app);
  handleInteraction(app);

  ImGui::End();
}

void CanvasPanel::anchorView(EditorState& s) {
  constexpr float kMargin = 30.0f;
  s.setPan(contentOrigin_.x + kMargin, contentOrigin_.y + kMargin);
  viewAnchored_ = true;
}

// Kod panelinde bir satıra tıklandığında App.focusNode() çağrılır. Burada
// düğüm seçilir, tuval ona kaydırılır ve kısa süreli kırmızı çerçeve ile
// "buraya geldin" sinyali verilir.
void CanvasPanel::consumeReveal(App& app) {
  // İsteği TEK SEFERDE al ve tüket: takeFocusRequest() çağrıldığı anda
  // isteği temizler, ikinci bir çağrı her zaman boş dönerdi.
  const std::string id = app.takeFocusRequest();
  if (id.empty()) return;
  EditorState& s = app.state();
  if (!s.graph().findNode(id)) return;
  s.select(id);

  const NodeRect* r = findRect(rects_, id);
  if (!r) return;
  // Düğümü içeriğin ortasına getir.
  s.setPan(contentOrigin_.x + contentSize_.x * 0.5f - (r->x + r->w * 0.5f) * s.zoom(),
           contentOrigin_.y + contentSize_.y * 0.5f - (r->y + r->h * 0.5f) * s.zoom());

  flashNode_ = id;
  flashUntil_ = ImGui::GetTime() + 1.6;
  app.setStatus("Odak: " + id + "  (Kod panelinden gelindi)");
}

// --------------------------------------------------------------- yerleşim

void CanvasPanel::layout(EditorState& s) {
  if (!s.layoutsDirty()) return;
  rects_.clear();
  rects_.reserve(s.graph().nodes.size());
  for (const auto& n : s.graph().nodes) {
    const gpi::NodeType* t = s.catalog().find(n.typeId);
    if (!t) continue;
    // Görünen port sayısı = bağlı olmayanlar; en az 1 satır
    std::size_t inCount = 0, outCount = 0;
    for (const auto& p : t->inputs) {
      if (p.kind == gpi::PortKind::Exec || p.kind == gpi::PortKind::Value) ++inCount;
    }
    for (const auto& p : t->outputs) {
      if (p.kind == gpi::PortKind::Exec || p.kind == gpi::PortKind::Value) ++outCount;
    }
    const std::size_t rows = std::max<std::size_t>(1, std::max(inCount, outCount));
    NodeRect r;
    r.nodeId = n.id;
    r.x = static_cast<float>(n.x);
    r.y = static_cast<float>(n.y);
    r.w = kNodeW;
    r.h = kHeaderH + static_cast<float>(rows) * kRowH + 10.0f;
    rects_.push_back(r);
  }
  s.rects() = rects_;
  s.clearLayoutDirty();
}

bool CanvasPanel::portPosition(EditorState& s, const gpi::PortKey& port, ImVec2& out) const {  const NodeRect* r = findRect(rects_, port.node);
  if (!r) return false;
  const gpi::NodeInstance* n = s.graph().findNode(port.node);
  if (!n) return false;
  const gpi::NodeType* t = s.catalog().find(n->typeId);
  if (!t) return false;

  float sx, sy;
  if (!t->output(port.port)) {
    // giriş portu
    std::size_t idx = 0;
    for (const auto& p : t->inputs) {
      if (p.name == port.port) break;
      ++idx;
    }
    s.graphToScreen(r->x, r->y + kHeaderH + 4.0f + static_cast<float>(idx) * kRowH + kRowH * 0.5f, sx,
                     sy);
    out = ImVec2(sx, sy);
    return true;
  }
  std::size_t idx = 0;
  for (const auto& p : t->outputs) {
    if (p.name == port.port) break;
    ++idx;
  }
  s.graphToScreen(r->x + r->w, r->y + kHeaderH + 4.0f + static_cast<float>(idx) * kRowH + kRowH * 0.5f,
                 sx, sy);
  out = ImVec2(sx, sy);
  return true;
}

// --------------------------------------------------------------- ızgara

void CanvasPanel::drawGrid(EditorState& s, ImDrawList* dl, const ImVec2& origin, const ImVec2& size) {
  dl->PushClipRect(origin, origin + size, true);
  dl->AddRectFilled(origin, origin + size, IM_COL32(15, 18, 24, 255));

  // Izgara, graf uzayında kGridStep adımlıdır ve pan/zoom ile birlikte kayar:
  //   ekran = graf * zoom + pan
  const float zoom = s.zoom();
  const float panX = s.panX();
  const float panY = s.panY();
  const float step = kGridStep * (zoom > 0.01f ? zoom : 1.0f);
  if (step >= 4.0f) {
    const ImU32 line = IM_COL32(23, 27, 35, 255);
    for (float x = panX + std::fmod(panX, step); x < origin.x + size.x; x += step) {
      if (x < origin.x) continue;
      dl->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + size.y), line);
    }
    for (float y = panY + std::fmod(panY, step); y < origin.y + size.y; y += step) {
      if (y < origin.y) continue;
      dl->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + size.x, y), line);
    }
  }
  dl->PopClipRect();
}

// --------------------------------------------------------------- kablolar

void CanvasPanel::drawEdges(App& app) {
  EditorState& s = app.state();
  const ImVec4 wire = hexToColor("#4A9EFF");
  const ImVec4 wireErr = hexToColor("#FF5656");
  const ImVec4 wireWarn = hexToColor("#FFC440");

  for (const auto& e : s.graph().edges) {
    ImVec2 a, b;
    if (!portPosition(s, e.from, a) || !portPosition(s, e.to, b)) continue;
    // Hatalı kablo (tip/akış uyuşmazlığı) kırmızı, şüpheli kablo sarı.
    const gpi::ProblemLevel lv =
        levelOfPort(app, e.to) != gpi::ProblemLevel::Note ? levelOfPort(app, e.to)
                                                          : levelOfPort(app, e.from);
    const ImVec4 col = lv == gpi::ProblemLevel::Error
                           ? wireErr
                           : (lv == gpi::ProblemLevel::Warning ? wireWarn : wire);
    drawEdgeCurve(a, b, col, lv == gpi::ProblemLevel::Note ? 2.0f : 3.0f, false);
  }

  // Sürüklenen kablo
  if (dragLink_.active) {
    ImVec2 a;
    portPosition(s, dragLink_.from, a);
    ImVec2 mouse = ImGui::GetMousePos();
    drawEdgeCurve(a, mouse, ImVec4(0.42f, 0.62f, 0.85f, 0.9f), 2.0f, true);
  }
}

// --------------------------------------------------------------- düğümler

void CanvasPanel::drawNodes(App& app) {
  EditorState& s = app.state();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetWindowDrawList()->GetClipRectMin();

  for (const auto& r : rects_) {
    const gpi::NodeInstance* n = s.graph().findNode(r.nodeId);
    if (!n) continue;
    const gpi::NodeType* t = s.catalog().find(n->typeId);
    if (!t) continue;

    float sx, sy;
    s.graphToScreen(r.x, r.y, sx, sy);
    const ImVec2 p0(sx, sy);
    const ImVec2 p1(sx + r.w * s.zoom(), sy + r.h * s.zoom());
    const ImVec4 accent = hexToColor(t->accentColor);
    const bool sel = s.isSelected(r.nodeId);
    const gpi::ProblemLevel nodeLevel = levelOfNode(app, r.nodeId);
    const ImU32 problem = problemColor(nodeLevel);

    // Gövde
    dl->AddRectFilled(p0, p1, IM_COL32(28, 33, 43, 255), 6.0f);
    ImU32 border = sel ? IM_COL32(255, 210, 74, 255) : IM_COL32(52, 60, 74, 255);
    float borderW = sel ? 2.0f : 1.0f;
    if (problem) {
      // Anlık denetim hatası varsa çerçeve rengi ve kalınlığı değişir.
      border = problem;
      borderW = 2.5f;
    }
    dl->AddRect(p0, p1, border, 6.0f, 0, borderW);

    // Başlık
    const ImVec2 h1(p0.x, p0.y + kHeaderH * s.zoom());
    dl->AddRectFilled(p0, h1, ImGui::ColorConvertFloat4ToU32(accent), 6.0f, ImDrawFlags_RoundCornersTop);
    dl->AddRectFilled(ImVec2(p0.x, h1.y - 6.0f), h1, ImGui::ColorConvertFloat4ToU32(accent));

    // Başlık metni
    dl->AddText(ImVec2(p0.x + 8.0f * s.zoom(), p0.y + 5.0f * s.zoom()), IM_COL32(12, 14, 18, 255),
                t->displayName.c_str());

    // Giriş portları (sol)
    float rowY = p0.y + kHeaderH * s.zoom() + 4.0f * s.zoom();
    for (const auto& port : t->inputs) {
      const ImVec2 dot(p0.x, rowY + kRowH * 0.5f * s.zoom());
      const bool connected = s.graph().edgeInto(gpi::PortKey{r.nodeId, port.name}) != nullptr;
      ImVec4 col = port.kind == gpi::PortKind::Exec
                       ? ImVec4(0.35f, 0.85f, 0.55f, 1.0f)
                       : (port.required && !connected ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f)
                                                     : ImVec4(0.75f, 0.78f, 0.85f, 1.0f));
      // Denetim bu portta hata/uyarı bildiriyorsa rengi eziyoruz.
      const ImU32 pl = problemColor(levelOfPort(app, gpi::PortKey{r.nodeId, port.name}));
      if (pl) col = ImGui::ColorConvertU32ToFloat4(pl);
      dl->AddCircleFilled(dot, kPortDot * s.zoom() * 0.5f, ImGui::ColorConvertFloat4ToU32(col));
      dl->AddText(ImVec2(p0.x + 12.0f * s.zoom(), dot.y - 7.0f), IM_COL32(200, 208, 220, 255),
                  port.displayName.c_str());
      rowY += kRowH * s.zoom();
    }

    // Çıkış portları (sağ)
    float outY = p0.y + kHeaderH * s.zoom() + 4.0f * s.zoom();
    for (const auto& port : t->outputs) {
      const ImVec2 dot(p1.x, outY + kRowH * 0.5f * s.zoom());
      const bool connected = !s.graph().edgesFrom(gpi::PortKey{r.nodeId, port.name}).empty();
      ImVec4 col = port.kind == gpi::PortKind::Exec
                       ? ImVec4(0.35f, 0.85f, 0.55f, 1.0f)
                       : (connected ? ImVec4(0.30f, 0.62f, 1.0f, 1.0f)
                                    : ImVec4(0.55f, 0.60f, 0.70f, 1.0f));
      const ImU32 pl = problemColor(levelOfPort(app, gpi::PortKey{r.nodeId, port.name}));
      if (pl) col = ImGui::ColorConvertU32ToFloat4(pl);
      dl->AddCircleFilled(dot, kPortDot * s.zoom() * 0.5f, ImGui::ColorConvertFloat4ToU32(col));
      // sağa yaslı metin
      const ImVec2 ts = ImGui::CalcTextSize(port.displayName.c_str());
      dl->AddText(ImVec2(p1.x - 12.0f * s.zoom() - ts.x, dot.y - 7.0f), IM_COL32(200, 208, 220, 255),
                  port.displayName.c_str());
      outY += kRowH * s.zoom();
    }

    // Seçili düğümün ID'si
    if (sel) {
      dl->AddText(ImVec2(p0.x, p1.y + 4.0f), IM_COL32(120, 130, 145, 255), n->id.c_str());
    }

    // Hata/uyarı rozeti: kaç sorun var, tek bakışta görünsün.
    if (nodeLevel != gpi::ProblemLevel::Note) {
      int count = 0;
      for (const auto& d : app.analysis().diagnostics) {
        if (d.nodeId == r.nodeId || d.fromNode == r.nodeId) ++count;
      }
      char buf[16];
      std::snprintf(buf, sizeof(buf), "%d", count);
      const ImVec2 ts = ImGui::CalcTextSize(buf);
      const ImVec2 badgeMin(p1.x - ts.x - 14.0f * s.zoom(), p0.y + 4.0f * s.zoom());
      const ImVec2 badgeMax(p1.x - 4.0f * s.zoom(), p0.y + 4.0f * s.zoom() + ts.y + 4.0f);
      dl->AddRectFilled(badgeMin, badgeMax, problem, 4.0f);
      dl->AddText(ImVec2(badgeMin.x + 5.0f, badgeMin.y + 2.0f), IM_COL32(20, 20, 24, 255), buf);
    }

    // Kod panelinden gelen odaklamanın kırmızı parlaması
    if (!flashNode_.empty() && flashNode_ == r.nodeId && ImGui::GetTime() < flashUntil_) {
      const float remain = static_cast<float>(flashUntil_ - ImGui::GetTime());
      const float a = std::min(1.0f, remain);
      dl->AddRect(p0 - ImVec2(3, 3), p1 + ImVec2(3, 3),
                  IM_COL32(255, 86, 86, static_cast<int>(200.0f * a)), 9.0f, 0, 3.0f);
    }

    // Fare düğümün üstündeyse denetim mesajlarını göster.
    if (ImGui::IsMouseHoveringRect(p0, p1, true)) {
      std::vector<std::string> msgs;
      for (const auto& d : app.analysis().diagnostics) {
        if (d.nodeId == r.nodeId || d.fromNode == r.nodeId) msgs.push_back(d.message);
      }
      if (!msgs.empty()) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(t->displayName.c_str());
        ImGui::Separator();
        for (const auto& m : msgs) ImGui::TextWrapped("%s", m.c_str());
        ImGui::EndTooltip();
      }
    }
  }
  (void)origin;
}

// --------------------------------------------------------------- etkileşim

void CanvasPanel::handleInteraction(App& app) {
  EditorState& s = app.state();
  const ImVec2 mouse = ImGui::GetMousePos();
  const bool inCanvas = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(
                                                            ImGui::GetWindowPos(),
                                                            ImGui::GetWindowPos() +
                                                                ImGui::GetWindowSize());

  // --- Sürükleme bitişi ---
  if (dragNode_.active && ImGui::IsMouseDragging(0)) {
    float gx, gy;
    s.screenToGraph(mouse, gx, gy);
    gpi::NodeInstance* n = s.graph().findNode(dragNode_.nodeId);
    if (n) {
      n->x = gx - dragNode_.dx;
      n->y = gy - dragNode_.dy;
      s.invalidateLayouts();
    }
  }
  if (dragNode_.active && !ImGui::IsMouseDown(0)) {
    dragNode_.active = false;
    s.markDirty();
  }

  // --- Kablo bırakma ---
  if (dragLink_.active && !ImGui::IsMouseDown(0)) {
    dragLink_.active = false;
    // Hedef port bul
    for (const auto& r : rects_) {
      const gpi::NodeInstance* n = s.graph().findNode(r.nodeId);
      if (!n) continue;
      const gpi::NodeType* t = s.catalog().find(n->typeId);
      if (!t) continue;
      // giriş portları
      for (const auto& port : t->inputs) {
        ImVec2 pos;
        if (!portPosition(s, gpi::PortKey{r.nodeId, port.name}, pos)) continue;
        if ((mouse.x - pos.x) * (mouse.x - pos.x) + (mouse.y - pos.y) * (mouse.y - pos.y) < 144.0f) {
          if (dragLink_.fromOutput) {
            std::string err;
            s.beginTransaction("Kablo bağla");
            if (s.connect(dragLink_.from, gpi::PortKey{r.nodeId, port.name}, &err)) {
              s.commitTransaction();
            } else {
              s.cancelTransaction();
              linkError_ = err;
            }
          } else {
            std::string err;
            s.beginTransaction("Kablo bağla");
            if (s.connect(gpi::PortKey{r.nodeId, port.name}, dragLink_.from, &err)) {
              s.commitTransaction();
            } else {
              s.cancelTransaction();
              linkError_ = err;
            }
          }
          return;
        }
      }
    }
  }

  if (!inCanvas) return;

  // --- Sağ tık menüsü: düğüm ekle ---
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && (ImGui::GetIO().KeyCtrl)) {
    if (ImGui::BeginPopup("##dugumEkle")) {
      std::string picked;
      for (const auto& [id, t] : s.catalog().all()) {
        if (ImGui::MenuItem(t.displayName.c_str())) picked = id;
      }
      ImGui::EndPopup();
      if (!picked.empty()) {
        float gx, gy;
        s.screenToGraph(mouse, gx, gy);
        s.beginTransaction("Düğüm ekle");
        s.addNode(picked, gx - 60.0f, gy - 20.0f);
        s.commitTransaction();
      }
    }
  }

  // --- Sol tık: port/düğüm seçimi, sürükleme ---
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(0)) {
    bool consumed = false;
    for (const auto& r : rects_) {
      const gpi::NodeInstance* n = s.graph().findNode(r.nodeId);
      if (!n) continue;
      const gpi::NodeType* t = s.catalog().find(n->typeId);
      if (!t) continue;
      // çıkış portu
      for (const auto& port : t->outputs) {
        ImVec2 pos;
        if (!portPosition(s, gpi::PortKey{r.nodeId, port.name}, pos)) continue;
        if ((mouse.x - pos.x) * (mouse.x - pos.x) + (mouse.y - pos.y) * (mouse.y - pos.y) < 100.0f) {
          dragLink_.from = gpi::PortKey{r.nodeId, port.name};
          dragLink_.active = true;
          dragLink_.fromOutput = true;
          consumed = true;
          break;
        }
      }
      if (consumed) break;
      // giriş portu
      for (const auto& port : t->inputs) {
        ImVec2 pos;
        if (!portPosition(s, gpi::PortKey{r.nodeId, port.name}, pos)) continue;
        if ((mouse.x - pos.x) * (mouse.x - pos.x) + (mouse.y - pos.y) * (mouse.y - pos.y) < 100.0f) {
          if (const gpi::Edge* e = s.graph().edgeInto(gpi::PortKey{r.nodeId, port.name})) {
            // kabloyu sök, yeniden bağlanabilir
            gpi::Edge copy = *e;
            s.beginTransaction("Kablo sök");
            s.removeEdge(copy);
            s.commitTransaction();
            dragLink_.from = copy.from;
            dragLink_.active = true;
            dragLink_.fromOutput = true;
          }
          consumed = true;
          break;
        }
      }
      if (consumed) break;
    }
    if (!consumed) {
      // düğüm gövdesi
      bool onNode = false;
      for (const auto& r : rects_) {
        float sx, sy;
        s.graphToScreen(r.x, r.y, sx, sy);
        const ImVec2 p0(sx, sy);
        const ImVec2 p1(sx + r.w * s.zoom(), sy + r.h * s.zoom());
        if (mouse.x >= p0.x && mouse.x <= p1.x && mouse.y >= p0.y && mouse.y <= p1.y) {
          s.select(r.nodeId);
          float gx, gy;
          s.screenToGraph(mouse, gx, gy);
          dragNode_.nodeId = r.nodeId;
          dragNode_.dx = gx - r.x;
          dragNode_.dy = gy - r.y;
          dragNode_.active = true;
          onNode = true;
          break;
        }
      }
      if (!onNode) {
        s.clearSelection();
        // Boş alana basıldı: sürükleyince tuvali kaydırmak için kol kur.
        panArm_ = true;
        panStart_ = mouse;
        panStartX_ = s.panX();
        panStartY_ = s.panY();
      }
    }
  }

  // --- Tuvali kaydırma: orta tuş, Win+sol VEYA boş alana basıp sürükleme ---
  const bool superPan = ImGui::GetIO().KeySuper != 0;
  const bool leftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
  const bool emptyPan = panArm_ && leftDown && !dragNode_.active && !dragLink_.active;
  if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || emptyPan ||
      (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && superPan)) {
    panning_ = true;
    s.setPan(panStartX_ + (mouse.x - panStart_.x), panStartY_ + (mouse.y - panStart_.y));
  }
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle) && !leftDown) panning_ = false;
  if (!leftDown) panArm_ = false;

  // --- Tekerlek: zoom ---
  if (ImGui::IsMouseHoveringRect(ImGui::GetWindowPos(),
                                 ImGui::GetWindowPos() + ImGui::GetWindowSize()) &&
      ImGui::GetIO().MouseWheel != 0.0f) {
    const float old = s.zoom();
    s.setZoom(old * (1.0f + ImGui::GetIO().MouseWheel * 0.12f));
    // imleci sabit tut
    float gx, gy;
    s.screenToGraph(mouse, gx, gy);
    s.graphToScreen(gx, gy, panStart_.x, panStart_.y);
    s.setPan(mouse.x - panStart_.x, mouse.y - panStart_.y);
  }
}

// --------------------------------------------------------------- kaplama

void CanvasPanel::drawOverlay(App& app) {
  EditorState& s = app.state();
  ImGui::SetCursorPos(ImVec2(10, 10));
  if (ImGui::SmallButton("Yakınlaştır +")) s.setZoom(s.zoom() * 1.15f);
  ImGui::SameLine();
  if (ImGui::SmallButton("Uzaklaştır -")) s.setZoom(s.zoom() / 1.15f);
  ImGui::SameLine();
  if (ImGui::SmallButton("Sığdır")) {
    s.setZoom(1.0f);
    anchorView(s);  // pan'ı tuvalin köşesine sabitle
  }
  ImGui::SameLine();
  {
    ImGui::Text("  %d%%", static_cast<int>(s.zoom() * 100));
  }

  if (!linkError_.empty()) {
    ImGui::SetCursorPos(ImVec2(10, ImGui::GetWindowHeight() - 60));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.45f, 1.0f));
    ImGui::TextWrapped("%s", linkError_.c_str());
    ImGui::PopStyleColor();
    if (ImGui::SmallButton("Kapat")) linkError_.clear();
  }

  // Seçili düğüm varsa silme ipucu
  if (!s.selected().empty()) {
    ImGui::SetCursorPos(ImVec2(10, ImGui::GetWindowHeight() - 30));
    ImGui::TextDisabled("%s seçili - Sil: çöp kutusu", s.selected().c_str());
  }

  // Boş graf: tek tıklık başlangıçlar sun ("özgürlükçü" akış).
  if (s.graph().nodes.empty()) {
    ImGui::SetCursorPos(ImVec2(12, 52));
    ImGui::BeginChild("##emptyhint", ImVec2(330, 0), ImGuiChildFlags_Borders);
    ImGui::Text("Henüz düğüm yok. Başlamak için:");
    ImGui::Separator();
    if (ImGui::Button("Örnek dize programını aç")) app.loadSampleProgram();
    ImGui::TextDisabled("     İki metni birleştir, büyük harfe çevir, yaz");
    if (ImGui::Button("Kataloğa göz at")) app.revealInCatalog();
    if (ImGui::Button("İlk metin düğümünü ekle")) {
      s.beginTransaction("Metin ekle");
      float gx, gy;
      s.screenToGraph(viewCenter_, gx, gy);
      const std::string id = s.addNode("lit.text", gx - 95, gy - 20);
      s.select(id);
      s.commitTransaction();
    }
    ImGui::EndChild();
  }
  (void)app;
}

}  // namespace shell
