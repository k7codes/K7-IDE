#include "gpi/Json.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace gpi::json {

namespace {
const Value kNull;
}  // namespace

bool Value::asBool(bool def) const {
  if (kind_ == Kind::Bool) return bool_;
  if (kind_ == Kind::Number) return num_ != 0;
  return def;
}

double Value::asDouble(double def) const {
  if (kind_ == Kind::Number) return num_;
  if (kind_ == Kind::Bool) return bool_ ? 1 : 0;
  if (kind_ == Kind::String) {
    try {
      return std::stod(str_);
    } catch (...) {
      return def;
    }
  }
  return def;
}

long long Value::asInt(long long def) const {
  if (kind_ == Kind::Number) return static_cast<long long>(num_);
  if (kind_ == Kind::Bool) return bool_ ? 1 : 0;
  if (kind_ == Kind::String) {
    try {
      return std::stoll(str_);
    } catch (...) {
      return def;
    }
  }
  return def;
}

std::string Value::asString(std::string_view def) const {
  if (kind_ == Kind::String) return str_;
  if (kind_ == Kind::Bool) return bool_ ? "true" : "false";
  if (kind_ == Kind::Number) {
    if (num_ == std::floor(num_) && std::fabs(num_) < 1e15) {
      return std::to_string(static_cast<long long>(num_));
    }
    std::ostringstream os;
    os << num_;
    return os.str();
  }
  return std::string(def);
}

const Array& Value::items() const {
  static const Array empty;
  return kind_ == Kind::Array ? arr_ : empty;
}

const Object& Value::fields() const {
  static const Object empty;
  return kind_ == Kind::Object ? obj_ : empty;
}

std::size_t Value::size() const {
  if (kind_ == Kind::Array) return arr_.size();
  if (kind_ == Kind::Object) return obj_.size();
  if (kind_ == Kind::String) return str_.size();
  return 0;
}

void Value::push(Value v) {
  if (kind_ != Kind::Array) {
    kind_ = Kind::Array;
    arr_.clear();
  }
  arr_.push_back(std::move(v));
}

bool Value::has(std::string_view key) const {
  return kind_ == Kind::Object && obj_.find(std::string(key)) != obj_.end();
}

const Value& Value::operator[](std::string_view key) const {
  if (kind_ != Kind::Object) return kNull;
  auto it = obj_.find(std::string(key));
  return it == obj_.end() ? kNull : it->second;
}

Value& Value::operator[](std::string_view key) {
  if (kind_ != Kind::Object) {
    kind_ = Kind::Object;
    obj_.clear();
  }
  return obj_[std::string(key)];
}

const Value& Value::at(std::size_t i) const {
  if (kind_ != Kind::Array || i >= arr_.size()) return kNull;
  return arr_[i];
}

void Value::set(std::string_view key, Value v) { (*this)[key] = std::move(v); }

std::string escape(std::string_view s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out;
}

void Value::dumpTo(std::string& out, int indent, int level) const {
  const bool pretty = indent >= 0;
  const std::string nl = pretty ? "\n" : "";
  const std::string pad = pretty ? std::string(static_cast<std::size_t>(indent * (level + 1)), ' ') : "";
  const std::string padEnd = pretty ? std::string(static_cast<std::size_t>(indent * level), ' ') : "";

  switch (kind_) {
    case Kind::Null: out += "null"; break;
    case Kind::Bool: out += bool_ ? "true" : "false"; break;
    case Kind::Number: {
      if (num_ == std::floor(num_) && std::fabs(num_) < 1e15) {
        out += std::to_string(static_cast<long long>(num_));
      } else {
        char buf[40];
        std::snprintf(buf, sizeof buf, "%.17g", num_);
        out += buf;
      }
      break;
    }
    case Kind::String:
      out += '"';
      out += escape(str_);
      out += '"';
      break;
    case Kind::Array: {
      if (arr_.empty()) { out += "[]"; break; }
      out += '[';
      out += nl;
      for (std::size_t i = 0; i < arr_.size(); ++i) {
        out += pad;
        arr_[i].dumpTo(out, indent, level + 1);
        if (i + 1 < arr_.size()) out += ',';
        out += nl;
      }
      out += padEnd;
      out += ']';
      break;
    }
    case Kind::Object: {
      if (obj_.empty()) { out += "{}"; break; }
      out += '{';
      out += nl;
      std::size_t i = 0;
      for (const auto& [k, v] : obj_) {
        out += pad;
        out += '"';
        out += escape(k);
        out += "\":";
        if (pretty) out += ' ';
        v.dumpTo(out, indent, level + 1);
        if (++i < obj_.size()) out += ',';
        out += nl;
      }
      out += padEnd;
      out += '}';
      break;
    }
  }
}

std::string Value::dump(int indent) const {
  std::string out;
  dumpTo(out, indent, 0);
  return out;
}

namespace {

struct Parser {
  std::string_view s;
  std::size_t i = 0;
  std::string err;

