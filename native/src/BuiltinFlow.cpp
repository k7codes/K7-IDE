// Gözle Programlama IDE - Akış, koleksiyon, G/Ç ve değer düğümleri
//
// Yapılandırılmış kontrol akışı (if/else, döngü, dallanma) düğümlerinin `control`
// özelliği emit edici tarafından özel olarak ele alınır; bu dosyadaki emit
// geri çağrıları yalnızca koşul değerlerini ve veri portlarını üretir.
#include <algorithm>
#include <cctype>

#include "gpi/NodeCatalog.hpp"

namespace gpi::builtin {

namespace {

NodeType ifNode() {
  NodeType n;
  n.id = "flow.if";
  n.displayName = "Eğer / Değilse";
  n.category = "Akış";
  n.icon = "\xEE\xA7\x99";
  n.accentColor = "#38BDF8";
  n.isFlowControl = true;
  n.library = "GPI";
  n.description =
      "Koşul doğruysa 'Doğru' çıkışını, yanlışsa 'Yanlış' çıkışını izler. "
      "İki dalın ortak devamı otomatik olarak bulunur.";
  n.inputs = {execIn("In", "Giriş"), in("Condition", t::Bool(), "false", "Koşul")};
  n.outputs = {execOut("True", "Doğru"), execOut("False", "Yanlış")};
  // Koşul bağlanmazsa üretilen kod anlamsız bir "hep yanlış" dal olur.
  n.inputs[1].required = true;
  n.properties["control"] = "if";
  return n;
}

NodeType whileNode() {
  NodeType n;
  n.id = "flow.while";
  n.displayName = "Koşul Sağlandıkça";
  n.category = "Akış";
  n.icon = "\xEE\x97\x95";
  n.accentColor = "#38BDF8";
  n.isFlowControl = true;
  n.library = "GPI";
  n.description = "Koşul doğru olduğu sürece 'Gövde' çıkışını tekrarlar, sonra 'Bitti'ye geçer.";
  n.inputs = {execIn("In", "Giriş"), in("Condition", t::Bool(), "false", "Koşul")};
  n.outputs = {execOut("Body", "Gövde"), execOut("Done", "Bitti")};
  n.inputs[1].required = true;
  n.properties["control"] = "while";
  return n;
}

NodeType forRangeNode() {
  NodeType n;
  n.id = "flow.forrange";
  n.displayName = "Sayılarla Tekrarla";
  n.category = "Akış";
  n.icon = "\xEE\xA3\xAF";
  n.accentColor = "#38BDF8";
  n.isFlowControl = true;
  n.library = "GPI";
  n.description = "Start'tan End'e kadar Step adımıyla ilerler, 'Index' her yinelemede güncellenir.";
  n.inputs = {execIn("In", "Giriş"), in("Start", t::Int(), "0", "Başlangıç"),
              in("End", t::Int(), "10", "Bitiş"), in("Step", t::Int(), "1", "Adım")};
  n.outputs = {out("Index", t::Int(), "Sıra"), execOut("Body", "Gövde"), execOut("Done", "Bitti")};
  return n;
}

NodeType foreachNode(const std::string& id, const std::string& name, TypeRef elem,
                     const std::string& cppType) {
  NodeType n;
  n.id = id;
  n.displayName = name;
  n.category = "Akış";
  n.icon = "\xEE\xA3\x97";
  n.accentColor = "#38BDF8";
  n.isFlowControl = true;
  n.library = "GPI";
  n.description = "Listedeki her eleman için 'Gövde' çıkışını çalıştırır.";
  n.inputs = {execIn("In", "Giriş"), in("Items", TypeRef::list(elem), "", "Liste")};
  n.outputs = {out("Element", elem, "Eleman"), out("Index", t::Int(), "Sıra"),
               execOut("Body", "Gövde"), execOut("Done", "Bitti")};
  n.inputs[1].required = true;
  n.properties["control"] = "foreach";
  n.properties["cppElementType"] = cppType;
  return n;
}

NodeType simpleFlow(const std::string& id, const std::string& name, const std::string& icon,
                    const std::string& control, const std::string& desc) {
  NodeType n;
  n.id = id;
  n.displayName = name;
  n.category = "Akış";
  n.icon = icon;
  n.accentColor = "#38BDF8";
  n.isFlowControl = true;
  n.library = "GPI";
  n.description = desc;
  n.inputs = {execIn("In", "Giriş")};
  n.outputs = {execOut("Next", "Devam")};
  n.properties["control"] = control;
  return n;
}

NodeType branchNode() {
  NodeType n;
  n.id = "flow.branch";
  n.displayName = "Değere Göre Dallan";
  n.category = "Akış";
  n.icon = "\xEE\xA7\x99";
  n.accentColor = "#38BDF8";
  n.isFlowControl = true;
  n.library = "GPI";
  n.description = "Değeri dallar (0, 1, 2 ...) ve 'Varsayılan' çıkışına düşer.";
  n.inputs = {execIn("In", "Giriş"), in("Value", t::Int(), "0", "Değer")};
  n.outputs = {execOut("Case0", "0"), execOut("Case1", "1"), execOut("Case2", "2"),
               execOut("Case3", "3"), execOut("Default", "Varsayılan")};
  n.properties["control"] = "switch";
  return n;
}

// ---- Liste düğümleri (somut eleman tipiyle) ----
NodeType listNew(TypeRef elem, const std::string& suffix) {
  NodeType n;
  n.id = "list.new." + suffix;
  n.displayName = "Yeni Liste";
  n.category = "Koleksiyon";
  n.icon = "\xEE\xA3\xBD";
  n.accentColor = "#0EA5E9";
  n.pure = true;
  n.library = "System";
  n.description = "Boş bir " + std::string(toString(elem.kind)) + " listesi oluşturur.";
  n.inputs = {};
  n.outputs = {out("List", TypeRef::list(elem), "Liste")};
  auto call = [](EmitContext& c) {
    if (c.isCSharp()) return "new List<" + c.types().csharp(c.portType("List").args[0]) + ">()";
    return "std::vector<" + std::string(c.prop("cppElementType")) + ">{}";
  };
  n.emitExpr = call;
  n.emit = [call](EmitContext& c) {
    const std::string v = c.local("List");
    c.bindOutput("List", v);
    c.line((c.isCSharp() ? "var " : "auto ") + v + " = " + call(c) + ";");
    c.markMapping("List");
  };
  n.cppHeader = "<vector>";
  return n;
}

NodeType listAdd(TypeRef elem, const std::string& suffix, const std::string& cppType) {
  NodeType n;
  n.id = "list.add." + suffix;
  n.displayName = "Listeye Ekle";
  n.category = "Koleksiyon";
  n.icon = "\xEE\x9C\x90";
  n.accentColor = "#0EA5E9";
  n.library = "System";
  n.description = "Listenin sonuna eleman ekler ve listeyi geri döndürür.";
  n.inputs = {execIn("In", "Giriş"), in("List", TypeRef::list(elem), "", "Liste"),
              in("Item", elem, "", "Eleman")};
  n.outputs = {out("List", TypeRef::list(elem), "Liste"), execOut("Next", "Devam")};
  n.emitExpr = [](EmitContext& c) {
    if (c.isCSharp()) return c.value("List") + ".Add(" + c.value("Item") + ")";
    return "(" + c.value("List") + ".push_back(" + c.value("Item") + "), " + c.value("List") + ")";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("List");
      c.bindOutput("List", v);
      if (c.isCSharp()) {
        c.line(v + " = " + c.value("List") + ";");
        c.line(nt->emitExpr(c) + ";");
      } else {
        c.line("auto " + v + " = " + nt->emitExpr(c) + ";");
      }
      c.markMapping("List");
    }
  };
  n.cppHeader = "<vector>";
  return n;
}

NodeType listGet(TypeRef elem, const std::string& suffix, const std::string& cppType) {
  NodeType n;
  n.id = "list.get." + suffix;
  n.displayName = "Listeden Al";
  n.category = "Koleksiyon";
  n.icon = "\xEE\x9E\xB3";
  n.accentColor = "#0EA5E9";
  n.pure = true;
  n.library = "System";
  n.description = "Verilen sıradaki elemanı döndürür. Sıra geçersizse hata oluşur.";
  n.inputs = {in("List", TypeRef::list(elem), "", "Liste"), in("Index", t::Int(), "0", "Sıra")};
  n.outputs = {out("Item", elem, "Eleman")};
  n.emitExpr = [](EmitContext& c) {
    if (c.isCSharp()) return c.value("List") + "[" + c.value("Index") + "]";
    return c.value("List") + ".at(" + c.value("Index") + ")";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("Item");
      c.bindOutput("Item", v);
      c.line(v + " = " + nt->emitExpr(c) + ";");
      c.markMapping("Item");
    }
  };
  n.cppHeader = "<vector>";
  return n;
}

