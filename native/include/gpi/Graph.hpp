// Gözle Programlama IDE - C++ çekirdek: Grafik Ara Temsili (IR)
// Programın gerçek kaynağı budur; kod metni bundan *türetilir*.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "gpi/Types.hpp"

namespace gpi {

struct NodeId {
  std::uint64_t value = 0;
  bool valid() const { return value != 0; }
  bool operator==(const NodeId& o) const { return value == o.value; }
  bool operator<(const NodeId& o) const { return value < o.value; }
};

struct PortKey {
  std::string node;  // node kimliği (string olarak taşınır, serileştirme kolaylığı)
  std::string port;
  bool operator==(const PortKey& o) const { return node == o.node && port == o.port; }
  bool operator<(const PortKey& o) const {
    return node != o.node ? node < o.node : port < o.port;
  }
};

enum class PortDirection : int { Input = 0, Output = 1 };
enum class PortKind : int { Value = 0, Exec = 1 };  // Exec = akış kontrol portu

struct PortDef {
  std::string name;             // iç ad
  std::string displayName;      // arayüzde görünen ad (Türkçe olabilir)
  PortDirection direction = PortDirection::Input;
  PortKind kind = PortKind::Value;
  TypeRef type;
  std::string defaultLiteral;   // bağlanmamışsa kullanılacak değer metni
  std::string description;
  bool advanced = false;        // detay panelinde gizli
  // true ise bağlanmadığında hata üretir. Bir literal varsayılanı olan
  // portlar (ör. for döngüsünün Başlangıcı) required olmaz; koşul gibi
  // portlar required=true olur.
  bool required = false;
  int order = 0;
};

struct NodeInstance {
  std::string id;               // "n7"
  std::string typeId;           // katalogdaki düğüm tipi
  double x = 0, y = 0;
  // Bağlanmamış giriş portlarının değerleri (kod içinde literal olarak yazılır)
  std::map<std::string, std::string> literals;
  // Düğüm tipine özel özellikler (ör. "Karşılaştırma: GreaterThan", "Metin: 10")
  std::map<std::string, std::string> properties;
  std::string comment;
  bool collapsed = false;
  bool bypassed = false;        // araya girmeden geçir (pasif düğüm)
};

struct Edge {
  PortKey from;                 // çıkış (output)
  PortKey to;                   // giriş (input)
  bool operator==(const Edge& o) const { return from == o.from && to == o.to; }
  bool operator<(const Edge& o) const {
    return !(from == o.from) ? from < o.from : to < o.to;
  }
};

struct SubGraph {
  std::string id;
  std::string name;
  std::string description;
  // Bu alt graf bir düğüm tipi olarak dışa açılabilir (Fonksiyon/Metot düğümü)
  std::vector<PortDef> inputs;
  std::vector<PortDef> outputs;
  bool isEvent = false;
};

struct Graph {
  std::vector<NodeInstance> nodes;
  std::vector<Edge> edges;
  std::map<std::string, SubGraph> subgraphs;
  std::vector<std::string> entryNodes;   // akışın başladığı düğümler
  std::map<std::string, std::string> variables;  // "ad" -> "tip anahtarı"
  std::string name = "AnaProgram";

  const NodeInstance* findNode(const std::string& id) const;
  NodeInstance* findNode(const std::string& id);
  // Bir giriş portuna bağlı kenarı bulur
  const Edge* edgeInto(const PortKey& input) const;
  std::vector<const Edge*> edgesFrom(const PortKey& output) const;
  void removeNode(const std::string& id);
  void removeEdge(const Edge& e);
  bool validate(std::vector<std::string>& errors) const;
};

// Analiz sonuçları

// Bir sorunun ciddiyet derecesi. Toolchain.hpp'teki Severity ile aynı sırayı
// taşır ama Graph.hpp, Toolchain.hpp'i içe aktarmadan bağımsız kalsın diye
// ayrı bir türdür.
enum class ProblemLevel : int { Note = 0, Warning = 1, Error = 2 };

// Bir hatanın/uyarının graf üzerindeki karşılığı. Arayüz bu sayede
// hatalı düğümü, portu veya kabloyu o üretim turunu beklemeden vurgulayabilir.
struct NodeDiagnostic {
  ProblemLevel level = ProblemLevel::Error;
  std::string nodeId;    // vurgulanacak düğüm (yoksa boş)
  std::string portName;  // vurgulanacak port (yoksa boş)
  std::string fromNode;  // tip uyuşmazlığı gibi kablo hatalarında kaynak düğüm
  std::string fromPort;  // ... ve kaynak port
  std::string message;   // kullanıcıya gösterilecek açıklama

  [[nodiscard]] bool isError() const { return level == ProblemLevel::Error; }
  [[nodiscard]] bool isWarning() const { return level == ProblemLevel::Warning; }
};

struct AnalysisResult {
  bool ok = false;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
  std::vector<std::string> executionOrder;  // topo-sıralı düğüm kimlikleri
  std::vector<PortKey> usedLiterals;         // bağlanmamış, literal gerektiren portlar
  // errors/warnings ile birebir eşleşen, düğüm/port konumlu sürüm.
  std::vector<NodeDiagnostic> diagnostics;

  // Bir düğümün en kötü durumu (yoksa Note).
  [[nodiscard]] ProblemLevel levelOfNode(const std::string& nodeId) const;
  // Bir portun en kötü durumu.
  [[nodiscard]] ProblemLevel levelOfPort(const std::string& nodeId,
                                         const std::string& portName) const;
  // nodeId için ilgili tüm sorun metinleri (düğüm ipucu balonunda gösterilir).
  [[nodiscard]] std::vector<std::string> messagesForNode(const std::string& nodeId) const;
  bool anyError() const;
};

AnalysisResult analyze(const Graph& g, const class NodeCatalog& catalog);

}  // namespace gpi
