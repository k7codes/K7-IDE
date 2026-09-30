#include "gpi/NodeCatalog.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace gpi {

NodeCatalog::NodeCatalog() { loadBuiltins(); }

void NodeCatalog::addCategory(std::string id, std::string display, std::string icon) {
  NodeCategory c;
  c.id = std::move(id);
  c.displayName = std::move(display);
  c.icon = std::move(icon);
  cats_[c.id] = std::move(c);
}

void NodeCatalog::registerType(NodeType t) {
  const std::string key = t.id;
  types_[key] = std::move(t);
}

const NodeType* NodeCatalog::find(std::string_view id) const {
  auto it = types_.find(std::string(id));
  return it == types_.end() ? nullptr : &it->second;
}

std::vector<NodeCategory> NodeCatalog::categories() const {
  std::vector<NodeCategory> out;
  out.reserve(cats_.size());
  for (const auto& [id, c] : cats_) out.push_back(c);
  std::sort(out.begin(), out.end(), [](const NodeCategory& a, const NodeCategory& b) {
    return a.displayName < b.displayName;
  });
  return out;
}

namespace {

// Somut liste/ dizi varyantlarını tek bir şablondan türetir
NodeType retypeCollection(NodeType base, std::string newId, std::string display, TypeRef elem,
                          const std::string& cppElem) {
  NodeType n = std::move(base);
  n.id = std::move(newId);
  n.displayName = std::move(display);
  n.properties["cppElementType"] = cppElem;
  const TypeRef listT = TypeRef::list(elem);
  for (auto& p : n.inputs) {
    if (p.name == "List" || p.name == "Dict" || p.name == "Items") p.type = listT;
    if (p.name == "Item" || p.name == "Value" || p.name == "Element") p.type = elem;
  }
  for (auto& p : n.outputs) {
    if (p.name == "List" || p.name == "Dict" || p.name == "Items") p.type = listT;
    if (p.name == "Item" || p.name == "Value" || p.name == "Element") p.type = elem;
  }
  for (auto& p : n.inputs) {
    if (p.name == "List" || p.name == "Items" || p.name == "Dict") p.defaultLiteral.clear();
    if (p.name == "Item" || p.name == "Value") p.defaultLiteral.clear();
  }
  n.description = std::string(toString(elem.kind)) + " tipinde elemanlar içerir.";
  return n;
}

NodeType retypeForeach(NodeType base, std::string newId, std::string display, TypeRef elem,
                       const std::string& cppElem) {
  NodeType n = retypeCollection(std::move(base), std::move(newId), std::move(display), elem, cppElem);
  n.isFlowControl = true;
  n.properties["control"] = "foreach";
  return n;
}

std::string lowerAscii(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return s;
}

// Türkçe karakterleri de katlayan basit bir arama normalize edici
std::string foldForSearch(std::string s) {
  static const std::pair<const char*, char> map[] = {
      {"ç", 'c'}, {"ğ", 'g'}, {"ı", 'i'}, {"ö", 'o'}, {"ş", 's'}, {"ü", 'u'},
      {"Ç", 'c'}, {"Ğ", 'g'}, {"İ", 'i'}, {"Ö", 'o'}, {"Ş", 's'}, {"Ü", 'u'}};
  for (const auto& [from, to] : map) {
    std::size_t pos = 0;
    const std::size_t len = std::char_traits<char>::length(from);
    while ((pos = s.find(from, pos)) != std::string::npos) {
      s.replace(pos, len, 1, to);
      pos += 1;
    }
  }
  return lowerAscii(std::move(s));
}

}  // namespace