NodeType listCount(TypeRef elem, const std::string& suffix) {
  NodeType n;
  n.id = "list.count." + suffix;
  n.displayName = "Eleman Sayısı";
  n.category = "Koleksiyon";
  n.icon = "\xEE\xA2\xAB";
  n.accentColor = "#0EA5E9";
  n.pure = true;
  n.library = "System";
  n.inputs = {in("List", TypeRef::list(elem), "", "Liste")};
  n.outputs = {out("Count", t::Int(), "Sayı")};
  n.emitExpr = [](EmitContext& c) {
    if (c.isCSharp()) return c.value("List") + ".Count";
    return "static_cast<int>(" + c.value("List") + ".size())";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("Count");
      c.bindOutput("Count", v);
      c.line((c.isCSharp() ? "int " : "const int ") + v + " = " + nt->emitExpr(c) + ";");
      c.markMapping("Count");
    }
  };
  n.cppHeader = "<vector>";
  return n;
}

NodeType listSet(TypeRef elem, const std::string& suffix) {
  NodeType n;
  n.id = "list.set." + suffix;
  n.displayName = "Listede Değiştir";
  n.category = "Koleksiyon";
  n.icon = "\xEE\x9C\x8F";
  n.accentColor = "#0EA5E9";
  n.library = "System";
  n.inputs = {execIn("In", "Giriş"), in("List", TypeRef::list(elem), "", "Liste"),
              in("Index", t::Int(), "0", "Sıra"), in("Item", elem, "", "Yeni Değer")};
  n.outputs = {out("List", TypeRef::list(elem), "Liste"), execOut("Next", "Devam")};
  n.emit = [](EmitContext& c) {
    if (c.isCSharp()) {
      c.line(c.value("List") + "[" + c.value("Index") + "] = " + c.value("Item") + ";");
      c.bindOutput("List", c.value("List"));
    } else {
      c.line(c.value("List") + ".at(" + c.value("Index") + ") = " + c.value("Item") + ";");
      c.bindOutput("List", c.value("List"));
    }
    c.markMapping("List");
  };
  n.cppHeader = "<vector>";
  return n;
}

