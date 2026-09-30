// Shell: üretim + derleme + çalıştırma servisi (arka planda çalışır)
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gpi/Codegen.hpp"
#include "gpi/Toolchain.hpp"

namespace shell {

class App;
class EditorState;

enum class BuildPhase { Idle, Generating, Compiling, Running, Success, Failed };

class BuildService {
 public:
  ~BuildService();

  // Derlemeyi başlatır. run=true ise derleme bitince çalıştırır.
  void start(EditorState& state, bool run);
  // Her karede çağrılır: bitti mi kontrol eder, sonucu panellere taşır.
  void tick();
  void process(App& app);

  bool busy() const { return phase_ == BuildPhase::Generating || phase_ == BuildPhase::Compiling ||
                            phase_ == BuildPhase::Running; }
  BuildPhase phase() const { return phase_; }
  // phaseText_ iş parçacığı tarafından da yazıldığı için kilitli okunur.
  std::string phaseText() const {
    std::lock_guard<std::mutex> lk(textMutex_);
    return phaseText_;
  }
  // İş parçacığı bitti mi?
  bool finished() const { return done_.load(); }
  bool buildOk() const { return buildResult_.launched && buildResult_.success; }
  bool runOk() const { return hasRun_ && runResult_.success; }

  // Sonuçlar
  const std::string& code() const { return code_; }
  const std::string& programOutput() const { return programOutput_; }
  const std::vector<gpi::Diagnostic>& diagnostics() const { return diagnostics_; }
  const std::vector<std::string>& messages() const { return messages_; }
  const gpi::GenResult& genResult() const { return genResult_; }
  const gpi::BuildResult& runResult() const { return runResult_; }
  // Başarılı derlemenin ürününün tam yolu (Explorer'da göstermek için).
  const std::string& exePath() const { return exePath_; }

 private:
  void worker(gpi::Graph snapshot, gpi::ProjectOptions opts, bool run);
  std::vector<gpi::Diagnostic> allDiagnostics() const;
  // İş parçacığı durum metnini güvenle yazar.
  void setPhaseText(const std::string& s);

  std::thread thread_;
  std::atomic<bool> done_{true};
  std::atomic<bool> cancel_{false};
  mutable std::mutex textMutex_;

  BuildPhase phase_ = BuildPhase::Idle;
  std::string phaseText_;

  // Çalışan iş parçacığının doldurduğu sonuçlar
  gpi::GenResult genResult_;
  gpi::BuildResult buildResult_;
  gpi::BuildResult runResult_;
  std::string exePath_;
  std::vector<gpi::Diagnostic> runDiagnostics_;
  std::vector<std::string> workerMessages_;
  std::string runOutput_;
  bool hasRun_ = false;
  bool wasRunRequested_ = false;
  bool app_code_cache_valid_ = false;

  // Panellerin okuduğu sonuçlar
  std::string code_;
  std::string programOutput_;
  std::vector<gpi::Diagnostic> diagnostics_;
  std::vector<std::string> messages_;
};

}  // namespace shell
