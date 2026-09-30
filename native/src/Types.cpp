#include "gpi/Types.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_map>

namespace gpi {

std::string_view toString(TypeKind k) noexcept {
  switch (k) {
    case TypeKind::Unknown: return "Bilinmiyor";
    case TypeKind::Void: return "Boşluk";
    case TypeKind::Bool: return "Mantıksal";
    case TypeKind::Int8: return "Bayt";
    case TypeKind::Int16: return "Kısa";
    case TypeKind::Int: return "Tam Sayı";
    case TypeKind::Long: return "Uzun";
    case TypeKind::Float: return "Ondalık";
    case TypeKind::Double: return "Çift Ondalık";
    case TypeKind::Char: return "Karakter";
    case TypeKind::String: return "Metin";
    case TypeKind::Byte: return "İmzasız Bayt";
    case TypeKind::Object: return "Nesne";
    case TypeKind::Any: return "Otomatik";
    case TypeKind::Struct: return "Yapı";
    case TypeKind::Class: return "Sınıf";
    case TypeKind::Enum: return "Sıralama";
    case TypeKind::Array: return "Dizi";
    case TypeKind::List: return "Liste";
    case TypeKind::Dictionary: return "Sözlük";
    case TypeKind::Pointer: return "İşaretçi";
    case TypeKind::Reference: return "Referans";
    case TypeKind::Delegate: return "Temsilci";
    case TypeKind::Variant: return "Çok Biçimli";
  }
  return "Bilinmiyor";
}

TypeRef::TypeRef(TypeKind k, std::string n) : name(std::move(n)), kind(k) {
  if (name.empty()) {
    switch (k) {
      case TypeKind::Bool: name = "Boolean"; break;
      case TypeKind::Int8: name = "SByte"; break;
      case TypeKind::Int16: name = "Int16"; break;
      case TypeKind::Int: name = "Int32"; break;
      case TypeKind::Long: name = "Int64"; break;
      case TypeKind::Float: name = "Single"; break;
      case TypeKind::Double: name = "Double"; break;
      case TypeKind::Char: name = "Char"; break;
      case TypeKind::String: name = "String"; break;
      case TypeKind::Byte: name = "Byte"; break;
      case TypeKind::Object: name = "Object"; break;
      case TypeKind::Any: name = "Any"; break;
      case TypeKind::Void: name = "Void"; break;
      case TypeKind::Enum: name = "Enum"; break;
      case TypeKind::Delegate: name = "Delegate"; break;
      case TypeKind::Variant: name = "Variant"; break;
      default: name = "Object"; break;
    }
  }
}

TypeRef TypeRef::primitive(TypeKind k) { return TypeRef(k); }

TypeRef TypeRef::of(std::string n, TypeKind k) { return TypeRef(k, std::move(n)); }

TypeRef TypeRef::list(TypeRef element) {
  TypeRef t(TypeKind::List, "List");
  t.args.push_back(std::move(element));
  return t;
}

TypeRef TypeRef::array(TypeRef element) {
  TypeRef t(TypeKind::Array, "Array");
  t.args.push_back(std::move(element));
  return t;
}

TypeRef TypeRef::dict(TypeRef key, TypeRef value) {
  TypeRef t(TypeKind::Dictionary, "Dictionary");
  t.args.push_back(std::move(key));
  t.args.push_back(std::move(value));
  return t;
}

TypeRef TypeRef::pointer(TypeRef pointee) {
  TypeRef t(TypeKind::Pointer, "Pointer");
  t.args.push_back(std::move(pointee));
  return t;
}

bool TypeRef::sameAs(const TypeRef& o) const {
  if (kind != o.kind) return false;
  if (name != o.name) return false;
  if (args.size() != o.args.size()) return false;
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (!args[i].sameAs(o.args[i])) return false;
  }
  return true;
}

std::string TypeRef::toString() const {
  std::string out = name;
  if (!args.empty()) {
    out += '<';
    for (std::size_t i = 0; i < args.size(); ++i) {
      if (i) out += ", ";
      out += args[i].toString();
    }
    out += '>';
  }
  if (kind == TypeKind::Pointer && args.size() == 1) out = args[0].toString() + "*";
  if (nullable) out += '?';
  return out;
}