NodeType dictNew(TypeRef value, const std::string& suffix) {
  NodeType n;
  n.id = "dict.new." + suffix;
  n.displayName = "Yeni Sözlük";
  n.category = "Koleksiyon";
  n.icon = "\xEE\xA3\x97";
  n.accentColor = "#0EA5E9";
  n.pure = true;
  n.library = "System";
  n.description = "Anahtarı metin olan boş bir sözlük oluşturur.";
  n.inputs = {};
  n.outputs = {out("Dict", TypeRef::dict(t::String(), value), "Sözlük")};
  auto call = [](EmitContext& c) {
    if (c.isCSharp()) {
      return "new Dictionary<string, " + c.types().csharp(c.portType("Dict").args[1]) + ">()";
    }
    return "std::map<std::string, " + std::string(c.prop("cppElementType")) + ">{}";
  };
  n.emitExpr = call;
  n.emit = [call](EmitContext& c) {
    const std::string v = c.local("Dict");
    c.bindOutput("Dict", v);
    c.line((c.isCSharp() ? "var " : "auto ") + v + " = " + call(c) + ";");
    c.markMapping("Dict");
  };
  n.cppHeader = "<map>";
  return n;
}

NodeType dictSet(TypeRef value, const std::string& suffix) {
  NodeType n;
  n.id = "dict.set." + suffix;
  n.displayName = "Sözlüğe Yaz";
  n.category = "Koleksiyon";
  n.icon = "\xEE\x9C\x8F";
  n.accentColor = "#0EA5E9";
  n.library = "System";
  n.inputs = {execIn("In", "Giriş"), in("Dict", TypeRef::dict(t::String(), value), "", "Sözlük"),
              in("Key", t::String(), "\"\"", "Anahtar"), in("Value", value, "", "Değer")};
  n.outputs = {out("Dict", TypeRef::dict(t::String(), value), "Sözlük"), execOut("Next", "Devam")};
  n.emit = [](EmitContext& c) {
    c.line(c.value("Dict") + "[" + c.value("Key") + "] = " + c.value("Value") + ";");
    c.bindOutput("Dict", c.value("Dict"));
    c.markMapping("Dict");
  };
  n.cppHeader = "<map>";
  return n;
}

