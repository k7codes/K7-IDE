// Shell: uygulama durumu uygulaması
#include "shell/EditorState.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "gpi/Json.hpp"
#include "imgui.h"

namespace fs = std::filesystem;

namespace shell {

EditorState::EditorState() { catalog_.loadBuiltins(); }

std::string EditorState::serializeGraph() const {
  return gpi::graphToJson(graph_).dump(2);
}

bool EditorState::deserializeGraph(const std::string& text, std::string* error) {
  gpi::json::Value v = gpi::json::Value::parse(text, error);
  if (!v.isObject()) {
    if (error && error->empty()) *error = "Geçersiz graf JSON'u.";
    return false;
  }
  gpi::Graph g;
  if (!gpi::graphFromJson(v, g, error)) return false;
  graph_ = std::move(g);
  return true;
}

// ---------------------------------------------------------------- düzenleme

std::string EditorState::nextNodeId() {
  return "n" + std::to_string(++idCounter_);
}

std::string EditorState::addNode(const std::string& typeId, float x, float y) {
  const gpi::NodeType* t = catalog_.find(typeId);
  gpi::NodeInstance n;
  n.id = nextNodeId();
  n.typeId = typeId;
  n.x = x;
  n.y = y;
  // Varsayılan literal'ları doldur
  if (t) catalog_.applyDefaults(*t, n);
  // Kimlik çakışmasını önle
  while (graph_.findNode(n.id)) {
    n.id = nextNodeId();
  }
  graph_.nodes.push_back(std::move(n));
  invalidateLayouts();
  markDirty();
  return graph_.nodes.back().id;
}

void EditorState::removeNode(const std::string& id) {
  graph_.removeNode(id);
  if (selected_ == id) selected_ = "";
  invalidateLayouts();
  markDirty();
}

void EditorState::removeEdge(const gpi::Edge& e) {
  graph_.removeEdge(e);
  invalidateLayouts();
  markDirty();
}

void EditorState::disconnectInput(const gpi::PortKey& input) {
  const gpi::Edge* e = graph_.edgeInto(input);
  if (!e) return;
  // Bağlantı kalkınca port varsayılanına döner
  graph_.removeEdge(*e);
  invalidateLayouts();
  markDirty();
}

bool EditorState::connect(const gpi::PortKey& from, const gpi::PortKey& to, std::string* error) {
  if (from.node == to.node) {
    if (error) *error = "Bir düğüm kendine bağlanamaz.";
    return false;
  }
  const gpi::NodeInstance* sn = graph_.findNode(from.node);
  const gpi::NodeInstance* dn = graph_.findNode(to.node);
  if (!sn || !dn) {
    if (error) *error = "Düğüm bulunamadı.";
    return false;
  }
  const gpi::NodeType* st = catalog_.find(sn->typeId);
  const gpi::NodeType* dt = catalog_.find(dn->typeId);
  if (!st || !dt) {
    if (error) *error = "Bilinmeyen düğüm tipi.";
    return false;
  }
  const gpi::PortDef* sp = st->output(from.port);
  const gpi::PortDef* dp = dt->input(to.port);
  if (!sp || !dp) {
    if (error) *error = "Port bulunamadı.";
    return false;
  }
  // Girişe ikinci kablo bağlanmaz
  if (graph_.edgeInto(to)) {
    if (error) *error = "Bu giriş zaten bağlı.";
    return false;
  }
  // Exec girişleri birden fazla kablo kabul eder (birleştirme noktası)
  const gpi::Edge e{from, to};
  if (std::find(graph_.edges.begin(), graph_.edges.end(), e) != graph_.edges.end()) return true;
  graph_.edges.push_back(e);
  invalidateLayouts();
  markDirty();
  return true;
}

// ---------------------------------------------------------------- undo/redo

void EditorState::pushUndo(const std::string& label) {
  undo_.emplace_back(label, serializeGraph());
  if (undo_.size() > 100) undo_.pop_front();
  ++undoDepth_;
  redo_.clear();
  lastRedoLabel_.clear();
}

void EditorState::beginTransaction(const std::string& label) {
  if (inTransaction_) return;
  inTransaction_ = true;
  transactionLabel_ = label;
  transactionSnapshot_ = serializeGraph();
}

void EditorState::commitTransaction() {
  if (!inTransaction_) return;
  inTransaction_ = false;
  if (transactionSnapshot_ == serializeGraph()) return;  // değişiklik yok
  undo_.emplace_back(transactionLabel_, transactionSnapshot_);
  if (undo_.size() > 100) undo_.pop_front();
  ++undoDepth_;
  redo_.clear();
  invalidateLayouts();
  ++revision_;
}

void EditorState::cancelTransaction() {
  if (!inTransaction_) return;
  inTransaction_ = false;
  if (!deserializeGraph(transactionSnapshot_, nullptr)) markDirty();
}

const std::string& EditorState::undoLabel() const {
  return undo_.empty() ? lastUndoLabel_ : undo_.back().first;
}

const std::string& EditorState::redoLabel() const {
  return redo_.empty() ? lastRedoLabel_ : redo_.back().first;
}

void EditorState::undo() {
  if (undo_.empty()) return;
  redo_.emplace_back(undo_.back().first, serializeGraph());
  const std::string target = undo_.back().second;
  const std::string label = undo_.back().first;
  undo_.pop_back();
  if (--undoDepth_ < 0) undoDepth_ = 0;
  if (!deserializeGraph(target, nullptr)) return;
  lastUndoLabel_ = label;
  lastRedoLabel_ = redo_.back().first;
  selected_.clear();
  invalidateLayouts();
  markDirty();
}

void EditorState::redo() {
  if (redo_.empty()) return;
  undo_.emplace_back(redo_.back().first, serializeGraph());
  const std::string target = redo_.back().second;
  const std::string label = redo_.back().first;
  redo_.pop_back();
  ++undoDepth_;
  if (!deserializeGraph(target, nullptr)) return;
  lastRedoLabel_ = label;
  lastUndoLabel_ = undo_.back().first;
  selected_.clear();
  invalidateLayouts();
  markDirty();
}

// ---------------------------------------------------------------- seçim

void EditorState::select(const std::string& id) { selected_ = id; }
void EditorState::clearSelection() { selected_.clear(); }
void EditorState::addToSelection(const std::string& id) { selected_ = id; }

// ---------------------------------------------------------------- görünüm

void EditorState::screenToGraph(const ImVec2& p, float& gx, float& gy) const {
  gx = (p.x - panX_) / zoom_;
  gy = (p.y - panY_) / zoom_;
}

void EditorState::graphToScreen(float gx, float gy, float& sx, float& sy) const {
  sx = gx * zoom_ + panX_;
  sy = gy * zoom_ + panY_;
}

// ---------------------------------------------------------------- proje

void EditorState::setLanguage(gpi::TargetLang l) {
  options_.lang = l;
  markDirty();
}

bool EditorState::newProject(const std::string& appName) {
  graph_ = gpi::Graph{};
  graph_.name = appName;
  options_.appName = appName;
  options_.rootNamespace = appName;
  filePath_.clear();
  undo_.clear();
  redo_.clear();
  undoDepth_ = 0;
  idCounter_ = 0;
  selected_.clear();
  dirty_ = false;
  invalidateLayouts();
  ++revision_;
  return true;
}

bool EditorState::load(const std::string& path, std::string* error) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    if (error) *error = "Dosya açılamadı: " + path;
    return false;
  }
  std::stringstream ss;
  ss << f.rdbuf();
  if (!deserializeGraph(ss.str(), error)) return false;
  filePath_ = path;
  options_.appName = graph_.name;
  undo_.clear();
  redo_.clear();
  undoDepth_ = 0;
  selected_.clear();
  dirty_ = false;
  invalidateLayouts();
  ++revision_;
  return true;
}

