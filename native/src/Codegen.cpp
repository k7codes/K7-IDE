// Gözle Programlama IDE - Kod üretici
//
// Graf -> C# / C++ kaynak kodu. Akış (exec) portları gerçek yapılandırılmış
// kontrol akışına dönüştürülür: if/else, while, for, foreach, switch.
#include "gpi/Codegen.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <set>
#include <sstream>
#include <unordered_map>

namespace gpi {

namespace {

// Üretim sırasında düğümlerin önüne yazılan geçici işaretçi. Her iki hedef
// dilde de geçerli bir yorum olduğundan, silinemezse bile derlemeyi bozmaz.
constexpr std::string_view kNodeMarker = "//#gpi:";

std::string trim(const std::string& s) {
  std::size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}

std::string sanitize(std::string_view s) {
  std::string out;
  for (char c : s) {
    out += (std::isalnum(static_cast<unsigned char>(c)) || c == '_') ? c : '_';
  }
  if (out.empty()) out = "v";
  if (std::isdigit(static_cast<unsigned char>(out[0]))) out.insert(out.begin(), '_');
  return out;
}

}  // namespace

// ===========================================================================
//  Emitter
// ===========================================================================

class Emitter {
 public:
  Emitter(const Graph& g, const NodeCatalog& cat, const ProjectOptions& opt, Lang lang,
          GenResult* res)
      : g_(g), cat_(cat), opt_(opt), lang_(lang), ts_(), res_(res) {
    state_.graph = &g_;
    state_.catalog = &cat_;
    state_.types = &ts_;
    state_.lang = lang;
    state_.inlinePure = opt.inlinePureNodes;
    state_.withComments = opt.includeComments;

    for (const auto& n : g_.nodes) {
      if (const NodeType* nt = cat_.find(n.typeId)) {
        orderOf_[n.id] = 0;
        if (const NodeType* t = nt) {
          for (const auto& u : t->csUsings) res_->requiredUsings.push_back(u);
          if (!t->cppHeader.empty()) res_->requiredUsings.push_back("H:" + t->cppHeader);
        }
      }
    }
  }

  void run(std::string* body) {
    body_ = body;
    AnalysisResult an = analyze(g_, cat_);
    res_->warnings = an.warnings;
    res_->diagnostics = an.diagnostics;
    if (!an.ok) {
      res_->ok = false;
      res_->errors = an.errors;
      return;
    }
    for (std::size_t i = 0; i < an.executionOrder.size(); ++i) {
      orderOf_[an.executionOrder[i]] = static_cast<int>(i);
      ordered_.push_back(an.executionOrder[i]);
    }
    emitBody(std::set<std::string>(ordered_.begin(), ordered_.end()), 0);
    collectUsings();
    // Analiz geçti: gövde üretildi. Sarıcı (generate) en sonda
    // entryFunction/harita ekleyip r.ok = true yapacak.
    res_->ok = true;
  }

  [[nodiscard]] const ResolveState& state() const { return state_; }
  [[nodiscard]] const TypeSystem& types() const { return ts_; }
  [[nodiscard]] Lang lang() const { return lang_; }
  [[nodiscard]] std::vector<std::string> usingList() const {
    return {usings_.begin(), usings_.end()};
  }
  [[nodiscard]] std::vector<std::string> headerList() const {
    return {headers_.begin(), headers_.end()};
  }

 private:
  const Graph& g_;
  const NodeCatalog& cat_;
  const ProjectOptions& opt_;
  Lang lang_;
  TypeSystem ts_;
  GenResult* res_;
  std::string* body_ = nullptr;
  ResolveState state_;
  std::vector<std::string> ordered_;
  std::unordered_map<std::string, int> orderOf_;
  std::set<std::string> emitted_;
  std::set<std::string> usings_;
  std::set<std::string> headers_;
  int usedNames_ = 0;

  // --- yardımcılar ---
  std::vector<std::string> allIds() const { return ordered_; }