NodeType dictGet(TypeRef value, const std::string& suffix) {
  NodeType n;
  n.id = "dict.get." + suffix;
  n.displayName = "Sözlükten Oku";
  n.category = "Koleksiyon";
  n.icon = "\xEE\x9E\xB3";
  n.accentColor = "#0EA5E9";
  n.pure = true;
  n.library = "System";
  n.description = "Anahtar yoksa verilen Varsayılan değerini döndürür.";
  n.inputs = {in("Dict", TypeRef::dict(t::String(), value), "", "Sözlük"),
              in("Key", t::String(), "\"\"", "Anahtar"), in("Default", value, "", "Varsayılan")};
  n.outputs = {out("Value", value, "Değer")};
  n.emitExpr = [](EmitContext& c) {
    if (c.isCSharp()) {
      return "((" + c.value("Dict") + ".TryGetValue(" + c.value("Key") + ", out var __gpi_tmp)) " +
             "? __gpi_tmp : " + c.value("Default") + ")";
    }
    return "((" + c.value("Dict") + ".count(" + c.value("Key") + ") ? " + c.value("Dict") +
           ".at(" + c.value("Key") + ") : " + c.value("Default") + "))";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("Value");
      c.bindOutput("Value", v);
      c.line((c.isCSharp() ? "var " : "const auto ") + v + " = " + nt->emitExpr(c) + ";");
      c.markMapping("Value");
    }
  };
  n.cppHeader = "<map>";
  return n;
}

NodeType dictHas(const std::string& suffix) {
  NodeType n;
  n.id = "dict.has." + suffix;
  n.displayName = "Anahtar Var mı";
  n.category = "Koleksiyon";
  n.icon = "\xEE\x9C\xA1";
  n.accentColor = "#0EA5E9";
  n.pure = true;
  n.library = "System";
  n.inputs = {in("Dict", TypeRef::dict(t::String(), t::Any()), "", "Sözlük"),
              in("Key", t::String(), "\"\"", "Anahtar")};
  n.outputs = {out("Result", t::Bool(), "Sonuç")};
  n.emitExpr = [](EmitContext& c) {
    if (c.isCSharp()) return c.value("Dict") + ".ContainsKey(" + c.value("Key") + ")";
    return c.value("Dict") + ".count(" + c.value("Key") + ") > 0";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("Result");
      c.bindOutput("Result", v);
      c.line((c.isCSharp() ? "bool " : "const bool ") + v + " = " + nt->emitExpr(c) + ";");
      c.markMapping("Result");
    }
  };
  n.cppHeader = "<map>";
  return n;
}

}  // namespace

// ---------------------------------------------------------------- Değişkenler

namespace {

// Değişken adı: kullanıcı metin portundan ham literal gelir; tırnakları
// soyup ayrıntılandırıyoruz.
std::string varName(EmitContext& c) {
  std::string raw = c.rawLiteral("Name");
  if (raw.empty()) raw = c.prop("name", "degisken");
  if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
    raw = raw.substr(1, raw.size() - 2);
  }
  std::string out;
  for (char ch : raw) {
    out += (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') ? ch : '_';
  }
  if (out.empty() || std::isdigit(static_cast<unsigned char>(out[0]))) out = "v_" + out;
  return out;
}

NodeType varMake(const std::string& suffix, const std::string& label, TypeRef ty,
                 const std::string& csInit, const std::string& cppInit,
                 const std::string& cppType) {
  NodeType n;
  n.id = "var.make." + suffix;
  n.displayName = label + " Tanımla";
  n.category = "Değişken";
  n.icon = "\xEE\xA2\xB3";
  n.accentColor = "#10B981";
  n.library = "GPI";
  n.description = label + " tipinde bir değişken oluşturur ve başlangıç değerini verir.";
  n.inputs = {execIn("In", "Giriş"), in("Name", t::String(), "\"degisken\"", "Değişken Adı"),
              in("Initial", ty, csInit, "Başlangıç Değeri")};
  n.outputs = {execOut("Next", "Devam")};
  n.csUsings = {};
  n.emit = [cppType](EmitContext& c) {
    const std::string nm = varName(c);
    (void)cppType;
    // C#'ta tip zaten bildirimin parçası, C++'ta std::int32_t gibi
    // sabit genişlikli ad kullanıyoruz.
    const std::string decl = c.isCSharp() ? c.typeName(c.portType("Initial"))
                                          : c.nativeTypeName(c.portType("Initial"));
    c.line(decl + " " + nm + " = " + c.value("Initial") + ";");
  };
  return n;
}

NodeType varGet(const std::string& suffix, const std::string& label, TypeRef ty) {
  NodeType n;
  n.id = "var.get." + suffix;
  n.displayName = label + " Değişkenini Oku";
  n.category = "Değişken";
  n.icon = "\xEE\x9E\xB3";
  n.accentColor = "#10B981";
  n.pure = true;
  n.library = "GPI";
  n.description = "Daha önce tanımlanmış bir değişkenin değerini verir.";
  n.inputs = {in("Name", t::String(), "\"degisken\"", "Değişken Adı")};
  n.outputs = {out("Value", ty, "Değer")};
  auto call = [](EmitContext& c) { return varName(c); };
  n.emitExpr = call;
  n.emit = [call](EmitContext& c) {
    const std::string v = c.local("Value");
    c.bindOutput("Value", v);
    c.line(v + " = " + call(c) + ";");
    c.markMapping("Value");
  };
  return n;
}

NodeType varSet(const std::string& suffix, const std::string& label, TypeRef ty) {
  NodeType n;
  n.id = "var.set." + suffix;
  n.displayName = label + " Değişkenine Yaz";
  n.category = "Değişken";
  n.icon = "\xEE\x9C\x8F";
  n.accentColor = "#10B981";
  n.library = "GPI";
  n.description = "Değişkene yeni değer atar.";
  n.inputs = {execIn("In", "Giriş"), in("Name", t::String(), "\"degisken\"", "Değişken Adı"),
              in("Value", ty, "", "Değer")};
  n.outputs = {execOut("Next", "Devam")};
  n.emit = [](EmitContext& c) {
    c.line(varName(c) + " = " + c.value("Value") + ";");
  };
  return n;
}

}  // namespace