std::vector<std::string> NodeCatalog::search(std::string_view query, std::string_view library) const {
  const std::string q = foldForSearch(std::string(query));
  const std::string lib = lowerAscii(std::string(library));

  struct Hit {
    double score;
    std::string id;
  };
  std::vector<Hit> hits;

  for (const auto& [id, t] : types_) {
    if (t.deprecated) continue;
    if (!lib.empty() && lowerAscii(t.library) != lib) continue;
    double score = 0;
    if (q.empty()) {
      score = 1;
    } else {
      const std::string disp = foldForSearch(t.displayName);
      const std::string nid = lowerAscii(id);
      const std::string cat = foldForSearch(t.category);
      const std::string desc = foldForSearch(t.description);
      if (nid == q) {
        score = 1000;
      } else if (disp == q) {
        score = 900;
      } else if (disp.rfind(q, 0) == 0) {
        score = 700;
      } else if (nid.find(q) != std::string::npos) {
        score = 600;
      } else if (disp.find(q) != std::string::npos) {
        score = 500;
      } else if (cat.find(q) != std::string::npos) {
        score = 300;
      } else if (desc.find(q) != std::string::npos) {
        score = 200;
      } else if (t.library.find(q) != std::string::npos) {
        score = 150;
      }
    }
    if (score > 0) hits.push_back({score, id});
  }

  std::sort(hits.begin(), hits.end(), [&](const Hit& a, const Hit& b) {
    if (a.score != b.score) return a.score > b.score;
    return a.id < b.id;
  });

  std::vector<std::string> out;
  out.reserve(hits.size());
  for (const auto& h : hits) out.push_back(h.id);
  return out;
}

NodeType NodeCatalog::makeReflectedType(std::string typeFullName, std::string methodName,
                                        std::string libraryName) const {
  // C# yansıtma servisi (Gpi.Sidecar) yalnızca imzaları gönderir; burada
  // tip bilgisi doldurulur ve şablon tabanlı bir düğüm üretilir.
  NodeType n;
  n.id = "api." + typeFullName + "." + methodName;
  n.displayName = methodName;
  n.category = "Kütüphane";
  n.icon = "\xEE\xA2\xB7";
  n.accentColor = "#EC4899";
  n.library = std::move(libraryName);
  n.pure = true;
  n.csUsings = {typeFullName.substr(0, typeFullName.find('.'))};
  n.csTemplate = typeFullName + "." + methodName + "({0})";
  n.cppTemplate = typeFullName + "::" + methodName + "({0})";
  n.description = typeFullName + " sınıfından yansıtılan düğüm.";
  n.inputs = {PortDef{"Arg0", "Argüman", PortDirection::Input, PortKind::Value,
                      TypeRef::primitive(TypeKind::Any), "0", "0. örnek", false, 0}};
  n.outputs = {PortDef{"Result", "Sonuç", PortDirection::Output, PortKind::Value,
                       TypeRef::primitive(TypeKind::Any), "", "", false, 0}};
  n.emit = [id = n.id, method = methodName, type = typeFullName](EmitContext& c) {
    const std::string v = c.local("Result");
    c.bindOutput("Result", v);
    c.line("var " + v + " = " + type + "." + method + "(" + c.value("Arg0") + ");");
  };
  return n;
}

void NodeCatalog::applyDefaults(const NodeType& type, NodeInstance& inst) const {
  for (const auto& p : type.inputs) {
    if (inst.literals.find(p.name) == inst.literals.end()) {
      inst.literals[p.name] = p.defaultLiteral;
    }
  }
  for (const auto& [k, v] : type.properties) {
    if (inst.properties.find(k) == inst.properties.end()) inst.properties[k] = v;
  }
}

