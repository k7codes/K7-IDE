#include "gpi/Emit.hpp"

#include <algorithm>
#include <cctype>

#include "gpi/NodeCatalog.hpp"

namespace gpi {

std::string_view langId(Lang l) noexcept {
  switch (l) {
    case Lang::CSharp: return "csharp";
    case Lang::Cpp: return "cpp";
    case Lang::CppHeader: return "cpph";
  }
  return "csharp";
}

namespace {

std::string sanitizeIdent(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') out += c;
    else out += '_';
  }
  if (out.empty()) out = "v";
  if (std::isdigit(static_cast<unsigned char>(out[0]))) out.insert(out.begin(), '_');
  return out;
}

bool isCSharpKeyword(std::string_view w) {
  static const char* kws[] = {"abstract", "as",     "base",     "bool",     "break",  "byte",
                              "case",     "catch",  "char",     "checked",  "class",  "const",
                              "continue", "decimal","default",  "delegate", "do",     "double",
                              "else",     "enum",   "event",    "explicit", "extern", "false",
                              "finally",  "fixed",  "float",    "for",      "foreach","goto",
                              "if",       "implicit","in",      "int",      "interface","internal",
                              "is",       "lock",   "long",     "namespace","new",    "null",
                              "object",   "operator","out",    "override", "params", "private",
                              "protected","public", "readonly", "ref",      "return", "sbyte",
                              "sealed",   "short",  "sizeof",   "stackalloc","static","string",
                              "struct",   "switch", "this",     "throw",    "true",   "try",
                              "typeof",   "uint",   "ulong",    "unchecked","unsafe","ushort",
                              "using",    "var",    "virtual",  "void",     "volatile","while"};
  for (const char* k : kws) {
    if (w == k) return true;
  }
  return false;
}

std::string csharpIdent(std::string s) {
  s = sanitizeIdent(s);
  return isCSharpKeyword(s) ? "@" + s : s;
}

}  // namespace

const PortDef* NodeType::input(std::string_view name) const {
  for (const auto& p : inputs) {
    if (p.name == name) return &p;
  }
  return nullptr;
}

const PortDef* NodeType::output(std::string_view name) const {
  for (const auto& p : outputs) {
    if (p.name == name) return &p;
  }
  return nullptr;
}

EmitContext::EmitContext(const Graph& graph, const NodeInstance& node, const TypeSystem& ts,
                         Lang lang, const NodeCatalog& catalog, std::string* out,
                         ResolveState* state)
    : graph_(graph), node_(node), ts_(ts), lang_(lang), catalog_(catalog), out_(out),
      state_(state) {
  if (state_) {
    state_->graph = &graph_;
    state_->catalog = &catalog;
    state_->types = &ts_;
    state_->lang = lang_;
  }
  usedNames_.insert(sanitizeIdent(node.id));
  if (const NodeType* nt = selfType()) {
    for (const auto& p : nt->outputs) usedNames_.insert(sanitizeIdent(p.name));
  }
}

const NodeType* EmitContext::selfType() const { return catalog_.find(node_.typeId); }

void EmitContext::setIndent(int level) { indent_ = level < 0 ? 0 : level; }

std::string EmitContext::indented(std::string_view text) const {
  std::string pad(static_cast<std::size_t>(indent_) * 4, ' ');
  std::string out;
  out.reserve(text.size() + pad.size() + 1);
  out += pad;
  out += text;
  return out;
}

void EmitContext::line(std::string_view text) {
  if (text.empty()) {
    *out_ += '\n';
    return;
  }
  // Çok satırlı metni doğru girintile
  std::size_t start = 0;
  while (start <= text.size()) {
    std::size_t nl = text.find('\n', start);
    std::string_view part = nl == std::string_view::npos
                                ? text.substr(start)
                                : text.substr(start, nl - start);
    if (part.empty() && nl == std::string_view::npos) break;
    if (!part.empty() && part.back() == '\r') part.remove_suffix(1);
    *out_ += indented(part);
    *out_ += '\n';
    if (nl == std::string_view::npos) break;
    start = nl + 1;
  }
}

void EmitContext::blank() { out_->push_back('\n'); }

void EmitContext::comment(std::string_view text) {
  if (lang_ == Lang::CSharp) {
    line("// " + std::string(text));
  } else {
    line("// " + std::string(text));
  }
}

std::string EmitContext::local(std::string_view suffix) const {
  std::string base = "v_" + sanitizeIdent(node_.id);
  if (!suffix.empty()) base += "_" + sanitizeIdent(suffix);
  return base;
}

std::string EmitContext::uniqueName(std::string_view hint) {
  std::string base = sanitizeIdent(hint);
  if (lang_ == Lang::CSharp) base = csharpIdent(base);
  std::string candidate = base;
  int i = 1;
  while (usedNames_.count(candidate)) candidate = base + "_" + std::to_string(i++);
  usedNames_.insert(candidate);
  return candidate;
}