NodeType makeVarMakeInt() { return varMake("int", "Tam Sayı", t::Int(), "0", "0", "int"); }
NodeType makeVarMakeString() {
  return varMake("string", "Metin", t::String(), "\"\"", "std::string{}", "std::string");
}
NodeType makeVarMakeDouble() { return varMake("double", "Ondalık", t::Double(), "0.0", "0.0", "double"); }
NodeType makeVarMakeBool() { return varMake("bool", "Mantıksal", t::Bool(), "false", "false", "bool"); }

NodeType makeVarGetInt() { return varGet("int", "Tam Sayı", t::Int()); }
NodeType makeVarGetString() { return varGet("string", "Metin", t::String()); }
NodeType makeVarGetDouble() { return varGet("double", "Ondalık", t::Double()); }
NodeType makeVarGetBool() { return varGet("bool", "Mantıksal", t::Bool()); }

NodeType makeVarSetInt() { return varSet("int", "Tam Sayı", t::Int()); }
NodeType makeVarSetString() { return varSet("string", "Metin", t::String()); }
NodeType makeVarSetDouble() { return varSet("double", "Ondalık", t::Double()); }
NodeType makeVarSetBool() { return varSet("bool", "Mantıksal", t::Bool()); }

NodeType makeIf() { return ifNode(); }
NodeType makeWhile() { return whileNode(); }
NodeType makeForRange() { return forRangeNode(); }

NodeType makeCompare() {
  NodeType n;
  n.id = "flow.compare.int";
  n.displayName = "Karşılaştır (Sayı)";
  n.category = "Karşılaştırma";
  n.icon = "\xEE\x9C\x9D";
  n.accentColor = "#A855F7";
  n.pure = true;
  n.library = "System";
  n.description = "A ve B sayılarını karşılaştırır. Karşılaştırma özelliğini seçebilirsiniz.";
  n.inputs = {in("A", t::Int(), "0", "A"), in("B", t::Int(), "0", "B")};
  n.outputs = {out("Result", t::Bool(), "Sonuç")};
  n.csUsings = {"System"};
  n.emitExpr = [](EmitContext& c) {
    const std::string a = c.value("A");
    const std::string b = c.value("B");
    const std::string op = c.prop("operator", "==");
    if (c.isCSharp()) {
      static const std::map<std::string, std::string> ops = {
          {"==", "=="}, {"!=", "!="}, {"<", "<"}, {"<=", "<="}, {">", ">"}, {">=", ">="}};
      auto it = ops.find(op);
      return "(" + a + " " + (it == ops.end() ? "==" : it->second) + " " + b + ")";
    }
    return "(" + a + " " + op + " " + b + ")";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("Result");
      c.bindOutput("Result", v);
      c.line((c.isCSharp() ? "bool " : "const bool ") + v + " = " + nt->emitExpr(c) + ";");
      c.markMapping("Result");
    }
  };
  return n;
}

NodeType makeCompareFloat() {
  NodeType n = makeCompare();
  n.id = "flow.compare.float";
  n.displayName = "Karşılaştır (Ondalık)";
  n.inputs = {in("A", t::Double(), "0", "A"), in("B", t::Double(), "0", "B")};
  return n;
}

NodeType makeCompareString() {
  NodeType n = makeCompare();
  n.id = "flow.compare.string";
  n.displayName = "Karşılaştır (Metin)";
  n.inputs = {in("A", t::String(), "\"\"", "A"), in("B", t::String(), "\"\"", "B")};
  n.description = "İki metni karşılaştırır.";
  return n;
}