std::string TypeRef::key() const {
  std::string out = name;
  if (!args.empty()) {
    out += '<';
    for (std::size_t i = 0; i < args.size(); ++i) {
      if (i) out += ',';
      out += args[i].key();
    }
    out += '>';
  }
  return out;
}

bool TypeRef::isNumeric() const {
  switch (kind) {
    case TypeKind::Int8:
    case TypeKind::Int16:
    case TypeKind::Int:
    case TypeKind::Long:
    case TypeKind::Float:
    case TypeKind::Double:
    case TypeKind::Byte:
    case TypeKind::Char:
      return true;
    default:
      return false;
  }
}

bool TypeRef::isPrimitive() const {
  return isNumeric() || kind == TypeKind::Bool || kind == TypeKind::String;
}

bool TypeRef::isReferenceSemantics() const {
  switch (kind) {
    case TypeKind::String:
    case TypeKind::Class:
    case TypeKind::Array:
    case TypeKind::List:
    case TypeKind::Dictionary:
    case TypeKind::Delegate:
      return true;
    default:
      return false;
  }
}

// --- TypeSystem ---

TypeSystem::TypeSystem() {
  table_.resize(static_cast<std::size_t>(TypeKind::Variant) + 1);
  auto set = [this](TypeKind k, std::string cs, std::string cpp, std::string csl, std::string cppl) {
    table_[static_cast<std::size_t>(k)] = TypeNames{std::move(cs), std::move(cpp), std::move(csl),
                                                    std::move(cppl)};
  };

  set(TypeKind::Void, "void", "void", "", "");
  set(TypeKind::Bool, "bool", "bool", "false", "false");
  set(TypeKind::Int8, "sbyte", "std::int8_t", "(sbyte)0", "std::int8_t{0}");
  set(TypeKind::Int16, "short", "std::int16_t", "(short)0", "std::int16_t{0}");
  set(TypeKind::Int, "int", "std::int32_t", "0", "std::int32_t{0}");
  set(TypeKind::Long, "long", "std::int64_t", "0L", "std::int64_t{0}");
  set(TypeKind::Float, "float", "float", "0f", "0.0f");
  set(TypeKind::Double, "double", "double", "0.0", "0.0");
  set(TypeKind::Char, "char", "char", "'\0'", "'\\0'");
  set(TypeKind::String, "string", "std::string", "\"\"", "std::string{}");
  set(TypeKind::Byte, "byte", "std::uint8_t", "(byte)0", "std::uint8_t{0}");
  set(TypeKind::Object, "object", "std::any", "null", "std::any{}");
  set(TypeKind::Any, "var", "auto", "default", "{}");
  set(TypeKind::Struct, "struct", "struct", "default", "{}");
  set(TypeKind::Class, "object", "std::shared_ptr<void>", "null", "nullptr");
  set(TypeKind::Enum, "int", "int", "0", "0");
  set(TypeKind::Array, "T[]", "std::vector<T>", "Array.Empty<T>()", "std::vector<T>{}");
  set(TypeKind::List, "List<T>", "std::vector<T>", "new List<T>()", "std::vector<T>{}");
  set(TypeKind::Dictionary, "Dictionary<K,V>", "std::map<K,V>", "new Dictionary<K,V>()",
      "std::map<K,V>{}");
  set(TypeKind::Pointer, "IntPtr", "T*", "IntPtr.Zero", "nullptr");
  set(TypeKind::Reference, "ref T", "T&", "ref T", "T&");
  set(TypeKind::Delegate, "Func<T1,T2,R>", "std::function<R(T1,T2)>", "null", "{}");
  set(TypeKind::Variant, "object", "std::variant<T...>", "null", "T{}");
}

int TypeSystem::indexOf(TypeKind k) const {
  return static_cast<int>(k);
}

std::string TypeSystem::nameOf(TypeRef t) const {
  if (t.kind == TypeKind::Class || t.kind == TypeKind::Struct || t.kind == TypeKind::Enum) {
    return t.name;
  }
  return csharp(t);
}