  std::vector<std::string> sortedByOrder(const std::set<std::string>& ids) const {
    std::vector<std::string> out(ids.begin(), ids.end());
    std::sort(out.begin(), out.end(), [&](const std::string& a, const std::string& b) {
      return orderOf_.at(a) < orderOf_.at(b);
    });
    return out;
  }

  // Bir exec çıkışının ilk hedefi
  std::string firstTarget(const std::string& nodeId, const std::string& port) const {
    for (const Edge* e : g_.edgesFrom(PortKey{nodeId, port})) return e->to.node;
    return {};
  }

  // Akış üzerinden erişilebilir düğümler (veri kabloları hariç)
  std::set<std::string> flowReachable(const std::string& root,
                                      const std::set<std::string>& stop = {}) const {
    std::set<std::string> seen;
    if (root.empty()) return seen;
    std::vector<std::string> stack{root};
    while (!stack.empty()) {
      const std::string cur = stack.back();
      stack.pop_back();
      if (seen.count(cur)) continue;
      if (!stop.empty() && stop.count(cur) && cur != root) continue;
      seen.insert(cur);
      const NodeInstance* n = g_.findNode(cur);
      if (!n) continue;
      const NodeType* nt = cat_.find(n->typeId);
      if (!nt) continue;
      for (const auto& o : nt->outputs) {
        if (o.kind != PortKind::Exec) continue;
        for (const Edge* e : g_.edgesFrom(PortKey{cur, o.name})) {
          if (!seen.count(e->to.node)) stack.push_back(e->to.node);
        }
      }
    }
    return seen;
  }

  // Başlangıç düğümleri: exec girişi bağlanmamış ya da exec girişi olmayan düğümler
  std::vector<std::string> rootNodes() const {
    std::vector<std::string> roots;
    for (const auto& n : g_.nodes) {
      const NodeType* nt = cat_.find(n.typeId);
      if (!nt) continue;
      bool hasExecIn = false;
      bool connected = false;
      for (const auto& in : nt->inputs) {
        if (in.kind != PortKind::Exec) continue;
        hasExecIn = true;
        if (g_.edgeInto(PortKey{n.id, in.name})) connected = true;
      }
      if (!hasExecIn || !connected) roots.push_back(n.id);
    }
    std::sort(roots.begin(), roots.end(),
              [&](const std::string& a, const std::string& b) { return orderOf_.at(a) < orderOf_.at(b); });
    return roots;
  }

  std::string uniqueName(std::string_view hint) {
    std::string base = sanitize(hint);
    std::string candidate = base;
    int i = 1;
    while (takenNames().count(candidate)) candidate = base + "_" + std::to_string(i++);
    return candidate;
  }

  std::set<std::string> takenNames() const {
    std::set<std::string> out;
    for (const auto& [k, v] : state_.materialized) {
      out.insert(v);
      (void)k;
    }
    return out;
  }

  void markUsed(const std::string& name) { state_.materialized[PortKey{"@" + name, ""}] = name; }

  EmitContext ctxFor(const NodeInstance& n) {
    return EmitContext(g_, n, ts_, lang_, cat_, body_, &state_);
  }

  void collectUsings() {
    for (const auto& u : res_->requiredUsings) {
      if (u.rfind("H:", 0) == 0) {
        headers_.insert(u.substr(2));
      } else if (!u.empty()) {
        usings_.insert(u);
      }
    }
    if (opt_.emitUsings) {
      if (lang_ == Lang::CSharp) {
        usings_.insert("System");
        usings_.insert("System.Collections.Generic");
      } else {
        headers_.insert("<iostream>");
        headers_.insert("<string>");
        headers_.insert("<vector>");
        headers_.insert("<map>");
        headers_.insert("<cstdint>");
      }
    }
  }