NodeType makeAnd() {
  NodeType n;
  n.id = "flow.and";
  n.displayName = "Ve";
  n.category = "Mantık";
  n.icon = "\xEE\x9E\x95";
  n.accentColor = "#A855F7";
  n.pure = true;
  n.library = "System";
  n.inputs = {in("A", t::Bool(), "false", "A"), in("B", t::Bool(), "false", "B")};
  n.outputs = {out("Result", t::Bool(), "Sonuç")};
  n.emitExpr = [](EmitContext& c) {
    return "(" + c.value("A") + " && " + c.value("B") + ")";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("Result");
      c.bindOutput("Result", v);
      c.line((c.isCSharp() ? "bool " : "const bool ") + v + " = " + nt->emitExpr(c) + ";");
      c.markMapping("Result");
    }
  };
  return n;
}

NodeType makeOr() {
  NodeType n = makeAnd();
  n.id = "flow.or";
  n.displayName = "Veya";
  n.icon = "\xEE\x9E\x94";
  n.emitExpr = [](EmitContext& c) { return "(" + c.value("A") + " || " + c.value("B") + ")"; };
  return n;
}

NodeType makeNot() {
  NodeType n;
  n.id = "flow.not";
  n.displayName = "Değil";
  n.category = "Mantık";
  n.icon = "\xEE\xA2\xB6";
  n.accentColor = "#A855F7";
  n.pure = true;
  n.library = "System";
  n.inputs = {in("A", t::Bool(), "false", "A")};
  n.outputs = {out("Result", t::Bool(), "Sonuç")};
  n.emitExpr = [](EmitContext& c) { return "(!" + c.value("A") + ")"; };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("Result");
      c.bindOutput("Result", v);
      c.line((c.isCSharp() ? "bool " : "const bool ") + v + " = " + nt->emitExpr(c) + ";");
      c.markMapping("Result");
    }
  };
  return n;
}

NodeType makeBranch() { return branchNode(); }
NodeType makeBreak() {
  return simpleFlow("flow.break", "Döngüden Çık", "\xEE\xA2\xA9", "break",
                    "İçinde bulunulan en yakın döngüden çıkar.");
}
NodeType makeContinue() {
  return simpleFlow("flow.continue", "Sonraki Yineleme", "\xEE\x9C\xAC", "continue",
                    "İçinde bulunulan döngünün sonraki adımına geçer.");
}
NodeType makeExit() {
  NodeType n = simpleFlow("flow.exit", "Programdan Çık", "\xEE\xA2\xBB", "return",
                          "Programı burada sonlandırır.");
  n.inputs.push_back(in("Value", t::Any(), "", "Dönüş Değeri"));
  return n;
}

NodeType makeListNew() { return listNew(t::Int(), "int"); }
NodeType makeListAdd() { return listAdd(t::Int(), "int", "int"); }
NodeType makeListGet() { return listGet(t::Int(), "int", "int"); }
NodeType makeListCount() { return listCount(t::Int(), "int"); }
NodeType makeListSet() { return listSet(t::Int(), "int"); }
NodeType makeListForEach() { return foreachNode("flow.foreach.int", "Listedeki Her Öğe", t::Int(), "int"); }

NodeType makeArrayNew() { return listNew(t::String(), "string"); }
NodeType makeArrayGet() { return listGet(t::String(), "string", "std::string"); }
NodeType makeArraySet() { return listSet(t::String(), "string"); }
NodeType makeArrayLength() { return listCount(t::String(), "string"); }
NodeType makeDictNew() { return dictNew(t::Int(), "int"); }
NodeType makeDictGet() { return dictGet(t::Int(), "int"); }
NodeType makeDictSet() { return dictSet(t::Int(), "int"); }
NodeType makeDictHas() { return dictHas("int"); }
NodeType makeDictRemove() { return dictHas("int"); }

NodeType makePrint() {
  NodeType n;
  n.id = "io.print";
  n.displayName = "Ekrana Yaz";
  n.category = "Giriş / Çıkış";
  n.icon = "\xEE\xA3\xA5";
  n.accentColor = "#EF4444";
  n.library = "System";
  n.description = "Değeri konsola yazar ve yeni satıra geçer.";
  n.inputs = {execIn("In", "Giriş"), in("Value", t::Any(), "\"\"", "Değer")};
  n.outputs = {execOut("Next", "Devam")};
  n.csUsings = {"System"};
  n.cppHeader = "<iostream>";
  n.emit = [](EmitContext& c) {
    if (c.isCSharp()) {
      c.line("Console.WriteLine(" + c.value("Value") + ");");
    } else {
      c.line("std::cout << " + c.value("Value") + " << std::endl;");
    }
  };
  return n;
}