std::string EmitContext::newTemp(std::string_view hint) {
  std::string base = sanitizeIdent(hint);
  std::string candidate;
  do {
    candidate = base.empty() ? "tmp" : base;
    if (tempCounter_ > 0) candidate += "_" + std::to_string(tempCounter_);
    ++tempCounter_;
  } while (usedNames_.count(candidate));
  usedNames_.insert(candidate);
  return candidate;
}

void EmitContext::bindOutput(std::string_view portName, std::string varName) {
  outVars_[PortKey{node_.id, std::string(portName)}] = varName;
  ownVars_[std::string(portName)] = varName;
  if (state_) state_->materialized[PortKey{node_.id, std::string(portName)}] = varName;
}

std::string EmitContext::outputVar(std::string_view portName) const {
  auto it = outVars_.find(PortKey{node_.id, std::string(portName)});
  if (it != outVars_.end()) return it->second;
  return local(portName);
}

TypeRef EmitContext::portType(std::string_view portName) const {
  const NodeType* nt = selfType();
  if (!nt) return t::Any();
  if (const PortDef* p = nt->input(portName)) return p->type;
  if (const PortDef* p = nt->output(portName)) return p->type;
  return t::Any();
}

TypeRef EmitContext::valueType(std::string_view portName) const {
  const PortKey key{node_.id, std::string(portName)};
  if (const Edge* e = graph_.edgeInto(key)) {
    const NodeInstance* src = graph_.findNode(e->from.node);
    if (src) {
      if (const NodeType* st = catalog_.find(src->typeId)) {
        if (const PortDef* p = st->output(e->from.port)) return p->type;
      }
    }
  }
  return portType(portName);
}

std::string EmitContext::rawLiteral(std::string_view portName) const {
  auto it = node_.literals.find(std::string(portName));
  if (it != node_.literals.end()) return it->second;
  const NodeType* nt = selfType();
  if (nt) {
    if (const PortDef* p = nt->input(portName)) return p->defaultLiteral;
  }
  return {};
}

std::string EmitContext::literalOr(std::string_view portName, std::string_view fallback) const {
  std::string v = rawLiteral(portName);
  return v.empty() ? std::string(fallback) : v;
}

std::string EmitContext::prop(std::string_view key, std::string_view fallback) const {
  auto it = node_.properties.find(std::string(key));
  return it == node_.properties.end() ? std::string(fallback) : it->second;
}

bool EmitContext::boolProp(std::string_view key, bool fallback) const {
  auto it = node_.properties.find(std::string(key));
  if (it == node_.properties.end()) return fallback;
  const std::string& v = it->second;
  return v == "true" || v == "1" || v == "evet" || v == "on" || v == "True";
}

int EmitContext::intProp(std::string_view key, int fallback) const {
  auto it = node_.properties.find(std::string(key));
  if (it == node_.properties.end()) return fallback;
  try {
    return std::stoi(it->second);
  } catch (...) {
    return fallback;
  }
}

double EmitContext::doubleProp(std::string_view key, double fallback) const {
  auto it = node_.properties.find(std::string(key));
  if (it == node_.properties.end()) return fallback;
  try {
    return std::stod(it->second);
  } catch (...) {
    return fallback;
  }
}

std::string EmitContext::typeName(const TypeRef& tp) const { return ts_.csharp(tp); }

std::string EmitContext::nativeTypeName(const TypeRef& tp) const { return ts_.cpp(tp); }

std::string EmitContext::asNative(const TypeRef& tp, std::string_view expr) const {
  if (lang_ == Lang::CSharp) return std::string(expr);
  // C++ üretirken C# literal'ini C++ literal'ine çevir
  std::string e = std::string(expr);
  if (tp.kind == TypeKind::String) {
    // "abc"  ->  std::string("abc")  (zaten string ise dokunma)
    if (e.size() >= 2 && e.front() == '"' && e.back() == '"') {
      return "std::string(" + e + ")";
    }
    return e;
  }
  if (tp.kind == TypeKind::Bool) {
    if (e == "true") return "true";
    if (e == "false") return "false";
    return "((" + e + ") != 0)";
  }
  if (tp.kind == TypeKind::Int || tp.kind == TypeKind::Long) {
    if (!e.empty() && e.back() == 'L') e.pop_back();
    return e;
  }
  if (tp.kind == TypeKind::Float) {
    if (e == "0f") return "0.0f";
  }
  return e;
}

// --- Çıktı çözümleme -------------------------------------------------