  // --- gövde üretimi ---
  void emitBody(const std::set<std::string>& set, int indent) {
    for (const std::string& id : sortedByOrder(set)) {
      if (emitted_.count(id)) continue;
      const NodeInstance* n = g_.findNode(id);
      if (!n) continue;
      const NodeType* nt = cat_.find(n->typeId);
      if (!nt) continue;
      if (nt->deprecated) continue;
      if (nt->isFlowControl) {
        emitControl(*n, *nt, indent);
        continue;
      }
      if (nt->pure && nt->emitExpr && opt_.inlinePureNodes) {
        // Saf düğüm: ifade kullanım yerinde gömülür, ayrı satır üretilmez.
        emitted_.insert(id);
        continue;
      }
      emitStatement(*n, *nt, indent);
    }
  }

  // Bir deyimin kodunu yazmadan hemen önce düğüm işaretçisi bırakır. İşaretçi
  // nihai koddan silinir ama önce satır numarasına çevrilir (bkz.
  // extractNodeLines). Böylece kod panelinden düğüme, tuvalden satıra
  // gezinme mümkün olur.
  void markStatement(const NodeInstance& n, EmitContext& c) {
    c.line(std::string(kNodeMarker) + n.id);
    state_.statementAnchor = n.id;
  }

  EmitContext makeFlowContext() {
    NodeInstance dummy;
    dummy.id = "@flow";
    dummy.typeId = "flow.comment";
    return EmitContext(g_, dummy, ts_, lang_, cat_, body_, &state_);
  }

  void emitStatement(const NodeInstance& n, const NodeType& nt, int indent) {
    if (!nt.emit) return;
    emitted_.insert(n.id);
    EmitContext c = ctxFor(n);
    c.setIndent(indent);
    markStatement(n, c);
    if (opt_.includeComments && !n.comment.empty()) {
      c.comment(n.comment);
    }
    nt.emit(c);
  }

  // --- yapılandırılmış kontrol akışı ---
  void emitControl(const NodeInstance& n, const NodeType& nt, int indent) {
    emitted_.insert(n.id);
    EmitContext c = ctxFor(n);
    c.setIndent(indent);
    markStatement(n, c);
    if (opt_.includeComments && !n.comment.empty()) c.comment(n.comment);

    const std::string kind = [&] {
      auto it = nt.properties.find("control");
      if (it != nt.properties.end()) return it->second;
      if (nt.id == "flow.if") return std::string("if");
      if (nt.id == "flow.while") return std::string("while");
      if (nt.id == "flow.forrange") return std::string("for");
      return std::string("");
    }();

    if (kind == "if") return emitIf(n, c, indent);
    if (kind == "while") return emitWhile(n, c, indent);
    if (kind == "for") return emitFor(n, c, indent);
    if (kind == "foreach") return emitForEach(n, nt, c, indent);
    if (kind == "switch") return emitSwitch(n, nt, c, indent);
    if (kind == "break") {
      c.line("break;");
      return;
    }
    if (kind == "continue") {
      c.line("continue;");
      return;
    }
    if (kind == "return") {
      auto lit = n.literals.find("Value");
      const bool hasValue =
          g_.edgeInto(PortKey{n.id, "Value"}) != nullptr || (lit != n.literals.end() && !lit->second.empty());
      if (hasValue) {
        c.line("return " + c.value("Value") + ";");
      } else {
        c.line("return;");
      }
      return;
    }
    if (nt.emit) nt.emit(c);
  }

  void emitIf(const NodeInstance& n, EmitContext& c, int indent) {
    const std::string cond = c.value("Condition");
    const std::string trueRoot = firstTarget(n.id, "True");
    const std::string falseRoot = firstTarget(n.id, "False");

    std::set<std::string> t = flowReachable(trueRoot);
    std::set<std::string> f = flowReachable(falseRoot);
    for (const auto& x : f) t.erase(x);
    for (const auto& x : t) f.erase(x);

    c.line("if (" + cond + ") {");
    if (!t.empty()) emitBody(t, indent + 1);
    if (!f.empty()) {
      c.line("} else {");
      emitBody(f, indent + 1);
    }
    c.line("}");
  }

