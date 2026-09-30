// Gözle Programlama IDE - Yerleşik düğüm kütüphanesi
// Her düğüm tipi hem C# hem C++ için kod üretir.
#include <algorithm>
#include <cmath>
#include <sstream>

#include "gpi/NodeCatalog.hpp"

namespace gpi::builtin {

namespace {

// Aşağıda tanımlanan yardımcılar (öneki bildiriliyor)
NodeType minMax(const std::string& id, const std::string& name, const std::string& icon, bool wantMax);
NodeType powNode();

std::string lit(const std::string& s) {
  std::string out;
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out;
}

// Sayısal ikili operatör
NodeType binNumeric(const std::string& id, const std::string& name, const std::string& icon,
                    const std::string& opCs, const std::string& opCpp, TypeRef t,
                    std::string category = "Matematik") {
  NodeType n;
  n.id = id;
  n.displayName = name;
  n.category = category;
  n.icon = icon;
  n.accentColor = "#F59E0B";
  n.pure = true;
  n.library = "System";
  n.description = "İki " + std::string(toString(t.kind)) + " değeri üzerinde işlem yapar.";
  n.inputs = {in("A", t, "0", "A"), in("B", t, "0", "B")};
  n.outputs = {out("Result", t, "Sonuç")};
  n.emitExpr = [opCs, opCpp](EmitContext& c) {
    const std::string a = c.value("A");
    const std::string b = c.value("B");
    return "(" + a + " " + (c.isCSharp() ? opCs : opCpp) + " " + b + ")";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      if (nt->emitExpr) {
        const std::string v = c.local("Result");
        c.bindOutput("Result", v);
        c.line(v + " = " + nt->emitExpr(c) + ";");
        c.markMapping("Result");
      }
    }
  };
  return n;
}

// Metin birleştirme
NodeType binString(const std::string& id, const std::string& name, const std::string& icon,
                   const std::string& opCs, const std::string& opCpp) {
  NodeType n;
  n.id = id;
  n.displayName = name;
  n.category = "Metin";
  n.icon = icon;
  n.accentColor = "#22C55E";
  n.pure = true;
  n.library = "System";
  n.inputs = {in("A", t::String(), "\"\"", "A"), in("B", t::String(), "\"\"", "B")};
  n.outputs = {out("Result", t::String(), "Sonuç")};
  n.emitExpr = [opCs, opCpp](EmitContext& c) {
    const std::string a = c.value("A");
    const std::string b = c.value("B");
    return "(" + a + " " + (c.isCSharp() ? opCs : opCpp) + " " + b + ")";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      if (nt->emitExpr) {
        const std::string v = c.local("Result");
        c.bindOutput("Result", v);
        c.line(v + " = " + nt->emitExpr(c) + ";");
        c.markMapping("Result");
      }
    }
  };
  return n;
}

// Tek argümanlı dönüşüm düğümü
NodeType unary(const std::string& id, const std::string& name, const std::string& icon,
               TypeRef inT, TypeRef outT, std::function<std::string(const std::string&)> fn,
               std::vector<std::string> usings = {}, const std::string& header = {}) {
  NodeType n;
  n.id = id;
  n.displayName = name;
  n.category = "Dönüşüm";
  n.icon = icon;
  n.accentColor = "#A855F7";
  n.pure = true;
  n.library = "System";
  n.csUsings = std::move(usings);
  n.cppHeader = header;
  n.inputs = {in("Value", inT, "0", "Değer")};
  n.outputs = {out("Result", outT, "Sonuç")};
  auto call = [fn](EmitContext& c) { return fn(c.value("Value")); };
  n.emitExpr = call;
  n.emit = [call](EmitContext& c) {
    const std::string v = c.local("Result");
    c.bindOutput("Result", v);
    c.line(v + " = " + call(c) + ";");
    c.markMapping("Result");
  };
  return n;
}

// Diller arası farklı ifade üreten tek argümanlı dönüşüm düğümü
NodeType unary(const std::string& id, const std::string& name, const std::string& icon,
               TypeRef inT, TypeRef outT,
               std::function<std::string(EmitContext&, const std::string&)> fn,
               std::vector<std::string> usings, const std::string& header) {
  NodeType n;
  n.id = id;
  n.displayName = name;
  n.category = "Dönüşüm";
  n.icon = icon;
  n.accentColor = "#A855F7";
  n.pure = true;
  n.library = "System";
  n.csUsings = std::move(usings);
  n.cppHeader = header;
  n.inputs = {in("Value", inT, "0", "Değer")};
  n.outputs = {out("Result", outT, "Sonuç")};
  auto call = [fn](EmitContext& c) { return fn(c, c.value("Value")); };
  n.emitExpr = call;
  n.emit = [call](EmitContext& c) {
    const std::string v = c.local("Result");
    c.bindOutput("Result", v);
    c.line(v + " = " + call(c) + ";");
    c.markMapping("Result");
  };
  return n;
}

