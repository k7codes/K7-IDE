// Shell: üretim + derleme + çalıştırma servisi
#include "shell/BuildService.hpp"

#include <filesystem>
#include <fstream>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include "gpi/Codegen.hpp"
#include "gpi/Graph.hpp"
#include "gpi/NodeCatalog.hpp"
#include "shell/App.hpp"
#include "shell/EditorState.hpp"
#include "shell/panels/CodePanel.hpp"

namespace fs = std::filesystem;

namespace shell {

BuildService::~BuildService() {
  cancel_ = true;
  if (thread_.joinable()) thread_.join();
}

void BuildService::start(EditorState& state, bool run) {
  if (thread_.joinable()) thread_.join();
  cancel_ = false;
  done_ = false;
  wasRunRequested_ = run;
  code_.clear();
  programOutput_.clear();
  diagnostics_.clear();
  messages_.clear();
  workerMessages_.clear();
  hasRun_ = false;
  phase_ = BuildPhase::Compiling;
  setPhaseText("Hazırlanıyor...");

  // İş parçacığı grafı kopyalamalı (UI ile yarışmasın)
  gpi::Graph snapshot = state.graph();
  gpi::ProjectOptions opts = state.options();
  thread_ = std::thread([this, snapshot, opts, run]() { worker(snapshot, opts, run); });
}

void BuildService::setPhaseText(const std::string& s) {
  std::lock_guard<std::mutex> lk(textMutex_);
  phaseText_ = s;
}

void BuildService::worker(gpi::Graph graph, gpi::ProjectOptions opts, bool run) {
  // --- 1. Katalog + üretim ---
  gpi::NodeCatalog catalog;
  catalog.loadBuiltins();
  gpi::CodeGenerator gen(catalog);

  setPhaseText("Kod üretiliyor...");
  genResult_ = gen.generate(graph, opts);
  for (const auto& m : genResult_.warnings) workerMessages_.push_back("Uyarı: " + m);

  if (!genResult_.ok) {
    for (const auto& e : genResult_.errors) workerMessages_.push_back("HATA: " + e);
    setPhaseText("Üretim başarısız");
    done_ = true;
    return;
  }

  // --- 2. Dosyaları yaz ---
  const fs::path root = fs::path("build") / "out-shell";
  std::error_code ec;
  fs::remove_all(root, ec);

  const bool cs = opts.lang == gpi::TargetLang::CSharp;
  const fs::path outDir = cs ? root : (root / "native");
  fs::create_directories(outDir, ec);

  gpi::ProjectGenerator proj;
  gpi::ProjectScaffold sc = proj.scaffold(opts);

  // Ana kaynak dosyasını yaz
  const std::string mainName = cs ? "Program.g.cs" : "main.cpp";
  {
    std::ofstream f(outDir / mainName, std::ios::binary);
    if (!f) {
      workerMessages_.push_back("HATA: kaynak yazılamadı");
      setPhaseText("Dosya yazılamadı");
      done_ = true;
      return;
    }
    f << genResult_.code;
  }
  for (const auto& gf : sc.files) {
    const fs::path p = root / fs::u8path(gf.relativePath);
    fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary);
    if (f) f << gf.content;
  }
  for (const auto& note : sc.notes) messages_.push_back("Proje: " + note);

  // --- 3. Derleme ---
  setPhaseText("Derleniyor...");
  gpi::ToolchainDiscovery disc;
  gpi::ToolchainReport tc = disc.probe();
  gpi::BuildPlanner bp;

  gpi::BuildPlan plan =
      cs ? bp.planCSharp(tc, root.string(), proj.projectFileName(opts), opts.lang)
         : bp.planCppDirect(tc, (outDir / "main.cpp").string(),
                            (outDir / (proj.assemblyBaseName(opts) + ".exe")).string());

  if (!plan.valid) {
    workerMessages_.push_back("HATA: derleme planı: " + plan.error);
    gpi::Diagnostic d;
    d.severity = gpi::Severity::Error;
    d.message = plan.error;
    diagnostics_.push_back(d);
    setPhaseText("Araç zinciri yok");
    done_ = true;
    return;
  }

  // Derlenecek ürünün yolu (başarıda Explorer'da göstermek için saklıyoruz).
  const std::string exeBase = proj.assemblyBaseName(opts);
  exePath_ = (cs ? (root / "bin" / "Release" / opts.framework / (exeBase + ".exe")).string()
                 : (outDir / (exeBase + ".exe")).string());