bool EditorState::save(const std::string& path, std::string* error) {
  std::error_code ec;
  const fs::path p(path);
  if (p.has_parent_path() && !p.parent_path().empty()) {
    fs::create_directories(p.parent_path(), ec);
  }
  std::ofstream f(path, std::ios::binary);
  if (!f) {
    if (error) *error = "Dosya yazılamadı: " + path;
    return false;
  }
  f << serializeGraph();
  if (!f) {
    if (error) *error = "Yazma hatası.";
    return false;
  }
  filePath_ = path;
  dirty_ = false;
  return true;
}

void EditorState::loadSample(const std::string& appName) {
  newProject(appName);
  beginTransaction("Örnek program");

  auto node = [&](const std::string& typeId, float x, float y) {
    return addNode(typeId, x, y);
  };
  auto lit = [&](const std::string& id, const std::string& value) {
    gpi::NodeInstance* n = graph_.findNode(id);
    if (n) n->literals["Value"] = value;
  };

  const std::string t0 = node("lit.text", 0, 0);
  lit(t0, "\"Merhaba, \"");
  const std::string t1 = node("lit.text", 0, 160);
  lit(t1, "\"görsel dünya!\"");
  const std::string c0 = node("str.concat", 240, 60);
  const std::string u0 = node("str.upper", 470, 60);
  const std::string p0 = node("io.print", 700, 60);

  std::string err;
  connect(gpi::PortKey{t0, "Value"}, gpi::PortKey{c0, "A"}, &err);
  connect(gpi::PortKey{t1, "Value"}, gpi::PortKey{c0, "B"}, &err);
  connect(gpi::PortKey{c0, "Result"}, gpi::PortKey{u0, "Value"}, &err);
  connect(gpi::PortKey{u0, "Result"}, gpi::PortKey{p0, "Value"}, &err);

  select(p0);
  commitTransaction();
  invalidateLayouts();
}

}  // namespace shell
