// Shell: grafik tuval paneli - düğüm/kablo çizimi ve düzenleme
#pragma once

#include <string>
#include <vector>

#include "shell/Panel.hpp"
#include "shell/EditorState.hpp"

namespace shell {

class CanvasPanel : public Panel {
 public:
  void draw(App& app) override;

  // Tuvalin görünen merkezi (düğüm eklemede kullanılır)
  ImVec2 viewCenter() const { return viewCenter_; }
  // Düğümün port koordinatları (kablo çizimi için)
  bool portPosition(EditorState& s, const gpi::PortKey& port, ImVec2& out) const;
  // Pan'ı içeriğin sol üst köşesine yeniden sabitler (örnek yüklenince).
  void resetView() { viewAnchored_ = false; }

 private:
  void layout(EditorState& s);
  void drawGrid(EditorState& s, ImDrawList* dl, const ImVec2& origin, const ImVec2& size);
  void drawEdges(App& app);
  void drawNodes(App& app);
  void handleInteraction(App& app);
  void drawOverlay(App& app);
  // Pan'ı tuvalin sol üst köşesine sabitler (graf koordinatları ekrana
  // göre değil, tuvalin içeriğine göre tanımlıdır).
  void anchorView(EditorState& s);
  // Kod panelinden gelen "şu düğüme odaklan" isteğini uygular.
  void consumeReveal(App& app);

  std::vector<NodeRect> rects_;
  DragNode dragNode_;
  DragLink dragLink_;
  bool panning_ = false;
  // Boş alana sol tık basıldığında doğrulanır; sürükleyince tuval kayar.
  bool panArm_ = false;
  ImVec2 panStart_{0, 0};
  float panStartX_ = 0, panStartY_ = 0;
  ImVec2 viewCenter_{0, 0};
  // Tuval içeriğinin ekran koordinatları (çizim ve isabet testi için)
  ImVec2 contentOrigin_{0, 0};
  ImVec2 contentSize_{0, 0};
  bool viewAnchored_ = false;
  std::string linkError_;
  // Silme/kablo düzenleme için
  bool deleteMode_ = false;
  // Odaklanan düğümün kısa süreli kırmızı parlaması (Kod -> Graf gezinmesi)
  std::string flashNode_;
  double flashUntil_ = 0.0;
};

}  // namespace shell