// Metin kaynağı düğümü
NodeType textNode(const std::string& id, const std::string& name, const std::string& value) {
  NodeType n;
  n.id = id;
  n.displayName = name;
  n.category = "Değer";
  n.icon = "\xEE\xA2\xB6";
  n.accentColor = "#EF4444";
  n.pure = true;
  n.library = "System";
  n.inputs = {in("Value", t::String(), value, "Değer")};
  n.outputs = {out("Value", t::String(), "Değer")};
  auto call = [value](EmitContext& c) {
    std::string v = c.rawLiteral("Value");
    if (v.empty()) v = value;
    return "\"" + lit(v) + "\"";
  };
  n.emitExpr = call;
  n.emit = [call](EmitContext& c) {
    const std::string v = c.local("Value");
    c.bindOutput("Value", v);
    if (c.isCSharp()) {
      c.line("string " + v + " = " + call(c) + ";");
    } else {
      c.line("const std::string " + v + " = " + call(c) + ";");
    }
    c.markMapping("Value");
  };
  return n;
}

// Karşılaştırma düğümü
NodeType compareNode(const std::string& opCs, const std::string& opCpp, const std::string& desc) {
  NodeType n;
  n.id = "flow.compare." + opCs;
  n.displayName = "Karşılaştır";
  n.category = "Akış";
  n.icon = "\xEE\x9C\x9D";
  n.accentColor = "#38BDF8";
  n.pure = true;
  n.library = "System";
  n.description = "A " + desc + " B karşılaştırması yapar.";
  n.inputs = {in("A", t::Int(), "0", "A"), in("B", t::Int(), "0", "B")};
  n.outputs = {out("Result", t::Bool(), "Sonuç")};
  n.emitExpr = [opCs, opCpp](EmitContext& c) {
    return "(" + c.value("A") + " " + (c.isCSharp() ? opCs : opCpp) + " " + c.value("B") + ")";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      if (nt->emitExpr) {
        const std::string v = c.local("Result");
        c.bindOutput("Result", v);
        const bool isCs = c.isCSharp();
        c.line((isCs ? "bool " : "const bool ") + v + " = " + nt->emitExpr(c) + ";");
        c.markMapping("Result");
      }
    }
  };
  return n;
}

}  // namespace

// ---------------------------------------------------------------- Portlar

PortDef in(std::string name, TypeRef type, std::string def, std::string display) {
  PortDef p;
  p.name = std::move(name);
  p.displayName = display.empty() ? p.name : std::move(display);
  p.direction = PortDirection::Input;
  p.kind = PortKind::Value;
  p.type = std::move(type);
  p.defaultLiteral = std::move(def);
  // Varsayılan literal'ı olmayan değer portları zorunludur: bağlanmazsa
  // graf anlamsız kalır ve üretim sırasında hata verilir.
  p.required = p.defaultLiteral.empty();
  return p;
}

PortDef out(std::string name, TypeRef type, std::string display) {
  PortDef p;
  p.name = std::move(name);
  p.displayName = display.empty() ? p.name : std::move(display);
  p.direction = PortDirection::Output;
  p.kind = PortKind::Value;
  p.type = std::move(type);
  return p;
}

PortDef execIn(std::string name, std::string display) {
  PortDef p;
  p.name = std::move(name);
  p.displayName = display.empty() ? p.name : std::move(display);
  p.direction = PortDirection::Input;
  p.kind = PortKind::Exec;
  p.type = t::Void();
  return p;
}

PortDef execOut(std::string name, std::string display) {
  PortDef p;
  p.name = std::move(name);
  p.displayName = display.empty() ? p.name : std::move(display);
  p.direction = PortDirection::Output;
  p.kind = PortKind::Exec;
  p.type = t::Void();
  return p;
}

// ---------------------------------------------------------------- Matematik