const TypeNames& TypeSystem::names(const TypeRef& t) const {
  static const TypeNames fallback{"object", "auto", "null", "{}"};
  if (t.kind == TypeKind::Class || t.kind == TypeKind::Struct || t.kind == TypeKind::Enum) {
    if (!t.name.empty() && t.name != "Object") {
      static thread_local std::unordered_map<std::string, TypeNames> custom;
      std::string k(t.kind == TypeKind::Enum ? "enum:" : t.kind == TypeKind::Struct ? "struct:" : "");
      k += t.name;
      auto it = custom.find(k);
      if (it == custom.end()) {
        TypeNames n;
        n.csharp = t.name;
        n.cpp = t.name;
        n.csharpLiteral = t.kind == TypeKind::Enum ? "(" + t.name + ")0" : "default";
        n.cppLiteral = t.kind == TypeKind::Enum ? "static_cast<" + t.name + ">(0)" : "{}";
        it = custom.emplace(k, std::move(n)).first;
      }
      return it->second;
    }
  }
  const std::size_t i = static_cast<std::size_t>(t.kind);
  if (i >= table_.size()) return fallback;
  const TypeNames& base = table_[i];
  if (t.args.empty()) return base;

  // Genel argümanlar açılır. Sonuç öbek öğeye önbelleklenir; çağıran taraf
  // `const&` ile güvenle tutabilsin diye yerel bir değer döndürülür.
  static thread_local std::unordered_map<std::string, TypeNames> cache;
  std::string ck(1, static_cast<char>('0' + static_cast<int>(t.kind)));
  for (const auto& a : t.args) ck += '|' + a.key();
  auto found = cache.find(ck);
  if (found != cache.end()) return found->second;

  const TypeRef& A = t.args[0];
  const std::string csA = csharp(A);
  const std::string cppA = cpp(A);
  TypeNames out;

  switch (t.kind) {
    case TypeKind::List:
      out.csharp = "List<" + csA + ">";
      out.csharpLiteral = "new List<" + csA + ">()";
      out.cpp = "std::vector<" + (A.kind == TypeKind::Any ? std::string("auto") : cppA) + ">";
      out.cppLiteral = out.cpp + "{}";
      break;
    case TypeKind::Array:
      out.csharp = csA + "[]";
      out.csharpLiteral = "Array.Empty<" + csA + ">()";
      out.cpp = "std::vector<" + (A.kind == TypeKind::Any ? std::string("auto") : cppA) + ">";
      out.cppLiteral = out.cpp + "{}";
      break;
    case TypeKind::Dictionary:
      out.csharp = "Dictionary<" + csA + ", " + csharp(t.args[1]) + ">";
      out.csharpLiteral = "new Dictionary<" + csA + ", " + csharp(t.args[1]) + ">()";
      out.cpp = "std::map<" + (A.kind == TypeKind::Any ? std::string("auto") : cppA) + ", " +
                cpp(t.args[1]) + ">";
      out.cppLiteral = out.cpp + "{}";
      break;
    case TypeKind::Pointer:
      out.csharp = "IntPtr";
      out.csharpLiteral = "IntPtr.Zero";
      out.cpp = cppA + "*";
      out.cppLiteral = "nullptr";
      break;
    case TypeKind::Reference:
      out.csharp = "ref " + csA;
      out.csharpLiteral = "ref " + csA;
      out.cpp = cppA + "&";
      out.cppLiteral = "{}";
      break;
    case TypeKind::Delegate: {
      std::string params, params2;
      for (std::size_t k = 0; k < t.args.size(); ++k) {
        if (k) {
          params += ", ";
          params2 += ", ";
        }
        params += csharp(t.args[k]);
        params2 += cpp(t.args[k]);
      }
      out.csharp = "System.Func<" + params + ">";
      out.csharpLiteral = "null";
      out.cpp = "std::function<" + (t.args.empty() ? std::string("void") : cpp(t.args.back())) +
                "(" + params2 + ")>";
      out.cppLiteral = "{}";
      break;
    }
    case TypeKind::Variant:
      out.csharp = "object";
      out.csharpLiteral = "null";
      out.cpp = "std::variant<" + csA + ">";
      out.cppLiteral = "std::variant<" + csA + ">{}";
      break;
    default:
      out = base;
      break;
  }
  return cache.emplace(ck, std::move(out)).first->second;
}