NodeType makeReadLine() {
  NodeType n;
  n.id = "io.readline";
  n.displayName = "Satır Oku";
  n.category = "Giriş / Çıkış";
  n.icon = "\xEE\x9C\x8D";
  n.accentColor = "#EF4444";
  n.library = "System";
  n.description = "Kullanıcıdan bir satır metin okur.";
  n.inputs = {execIn("In", "Giriş")};
  n.outputs = {execOut("Next", "Devam"), out("Text", t::String(), "Metin")};
  n.csUsings = {"System"};
  n.cppHeader = "<iostream>";
  n.emit = [](EmitContext& c) {
    const std::string v = c.local("Text");
    c.bindOutput("Text", v);
    if (c.isCSharp()) {
      c.line("string " + v + " = Console.ReadLine() ?? \"\";");
    } else {
      c.line("std::string " + v + ";  std::getline(std::cin, " + v + ");");
    }
    c.markMapping("Text");
  };
  return n;
}

NodeType makeDelay() {
  NodeType n;
  n.id = "io.delay";
  n.displayName = "Bekle";
  n.category = "Giriş / Çıkış";
  n.icon = "\xEE\xA0\xA3";
  n.accentColor = "#EF4444";
  n.library = "System";
  n.description = "Verilen milisaniye kadar bekler.";
  n.inputs = {execIn("In", "Giriş"), in("Milliseconds", t::Int(), "500", "Milisaniye")};
  n.outputs = {execOut("Next", "Devam")};
  n.csUsings = {"System"};
  n.cppHeader = "<thread>";
  n.emit = [](EmitContext& c) {
    if (c.isCSharp()) {
      c.line("System.Threading.Thread.Sleep(" + c.value("Milliseconds") + ");");
    } else {
      c.line("std::this_thread::sleep_for(std::chrono::milliseconds(" + c.value("Milliseconds") +
             "));");
    }
  };
  return n;
}

NodeType makeRandom() {
  NodeType n;
  n.id = "math.random";
  n.displayName = "Rastgele Sayı";
  n.category = "Matematik";
  n.icon = "\xEE\x9E\xB4";
  n.accentColor = "#F59E0B";
  n.pure = true;
  n.library = "System";
  n.description = "Min ile Max arasında (Max dahil değil) rastgele tam sayı üretir.";
  n.inputs = {in("Min", t::Int(), "0", "En Küçük"), in("Max", t::Int(), "100", "En Büyük")};
  n.outputs = {out("Result", t::Int(), "Sonuç")};
  n.csUsings = {"System"};
  n.cppHeader = "<random>";
  n.emitExpr = [](EmitContext& c) {
    if (c.isCSharp()) return "Random.Shared.Next(" + c.value("Min") + ", " + c.value("Max") + ")";
    return "(gpi::rand_int(" + c.value("Min") + ", " + c.value("Max") + "))";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("Result");
      c.bindOutput("Result", v);
      c.line((c.isCSharp() ? "int " : "const int ") + v + " = " + nt->emitExpr(c) + ";");
      c.markMapping("Result");
    }
  };
  return n;
}

NodeType makeComment() {
  NodeType n;
  n.id = "flow.comment";
  n.displayName = "Açıklama";
  n.category = "Akış";
  n.icon = "\xEE\xA2\xAB";
  n.accentColor = "#64748B";
  n.library = "GPI";
  n.description = "Kodda açıklama satırı üretir. Program akışını değiştirmez.";
  n.inputs = {execIn("In", "Giriş"), in("Text", t::String(), "\"\"", "Açıklama")};
  n.outputs = {execOut("Next", "Devam")};
  n.emit = [](EmitContext& c) {
    std::string txt = c.rawLiteral("Text");
    if (txt.size() >= 2 && txt.front() == '"' && txt.back() == '"') txt = txt.substr(1, txt.size() - 2);
    if (c.isCSharp()) {
      c.line("// " + txt);
    } else {
      c.line("// " + txt);
    }
  };
  return n;
}

