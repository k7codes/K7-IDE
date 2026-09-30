// Shell: uygulama düzeni, menüler, panel orkestrasyonu
#include "shell/App.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

#include "imgui.h"
#include "imgui_internal.h"

#include "shell/BuildService.hpp"
#include "shell/FileDialog.hpp"
#include "shell/Panel.hpp"
#include "shell/Theme.hpp"
#include "shell/panels/CatalogPanel.hpp"
#include "shell/panels/CanvasPanel.hpp"
#include "shell/panels/CodePanel.hpp"
#include "shell/panels/DetailsPanel.hpp"
#include "shell/panels/OutputPanel.hpp"
#include "shell/panels/SolutionPanel.hpp"
#include "shell/panels/ToolchainPanel.hpp"

namespace shell {

App::App() {
  catalog_ = std::make_unique<CatalogPanel>();
  canvas_ = std::make_unique<CanvasPanel>();
  details_ = std::make_unique<DetailsPanel>();
  code_ = std::make_unique<CodePanel>();
  output_ = std::make_unique<OutputPanel>();
  toolchain_ = std::make_unique<ToolchainPanel>();
  build_ = std::make_unique<BuildService>();
  solution_ = std::make_unique<SolutionPanel>();
  analyzer_ = std::make_unique<LiveAnalyzer>();
}

App::~App() = default;

void App::startup() {
  // Uygulama BOŞ bir graf ile açılır. Örnek/demo içerik yoktur: kullanıcı
  // Katalog panelinden düğümleri kendisi ekler.
  state_.newProject("Program1");
  toolchain_->refresh();
}

void App::shutdown() {}

void App::loadSampleProgram() {
  state_.loadSample("OrnekProgram");
  canvas_->resetView();
  revealInOutput();
  setStatus("Örnek dize programı yüklendi");
  // Kullanıcı "Derle ve Çalıştır" seçtiyse tek tıkta sonucu göster.
  if (autoRun_) {
    runRequested() = true;
  } else {
    buildRequested() = true;
  }
}

void App::refreshAnalysis() {
  analyzer_->request(state_.graph(), state_.options(), state_.revision());
  analysis_ = analyzer_->snapshot();
}

void App::draw() {
  ++frameCount_;

  // Graf değiştiyse anlık denetim + kod önizlemesini tazele. Bu, F5'e
  // basılmadan da hata vurgularının ve Kod panelinin güncel olmasını sağlar.
  refreshAnalysis();

  // Önce sonucu topla, sonra panellere dağıt
  build_->tick();
  build_->process(*this);

  if (buildRequested()) {
    buildRequested() = false;
    build_->start(state_, /*run=*/false);
  }
  if (runRequested()) {
    runRequested() = false;
    build_->start(state_, /*run=*/true);
  }
  if (newRequested()) {
    newRequested() = false;
    state_.newProject("YeniProgram");
    setStatus("Yeni proje oluşturuldu");
  }
  if (openRequested()) {
    openRequested() = false;
    std::string err;
    // Diyalog iptal edilirse pendingPath boş kalır; boş yol denemeyelim.
    if (pendingPath().empty()) {
      setStatus("Açma iptal edildi");
    } else if (!state_.load(pendingPath(), &err)) {
      setStatus("Açılamadı: " + err);
    } else {
      setStatus("Açıldı: " + state_.graph().name);
    }
    pendingPath().clear();
  }
  if (saveRequested()) {
    saveRequested() = false;
    std::string err;
    if (pendingPath().empty()) {
      setStatus("Kaydetme iptal edildi");
    } else if (!state_.save(pendingPath(), &err)) {
      setStatus("Kaydedilemedi: " + err);
    } else {
      setStatus("Kaydedildi: " + pendingPath());
    }
    pendingPath().clear();
  }
  // Menü çubuğu ImGui'nin kendi ana menü penceresinde çizilir. Bu pencere
  // viewport çalışma alanını otomatik olarak aşağı kaydırır; böylece dock
  // alanı menünün ALTINDA başlar ve panel sekmeleri gizlenmez. Ayrıca tam
  // ekran bir "host" pencere gerekmez (o pencere çizim sırası yüzünden tüm
  // panelleri örtüyordu).
  if (ImGui::BeginMainMenuBar()) {
    drawMenuItems();
    ImGui::EndMainMenuBar();
  }

  // DockBuilderDockWindow() yalnızca daha önce oluşturulmuş pencereleri
  // düğüme bağlayabilir. 1. karede ##durumcubugu henüz yok, bu yüzden
  // yerleşim 2. kareden itibaren kurulur.
  if (resetLayoutRequested() || (!layoutBuilt_ && ImGui::GetFrameCount() >= 2)) {
    resetLayoutRequested() = false;
    buildDefaultLayout();
    layoutBuilt_ = true;
  }

  // Durum çubuğu viewport'un alt kenarına sabitlenir ve çalışma alanını
  // kendisi daraltır. Dock alanı DOCKSPACEDAN ÖNCE çizilmelidir ki altta
  // kalan alanı hesabı doğru olsun.
  drawStatusBar();

  // Ana yerleşim alanı. DockBuilder'ın oluşturduğu kök düğümle AYNI ID
  // kullanılmalıdır; aksi halde paneller yerleşime bağlanmaz.
  ImGui::DockSpaceOverViewport(dockspaceId(), nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

  if (showCatalog_) catalog_->draw(*this);
  if (showSolution_) solution_->draw(*this);
  if (showCanvas_) canvas_->draw(*this);
  if (showDetails_) details_->draw(*this);
  if (showCode_) code_->draw(*this);
  if (showOutput_) output_->draw(*this);
  if (showToolchain_) toolchain_->draw(*this);

  if (quitRequested()) ::PostQuitMessage(0);
}

void App::drawMenuItems() {
  if (ImGui::BeginMenu("Dosya")) {
    if (ImGui::MenuItem("Yeni Proje")) newRequested() = true;
    if (ImGui::MenuItem("Örnek Program")) loadSampleProgram();
    if (ImGui::MenuItem("Aç...")) {
      openRequested() = true;
      char buf[512] = {0};
      if (openFileDialog("Graf Dosyasını Aç", "*.json", buf, sizeof(buf))) {
        pendingPath() = buf;
      }
    }
    if (ImGui::MenuItem("Kaydet")) {
      saveRequested() = true;
      const std::string p = state_.filePath().empty() ? std::string("proje.json") : state_.filePath();
      char buf[512] = {0};
      std::strncpy(buf, p.c_str(), sizeof(buf) - 1);
      if (saveFileDialog("Projeyi Kaydet", "*.json", buf, sizeof(buf))) {
        pendingPath() = buf;
      }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Çıkış")) quitRequested() = true;
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Düzen")) {
    const bool u = state_.canUndo();
    const bool r = state_.canRedo();
    std::string undoLabel = "Geri Al";
    if (u) undoLabel += "  (" + state_.undoLabel() + ")";
    if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, u)) state_.undo();
    if (ImGui::MenuItem("Yinele", "Ctrl+Y", false, r)) state_.redo();
    ImGui::Separator();
    if (ImGui::MenuItem("Tümünü Sil")) {
      state_.beginTransaction("Tümünü sil");
      state_.graph() = gpi::Graph{};
      state_.commitTransaction();
      setStatus("Graf temizlendi");
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Derle")) {
    if (ImGui::MenuItem("Üret ve Derle", "F5")) buildRequested() = true;
    if (ImGui::MenuItem("Çalıştır", "F6")) runRequested() = true;
    ImGui::Separator();
    if (ImGui::MenuItem("Derle ve Çalıştır", nullptr, autoRun_)) autoRun_ = !autoRun_;
    ImGui::MenuItem("Derlemede klasörü aç", nullptr, &openBuildFolder_);
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Görünüm")) {
    ImGui::MenuItem("Çözüm Gezgini", nullptr, &showSolution_);
    ImGui::MenuItem("Katalog", nullptr, &showCatalog_);
    ImGui::MenuItem("Tuval", nullptr, &showCanvas_);
    ImGui::MenuItem("Ayrıntılar", nullptr, &showDetails_);
    ImGui::MenuItem("Kod", nullptr, &showCode_);
    ImGui::MenuItem("Çıktı", nullptr, &showOutput_);
    ImGui::MenuItem("Araç Zinciri", nullptr, &showToolchain_);
    ImGui::Separator();
    if (ImGui::MenuItem("Yerleşimi Sıfırla")) resetLayoutRequested() = true;
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Yardım")) {
    ImGui::MenuItem("Hakkında");
    ImGui::EndMenu();
  }
  // NOT: EndMenuBar() çağrılmaz; BeginMainMenuBar() bunu kendisi yapar.
}

void App::drawStatusBar() {
  const float barH = ImGui::GetFontSize() + 12.0f;
  const ImGuiWindowFlags f = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
  // BeginViewportSideBar pencereyi viewport'un alt kenarına yapıştırır ve
  // viewport çalışma alanını barH kadar kısar; böylece dock alanı çubuğun
  // altına girmez.
  ImGui::BeginViewportSideBar("##durumcubugu", ImGui::GetMainViewport(), ImGuiDir_Down, barH, f);
  ImGui::TextUnformatted(statusText_.c_str());
  const std::string& phase = build_->phaseText();
  if (!phase.empty()) {
    const std::string txt = "| " + phase;
    const ImVec2 sz = ImGui::CalcTextSize(txt.c_str());
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(0.0f, std::max(0.0f, w - sz.x));
    ImGui::TextUnformatted(txt.c_str());
  }
  // Begin() false dönse de End() çağrılmak ZORUNDA.
  ImGui::End();
}

bool App::runSelfTest(const std::string& reportPath) {
  std::ostringstream r;
  bool okAll = true;

  r << "=== Gözle Programlama IDE :: Arayüz Kendi Testi ===\n\n";

  // --- Graf durumu ---
  const gpi::Graph& g = state_.graph();
  r << "Graf: " << g.name << "\n";
  r << "  düğüm sayısı : " << g.nodes.size() << "\n";
  r << "  kablo sayısı : " << g.edges.size() << "\n";
  r << "  katalog tipi: " << state_.catalog().all().size() << "\n";
  r << "  kategori     : " << state_.categories().size() << "\n";
  r << "  dil          : " << (state_.language() == gpi::TargetLang::CSharp ? "C#" : "C++") << "\n\n";

  // --- Analiz ---
  const gpi::AnalysisResult an = gpi::analyze(g, state_.catalog());
  r << "Analiz: " << (an.ok ? "GEÇERLİ" : "HATALI") << "  ("
    << an.executionOrder.size() << " düğüm sıralandı)\n";
  for (const auto& e : an.errors) r << "  HATA: " << e << "\n";
  for (const auto& w : an.warnings) r << "  UYARI: " << w << "\n";
  if (!an.ok) okAll = false;
  r << "\n";

  // --- Undo/redo ---
  state_.beginTransaction("test");
  const std::string extra = state_.addNode("math.add", 10, 10);
  state_.commitTransaction();
  const std::size_t withExtra = state_.graph().nodes.size();
  state_.undo();
  const std::size_t afterUndo = state_.graph().nodes.size();
  state_.redo();
  const std::size_t afterRedo = state_.graph().nodes.size();
  r << "Geri al/yinele: ekle=" << withExtra << " geriAl=" << afterUndo << " yinele=" << afterRedo
    << (afterUndo == withExtra - 1 && afterRedo == withExtra ? "  [TAMAM]" : "  [HATA]") << "\n";
  if (!(afterUndo == withExtra - 1 && afterRedo == withExtra)) okAll = false;
  state_.undo();
  (void)extra;
  r << "\n";

  // --- Kaydet/aç ---
  const std::string tmp = "build/out-shell-selftest.json";
  std::string err;
  const bool saved = state_.save(tmp, &err);
  const std::size_t before = state_.graph().nodes.size();
  state_.newProject("Baska");
  const bool loaded = state_.load(tmp, &err);
  const std::size_t after = state_.graph().nodes.size();
  r << "Kaydet/aç: kaydedildi=" << (saved ? "evet" : "HAYIR") << " düğüm " << before << " -> "
    << after << (saved && loaded && before == after ? "  [TAMAM]" : "  [HATA]") << "\n";
  if (!(saved && loaded && before == after)) {
    okAll = false;
    r << "  hata: " << err << "\n";
  }
  r << "\n";

  // --- Üretim + derleme + çalıştırma (F6 yolu) ---
  for (const bool cs : {true, false}) {
    state_.setLanguage(cs ? gpi::TargetLang::CSharp : gpi::TargetLang::Cpp);
    build_->start(state_, /*run=*/true);
    // İş parçacığını bekle
    while (!build_->finished()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    build_->tick();

    r << (cs ? "--- C# ---" : "--- C++ ---") << "\n";
    r << "  üretim: " << (build_->code().empty() ? "BAŞARISIZ" : "başarılı") << " ("
      << build_->code().size() << " bayt)\n";
    r << "  derleme: " << (build_->buildOk() ? "başarılı" : "başarısız") << "\n";
    r << "  çalıştırma: " << (build_->runOk() ? "başarılı" : "başarısız") << "\n";
    r << "  program çıktısı:\n";
    std::istringstream out(build_->programOutput());
    std::string line;
    bool any = false;
    while (std::getline(out, line)) {
      r << "    | " << line << "\n";
      any = true;
    }
    if (!any) r << "    | (boş)\n";
    r << "  ayrıntı: çıkışKodu=" << build_->runResult().exitCode
      << " satır=" << build_->runResult().log.size() << " bayt=" << build_->runResult().output.size()
      << " [" << build_->runResult().captureDebug << "]\n";
    // Üretilen kodu da yan dosyaya yaz (hata ayıklama için)
    {
      const fs::path codePath = fs::path(reportPath).parent_path() /
                                (cs ? "selftest-generated.cs" : "selftest-generated.cpp");
      std::ofstream cf(codePath, std::ios::binary);
      if (cf) cf << build_->code();
    }
    if (!build_->code().empty() && build_->buildOk() && build_->runOk()) {
      r << "  [TAMAM]\n";
    } else {
      okAll = false;
      r << "  [HATA]\n";
      for (const auto& m : build_->messages()) r << "    ! " << m << "\n";
      for (const auto& d : build_->diagnostics()) {
        r << "    ! " << d.code << " " << d.message << "\n";
      }
    }
    r << "\n";
  }

  // --- Boru hattı sağlığı (echo testi) ---
  {
    gpi::BuildPlanner probe;
    gpi::BuildResult pipe = probe.runAndCaptureOutput("cmd.exe", {"/d", "/c", "echo BORU_CALISIYOR"},
                                                      ".", 10, nullptr);
    r << "Boru testi: baslatildi=" << (pipe.launched ? "evet" : "hayir") << " cikis=" << pipe.exitCode
      << " [" << pipe.captureDebug << "] cikti=\"" << (pipe.output.empty() ? "(bos)" : pipe.output)
      << "\"\n";
    if (pipe.output.find("BORU_CALISIYOR") == std::string::npos) okAll = false;
  }

  r << "=== SONUÇ: " << (okAll ? "TÜM KONTROLLER GEÇTİ" : "BAŞARISIZ KONTROLLER VAR") << " ===\n";

  std::error_code ec;
  fs::create_directories(fs::path(reportPath).parent_path(), ec);
  std::ofstream f(reportPath, std::ios::binary);
  if (!f) return false;
  f << r.str();
  return okAll;
}

ImGuiID App::dockspaceId() const { return ImGui::GetID("GpiDockspace"); }

void App::buildDefaultLayout() {
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  const ImVec2 size = vp->WorkSize;

  const ImGuiID rootId = dockspaceId();
  ImGui::DockBuilderRemoveNode(rootId);
  ImGui::DockBuilderAddNode(rootId, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(rootId, size);

  // ÖNEMLİ: DockBuilderSplitNode(id, yön, oran, &yeniDüğüm, &kalanDüğüm)
  // DÖNÜŞ DEĞERİ "yeni" düğümdür, 5. parametreye yazılan "kalan" düğümdür.
  // Daha önce bu ikisi ters kullanılıyordu ve her panel yanlış düğüme
  // yerleşiyordu.
  //
  // NOT: Durum çubuğu artık dock ağacının parçası DEĞİLDİ; BeginViewportSideBar
  // ile viewport'un altına ekleniyor ve çalışma alanını kendisi daraltıyor.

  // 1) Sol sütun: Çözüm Gezgini (üst) + Operatör Kataloğu (alt).
  //    Aynı düğüm yığılınca ImGui bunları SEKME olarak gösterir; böylece
  //    arama kutusu Operatörler sekmesinde, proje ağacı Çözüm sekmesinde
  //    kalır ve ikisi birbirinin yerini almaz.
  ImGuiID mid = rootId;
  const ImGuiID left = ImGui::DockBuilderSplitNode(mid, ImGuiDir_Left, 0.22f, nullptr, &mid);
  ImGuiID leftTop = left;
  const ImGuiID leftBottom = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.45f, nullptr, &leftTop);

  // 2) Sağ sütun: Ayrıntılar (üst) + Araç Zinciri (alt)
  ImGuiID body = mid;
  const ImGuiID right = ImGui::DockBuilderSplitNode(body, ImGuiDir_Right, 0.27f, nullptr, &body);
  ImGuiID rightTop = right;
  const ImGuiID rightBottom =
      ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.58f, nullptr, &rightTop);

  // 3) Alt yerleşim: Kod (sol) | Çıktı (sağ)
  ImGuiID main = body;
  const ImGuiID bottom = ImGui::DockBuilderSplitNode(main, ImGuiDir_Down, 0.42f, nullptr, &main);
  ImGuiID bottomLeft = bottom;
  const ImGuiID bottomRight =
      ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Right, 0.5f, nullptr, &bottomLeft);

  ImGui::DockBuilderDockWindow("Cozum Gezgini", leftTop);
  ImGui::DockBuilderDockWindow("Katalog", leftBottom);
  ImGui::DockBuilderDockWindow("Tuval", main);
  ImGui::DockBuilderDockWindow("Ayrıntılar", rightTop);
  ImGui::DockBuilderDockWindow("Araç Zinciri", rightBottom);
  ImGui::DockBuilderDockWindow("Kod", bottomLeft);
  ImGui::DockBuilderDockWindow("Çıktı", bottomRight);
  ImGui::DockBuilderFinish(rootId);
  layoutBuilt_ = true;
}


}  // namespace shell