Conversion TypeSystem::conversion(const TypeRef& from, const TypeRef& to) const {
  if (from.sameAs(to)) return Conversion::Implicit;
  if (to.kind == TypeKind::Any || from.kind == TypeKind::Any) return Conversion::Implicit;
  if (to.kind == TypeKind::Object) return Conversion::Boxing;
  if (from.kind == TypeKind::Object) return Conversion::None;
  if (to.kind == TypeKind::Void) return Conversion::None;

  // Null atanabilirlik
  if (to.nullable && from.kind == TypeKind::Class) return Conversion::Implicit;

  // Sayısal taramalar
  if (from.isNumeric() && to.isNumeric()) {
    auto rank = [](TypeKind k) {
      switch (k) {
        case TypeKind::Byte:
        case TypeKind::Int8: return 1;
        case TypeKind::Int16:
        case TypeKind::Char: return 2;
        case TypeKind::Int: return 3;
        case TypeKind::Long: return 4;
        case TypeKind::Float: return 5;
        case TypeKind::Double: return 6;
        default: return 0;
      }
    };
    int a = rank(from.kind), b = rank(to.kind);
    if (b >= a) return Conversion::Implicit;
    if (b > 0) return Conversion::Explicit;
  }

  if (from.kind == TypeKind::Enum && to.isNumeric()) return Conversion::Explicit;
  if (from.isNumeric() && to.kind == TypeKind::Enum) return Conversion::Explicit;
  if (from.kind == TypeKind::Bool && to.isNumeric()) return Conversion::Explicit;
  if (from.isNumeric() && to.kind == TypeKind::Bool) return Conversion::Explicit;

  // Metin dönüşümleri
  //  - Metinden sayısal/bool/char'a: açık (parse)
  //  - Sayısaldan metne: AÇIK DEĞİL. Bir "Toplama" çıktısını doğrudan
  //    "Yaz" düğümüne bağlamak neredeyse her zaman hatadır; kullanıcı
  //    "Sayıyı Metne Çevir" düğümünü bilinçli olarak kullanmalıdır.
  if (from.kind == TypeKind::String) {
    if (to.isNumeric() || to.kind == TypeKind::Bool || to.kind == TypeKind::Char) {
      return Conversion::Explicit;
    }
  }
  if (to.kind == TypeKind::String &&
      (from.kind == TypeKind::Bool || from.kind == TypeKind::Char)) {
    return Conversion::Explicit;
  }

  // Koleksiyon uyumu (birebir aynı olmalı)
  if (from.kind == to.kind && from.kind != TypeKind::Unknown && !from.args.empty()) {
    for (std::size_t i = 0; i < from.args.size() && i < to.args.size(); ++i) {
      if (!from.args[i].sameAs(to.args[i])) {
        // Koleksiyon kovası uyumu (covariance) kabul etmiyoruz ama Any kabul ediyoruz
        if (from.args[i].kind == TypeKind::Any || to.args[i].kind == TypeKind::Any) continue;
        return Conversion::None;
      }
    }
    return Conversion::Implicit;
  }

  // Kullanıcı sınıfları
  if (from.kind == to.kind && (to.kind == TypeKind::Class || to.kind == TypeKind::Struct)) {
    return from.name == to.name ? Conversion::Implicit : Conversion::None;
  }

  return Conversion::None;
}

