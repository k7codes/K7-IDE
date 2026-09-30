// Shell: arka planda çalışan anlık denetim + kod üretim servisi.
//
// Amaç: kullanıcı bir düğüm eklediğinde, bir kablo bağladığında ya da bir
// değeri değiştirdiğinde TUANIN BEKLEMEDEN hata vurgularının ve Kod panelinin
// güncellenmesi. Derleme (F5) yine ayrı ve ayrıntılıdır; burada yalnızca
// üretim öncesi denetim yapılır, bu yüzden mikrosaniyeler sürer ve arayüz
// kilitlenmez.
//
// Akış:
//   App::draw() -> request(...)  : revision değiştiyse graf kopyasını kuyruğa al
//   worker thread               : analyze() + generate() çalıştırır
//   App::draw() -> snapshot()   : UI iş parçacığında sonucu kopyalar
#pragma once

#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gpi/Codegen.hpp"
#include "gpi/Graph.hpp"
#include "gpi/NodeCatalog.hpp"

namespace shell {

class LiveAnalyzer {
 public:
  // Tamamlanmış son çalışmanın anlık görüntüsü.
  struct Snapshot {
    std::uint64_t revision = 0;   // hangi graf sürümü üretildi
    bool ready = false;           // en az bir çalışma bitti mi
    bool analyzing = false;       // şu anda arka planda çalışıyor mu
    bool ok = false;              // kural ihlali var mı
    std::string code;             // üretilen kod (önizleme)
    std::vector<gpi::NodeDiagnostic> diagnostics;
    std::map<std::string, int> nodeLines;  // düğüm -> 1 tabanlı satır no
    int errorCount = 0;
    int warningCount = 0;

    [[nodiscard]] bool staleFor(std::uint64_t rev) const { return revision != rev; }
  };

  LiveAnalyzer();
  ~LiveAnalyzer();

  LiveAnalyzer(const LiveAnalyzer&) = delete;
  LiveAnalyzer& operator=(const LiveAnalyzer&) = delete;

  // Her karede çağrılması ucuzdur: revision değişmediyse hiçbir iş yapmaz.
  void request(const gpi::Graph& g, const gpi::ProjectOptions& opts, std::uint64_t revision);

  // UI iş parçacığında güvenle okunabilir (kopyalar döner).
  [[nodiscard]] Snapshot snapshot() const;

  // Proje değişti/yeni açıldı: bekleyen işi at, sonucu sıfırla.
  void reset();

 private:
  void worker();

  mutable std::mutex mtx_;
  std::condition_variable cv_;
  std::thread thread_;

  // istek tarafı
  gpi::Graph pending_;
  gpi::ProjectOptions pendingOpts_;
  std::uint64_t pendingRev_ = 0;
  bool hasPending_ = false;
  bool stop_ = false;
  bool running_ = false;

  // sonuç tarafı
  Snapshot result_;
  gpi::NodeCatalog catalog_;
};

}  // namespace shell