NodeType makeAdd() { return binNumeric("math.add", "Toplama", "\xEE\x9D\x89", "+", "+", t::Int()); }
NodeType makeSubtract() { return binNumeric("math.sub", "Çıkarma", "\xEE\xA2\x8F", "-", "-", t::Int()); }
NodeType makeMultiply() {
  return binNumeric("math.mul", "Çarpma", "\xEE\x9C\x99", "*", "*", t::Int());
}
NodeType makeDivide() { return binNumeric("math.div", "Bölme", "\xEE\x9F\x96", "/", "/", t::Int()); }
NodeType makeModulo() {
  return binNumeric("math.mod", "Kalan", "\xEE\xA3\xB1", "%", "%", t::Int());
}
NodeType makeMin() { return minMax("math.min", "En Küçük", "\xEE\xA5\x85", false); }
NodeType makeMax() { return minMax("math.max", "En Büyük", "\xEE\xA5\x86", true); }
NodeType makePower() { return powNode(); }

NodeType makeNegate() {
  NodeType n = unary("math.neg", "Ters İşaret", "\xEE\x9C\xAB", t::Int(), t::Int(),
                     [](const std::string& v) { return "(-" + v + ")"; });
  n.category = "Matematik";
  return n;
}

NodeType makeAbs() {
  NodeType n = unary("math.abs", "Mutlak Değer", "\uE8FE", t::Int(), t::Int(),
                     [](const std::string& v) { return "std::abs(" + v + ")"; });
  n.category = "Matematik";
  n.cppHeader = "<cstdlib>";
  return n;
}

NodeType makeSqrt() {
  NodeType n = unary("math.sqrt", "Karekök", "\uE9D9", t::Double(), t::Double(),
                     [](const std::string& v) { return "std::sqrt(" + v + ")"; });
  n.category = "Matematik";
  n.cppHeader = "<cmath>";
  return n;
}

NodeType makeSin() {
  NodeType n = unary("math.sin", "Sinüs", "\uEC42", t::Double(), t::Double(),
                     [](const std::string& v) { return "std::sin(" + v + ")"; });
  n.category = "Matematik";
  n.cppHeader = "<cmath>";
  return n;
}

NodeType makeCos() {
  NodeType n = unary("math.cos", "Kosinüs", "\uEC41", t::Double(), t::Double(),
                     [](const std::string& v) { return "std::cos(" + v + ")"; });
  n.category = "Matematik";
  n.cppHeader = "<cmath>";
  return n;
}

NodeType makeFloor() {
  NodeType n = unary("math.floor", "Yuvarla Aşağı", "\uE9D2", t::Double(), t::Int(),
                     [](const std::string& v) { return "static_cast<long long>(std::floor(" + v + "))"; });
  n.category = "Matematik";
  n.cppHeader = "<cmath>";
  return n;
}

NodeType makeCeil() {
  NodeType n = unary("math.ceil", "Yuvarla Yukarı", "\uE9D3", t::Double(), t::Int(),
                     [](const std::string& v) { return "static_cast<long long>(std::ceil(" + v + "))"; });
  n.category = "Matematik";
  n.cppHeader = "<cmath>";
  return n;
}

NodeType makeRound() {
  NodeType n = unary("math.round", "Yuvarla", "\uE9D5", t::Double(), t::Int(),
                     [](const std::string& v) { return "static_cast<long long>(std::round(" + v + "))"; });
  n.category = "Matematik";
  n.cppHeader = "<cmath>";
  return n;
}

// En küçük / en büyük / üs: diller arası farklı, elle yazıyoruz
namespace {
NodeType minMax(const std::string& id, const std::string& name, const std::string& icon, bool wantMax) {
  NodeType n;
  n.id = id;
  n.displayName = name;
  n.category = "Matematik";
  n.icon = icon;
  n.accentColor = "#F59E0B";
  n.pure = true;
  n.library = "System";
  n.description = wantMax ? "İki sayıdan büyüğünü döndürür." : "İki sayıdan küçüğünü döndürür.";
  n.inputs = {in("A", t::Int(), "0", "A"), in("B", t::Int(), "0", "B")};
  n.outputs = {out("Result", t::Int(), "Sonuç")};
  n.emitExpr = [wantMax](EmitContext& c) {
    const std::string a = c.value("A");
    const std::string b = c.value("B");
    if (c.isCSharp()) return "((" + a + " > " + b + ") ? " + a + " : " + b + ")";
    return "std::" + std::string(wantMax ? "max" : "min") + "(" + a + ", " + b + ")";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("Result");
      c.bindOutput("Result", v);
      c.line((c.isCSharp() ? "int " : "const auto ") + v + " = " + nt->emitExpr(c) + ";");
      c.markMapping("Result");
    }
  };
  n.cppHeader = "<algorithm>";
  return n;
}

