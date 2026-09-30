#include "gpi/Graph.hpp"

#include <algorithm>
#include <functional>
#include <queue>
#include <set>
#include <unordered_map>

#include "gpi/NodeCatalog.hpp"

namespace gpi {

const NodeInstance* Graph::findNode(const std::string& id) const {
  for (const auto& n : nodes) {
    if (n.id == id) return &n;
  }
  return nullptr;
}

NodeInstance* Graph::findNode(const std::string& id) {
  for (auto& n : nodes) {
    if (n.id == id) return &n;
  }
  return nullptr;
}

const Edge* Graph::edgeInto(const PortKey& input) const {
  for (const auto& e : edges) {
    if (e.to == input) return &e;
  }
  return nullptr;
}

std::vector<const Edge*> Graph::edgesFrom(const PortKey& output) const {
  std::vector<const Edge*> out;
  for (const auto& e : edges) {
    if (e.from == output) out.push_back(&e);
  }
  return out;
}

void Graph::removeNode(const std::string& id) {
  nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
                            [&](const NodeInstance& n) { return n.id == id; }),
              nodes.end());
  edges.erase(std::remove_if(edges.begin(), edges.end(),
                             [&](const Edge& e) { return e.from.node == id || e.to.node == id; }),
              edges.end());
  entryNodes.erase(std::remove(entryNodes.begin(), entryNodes.end(), id), entryNodes.end());
}

void Graph::removeEdge(const Edge& e) { edges.erase(std::remove(edges.begin(), edges.end(), e), edges.end()); }

bool Graph::validate(std::vector<std::string>& errors) const {
  errors.clear();
  std::set<std::string> ids;
  for (const auto& n : nodes) {
    if (n.id.empty()) {
      errors.push_back("Kimliksiz düğüm var.");
    } else if (!ids.insert(n.id).second) {
      errors.push_back("Yinelenen düğüm kimliği: " + n.id);
    }
  }
  for (const auto& e : edges) {
    if (!ids.count(e.from.node)) errors.push_back("Kablo kaynak düğümü yok: " + e.from.node);
    if (!ids.count(e.to.node)) errors.push_back("Kablo hedef düğümü yok: " + e.to.node);
  }
  return errors.empty();
}

namespace {

struct Dep {
  std::string id;
  int deps = 0;
};

// r.errors / r.warnings içine ekleme yapan ve aynı anda konumlu tanı üreten
// kısayol. Böylece hata metni ile düğüm/port eşlemesi birbirinden ayrılamaz.
void addProblem(AnalysisResult& r, ProblemLevel level, std::string nodeId,
                std::string portName, std::string fromNode, std::string fromPort,
                std::string message) {
  if (level == ProblemLevel::Error) {
    r.errors.push_back(message);
    r.ok = false;
  } else if (level == ProblemLevel::Warning) {
    r.warnings.push_back(message);
  } else {
    return;
  }
  NodeDiagnostic d;
  d.level = level;
  d.nodeId = std::move(nodeId);
  d.portName = std::move(portName);
  d.fromNode = std::move(fromNode);
  d.fromPort = std::move(fromPort);
  d.message = std::move(message);
  r.diagnostics.push_back(std::move(d));
}

}  // namespace

ProblemLevel AnalysisResult::levelOfNode(const std::string& nodeId) const {
  ProblemLevel worst = ProblemLevel::Note;
  for (const auto& d : diagnostics) {
    if (d.nodeId != nodeId && d.fromNode != nodeId) continue;
    if (static_cast<int>(d.level) > static_cast<int>(worst)) worst = d.level;
  }
  return worst;
}

ProblemLevel AnalysisResult::levelOfPort(const std::string& nodeId,
                                        const std::string& portName) const {
  ProblemLevel worst = ProblemLevel::Note;
  for (const auto& d : diagnostics) {
    const bool hit = (d.nodeId == nodeId && d.portName == portName) ||
                     (d.fromNode == nodeId && d.fromPort == portName);
    if (!hit) continue;
    if (static_cast<int>(d.level) > static_cast<int>(worst)) worst = d.level;
  }
  return worst;
}