NodeType makeLiteralText() {
  NodeType n;
  n.id = "lit.text";
  n.displayName = "Metin";
  n.category = "Değer";
  n.icon = "\xEE\xA3\x92";
  n.accentColor = "#EF4444";
  n.pure = true;
  n.library = "GPI";
  n.description = "Sabit bir metin değeri.";
  n.inputs = {in("Value", t::String(), "\"Merhaba\"", "Değer")};
  n.outputs = {out("Value", t::String(), "Değer")};
  auto call = [](EmitContext& c) { return c.value("Value"); };
  n.emitExpr = call;
  n.emit = [call](EmitContext& c) {
    const std::string v = c.local("Value");
    c.bindOutput("Value", v);
    c.line((c.isCSharp() ? "string " : "const std::string ") + v + " = " + call(c) + ";");
    c.markMapping("Value");
  };
  n.cppHeader = "<string>";
  return n;
}

NodeType makeLiteralNumber() {
  NodeType n;
  n.id = "lit.number";
  n.displayName = "Tam Sayı";
  n.category = "Değer";
  n.icon = "\xEE\xA7\x92";
  n.accentColor = "#EF4444";
  n.pure = true;
  n.library = "GPI";
  n.inputs = {in("Value", t::Int(), "0", "Değer")};
  n.outputs = {out("Value", t::Int(), "Değer")};
  auto call = [](EmitContext& c) { return c.value("Value"); };
  n.emitExpr = call;
  n.emit = [call](EmitContext& c) {
    const std::string v = c.local("Value");
    c.bindOutput("Value", v);
    c.line((c.isCSharp() ? "int " : "constexpr int ") + v + " = " + call(c) + ";");
    c.markMapping("Value");
  };
  return n;
}

NodeType makeLiteralDouble() {
  NodeType n = makeLiteralNumber();
  n.id = "lit.double";
  n.displayName = "Ondalık";
  n.inputs = {in("Value", t::Double(), "0.0", "Değer")};
  n.outputs = {out("Value", t::Double(), "Değer")};
  auto call = [](EmitContext& c) { return c.value("Value"); };
  n.emitExpr = call;
  n.emit = [call](EmitContext& c) {
    const std::string v = c.local("Value");
    c.bindOutput("Value", v);
    c.line((c.isCSharp() ? "double " : "const double ") + v + " = " + call(c) + ";");
    c.markMapping("Value");
  };
  return n;
}

NodeType makeLiteralBool() {
  NodeType n;
  n.id = "lit.bool";
  n.displayName = "Doğru / Yanlış";
  n.category = "Değer";
  n.icon = "\xEE\xA2\xBA";
  n.accentColor = "#EF4444";
  n.pure = true;
  n.library = "GPI";
  n.inputs = {in("Value", t::Bool(), "false", "Değer")};
  n.outputs = {out("Value", t::Bool(), "Değer")};
  auto call = [](EmitContext& c) { return c.value("Value"); };
  n.emitExpr = call;
  n.emit = [call](EmitContext& c) {
    const std::string v = c.local("Value");
    c.bindOutput("Value", v);
    c.line((c.isCSharp() ? "bool " : "constexpr bool ") + v + " = " + call(c) + ";");
    c.markMapping("Value");
  };
  return n;
}

NodeType makeMakeClass() {
  NodeType n;
  n.id = "def.class";
  n.displayName = "Sınıf Tanımı";
  n.category = "Tanım";
  n.icon = "\xEE\xA2\xB7";
  n.accentColor = "#8B5CF6";
  n.pure = true;
  n.library = "GPI";
  n.description = "Özellikler panelinden alanları tanımladığınız bir sınıf üretir.";
  n.inputs = {in("Fields", t::String(), "\"\"", "Alanlar (ad:tür,ad:tür)")};
  n.outputs = {out("Type", TypeRef::of("Object", TypeKind::Class), "Sınıf")};
  n.emit = [](EmitContext&) {};
  return n;
}

NodeType makeStructNew() {
  NodeType n;
  n.id = "struct.new";
  n.displayName = "Nesne Oluştur";
  n.category = "Tanım";
  n.icon = "\xEE\xA2\xB6";
  n.accentColor = "#8B5CF6";
  n.pure = true;
  n.library = "GPI";
  n.description = "Sınıf Tanımı düğümünden üretilen sınıfın yeni bir örneğini oluşturur.";
  n.inputs = {in("Type", TypeRef::of("Object", TypeKind::Class), "", "Sınıf")};
  n.outputs = {out("Instance", TypeRef::of("Object", TypeKind::Class), "Örnek")};
  n.emit = [](EmitContext& c) {
    const std::string ty = c.valueType("Type").name;
    const std::string v = c.local("Instance");
    c.bindOutput("Instance", v);
    c.line((c.isCSharp() ? ty + " " : "auto ") + v + " = " +
           (c.isCSharp() ? "new " + ty + "()" : ty + "{}") + ";");
    c.markMapping("Instance");
  };
  return n;
}

}  // namespace gpi::builtin