  void emitWhile(const NodeInstance& n, EmitContext& c, int indent) {
    const std::string cond = c.value("Condition");
    const std::string bodyRoot = firstTarget(n.id, "Body");
    std::set<std::string> body = flowReachable(bodyRoot);

    c.line("while (" + cond + ") {");
    if (!body.empty()) emitBody(body, indent + 1);
    c.line("}");
  }

  void emitFor(const NodeInstance& n, EmitContext& c, int indent) {
    const std::string start = c.value("Start");
    const std::string end = c.value("End");
    const std::string step = c.value("Step");
    const std::string idx = uniqueName("idx_" + n.id);
    markUsed(idx);

    // Döngü değişkeni, gövde içindeki düğümlere bağlanabilsin
    c.bindOutput("Index", idx);

    const std::string bodyRoot = firstTarget(n.id, "Body");
    std::set<std::string> body = flowReachable(bodyRoot);

    const std::string ty = c.isCSharp() ? "int" : "int";
    c.line("for (" + ty + " " + idx + " = " + start + "; " + idx + " < " + end + "; " + idx +
           " += " + step + ") {");
    if (!body.empty()) emitBody(body, indent + 1);
    c.line("}");
  }

  void emitForEach(const NodeInstance& n, const NodeType& nt, EmitContext& c, int indent) {
    const std::string items = c.value("Items");
    const TypeRef elemT = nt.outputs.empty() ? t::Any() : nt.outputs[0].type;
    const std::string elemVar = uniqueName("item_" + n.id);
    const std::string counter = uniqueName("no_" + n.id);
    markUsed(elemVar);
    markUsed(counter);
    c.bindOutput("Element", elemVar);
    if (nt.outputs.size() > 1) c.bindOutput("Index", counter);

    const std::string bodyRoot = firstTarget(n.id, "Body");
    std::set<std::string> body = flowReachable(bodyRoot);

    const std::string elemTy = c.isCSharp() ? c.typeName(elemT) : c.nativeTypeName(elemT);
    c.line("int " + counter + " = 0;");
    if (c.isCSharp()) {
      c.line("foreach (" + elemTy + " " + elemVar + " in " + items + ") {");
    } else {
      c.line("for (const " + elemTy + "& " + elemVar + " : " + items + ") {");
    }
    if (!body.empty()) emitBody(body, indent + 1);
    c.line(counter + "++;");
    c.line("}");
  }

