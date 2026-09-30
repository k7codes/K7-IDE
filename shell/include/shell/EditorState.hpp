// Shell: uygulama durumu - graf, seçim, geri al/yinele, proje ayarları
#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "gpi/Codegen.hpp"
#include "gpi/Graph.hpp"
#include "gpi/NodeCatalog.hpp"
#include "gpi/Serialization.hpp"
#include "gpi/Toolchain.hpp"

struct ImVec2;

namespace shell {

// Düğümün ekrandaki dikdörtgeni (tuval koordinatı, panelle birlikte kayar)
struct NodeRect {
  std::string nodeId;
  float x = 0, y = 0, w = 0, h = 0;
};

// Bağlantı çizim durumu
struct DragLink {
  gpi::PortKey from;     // kaynak port
  float startX = 0, startY = 0;
  bool active = false;
  bool fromOutput = true;
};

// Bir düğümü sürükleme
struct DragNode {
  std::string nodeId;
  float dx = 0, dy = 0;
  bool active = false;
};

class EditorState {
 public:
  EditorState();

  // --- Katalog ---
  const gpi::NodeCatalog& catalog() const { return catalog_; }
  std::vector<gpi::NodeCategory> categories() const { return catalog_.categories(); }

  // --- Graf düzenleme ---
  gpi::Graph& graph() { return graph_; }
  const gpi::Graph& graph() const { return graph_; }

  // Yeni düğüm ekler ve kimliğini döndürür.
  std::string addNode(const std::string& typeId, float x, float y);
  void removeNode(const std::string& id);
  void removeEdge(const gpi::Edge& e);
  // Port uyumluluğu kontrolü yaparak kablo ekler; uyumsuzsa hata döner.
  bool connect(const gpi::PortKey& from, const gpi::PortKey& to, std::string* error);
  // Bağlı olan girişteki kabloyu kaldırıp literal'a döner.
  void disconnectInput(const gpi::PortKey& input);

  // --- Geri al / yinele ---
  void beginTransaction(const std::string& label);
  void commitTransaction();
  void cancelTransaction();
  bool canUndo() const { return undoDepth_ > 0; }
  bool canRedo() const { return !redo_.empty(); }
  const std::string& undoLabel() const;
  const std::string& redoLabel() const;
  void undo();
  void redo();

  // --- Seçim ---
  void select(const std::string& id);
  void clearSelection();
  const std::string& selected() const { return selected_; }
  bool isSelected(const std::string& id) const { return selected_ == id; }
  void addToSelection(const std::string& id);

  // --- Görünüm ---
  float panX() const { return panX_; }
  float panY() const { return panY_; }
  void setPan(float x, float y) { panX_ = x; panY_ = y; }
  float zoom() const { return zoom_; }
  void setZoom(float z) {
    if (z < 0.35f) z = 0.35f;
    if (z > 2.5f) z = 2.5f;
    zoom_ = z;
  }
  void screenToGraph(const ImVec2& p, float& gx, float& gy) const;
  void graphToScreen(float gx, float gy, float& sx, float& sy) const;

  // --- Proje ---
  gpi::ProjectOptions& options() { return options_; }
  const gpi::ProjectOptions& options() const { return options_; }
  void setLanguage(gpi::TargetLang l);
  gpi::TargetLang language() const { return options_.lang; }

  // --- Dosya ---
  bool newProject(const std::string& appName);
  bool load(const std::string& path, std::string* error);
  bool save(const std::string& path, std::string* error);
  const std::string& filePath() const { return filePath_; }
  bool dirty() const { return dirty_; }
  // Gerçek operatörlerden oluşan örnek dize programını kurar (tek geri alma).
  void loadSample(const std::string& appName);
  // Graf üzerinde yapılan HER değişiklikte artar. Kod üretimi / hata denetimi
  // yalnızca bu sayı değiştiğinde yeniden çalışır; böylece her karede değil,
  // gerçekten bir şey değiştiğinde iş yapılır.
  void markDirty() {
    dirty_ = true;
    ++revision_;
  }
  std::uint64_t revision() const { return revision_; }

  // Kimlik üretimi (n1, n2, ...)
  std::string nextNodeId();

  // Düğüm ekran kutusu önbelleğini temizler (yerleşim değiştiğinde)
  void invalidateLayouts() { layoutDirty_ = true; }
  bool layoutsDirty() const { return layoutDirty_; }
  void clearLayoutDirty() { layoutDirty_ = false; }
  const std::vector<NodeRect>& rects() const { return rects_; }
  std::vector<NodeRect>& rects() { return rects_; }

 private:
  void pushUndo(const std::string& label);
  std::string serializeGraph() const;
  bool deserializeGraph(const std::string& text, std::string* error);

  gpi::NodeCatalog catalog_;
  gpi::Graph graph_;
  gpi::ProjectOptions options_;

  std::deque<std::pair<std::string, std::string>> undo_;  // (etiket, json)
  std::vector<std::pair<std::string, std::string>> redo_;
  int undoDepth_ = 0;
  std::string lastUndoLabel_;
  std::string lastRedoLabel_;

  bool inTransaction_ = false;
  std::string transactionLabel_;
  std::string transactionSnapshot_;

  std::string selected_;
  std::string filePath_;
  bool dirty_ = false;

  float panX_ = 0, panY_ = 0;
  float zoom_ = 1.0f;

  std::vector<NodeRect> rects_;
  bool layoutDirty_ = true;
  long idCounter_ = 0;
  std::uint64_t revision_ = 0;
};

}  // namespace shell
