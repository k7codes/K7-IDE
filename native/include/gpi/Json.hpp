// Gözle Programlama IDE - C++ çekirdek: Bağımlılıksız JSON
#pragma once

#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace gpi::json {

class Value;

using Object = std::map<std::string, Value>;
using Array = std::vector<Value>;

enum class Kind { Null, Bool, Number, String, Array, Object };

class Value {
 public:
  Value() : kind_(Kind::Null) {}
  Value(std::nullptr_t) : kind_(Kind::Null) {}
  Value(bool b) : kind_(Kind::Bool), bool_(b) {}
  Value(int v) : kind_(Kind::Number), num_(static_cast<double>(v)) {}
  Value(long long v) : kind_(Kind::Number), num_(static_cast<double>(v)) {}
  Value(double v) : kind_(Kind::Number), num_(v) {}
  Value(const char* s) : kind_(Kind::String), str_(s ? s : "") {}
  Value(std::string s) : kind_(Kind::String), str_(std::move(s)) {}
  Value(std::string_view s) : kind_(Kind::String), str_(s) {}
  Value(Array a) : kind_(Kind::Array), arr_(std::move(a)) {}
  Value(Object o) : kind_(Kind::Object), obj_(std::move(o)) {}

  static Value array() { return Value(Array{}); }
  static Value object() { return Value(Object{}); }
  static Value array(std::initializer_list<Value> items) { return Value(Array(items)); }

  [[nodiscard]] Kind kind() const { return kind_; }
  [[nodiscard]] bool isNull() const { return kind_ == Kind::Null; }
  [[nodiscard]] bool isObject() const { return kind_ == Kind::Object; }
  [[nodiscard]] bool isArray() const { return kind_ == Kind::Array; }
  [[nodiscard]] bool isString() const { return kind_ == Kind::String; }
  [[nodiscard]] bool isNumber() const { return kind_ == Kind::Number; }
  [[nodiscard]] bool isBool() const { return kind_ == Kind::Bool; }

  [[nodiscard]] bool asBool(bool def = false) const;
  [[nodiscard]] double asDouble(double def = 0) const;
  [[nodiscard]] long long asInt(long long def = 0) const;
  [[nodiscard]] std::string asString(std::string_view def = {}) const;
  [[nodiscard]] const Array& items() const;
  [[nodiscard]] const Object& fields() const;

  // Dizi işlemleri
  [[nodiscard]] std::size_t size() const;
  void push(Value v);
  // Nesne işlemleri
  [[nodiscard]] bool has(std::string_view key) const;
  const Value& operator[](std::string_view key) const;
  Value& operator[](std::string_view key);
  const Value& at(std::size_t i) const;
  void set(std::string_view key, Value v);

  [[nodiscard]] std::string dump(int indent = -1) const;

  // Ayrıştırma
  static Value parse(std::string_view text, std::string* error = nullptr);

 private:
  Kind kind_;
  bool bool_ = false;
  double num_ = 0;
  std::string str_;
  Array arr_;
  Object obj_;

  void dumpTo(std::string& out, int indent, int level) const;
};

// Yardımcılar
std::string escape(std::string_view s);
Value parseOrNull(std::string_view text);

}  // namespace gpi::json