std::optional<std::string> TypeSystem::convertExpr(const std::string& expr, const TypeRef& from,
                                                   const TypeRef& to, std::string_view lang) const {
  Conversion c = conversion(from, to);
  if (c == Conversion::None) return std::nullopt;
  if (c == Conversion::Implicit) return expr;

  if (lang == "csharp") {
    if (to.kind == TypeKind::String) {
      if (from.isNumeric()) return "Convert.ToString(" + expr + ")";
      if (from.kind == TypeKind::Bool) return "Convert.ToString(" + expr + ")";
      if (from.kind == TypeKind::Char) return "((int)" + expr + ").ToString()";
      return expr;
    }
    if (to.isNumeric()) {
      if (from.kind == TypeKind::String) {
        switch (to.kind) {
          case TypeKind::Int: return "int.Parse(" + expr + ")";
          case TypeKind::Long: return "long.Parse(" + expr + ")";
          case TypeKind::Double: return "double.Parse(" + expr + ")";
          case TypeKind::Float: return "float.Parse(" + expr + ")";
          case TypeKind::Int16: return "short.Parse(" + expr + ")";
          case TypeKind::Byte: return "byte.Parse(" + expr + ")";
          case TypeKind::Bool: return "bool.Parse(" + expr + ")";
          case TypeKind::Char: return "char.Parse(" + expr + ")";
          default: return expr;
        }
      }
      if (from.kind == TypeKind::Char) return "(long)" + expr;
      if (from.kind == TypeKind::Bool) return expr + " ? 1 : 0";
      if (from.kind == TypeKind::Object) return "Convert.ToInt32(" + expr + ")";
      return "(" + csharp(to) + ")" + expr;
    }
    if (to.kind == TypeKind::Bool && from.isNumeric()) return expr + " != 0";
    if (to.kind == TypeKind::Enum) return "(" + csharp(to) + ")" + expr;
    return "(" + csharp(to) + ")" + expr;
  }

  // C++
  if (to.kind == TypeKind::String) {
    if (from.kind == TypeKind::Bool) return "std::to_string(" + expr + " ? 1 : 0)";
    if (from.kind == TypeKind::Char) return "std::string(1, " + expr + ")";
    if (from.isNumeric()) return "std::to_string(" + expr + ")";
    return expr;
  }
  if (to.isNumeric()) {
    if (from.kind == TypeKind::String) return "std::stod(" + expr + ")";
    if (from.kind == TypeKind::Bool) return "(" + cpp(to) + ")(" + expr + " ? 1 : 0)";
    if (from.kind == TypeKind::Enum) return "static_cast<" + cpp(to) + ">(" + expr + ")";
    if (to.kind == TypeKind::Bool) return "((" + expr + ") != 0)";
    return "static_cast<" + cpp(to) + ">(" + expr + ")";
  }
  if (to.kind == TypeKind::Bool && from.isNumeric()) return "((" + expr + ") != 0)";
  if (to.kind == TypeKind::Enum) return "static_cast<" + cpp(to) + ">(" + expr + ")";
  return "static_cast<" + cpp(to) + ">(" + expr + ")";
}

std::string TypeSystem::interopMarshal(const TypeRef& t, std::string_view direction) const {
  const bool toNative = direction == "toNative";
  switch (t.kind) {
    case TypeKind::Bool: return "int";          // BOOL yerine int (kesin boyut)
    case TypeKind::Int8: return "signed char";
    case TypeKind::Byte: return "unsigned char";
    case TypeKind::Int16: return "short";
    case TypeKind::Char: return "unsigned short";
    case TypeKind::Int: return "int";
    case TypeKind::Long: return "long long";
    case TypeKind::Float: return "float";
    case TypeKind::Double: return "double";
    case TypeKind::String:
      return toNative ? "gpi::native::Str" : "string";
    case TypeKind::Void: return "void";
    case TypeKind::Pointer: return "void*";
    default: return toNative ? cpp(t) : csharp(t);
  }
}

std::string TypeSystem::nullLiteral(const TypeRef& t, std::string_view lang) const {
  if (lang == "csharp") {
    if (t.isNumeric() || t.kind == TypeKind::Bool || t.kind == TypeKind::Char) return "0";
    return "null";
  }
  if (t.kind == TypeKind::Bool) return "false";
  if (t.kind == TypeKind::Char) return "'\\0'";
  if (t.isNumeric()) return "0";
  if (t.kind == TypeKind::String) return "std::string{}";
  return "nullptr";
}

std::string TypeSystem::defaultValue(const TypeRef& t, std::string_view lang) const {
  const TypeNames& n = names(t);
  if (lang == "csharp") {
    if (t.kind == TypeKind::String) return "\"\"";
    if (t.kind == TypeKind::Bool) return "false";
    if (t.isNumeric()) return "0";
    return "default";
  }
  if (t.kind == TypeKind::Bool) return "false";
  if (t.kind == TypeKind::String) return "std::string{}";
  if (t.isNumeric()) return "0";
  return "nullptr";
}

}  // namespace gpi