void NodeCatalog::loadBuiltins() {
  using namespace builtin;

  addCategory("math", "Matematik", "\xEE\xA5\x85");
  addCategory("str", "Metin", "\xEE\xA2\xBD");
  addCategory("flow", "Akış", "\xEE\x97\x95");
  addCategory("logic", "Mantık", "\xEE\x9E\x95");
  addCategory("list", "Koleksiyon", "\xEE\xA3\xBD");
  addCategory("dict", "Koleksiyon", "\xEE\xA3\x97");
  addCategory("io", "Giriş / Çıkış", "\xEE\xA3\xA5");
  addCategory("value", "Değer", "\xEE\xA2\xAB");
  addCategory("def", "Tanım", "\xEE\xA2\xB7");

  // --- Değerler ---
  registerType(makeLiteralNumber());
  registerType(makeLiteralDouble());
  registerType(makeLiteralText());
  registerType(makeLiteralBool());

  // --- Matematik ---
  registerType(makeAdd());
  registerType(makeSubtract());
  registerType(makeMultiply());
  registerType(makeDivide());
  registerType(makeModulo());
  registerType(makeNegate());
  registerType(makeAbs());
  registerType(makeMin());
  registerType(makeMax());
  registerType(makePower());
  registerType(makeSqrt());
  registerType(makeSin());
  registerType(makeCos());
  registerType(makeFloor());
  registerType(makeCeil());
  registerType(makeRound());
  registerType(makeRandom());

  // --- Metin ---
  registerType(makeStringConcat());
  registerType(makeFormat());
  registerType(makeSubstring());
  registerType(makeToUpper());
  registerType(makeToLower());
  registerType(makeTrim());
  registerType(makeSplit());
  registerType(makeJoin());
  registerType(makeContains());
  registerType(makeReplace());
  registerType(makeIndexOf());
  registerType(makeStringLength());
  registerType(makeStringToInt());
  registerType(makeIntToString());
  registerType(makeStringSplitLines());

  // --- Akış ve mantık ---
  registerType(makeIf());
  registerType(makeWhile());
  registerType(makeForRange());
  registerType(makeBranch());
  registerType(makeBreak());
  registerType(makeContinue());
  registerType(makeExit());
  registerType(makeComment());
  registerType(makeCompare());
  registerType(makeCompareFloat());
  registerType(makeCompareString());
  registerType(makeAnd());
  registerType(makeOr());
  registerType(makeNot());

  // --- Koleksiyonlar ---
  registerType(makeListNew());
  registerType(makeListAdd());
  registerType(makeListGet());
  registerType(makeListSet());
  registerType(makeListCount());
  {
    const NodeType fe = makeListForEach();
    registerType(fe);
    registerType(retypeForeach(fe, "flow.foreach.string", "Listedeki Her Metin", t::String(),
                               "std::string"));
    registerType(retypeForeach(fe, "flow.foreach.double", "Listedeki Her Ondalık", t::Double(),
                               "double"));
  }
  {
    const NodeType ls = makeListNew();
    registerType(retypeCollection(ls, "list.new.string", "Yeni Metin Listesi", t::String(),
                                  "std::string"));
    registerType(retypeCollection(ls, "list.new.double", "Yeni Ondalık Listesi", t::Double(),
                                  "double"));
    const NodeType la = makeListAdd();
    registerType(retypeCollection(la, "list.add.string", "Listeye Ekle (Metin)", t::String(),
                                  "std::string"));
    registerType(retypeCollection(la, "list.add.double", "Listeye Ekle (Ondalık)", t::Double(),
                                  "double"));
    const NodeType lg = makeListGet();
    registerType(retypeCollection(lg, "list.get.string", "Listeden Al (Metin)", t::String(),
                                  "std::string"));
    registerType(retypeCollection(lg, "list.get.double", "Listeden Al (Ondalık)", t::Double(),
                                  "double"));
    const NodeType lc = makeListCount();
    registerType(retypeCollection(lc, "list.count.string", "Eleman Sayısı (Metin)", t::String(),
                                  "std::string"));
    registerType(retypeCollection(lc, "list.count.double", "Eleman Sayısı (Ondalık)", t::Double(),
                                  "double"));
  }
  registerType(makeArrayNew());
  registerType(makeArrayGet());
  registerType(makeArraySet());
  registerType(makeArrayLength());
  registerType(makeDictNew());
  registerType(makeDictGet());
  registerType(makeDictSet());
  registerType(makeDictHas());

  // --- Giriş / Çıkış ---
  registerType(makePrint());
  registerType(makeReadLine());
  registerType(makeDelay());

  // --- Değişkenler ---
  addCategory("var", "Değişken", "\xEE\xA2\xB3");
  registerType(makeVarMakeInt());
  registerType(makeVarMakeString());
  registerType(makeVarMakeDouble());
  registerType(makeVarMakeBool());
  registerType(makeVarGetInt());
  registerType(makeVarGetString());
  registerType(makeVarGetDouble());
  registerType(makeVarGetBool());
  registerType(makeVarSetInt());
  registerType(makeVarSetString());
  registerType(makeVarSetDouble());
  registerType(makeVarSetBool());

  // --- Tanımlar ---
  registerType(makeMakeClass());
  registerType(makeStructNew());
}

}  // namespace gpi