  void emitSwitch(const NodeInstance& n, const NodeType& nt, EmitContext& c, int indent) {
    const std::string val = c.value("Value");
    c.line("switch (" + val + ") {");
    for (const auto& o : nt.outputs) {
      if (o.kind != PortKind::Exec) continue;
      if (o.name == "Default") continue;
      std::set<std::string> branch = flowReachable(firstTarget(n.id, o.name));
      c.line("case " + o.displayName + ": {");
      if (!branch.empty()) emitBody(branch, indent + 1);
      c.line("  break;");
      c.line("}");
    }
    std::set<std::string> def = flowReachable(firstTarget(n.id, "Default"));
    c.line("default: {");
    if (!def.empty()) emitBody(def, indent + 1);
    c.line("  break;");
    c.line("}");
    c.line("}");
  }
};

// ===========================================================================
//  CodeGenerator
// ===========================================================================

// İşaretçileri nihai koddan ayıklar ve düğüm -> satır haritasını üretir.
// Tek geçişte yapılır: işaretçi görüldüğünde "bir sonraki yazılacak satırın
// numarası" o düğümün satırıdır, böylece silme sonrası kayma olmaz.
void extractNodeLines(std::string& code, std::map<std::string, int>& nodeLines,
                      const std::vector<std::pair<std::string, std::string>>& inlineAliases) {
  std::string out;
  out.reserve(code.size());
  int nextLine = 1;
  std::size_t pos = 0;
  while (pos <= code.size()) {
    const std::size_t nl = code.find('\n', pos);
    const std::size_t end = (nl == std::string::npos) ? code.size() : nl;
    const std::string raw = code.substr(pos, end - pos);
    const std::string body = trim(raw);

    if (body.rfind(kNodeMarker, 0) == 0) {
      const std::string id = body.substr(kNodeMarker.size());
      // Aynı düğüm birden çok kez üretilebilir (döngü gövdesi gibi); ilk yerini tut.
      if (!id.empty()) nodeLines.emplace(id, nextLine);
    } else {
      out += raw;
      out += '\n';
      ++nextLine;
    }

    if (nl == std::string::npos) break;
    pos = nl + 1;
  }

  // İfadeye gömülen saf düğümler ayrı satır üretmez; onları barındıran deyimin
  // satırına bağla. Böylece kod panelinde bir hataya tıklandığında doğru düğüm
  // bulunur.
  for (const auto& [inlineId, ownerId] : inlineAliases) {
    const auto it = nodeLines.find(ownerId);
    if (it == nodeLines.end()) continue;
    nodeLines.emplace(inlineId, it->second);
  }

  code.swap(out);
}

namespace {

void addHeaderComments(std::string& out, const Graph& g, const ProjectOptions& opt, Lang lang) {
  out += "// ============================================================\n";
  out += "//  " + g.name + "\n";
  out += "//  Gözle Programlama IDE tarafından üretildi. Elle düzenlemeniz\n";
  out += "//  programı etkilemez; düzenlemeler graf üzerinde yapılmalıdır.\n";
  out += "//  Dil: " + std::string(lang == Lang::CSharp ? "C#" : "C++") + "\n";
  if (opt.interopEnabled) {
    out += "//  Kip: Karma (C++ yerel kütüphane + C# yönetici)\n";
  }
  out += "// ============================================================\n\n";
}

}  // namespace

GenResult CodeGenerator::generate(const Graph& g, const ProjectOptions& opts) const {
  GenResult r;
  const Lang lang = opts.lang == TargetLang::CSharp ? Lang::CSharp : Lang::Cpp;

  Emitter em(g, catalog_, opts, lang, &r);
  std::string body;
  em.run(&body);

  if (!r.ok) return r;

  if (lang == Lang::CSharp) {
    std::string out;
    addHeaderComments(out, g, opts, lang);
    for (const auto& u : em.usingList()) out += "using " + u + ";\n";
    out += "\n";
    out += "namespace " + (opts.rootNamespace.empty() ? "Program" : opts.rootNamespace) + "\n{\n";
    out += "    internal static class Program\n    {\n";
    if (opts.outputType == "Library") {
      out += "        public static void Run()\n        {\n";
    } else {
      out += "        public static void Main(string[] args)\n        {\n";
    }
    r.entryFunction = "Program.Main";
    // Gövdeyi girintile
    std::istringstream in(body);
    std::string line;
    while (std::getline(in, line)) {
      if (line.empty()) {
        out += "\n";
      } else {
        out += "            " + line + "\n";
      }
    }
    out += "        }\n    }\n}\n";
    r.code = std::move(out);
  } else {
    std::string out;
    addHeaderComments(out, g, opts, lang);
    for (const auto& h : em.headerList()) out += "#include " + h + "\n";
    out += "\n";
    if (opts.interopEnabled) {
      out += "#include <cstring>\n#include <string>\n\n";
      out += "extern \"C\" {\n";
      out += "  __declspec(dllexport) int gpi_native_version() { return 1; }\n";
      out += "}\n";
      r.entryFunction = "gpi_native_version";
    } else {
      out += "int main(int argc, char** argv)\n{\n";
      out += "    (void)argc; (void)argv;\n";
      std::istringstream in(body);
      std::string line;
      while (std::getline(in, line)) {
        if (line.empty()) {
          out += "\n";
        } else {
          out += "    " + line + "\n";
        }
      }
      out += "    return 0;\n}\n";
      r.entryFunction = "main";
    }
    r.code = std::move(out);
  }

  // Nihai kod hazır. Şimdi düğüm işaretçilerini satır numarasına çevirip sil.
  extractNodeLines(r.code, r.nodeLines, em.state().inlineAliases);

  // Port bazlı eski harita düğüm haritasından türetilir; böylece eski alan da
  // en azından NİHAİ dosyanın satırlarını gösterir (daha önce sarmalanmamış
  // gövdeye göre sayıyordu, bu yüzden hep yanlıştı).
  for (const auto& [nodeId, line] : r.nodeLines) {
    const NodeInstance* n = g.findNode(nodeId);
    if (!n) continue;
    const NodeType* nt = catalog_.find(n->typeId);
    if (!nt) continue;
    for (const auto& p : nt->inputs) {
      SourceMapping m;
      m.port = PortKey{nodeId, p.name};
      m.line = line;
      m.symbol = "v_" + sanitize(nodeId) + "_" + sanitize(p.name);
      r.mapping[nodeId + "." + p.name] = m;
    }
    for (const auto& p : nt->outputs) {
      SourceMapping m;
      m.port = PortKey{nodeId, p.name};
      m.line = line;
      m.symbol = "v_" + sanitize(nodeId) + "_" + sanitize(p.name);
      r.mapping[nodeId + "." + p.name] = m;
    }
  }

  r.ok = true;
  return r;
}

GenResult CodeGenerator::generateFunction(const Graph& g, const SubGraph& sg,
                                           const ProjectOptions& opts,
                                           std::string_view fnName) const {
  GenResult r;
  const Lang lang = opts.lang == TargetLang::CSharp ? Lang::CSharp : Lang::Cpp;
  Emitter em(g, catalog_, opts, lang, &r);
  std::string body;
  em.run(&body);
  if (!r.ok) return r;

  const std::string fn(sanitize(fnName));
  std::string out;
  out += "// " + sg.name + (sg.description.empty() ? "" : " - " + sg.description) + "\n";
  out += "// Bu fonksiyon bir alt grafiktan üretildi.\n\n";

  std::istringstream in(body);
  std::string line;
  auto indent = [&](int n) {
    for (int i = 0; i < n; ++i) out += "    ";
  };

  if (lang == Lang::CSharp) {
    std::string ps;
    for (std::size_t i = 0; i < sg.inputs.size(); ++i) {
      if (i) ps += ", ";
      ps += em.types().csharp(sg.inputs[i].type) + " " + sanitize(sg.inputs[i].name);
    }
    std::string rs;
    for (std::size_t i = 0; i < sg.outputs.size(); ++i) {
      if (i) rs += ", ";
      rs += em.types().csharp(sg.outputs[i].type) + " " + sanitize(sg.outputs[i].name);
    }
    out += "    public static " + (rs.empty() ? std::string("void") : rs) + " " + fn + "(" + ps +
           ")\n    {\n";
    while (std::getline(in, line)) {
      out += line.empty() ? "\n" : "        " + line + "\n";
    }
    if (!rs.empty()) {
      out += "        return " + [&] {
        std::string s;
        for (std::size_t i = 0; i < sg.outputs.size(); ++i) {
          if (i) s += ", ";
          s += sanitize(sg.outputs[i].name);
        }
        return s;
      }() + ";\n";
    }
    out += "    }\n";
    r.entryFunction = fn;
  } else {
    std::string ps;
    for (std::size_t i = 0; i < sg.inputs.size(); ++i) {
      if (i) ps += ", ";
      ps += em.types().cpp(sg.inputs[i].type) + " " + sanitize(sg.inputs[i].name);
    }
    if (!ps.empty()) ps = "const " + ps;
    out += fn + "::Ret " + fn + "(" + ps + ")\n{\n";
    while (std::getline(in, line)) {
      out += line.empty() ? "\n" : "    " + line + "\n";
    }
    out += "    return {};\n}\n";
    r.entryFunction = fn;
  }

  r.code = std::move(out);
  r.ok = true;
  return r;
}

}  // namespace gpi