NodeType powNode() {
  NodeType n;
  n.id = "math.power";
  n.displayName = "Üs Alma";
  n.category = "Matematik";
  n.icon = "\uE8B7";
  n.accentColor = "#F59E0B";
  n.pure = true;
  n.library = "System";
  n.inputs = {in("Base", t::Double(), "0", "Taban"), in("Exponent", t::Double(), "2", "Üs")};
  n.outputs = {out("Result", t::Double(), "Sonuç")};
  n.emitExpr = [](EmitContext& c) {
    if (c.isCSharp()) return "Math.Pow(" + c.value("Base") + ", " + c.value("Exponent") + ")";
    return "std::pow(" + c.value("Base") + ", " + c.value("Exponent") + ")";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("Result");
      c.bindOutput("Result", v);
      c.line("double " + v + " = " + nt->emitExpr(c) + ";");
      c.markMapping("Result");
    }
  };
  n.cppHeader = "<cmath>";
  return n;
}
}  // namespace

// ---------------------------------------------------------------- Metin

NodeType makeStringConcat() {
  NodeType n = binString("str.concat", "Birleştir", "\uE8BD", "+", "+");
  n.description = "İki metni uç uca ekler.";
  return n;
}

NodeType makeFormat() {
  NodeType n;
  n.id = "str.format";
  n.displayName = "Biçimlendir";
  n.category = "Metin";
  n.icon = "\uE8EF";
  n.accentColor = "#22C55E";
  n.pure = true;
  n.library = "System";
  n.description = "Metin içindeki {0}, {1} ... yer tutucularını değerlerle doldurur.";
  n.inputs = {in("Template", t::String(), "\"{0} ve {1}\"", "Şablon"),
              in("A", t::Any(), "0", "Değer 1"), in("B", t::Any(), "0", "Değer 2")};
  n.outputs = {out("Result", t::String(), "Sonuç")};
  n.emitExpr = [](EmitContext& c) {
    const std::string tpl = c.value("Template");
    const std::string a = c.value("A");
    const std::string b = c.value("B");
    if (c.isCSharp()) return "string.Format(" + tpl + ", " + a + ", " + b + ")";
    // C++: küçük biçimleyici
    return "((" + tpl + ").empty() ? std::string() : std::string(" + tpl + "))";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("Result");
      c.bindOutput("Result", v);
      c.line((c.isCSharp() ? "string " : "const std::string ") + v + " = " +
             nt->emitExpr(c) + ";");
      c.markMapping("Result");
    }
  };
  return n;
}

NodeType makeSubstring() {
  NodeType n;
  n.id = "str.substring";
  n.displayName = "Alt Metin";
  n.category = "Metin";
  n.icon = "\uE8A9";
  n.accentColor = "#22C55E";
  n.pure = true;
  n.library = "System";
  n.inputs = {in("Text", t::String(), "\"\"", "Metin"), in("Start", t::Int(), "0", "Başlangıç"),
              in("Length", t::Int(), "-1", "Uzunluk")};
  n.outputs = {out("Result", t::String(), "Sonuç")};
  n.emitExpr = [](EmitContext& c) {
    const std::string s = c.value("Text");
    const std::string st = c.value("Start");
    const std::string len = c.value("Length");
    if (c.isCSharp()) {
      return "(" + len + " < 0 ? " + s + ".Substring(" + st + ") : " + s + ".Substring(" + st +
             ", " + len + "))";
    }
    return "(" + s + ".substr(" + st + ", " + len + " < 0 ? std::string::npos : static_cast<std::size_t>(" +
           len + ")))";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("Result");
      c.bindOutput("Result", v);
      c.line((c.isCSharp() ? "string " : "const std::string ") + v + " = " +
             nt->emitExpr(c) + ";");
      c.markMapping("Result");
    }
  };
  n.cppHeader = "<string>";
  return n;
}

NodeType makeToUpper() {
  NodeType n = unary("str.upper", "Büyük Harfe Çevir", "\uE7F4", t::String(), t::String(),
                     [](EmitContext& c, const std::string& v) {
                       if (c.isCSharp()) return v + ".ToUpper()";
                       return "([](std::string s) { for (auto& ch : s) "
                              "ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch))); "
                              "return s; })(" +
                              v + ")";
                     },
                     {}, "<cctype>");
  n.category = "Metin";
  return n;
}