std::vector<std::string> AnalysisResult::messagesForNode(const std::string& nodeId) const {
  std::vector<std::string> out;
  for (const auto& d : diagnostics) {
    if (d.nodeId == nodeId || d.fromNode == nodeId) out.push_back(d.message);
  }
  return out;
}

bool AnalysisResult::anyError() const {
  for (const auto& d : diagnostics) {
    if (d.isError()) return true;
  }
  return false;
}

AnalysisResult analyze(const Graph& g, const NodeCatalog& catalog) {
  AnalysisResult r;
  r.ok = true;

  std::vector<std::string> structural;
  if (!g.validate(structural)) {
    r.ok = false;
    r.errors.insert(r.errors.end(), structural.begin(), structural.end());
  }

  std::unordered_map<std::string, int> indeg;
  std::unordered_map<std::string, std::vector<std::string>> adj;

  for (const auto& n : g.nodes) indeg[n.id] = 0;

  // Düğüm tipleri var mı? (kenaro bağlı olmayan düğümler de denetlenir)
  for (const auto& n : g.nodes) {
    if (catalog.find(n.typeId)) continue;
    addProblem(r, ProblemLevel::Error, n.id, "", "", "", "Bilinmeyen düğüm tipi: " + n.typeId);
  }

  // Zorunlu girişler bağlanmış mı? (required = true olan portlar)
  for (const auto& n : g.nodes) {
    const NodeType* nt = catalog.find(n.typeId);
    if (!nt) continue;
    for (const auto& p : nt->inputs) {
      if (!p.required) continue;
      if (g.edgeInto(PortKey{n.id, p.name})) continue;
      addProblem(r, ProblemLevel::Error, n.id, p.name, "", "",
                 nt->displayName + " düğümünde '" + p.displayName +
                     "' girişi bağlanmamış. Bu portu bağla ya da Detaylar "
                     "panelinden değer gir.");
    }
  }

  for (const auto& e : g.edges) {
    // Port varlık kontrolü
    const NodeInstance* src = g.findNode(e.from.node);
    const NodeInstance* dst = g.findNode(e.to.node);
    if (!src || !dst) continue;
    const NodeType* st = catalog.find(src->typeId);
    const NodeType* dt = catalog.find(dst->typeId);
    if (!st) {
      addProblem(r, ProblemLevel::Error, src->id, "", "", "",
                 "Bilinmeyen düğüm tipi: " + src->typeId);
      continue;
    }
    if (!dt) {
      addProblem(r, ProblemLevel::Error, dst->id, "", "", "",
                 "Bilinmeyen düğüm tipi: " + dst->typeId);
      continue;
    }
    const PortDef* sp = st->output(e.from.port);
    const PortDef* dp = dt->input(e.to.port);
    if (!sp) {
      addProblem(r, ProblemLevel::Error, src->id, e.from.port, "", "",
                 src->typeId + " düğümünde çıkış portu yok: " + e.from.port);
      continue;
    }
    if (!dp) {
      addProblem(r, ProblemLevel::Error, dst->id, e.to.port, e.from.node, e.from.port,
                 dst->typeId + " düğümünde giriş portu yok: " + e.to.port);
      continue;
    }
    if (sp->kind != dp->kind) {
      const std::string isWhat = sp->kind == PortKind::Exec ? "akış (Exec)" : "değer";
      const std::string wantWhat = dp->kind == PortKind::Exec ? "akış (Exec)" : "değer";
      addProblem(r, ProblemLevel::Error, dst->id, e.to.port, src->id, e.from.port,
                 "Tür uyuşmazlığı (akış/değer): " + src->id + "." + e.from.port +
                     " bir " + isWhat + " portu, ama " + dst->id + "." + e.to.port +
                     " bir " + wantWhat + " portu bekliyor.");
      continue;
    }

    if (sp->kind == PortKind::Value) {
      TypeSystem ts;
      Conversion c = ts.conversion(sp->type, dp->type);
      if (c == Conversion::None) {
        addProblem(r, ProblemLevel::Error, dst->id, e.to.port, src->id, e.from.port,
                   "Tip uyuşmazlığı: " + sp->type.toString() + " (" + src->id + "." +
                       e.from.port + ")  ➜  " + dp->type.toString() + " (" + dst->id + "." +
                       e.to.port + "). Araya uygun bir dönüştürücü ekle "
                       "(ör. Metin → Tam Sayı).");
      } else if (c == Conversion::Explicit) {
        addProblem(r, ProblemLevel::Warning, dst->id, e.to.port, src->id, e.from.port,
                   "Örtük dönüşüm: " + sp->type.toString() + " ➜ " + dp->type.toString() +
                       " (" + dst->id + "." + e.to.port + "). Derlemede otomatik çevrilecek.");
      }
    }

    adj[e.from.node].push_back(e.to.node);
    indeg[e.to.node]++;
  }

  // Akış (exec) portlarında tek çıkış kuralı uyarısı
  for (const auto& n : g.nodes) {
    const NodeType* nt = catalog.find(n.typeId);
    if (!nt) continue;
    for (const auto& o : nt->outputs) {
      if (o.kind != PortKind::Exec) continue;
      PortKey k{n.id, o.name};
      auto outs = g.edgesFrom(k);
      if (outs.size() > 1) {
        addProblem(r, ProblemLevel::Warning, n.id, o.name, "", "",
                   nt->displayName + " düğümünün '" + o.displayName +
                       " çıkışı birden fazla düğüme bağlı. Akış her ikisine de gider; "
                       "sırayla çalıştırmak için araya bir dallanma düğümü ekle.");
      }
    }
  }

  // Eksik literal uyarıları (port literal taşıyabilir ama boşsa uyar)
  for (const auto& n : g.nodes) {
    const NodeType* nt = catalog.find(n.typeId);
    if (!nt) continue;
    for (const auto& in : nt->inputs) {
      PortKey k{n.id, in.name};
      if (g.edgeInto(k)) continue;
      // Akış (exec) girişleri varsayılan taşımaz ve zincirin BİR yerde
      // başlaması gerekir; kök düğüm için "bağlanmadı" uyarısı yanlış olur.
      if (in.kind == PortKind::Exec) continue;
      if (in.defaultLiteral.empty() && !in.advanced) {
        addProblem(r, ProblemLevel::Warning, n.id, in.name, "", "",
                   nt->displayName + ": '" + in.displayName + "' bağlanmadı (varsayılan " +
                       TypeSystem().names(in.type).csharpLiteral + " kullanılacak).");
        r.usedLiterals.push_back(k);
      }
    }
  }

  // Topolojik sıralama (kararlı olsun: kimliğe göre)
  std::vector<std::string> ready;
  std::unordered_map<std::string, int> local = indeg;
  for (const auto& n : g.nodes) {
    if (local[n.id] == 0) ready.push_back(n.id);
  }
  std::sort(ready.begin(), ready.end());

  std::size_t index = 0;
  while (index < ready.size()) {
    const std::string cur = ready[index++];
    r.executionOrder.push_back(cur);
    std::vector<std::string> next = adj[cur];
    std::sort(next.begin(), next.end());
    for (const auto& nx : next) {
      if (--local[nx] == 0) ready.push_back(nx);
    }
  }

  if (r.executionOrder.size() != g.nodes.size()) {
    std::vector<std::string> cyclic;
    for (const auto& n : g.nodes) {
      if (local[n.id] > 0) cyclic.push_back(n.id);
    }
    std::sort(cyclic.begin(), cyclic.end());
    std::string list;
    for (std::size_t i = 0; i < cyclic.size(); ++i) {
      if (i) list += ", ";
      list += cyclic[i];
    }
    // Döngüdeki HER düğüm hatadır: arayüz hepsini kırmızı vurgulayabilsin.
    // (addProblem ilk çağrıda r.ok=false yapıyor, sonrakiler de hata olarak eklenir.)
    for (const auto& id : cyclic) {
      addProblem(r, ProblemLevel::Error, id, "", "", "",
                 "Döngüsel bağımlılık: " + id +
                     " düğümü kendisine ya da başka bir döngüdeki düğüme bağımlı. "
                     "Program hangisinin önce çalışacağına karar veremez.");
    }
  }

  return r;
}

}  // namespace gpi
