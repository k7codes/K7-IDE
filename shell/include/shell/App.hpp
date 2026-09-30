// Shell: uygulama düzeni ve yaşam döngüsü
#pragma once

#include <memory>
#include <string>
#include <vector>

// ImGuiID (dock kök düğümü kimliği) bu başlıkta kullanılıyor.
#include "imgui.h"

#include "shell/EditorState.hpp"
#include "shell/LiveAnalyzer.hpp"
#include "shell/Platform.hpp"

namespace shell {

class CatalogPanel;
class CanvasPanel;
class DetailsPanel;
class CodePanel;
class OutputPanel;
class ToolchainPanel;
class SolutionPanel;
class BuildService;

class App {
 public:
  App();
  ~App();

  void startup();
  void shutdown();
  void draw();

  EditorState& state() { return state_; }
  const EditorState& state() const { return state_; }

  // Paneller arasında paylaşılan durum
  bool& showCatalog() { return showCatalog_; }
  bool& showCanvas() { return showCanvas_; }
  bool& showDetails() { return showDetails_; }
  bool& showCode() { return showCode_; }
  bool& showOutput() { return showOutput_; }
  bool& showToolchain() { return showToolchain_; }
  bool& showSolution() { return showSolution_; }

  // --- anlık denetim ---
  // Her karede bir kez tazelenir; paneller aynı kare içinde aynı sonucu görür
  // (böylece kare başına birden çok kilit alınmaz).
  const LiveAnalyzer::Snapshot& analysis() const { return analysis_; }
  // Denetim sonucu grafın güncel sürümünden eski mi? (iş sürüyor)
  bool analysisPending() const { return analysis_.staleFor(state_.revision()); }

  // Tuvalde bir düğümü seçip odaklar (Kod panelinden tıklandığında çağrılır).
  void focusNode(const std::string& id) { revealNode_ = id; }
  // Odak isteğini bir kez okuyup boşaltır (tüketici deseninin basit hali).
  std::string takeFocusRequest() {
    std::string id;
    id.swap(revealNode_);
    return id;
  }
  // Çözüm gezgini panelinden gelen "bu dosyayı göster" isteği.
  void revealInCode() { showCode_ = true; }
  void revealInOutput() { showOutput_ = true; }
  void revealInCatalog() { showCatalog_ = true; }

  // "Örnek Program": gerçek operatörlerden dize demosu kurar ve çalıştırır.
  void loadSampleProgram();

  // Derleme/çalıştırma durumu
  bool& buildRequested() { return buildRequested_; }
  bool& runRequested() { return runRequested_; }
  bool& autoRun() { return autoRun_; }
  // Derleme başarılı olunca ürünün klasörünü Explorer'da aç (menüden değiştirilir).
  bool& openBuildFolder() { return openBuildFolder_; }
  const std::string& statusText() const { return statusText_; }
  void setStatus(const std::string& s) { statusText_ = s; }

  // Kayıt/çıkış istekleri
  bool& saveRequested() { return saveRequested_; }
  bool& quitRequested() { return quitRequested_; }
  bool& newRequested() { return newRequested_; }
  bool& openRequested() { return openRequested_; }
  bool& resetLayoutRequested() { return resetLayoutRequested_; }

  // Dosya yolu önerileri
  std::string& pendingPath() { return pendingPath_; }
  const std::string& pendingPath() const { return pendingPath_; }

  // Paneller
  CanvasPanel& canvas() { return *canvas_; }
  DetailsPanel& details() { return *details_; }
  CodePanel& code() { return *code_; }
  OutputPanel& output() { return *output_; }
  CatalogPanel& catalog() { return *catalog_; }
  SolutionPanel& solution() { return *solution_; }
  ToolchainPanel& toolchain() { return *toolchain_; }
  BuildService& build() { return *build_; }
  LiveAnalyzer& analyzer() { return *analyzer_; }

  // Arayüzü açmadan tüm ardışık düzeni sınar, raporu dosyaya yazar.
  // true dönerse başarılıdır. (test/otomasyon amaçlı)
  bool runSelfTest(const std::string& reportPath);

  // Çizilen kare sayısı (render döngüsü çalışıyor mu diye)
  int frameCount() const { return frameCount_; }
  void countFrame() { ++frameCount_; }

  // Dock kök düğümünün kimliği. Hem DockBuilder hem DockSpaceOverViewport
  // aynı ID'yi kullanmalıdır; 0 kullanmak yerleşimi panellerden koparır.
  ImGuiID dockspaceId() const;

 private:
  void drawMenuItems();
  void drawStatusBar();
  void buildDefaultLayout();
  // Grafı arka plan denetimine gönderir ve sonucu bir kez alır.
  void refreshAnalysis();

  EditorState state_;
  std::unique_ptr<CatalogPanel> catalog_;
  std::unique_ptr<SolutionPanel> solution_;
  std::unique_ptr<CanvasPanel> canvas_;
  std::unique_ptr<DetailsPanel> details_;
  std::unique_ptr<CodePanel> code_;
  std::unique_ptr<OutputPanel> output_;
  std::unique_ptr<ToolchainPanel> toolchain_;
  std::unique_ptr<BuildService> build_;
  std::unique_ptr<LiveAnalyzer> analyzer_;

  // Bu kare için geçerli anlık denetim sonucu (paneller bunu okur).
  LiveAnalyzer::Snapshot analysis_;
  // Tuvalin odaklanması gereken düğüm (Kod paneli -> graf gezinmesi).
  std::string revealNode_;

  bool showCatalog_ = true;
  bool showCanvas_ = true;
  bool showDetails_ = true;
  bool showCode_ = true;
  bool showOutput_ = true;
  bool showToolchain_ = true;
  bool showSolution_ = true;

  bool buildRequested_ = false;
  bool runRequested_ = false;
  bool autoRun_ = true;
  bool openBuildFolder_ = true;
  std::string statusText_ = "Hazır";

  bool saveRequested_ = false;
  bool quitRequested_ = false;
  bool newRequested_ = false;
  bool openRequested_ = false;
  bool resetLayoutRequested_ = false;
  std::string pendingPath_;

  ImGuiID catalogDock_ = 0;
  ImGuiID detailsDock_ = 0;
  ImGuiID codeDock_ = 0;
  ImGuiID outputDock_ = 0;
  bool layoutBuilt_ = false;
  int frameCount_ = 0;
};

}  // namespace shell
