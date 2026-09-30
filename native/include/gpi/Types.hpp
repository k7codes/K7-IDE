// Gözle Programlama IDE - C++ çekirdek: Tip sistemi
// Görsel tiplerin hem C# hem C++ tarafındaki karşılıklarını ve dönüşüm kurallarını yönetir.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gpi {

enum class TypeKind : int {
  Unknown = 0,
  Void,
  Bool,
  Int8,
  Int16,
  Int,
  Long,
  Float,
  Double,
  Char,
  String,
  Byte,
  Object,
  Any,        // C#: var/dynamic  |  C++: auto
  Struct,     // kullanıcı değer tipi
  Class,      // kullanıcı referans tipi
  Enum,
  Array,
  List,
  Dictionary,
  Pointer,    // C++: T*
  Reference,  // C++: T&
  Delegate,   // C#: Func<...>  |  C++: std::function<...>
  Variant,    // C#: object      |  C++: std::variant<...>
};

std::string_view toString(TypeKind k) noexcept;

// Görsel bir tip referansı. "List<Int32>" gibi bir şeyi temsil eder.
struct TypeRef {
  std::string name;                  // kanonik ad: "Int32", "String", "System.Math"
  TypeKind kind = TypeKind::Unknown;
  std::vector<TypeRef> args;         // genel argümanlar
  bool nullable = false;             // referans tipler için null olabilir
  bool isConst = false;              // C++ tarafında const

  TypeRef() = default;
  TypeRef(TypeKind k, std::string n = {});

  static TypeRef primitive(TypeKind k);
  static TypeRef of(std::string name, TypeKind k);
  static TypeRef list(TypeRef element);
  static TypeRef array(TypeRef element);
  static TypeRef dict(TypeRef key, TypeRef value);
  static TypeRef pointer(TypeRef pointee);

  bool sameAs(const TypeRef& other) const;
  bool operator==(const TypeRef& o) const { return sameAs(o); }
  bool operator!=(const TypeRef& o) const { return !sameAs(o); }

  // İnsan tarafından okunabilir: "Int32", "List<String>", "Widget?"
  std::string toString() const;
  // Makine tarafından okunabilir, normalize: "list<string>"
  std::string key() const;

  bool isNumeric() const;
  bool isPrimitive() const;
  bool isReferenceSemantics() const;  // C#'te null olabilir
};

// C# ve C++'teki somut yazımlar.
struct TypeNames {
  std::string csharp;
  std::string cpp;
  // C++'te `const std::string&` gibi geçici ifade için gerekli uygunluk
  std::string csharpLiteral;  // uç değer için C# literal örneği
  std::string cppLiteral;
};

enum class Conversion : int { None = 0, Implicit, Explicit, Boxing };

class TypeSystem {
 public:
  TypeSystem();

  const TypeNames& names(const TypeRef& t) const;
  std::string csharp(const TypeRef& t) const { return names(t).csharp; }
  std::string cpp(const TypeRef& t) const { return names(t).cpp; }

  // Bir değerin hedef tipe atanıp atanamayacağı. Parametre tipi döner.
  Conversion conversion(const TypeRef& from, const TypeRef& to) const;
  // Dönüşüm ifadesi üretir (gerekirse cast/parse ekler), aksi halde nullopt.
  std::optional<std::string> convertExpr(const std::string& expr, const TypeRef& from,
                                         const TypeRef& to, std::string_view lang) const;

  // Verilen tip, atanabilir tüm portlar için geçerli mi?
  bool assignable(const TypeRef& from, const TypeRef& to) const {
    return conversion(from, to) != Conversion::None;
  }

  // C# / C++ arasında tip köprüsü (interop) dönüşümü.
  std::string interopMarshal(const TypeRef& t, std::string_view direction) const;

  // Null literal üretimi
  std::string nullLiteral(const TypeRef& t, std::string_view lang) const;
  // "default" değer üretimi
  std::string defaultValue(const TypeRef& t, std::string_view lang) const;

 private:
  std::vector<TypeNames> table_;
  int indexOf(TypeKind k) const;
  std::string nameOf(TypeRef t) const;  // generic açılım dâhil
};

// Sık kullanılan tipler için kısayol sabitler
namespace t {
inline TypeRef Void() { return TypeRef::primitive(TypeKind::Void); }
inline TypeRef Bool() { return TypeRef::primitive(TypeKind::Bool); }
inline TypeRef Int() { return TypeRef::primitive(TypeKind::Int); }
inline TypeRef Long() { return TypeRef::primitive(TypeKind::Long); }
inline TypeRef Float() { return TypeRef::primitive(TypeKind::Float); }
inline TypeRef Double() { return TypeRef::primitive(TypeKind::Double); }
inline TypeRef Char() { return TypeRef::primitive(TypeKind::Char); }
inline TypeRef String() { return TypeRef::primitive(TypeKind::String); }
inline TypeRef Byte() { return TypeRef::primitive(TypeKind::Byte); }
inline TypeRef Object() { return TypeRef::primitive(TypeKind::Object); }
inline TypeRef Any() { return TypeRef::primitive(TypeKind::Any); }
}  // namespace t

}  // namespace gpi