  void skipWs() {
    while (i < s.size()) {
      char c = s[i];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        ++i;
      } else if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
        while (i < s.size() && s[i] != '\n') ++i;
      } else {
        break;
      }
    }
  }

  bool fail(const char* m) {
    if (err.empty()) err = std::string(m) + " (konum " + std::to_string(i) + ")";
    return false;
  }

  bool parseValue(Value& out);
  bool parseString(std::string& out);
  bool parseNumber(Value& out);
};

void appendUtf8(std::string& out, unsigned int cp) {
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

bool Parser::parseString(std::string& out) {
  if (i >= s.size() || s[i] != '"') return fail("beklenen '\"'");
  ++i;
  out.clear();
  while (i < s.size()) {
    char c = s[i++];
    if (c == '"') return true;
    if (c != '\\') {
      out += c;
      continue;
    }
    if (i >= s.size()) return fail("kaçış sonu yok");
    char e = s[i++];
    switch (e) {
      case '"': out += '"'; break;
      case '\\': out += '\\'; break;
      case '/': out += '/'; break;
      case 'b': out += '\b'; break;
      case 'f': out += '\f'; break;
      case 'n': out += '\n'; break;
      case 'r': out += '\r'; break;
      case 't': out += '\t'; break;
      case 'u': {
        if (i + 4 > s.size()) return fail("\\u kaçışı eksik");
        unsigned int cp = 0;
        for (int k = 0; k < 4; ++k) {
          char h = s[i++];
          cp <<= 4;
          if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
          else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
          else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
          else return fail("geçersiz onaltılık");
        }
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 <= s.size() && s[i] == '\\' && s[i + 1] == 'u') {
          i += 2;
          unsigned int lo = 0;
          for (int k = 0; k < 4; ++k) {
            char h = s[i++];
            lo <<= 4;
            if (h >= '0' && h <= '9') lo |= static_cast<unsigned>(h - '0');
            else if (h >= 'a' && h <= 'f') lo |= static_cast<unsigned>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') lo |= static_cast<unsigned>(h - 'A' + 10);
            else return fail("geçersiz onaltılık");
          }
          cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
        }
        appendUtf8(out, cp);
        break;
      }
      default: return fail("geçersiz kaçış");
    }
  }
  return fail("kapanmamış dize");
}

bool Parser::parseNumber(Value& out) {
  std::size_t start = i;
  if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
  while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '.' ||
                          s[i] == 'e' || s[i] == 'E' || s[i] == '-' || s[i] == '+')) {
    ++i;
  }
  if (start == i) return fail("geçersiz sayı");
  try {
    out = Value(std::stod(std::string(s.substr(start, i - start))));
  } catch (...) {
    return fail("sayı çözülemedi");
  }
  return true;
}

bool Parser::parseValue(Value& out) {
  skipWs();
  if (i >= s.size()) return fail("beklenen değer");
  char c = s[i];
  if (c == '{') {
    ++i;
    Object o;
    skipWs();
    if (i < s.size() && s[i] == '}') { ++i; out = Value(std::move(o)); return true; }
    while (true) {
      skipWs();
      std::string key;
      if (!parseString(key)) return false;
      skipWs();
      if (i >= s.size() || s[i] != ':') return fail("beklenen ':'");
      ++i;
      Value v;
      if (!parseValue(v)) return false;
      o[std::move(key)] = std::move(v);
      skipWs();
      if (i < s.size() && s[i] == ',') { ++i; continue; }
      if (i < s.size() && s[i] == '}') { ++i; break; }
      return fail("beklenen ',' veya '}'");
    }
    out = Value(std::move(o));
    return true;
  }
  if (c == '[') {
    ++i;
    Array a;
    skipWs();
    if (i < s.size() && s[i] == ']') { ++i; out = Value(std::move(a)); return true; }
    while (true) {
      Value v;
      if (!parseValue(v)) return false;
      a.push_back(std::move(v));
      skipWs();
      if (i < s.size() && s[i] == ',') { ++i; continue; }
      if (i < s.size() && s[i] == ']') { ++i; break; }
      return fail("beklenen ',' veya ']'");
    }
    out = Value(std::move(a));
    return true;
  }
  if (c == '"') {
    std::string str;
    if (!parseString(str)) return false;
    out = Value(std::move(str));
    return true;
  }
  if (s.compare(i, 4, "true") == 0) { i += 4; out = Value(true); return true; }
  if (s.compare(i, 5, "false") == 0) { i += 5; out = Value(false); return true; }
  if (s.compare(i, 4, "null") == 0) { i += 4; out = Value(); return true; }
  if (c == 'N' && s.compare(i, 3, "NaN") == 0) { i += 3; out = Value(0.0); return true; }
  return parseNumber(out);
}

}  // namespace

Value Value::parse(std::string_view text, std::string* error) {
  Parser p{text};
  Value v;
  if (!p.parseValue(v)) {
    if (error) *error = p.err;
    return Value();
  }
  if (error) error->clear();
  return v;
}

Value parseOrNull(std::string_view text) {
  std::string err;
  return Value::parse(text, &err);
}

}  // namespace gpi::json