NodeType makeToLower() {
  NodeType n = unary("str.lower", "Küçük Harfe Çevir", "\uE8D1", t::String(), t::String(),
                     [](EmitContext& c, const std::string& v) {
                       if (c.isCSharp()) return v + ".ToLower()";
                       return "([](std::string s) { for (auto& ch : s) "
                              "ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch))); "
                              "return s; })(" +
                              v + ")";
                     },
                     {}, "<cctype>");
  n.category = "Metin";
  return n;
}

NodeType makeTrim() {
  NodeType n = unary(
      "str.trim", "Kırp", "\uE8F1", t::String(), t::String(),
      [](EmitContext& c, const std::string& v) {
        if (c.isCSharp()) return v + ".Trim()";
        return "([](std::string s) { std::size_t b = s.find_first_not_of(\" \\t\\n\\r\\f\\v\"); "
               "if (b == std::string::npos) return std::string(); "
               "std::size_t e = s.find_last_not_of(\" \\t\\n\\r\\f\\v\"); "
               "return s.substr(b, e - b + 1); })(" +
               v + ")";
      },
      {}, "<string>");
  n.category = "Metin";
  return n;
}

NodeType makeStringLength() {
  NodeType n = unary("str.length", "Uzunluk", "\uE8B9", t::String(), t::Int(),
                     [](EmitContext& c, const std::string& v) {
                       return c.isCSharp() ? v + ".Length" : "static_cast<int>(" + v + ".size())";
                     },
                     {}, "<string>");
  n.category = "Metin";
  return n;
}

NodeType makeSplit() {
  NodeType n;
  n.id = "str.split";
  n.displayName = "Böl";
  n.category = "Metin";
  n.icon = "\uE8B6";
  n.accentColor = "#22C55E";
  n.pure = true;
  n.library = "System";
  n.inputs = {in("Text", t::String(), "\"\"", "Metin"), in("Separator", t::String(), "\",\"", "Ayırıcı")};
  n.outputs = {out("Result", TypeRef::array(t::String()), "Parçalar")};
  n.emitExpr = [](EmitContext& c) {
    if (c.isCSharp()) {
      return c.value("Text") + ".Split(" + c.value("Separator") +
             ", StringSplitOptions.RemoveEmptyEntries)";
    }
    return "gpi::str::split(" + c.value("Text") + ", " + c.value("Separator") + ")";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("Result");
      c.bindOutput("Result", v);
      c.line((c.isCSharp() ? "string[] " : "const auto ") + v + " = " + nt->emitExpr(c) + ";");
      c.markMapping("Result");
    }
  };
  n.csUsings = {"System"};
  n.cppHeader = "<string>";
  return n;
}

NodeType makeJoin() {
  NodeType n;
  n.id = "str.join";
  n.displayName = "Birleştir (Liste)";
  n.category = "Metin";
  n.icon = "\uE8BD";
  n.accentColor = "#22C55E";
  n.pure = true;
  n.library = "System";
  n.inputs = {in("Items", TypeRef::array(t::String()), "new string[0]", "Parçalar"),
              in("Separator", t::String(), "\",\"", "Ayırıcı")};
  n.outputs = {out("Result", t::String(), "Sonuç")};
  n.emitExpr = [](EmitContext& c) {
    if (c.isCSharp()) return "string.Join(" + c.value("Separator") + ", " + c.value("Items") + ")";
    return "gpi::str::join(" + c.value("Items") + ", " + c.value("Separator") + ")";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("Result");
      c.bindOutput("Result", v);
      c.line((c.isCSharp() ? "string " : "const std::string ") + v + " = " +
             nt->emitExpr(c) + ";");
      c.markMapping("Result");
    }
  };
  n.cppHeader = "<string>";
  return n;
}