std::string EmitContext::resolveOutput(const PortKey& key) const {
  if (!state_) return local(key.port);
  if (state_->depth > state_->maxDepth) {
    // Aşırı derinlik: materyalize edilmiş değişkene düş
    auto m = state_->materialized.find(key);
    return m == state_->materialized.end() ? ("ref_" + sanitizeIdent(key.node) + "_" +
                                             sanitizeIdent(key.port))
                                           : m->second;
  }
  auto memo = state_->memo.find(key);
  if (memo != state_->memo.end()) return memo->second;

  const NodeInstance* src = state_->graph ? state_->graph->findNode(key.node) : nullptr;
  if (!src) return "null";
  const NodeType* st = state_->catalog ? state_->catalog->find(src->typeId) : nullptr;
  if (!st) return "null";

  // Saf düğüm → ifade olarak göm
  if (state_->inlinePure && st->pure && st->emitExpr) {
    std::string nested;
    ResolveState nestedState = *state_;  // memo/materalize kopyası (ağacın çocukları için)
    nestedState.depth = state_->depth + 1;
    // Çocukların memo'ya yazması ana ağacı kirletmesin, sonra geri alınır.
    const auto savedMemo = state_->memo;
    EmitContext sub(*state_->graph, *src, *state_->types, state_->lang, *state_->catalog, &nested,
                    &nestedState);
    std::string expr;
    try {
      expr = st->emitExpr(sub);
    } catch (...) {
      expr = "null";
    }
    state_->memo = savedMemo;
    // nestedState bir kopyadır: içinde toplanan satır eşlemesi ana duruma taşınır.
    // Bu saf düğüm ayrı bir satır üretmediği için, kodun gömüldüğü deyimin
    // satırına bağlanır.
    state_->inlineAliases.insert(state_->inlineAliases.end(),
                                 nestedState.inlineAliases.begin(),
                                 nestedState.inlineAliases.end());
    if (!state_->statementAnchor.empty() && state_->statementAnchor != key.node) {
      state_->inlineAliases.emplace_back(key.node, state_->statementAnchor);
    }
    if (!expr.empty() && state_->depth + 1 <= state_->maxDepth) {
      state_->memo[key] = expr;
      return expr;
    }
  }

  auto m = state_->materialized.find(key);
  if (m != state_->materialized.end()) {
    state_->memo[key] = m->second;
    return m->second;
  }
  return "ref_" + sanitizeIdent(key.node) + "_" + sanitizeIdent(key.port);
}

std::string EmitContext::outRef(std::string_view portName) const {
  if (const NodeType* nt = selfType()) {
    if (const PortDef* p = nt->output(portName)) {
      if (p->kind == PortKind::Exec) return std::string();
    }
  }
  return resolveOutput(PortKey{node_.id, std::string(portName)});
}

std::string EmitContext::value(std::string_view portName) const {
  const PortKey key{node_.id, std::string(portName)};
  const NodeType* nt = selfType();
  const PortDef* def = nt ? nt->input(portName) : nullptr;
  if (def && def->kind == PortKind::Exec) return {};

  if (const Edge* e = graph_.edgeInto(key)) {
    const std::string srcExpr = resolveOutput(e->from);
    const TypeRef fromT = valueType(portName);
    const TypeRef toT = def ? def->type : fromT;
    if (ts_.conversion(fromT, toT) == Conversion::Implicit) return srcExpr;
    if (auto conv = ts_.convertExpr(srcExpr, fromT, toT, langId(lang_))) return *conv;
    return srcExpr;
  }

  std::string lit = rawLiteral(portName);
  const TypeRef target = def ? def->type : t::Any();
  if (lit.empty()) {
    if (def && !def->defaultLiteral.empty()) lit = def->defaultLiteral;
    else return ts_.defaultValue(target, langId(lang_));
  }
  // C++ üretiminde C# literal'ini uyarlamak gerekir
  return asNative(target, lit);
}

std::vector<std::string> EmitContext::nextFlowNodes(std::string_view execPort) const {
  std::vector<std::string> out;
  for (const Edge* e : graph_.edgesFrom(PortKey{node_.id, std::string(execPort)})) {
    out.push_back(e->to.node);
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

void EmitContext::pushScope(std::map<std::string, std::string> vars) {
  scopeStack_.push_back(std::move(vars));
}

void EmitContext::popScope() {
  if (!scopeStack_.empty()) scopeStack_.pop_back();
}

std::string EmitContext::lookupScoped(std::string_view name) const {
  for (auto it = scopeStack_.rbegin(); it != scopeStack_.rend(); ++it) {
    auto f = it->find(std::string(name));
    if (f != it->end()) return f->second;
  }
  return {};
}

void EmitContext::markMapping(std::string_view portName) {
  if (!state_) return;
  int lineNo = 1;
  for (char c : *out_) {
    if (c == '\n') ++lineNo;
  }
  state_->lineMap.emplace_back(PortKey{node_.id, std::string(portName)}, lineNo);
}

}  // namespace gpi