  gpi::BuildRunner runner;
  buildResult_ = runner.run(plan.request, nullptr);
  for (const auto& line : buildResult_.log) {
    // Konsol çıktısını da göster
    (void)line;
  }
  for (const auto& d : buildResult_.diagnostics) diagnostics_.push_back(d);

  if (!buildResult_.success) {
    workerMessages_.push_back("HATA: derleme başarısız (kod " + std::to_string(buildResult_.exitCode) + ")");
    setPhaseText("Derleme başarısız");
    done_ = true;
    return;
  }
  workerMessages_.push_back("Derleme başarılı (" + std::to_string(static_cast<int>(buildResult_.durationMs)) +
                      " ms)");

  // --- 4. Çalıştırma (isteğe bağlı) ---
  if (run) {
    setPhaseText("Çalıştırılıyor...");
    const fs::path exe = exePath_;
    if (!fs::exists(exe)) {
      workerMessages_.push_back("HATA: çalıştırılabilir bulunamadı: " + exe.string());
      setPhaseText("Çalıştırılabilir yok");
      done_ = true;
      return;
    }
    runResult_ = bp.runAndCaptureOutput(exe.string(), {}, root.string(), 60, nullptr);
    runOutput_ = runResult_.output;
    hasRun_ = true;
    if (!runResult_.success) {
      workerMessages_.push_back("HATA: program hata ile bitti (kod " + std::to_string(runResult_.exitCode) +
                          ")");
      setPhaseText("Program hatası");
      done_ = true;
      return;
    }
    workerMessages_.push_back("Program başarıyla çalıştı");
    setPhaseText("Tamamlandı");
  } else {
    setPhaseText("Derlendi");
  }
  done_ = true;
}

void BuildService::tick() {
  // Yalnızca iş parçacığı bittiğinde UI sonuçlarına dokunur.
  if (!done_.load()) return;
  if (thread_.joinable()) thread_.join();
  if (phase_ == BuildPhase::Idle) return;  // zaten işlendi

  // Başarısızlık üretim aşamasında da olabilir: genResult_.ok kontrolü,
  // aksi halde üretim hatası "başarılı" görünürdü.
  const bool genOk = genResult_.ok;
  const bool compiled = buildResult_.launched && buildResult_.success;
  if (!genOk || !compiled) {
    phase_ = BuildPhase::Failed;
  } else if (hasRun_ && !runResult_.success) {
    phase_ = BuildPhase::Failed;
  } else {
    phase_ = BuildPhase::Success;
  }
  // Sonuçları panellere taşı
  code_ = genResult_.code;
  programOutput_ = runOutput_;
  messages_ = workerMessages_;
  diagnostics_ = allDiagnostics();
  app_code_cache_valid_ = true;
}

void BuildService::process(App& app) {
  if (phase_ == BuildPhase::Idle) return;
  app.code().setCode(code_);
  if (app_code_cache_valid_) {
    // İş parçacığı birleştiğinden sonra okumak güvenli.
    const std::string text = phaseText();
    const bool wasSuccess = phase_ == BuildPhase::Success;
    if (wasSuccess) {
      app.setStatus(wasRunRequested_ ? "Program çalıştı - " + text : "Derleme tamam - " + text);
    } else {
      app.setStatus("HATA - " + text);
    }
    // Bir kez bildir
    app_code_cache_valid_ = false;
    phase_ = BuildPhase::Idle;

    // Başarılı derlemede ürünün klasörünü Explorer'da aç.
    if (wasSuccess && !exePath_.empty() && app.openBuildFolder()) {
      std::wstring sel = L"/select,\"";
      sel += fs::u8path(exePath_).wstring();
      sel += L"\"";
      ::ShellExecuteW(nullptr, L"open", L"explorer.exe", sel.c_str(), nullptr, SW_SHOWNORMAL);
    }
  }
}

std::vector<gpi::Diagnostic> BuildService::allDiagnostics() const {
  std::vector<gpi::Diagnostic> all;
  for (const auto& e : genResult_.errors) {
    gpi::Diagnostic d;
    d.severity = gpi::Severity::Error;
    d.message = e;
    all.push_back(d);
  }
  for (const auto& d : buildResult_.diagnostics) all.push_back(d);
  for (const auto& d : runDiagnostics_) all.push_back(d);
  return all;
}

}  // namespace shell