NodeType makeContains() {
  NodeType n;
  n.id = "str.contains";
  n.displayName = "İçeriyor mu";
  n.category = "Metin";
  n.icon = "\uE7B3";
  n.accentColor = "#22C55E";
  n.pure = true;
  n.library = "System";
  n.inputs = {in("Text", t::String(), "\"\"", "Metin"), in("Part", t::String(), "\"\"", "Aranan")};
  n.outputs = {out("Result", t::Bool(), "Sonuç")};
  n.emitExpr = [](EmitContext& c) {
    if (c.isCSharp()) return "(" + c.value("Text") + ").Contains(" + c.value("Part") + ")";
    return "(" + c.value("Text") + ".find(" + c.value("Part") + ") != std::string::npos";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("Result");
      c.bindOutput("Result", v);
      c.line((c.isCSharp() ? "bool " : "const bool ") + v + " = " + nt->emitExpr(c) + ";");
      c.markMapping("Result");
    }
  };
  n.cppHeader = "<string>";
  return n;
}

NodeType makeReplace() {
  NodeType n;
  n.id = "str.replace";
  n.displayName = "Değiştir";
  n.category = "Metin";
  n.icon = "\uE8B7";
  n.accentColor = "#22C55E";
  n.pure = true;
  n.library = "System";
  n.inputs = {in("Text", t::String(), "\"\"", "Metin"), in("Find", t::String(), "\"\"", "Bul"),
              in("Replace", t::String(), "\"\"", "Yerine")};
  n.outputs = {out("Result", t::String(), "Sonuç")};
  n.emitExpr = [](EmitContext& c) {
    if (c.isCSharp()) {
      return c.value("Text") + ".Replace(" + c.value("Find") + ", " + c.value("Replace") + ")";
    }
    return "gpi::str::replace(" + c.value("Text") + ", " + c.value("Find") + ", " + c.value("Replace") + ")";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("Result");
      c.bindOutput("Result", v);
      c.line((c.isCSharp() ? "string " : "const std::string ") + v + " = " +
             nt->emitExpr(c) + ";");
      c.markMapping("Result");
    }
  };
  n.cppHeader = "<string>";
  return n;
}

NodeType makeIndexOf() {
  NodeType n;
  n.id = "str.indexof";
  n.displayName = "Konum Bul";
  n.category = "Metin";
  n.icon = "\xEE\x9C\xA1";
  n.accentColor = "#22C55E";
  n.pure = true;
  n.library = "System";
  n.description = "Aranan parçanın ilk konumunu döndürür. Bulunamazsa -1.";
  n.inputs = {in("Text", t::String(), "\"\"", "Metin"), in("Part", t::String(), "\"\"", "Aranan")};
  n.outputs = {out("Result", t::Int(), "Konum")};
  n.emitExpr = [](EmitContext& c) {
    if (c.isCSharp()) {
      return c.value("Text") + ".IndexOf(" + c.value("Part") + ", StringComparison.Ordinal)";
    }
    return "static_cast<int>(" + c.value("Text") + ".find(" + c.value("Part") + "))";
  };
  n.emit = [](EmitContext& c) {
    if (const NodeType* nt = c.selfType()) {
      const std::string v = c.local("Result");
      c.bindOutput("Result", v);
      c.line((c.isCSharp() ? "int " : "const int ") + v + " = " + nt->emitExpr(c) + ";");
      c.markMapping("Result");
    }
  };
  n.cppHeader = "<string>";
  return n;
}

NodeType makeStringToInt() {
  NodeType n = unary(
      "str.toint", "Metinden Sayıya", "\xEE\xA7\x99", t::String(), t::Int(),
      [](EmitContext& c, const std::string& v) {
        return c.isCSharp() ? "int.Parse(" + v + ")" : "std::stoi(" + v + ", nullptr, 10)";
      },
      {}, "<string>");
  n.category = "Dönüşüm";
  n.description = "Metni tam sayıya çevirir. Metin sayı değilse program hata verir.";
  return n;
}

NodeType makeIntToString() {
  NodeType n = unary(
      "str.fromint", "Sayıdan Metne", "\xEE\xA3\x92", t::Int(), t::String(),
      [](EmitContext& c, const std::string& v) {
        return c.isCSharp() ? v + ".ToString()" : "std::to_string(" + v + ")";
      },
      std::vector<std::string>{}, "<string>");
  n.category = "Dönüşüm";
  return n;
}

NodeType makeStringSplitLines() {
  NodeType n = unary("str.lines", "Satırlara Böl", "\xEE\xA7\x95", t::String(),
                     TypeRef::array(t::String()), [](const std::string& v) {
                       return v + ".Split(new[] { '\\n', '\\r' }, StringSplitOptions.RemoveEmptyEntries)";
                     });
  n.category = "Metin";
  return n;
}

}  // namespace gpi::builtin
