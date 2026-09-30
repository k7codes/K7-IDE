// Gözle Programlama IDE - C++ çekirdek: Kod üretim bağlamı
#pragma once

#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "gpi/Graph.hpp"
#include "gpi/Types.hpp"

namespace gpi {

class NodeCatalog;
struct NodeType;

enum class Lang { CSharp, Cpp, CppHeader };

std::string_view langId(Lang l) noexcept;

// Saf düğümlerin ifadeye gömülmesi için üretim sırasında paylaşılan durum.
struct ResolveState {
  const Graph* graph = nullptr;
  const NodeCatalog* catalog = nullptr;
  const TypeSystem* types = nullptr;
  Lang lang = Lang::CSharp;
  bool inlinePure = true;
  bool withComments = true;
  int maxDepth = 24;
  int depth = 0;
  // çıktı portu -> (materyalize edilmiş yerel değişken adı)
  std::map<PortKey, std::string> materialized;
  // çıktı portu -> önceden hesaplanmış ifade
  std::map<PortKey, std::string> memo;
  // satır numarası kaydı (kod görüntüleyicide graf <-> kod gezinmesi)
  std::vector<std::pair<PortKey, int>> lineMap;

  // --- graf <-> kod satırı eşlemesi ---
  // Üretim sırasında düğümlerin önüne "//#gpi:<id>" işaretçisi yazılır. Sarmalama
  // (başlık, using'ler, sınıf, metot) bittikten SONRA tek geçişte bu işaretçiler
  // bulundukları satır numarasına çevrilir ve koddan silinir. Böylece satır
  // numaraları daima nihai dosyadaki gerçek satırları gösterir.
  std::string statementAnchor;                              // üretilen deyimin sahibi
  std::vector<std::pair<std::string, std::string>> inlineAliases;  // (saf düğüm, sahibi)
};

// Bir düğümün kodunu yazarken kullanabileceği bağlam.
class EmitContext {
 public:
  EmitContext(const Graph& graph, const NodeInstance& node, const TypeSystem& ts, Lang lang,
              const NodeCatalog& catalog, std::string* out, ResolveState* state);

  [[nodiscard]] std::string_view language() const { return langId(lang_); }
  [[nodiscard]] Lang lang() const { return lang_; }
  // C# mi? (C++ kodu üretirken emitçiler bununla ayrım yapar)
  [[nodiscard]] bool isCSharp() const { return lang_ == Lang::CSharp; }
  [[nodiscard]] const TypeSystem& types() const { return ts_; }
  [[nodiscard]] const NodeInstance& node() const { return node_; }
  [[nodiscard]] const Graph& graph() const { return graph_; }
  [[nodiscard]] const NodeCatalog& catalog() const { return catalog_; }
  [[nodiscard]] ResolveState& state() const { return *state_; }
  [[nodiscard]] std::string* buffer() const { return out_; }

  // Giriş portunun değerini ifade olarak döndürür.
  // Bağlıysa kaynağın ifadesi, bağlı değilse literal metni (dönüşüm uygulanmış).
  [[nodiscard]] std::string value(std::string_view portName) const;

  // Bir çıktı portunun sonucunu ifade olarak döndürür.
  // Saf düğümler ifade olarak gömülür, diğerleri materyalize edilmiş değişkenden okunur.
  [[nodiscard]] std::string outRef(std::string_view portName) const;

  // Giriş portunun tipi.
  [[nodiscard]] TypeRef portType(std::string_view portName) const;
  // Giriş portunun *kaynağından gelen* gerçek tip (dönüşüm öncesi), yoksa port tipi.
  [[nodiscard]] TypeRef valueType(std::string_view portName) const;

  // Ham literal metni (kullanıcının yazdığı), dönüşümsüz.
  [[nodiscard]] std::string rawLiteral(std::string_view portName) const;
  [[nodiscard]] std::string literalOr(std::string_view portName, std::string_view fallback) const;

  [[nodiscard]] std::string prop(std::string_view key, std::string_view fallback = {}) const;
  [[nodiscard]] bool boolProp(std::string_view key, bool fallback = false) const;
  [[nodiscard]] int intProp(std::string_view key, int fallback = 0) const;
  [[nodiscard]] double doubleProp(std::string_view key, double fallback = 0) const;

  // Satır yazma (girinti uygulanır)
  void line(std::string_view text);
  void blank();
  void comment(std::string_view text);
  [[nodiscard]] std::string indented(std::string_view text) const;
  void setIndent(int level);
  [[nodiscard]] int indent() const { return indent_; }
  [[nodiscard]] std::string local(std::string_view suffix) const;

  // Benzersiz yerel değişken adı üretir
  [[nodiscard]] std::string newTemp(std::string_view hint);
  [[nodiscard]] std::string uniqueName(std::string_view hint);

  // Bu düğümün çıktılarını yerel değişkenlere bağlar ve kaydeder
  void bindOutput(std::string_view portName, std::string varName);
  [[nodiscard]] std::string outputVar(std::string_view portName) const;

  // Tip adları
  [[nodiscard]] std::string typeName(const TypeRef& t) const;
  // C++ standart kütüphane tipi (std::string gibi)
  [[nodiscard]] std::string nativeTypeName(const TypeRef& t) const;
  // Değer ifadesini hedef diliğin uygun biçime çevirir (C++'ta string literal vb.)
  [[nodiscard]] std::string asNative(const TypeRef& t, std::string_view expr) const;

  // Akış (exec) kablolarını gezinme yardımcıları
  [[nodiscard]] std::vector<std::string> nextFlowNodes(std::string_view execPort) const;

  // İç içe üretimde üst kapsamdaki değişkenler
  void pushScope(std::map<std::string, std::string> vars);
  void popScope();
  [[nodiscard]] std::string lookupScoped(std::string_view name) const;

  // Üretilen kodun satır sayacı (harita için)
  void markMapping(std::string_view portName);

  // Üretilen düğümün kendi tip tanımı (katalogdan). Emit geri çağrıları
  // port listesine ve saf/akış özelliklerine bakmak için kullanır.
  [[nodiscard]] const NodeType* selfType() const;

 private:
  const Graph& graph_;
  const NodeInstance& node_;
  const TypeSystem& ts_;
  Lang lang_;
  const NodeCatalog& catalog_;
  std::string* out_;
  ResolveState* state_;
  int indent_ = 0;
  int tempCounter_ = 0;
  std::map<PortKey, std::string> outVars_;
  std::map<std::string, std::string> ownVars_;
  std::set<std::string> usedNames_;
  std::vector<std::map<std::string, std::string>> scopeStack_;

  std::string resolveOutput(const PortKey& key) const;
};

using EmitStatementFn = std::function<void(EmitContext&)>;
using EmitExpressionFn = std::function<std::string(EmitContext&)>;

}  // namespace gpi
