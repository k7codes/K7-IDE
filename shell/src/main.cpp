// Shell uygulama girişi: pencere aç, ImGui kur, olay döngüsü
#include "shell/Platform.hpp"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"  // pencere teşhisi için (ImGuiWindow/ImGuiContext)

#include "shell/App.hpp"
#include "shell/CrashHandler.hpp"
#include "shell/Theme.hpp"
#include "shell/panels/CanvasPanel.hpp"

#include <shellapi.h>

#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace shell;

namespace shell {
extern bool g_gpiTraceEnabled;
}

namespace {
// Başlangıç akışını dosyaya izler: uygulama nerede takılıyor?
// NOT: _wfopen_s + "ccs=UTF-8" yalnızca 3 baytlık BOM yazıp metni düşürüyor.
// Bu yüzden geniş yol std::filesystem üzerinden std::ofstream ile açılıyor.
// /trace bayrağı verilmedikçe hiçbir şey yazılmaz.
void traceStep(const std::string& what) {
  if (!shell::g_gpiTraceEnabled) return;
  std::ofstream os(std::filesystem::path(L"build/startup.log"),
                   std::ios::app | std::ios::binary);
  if (os) {
    os << what << "\n";
    os.flush();
  }
}
}  // namespace

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow) {
  shell::installCrashHandler();

  // /selftest [raporYolu] : arayüzü açmadan tüm ardışık düzeni sına.
  // /probe   [raporYolu] : N kare çizip ImGui çizim istatistiklerini yaz.
  // /trace              : başlangıç akışını build/startup.log'a yaz.
  std::wstring report = L"build/shell-selftest.txt";
  bool selftest = false;
  bool probe = false;
  int probeFrames = 120;
  {
    // NOT: nCmdShow, çağıranın STARTUPINFO değeridir; Start-Process / gizli
    // başlatmalarda 0 gelir. Argümanları nCmdShow'a bağlı koymak /selftest ve
    // /probe komutlarının sessizce yok sayılmasına yol açıyordu.
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc && argv; ++i) {
      if (::CompareStringOrdinal(argv[i], -1, L"/selftest", -1, TRUE) == CSTR_EQUAL) {
        selftest = true;
        if (i + 1 < argc) report = argv[i + 1];
      } else if (::CompareStringOrdinal(argv[i], -1, L"/probe", -1, TRUE) == CSTR_EQUAL) {
        probe = true;
        if (i + 1 < argc) report = argv[i + 1];
        if (i + 2 < argc) probeFrames = _wtoi(argv[i + 2]);
      } else if (::CompareStringOrdinal(argv[i], -1, L"/trace", -1, TRUE) == CSTR_EQUAL) {
        shell::g_gpiTraceEnabled = true;
      }
    }
    if (argv) ::LocalFree(argv);
  }
  traceStep("wWinMain giris");

  Platform platform;
  traceStep("Platform::create cagriliyor");
  if (!platform.create(L"Gözle Programlama IDE", 1680, 1000)) {
    MessageBoxW(nullptr, L"Pencere oluşturulamadı.", L"Gözle Programlama IDE", MB_ICONERROR);
    return 1;
  }
  traceStep("Platform::create tamam");

  if (selftest) {
    // Pencerede hiç görüntü üretmeden doğrula
    App app;
    app.startup();
    const std::string out(report.begin(), report.end());
    const bool ok = app.runSelfTest(out);
    app.shutdown();
    platform.destroy();
    return ok ? 0 : 1;
  }

  platform.show();
  traceStep("platform.show cagrildi");

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  traceStep("ImGui context olustu");
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  // YAZI TİPİ SIRASI ÖNEMLİ: ImGui'de ilk eklenen font varsayılan olur.
  // Daha önce ilk font Segoe MDL2 (yalnızca ikon, PUA 0xF000-0xF8FF) idi ve
  // varsayılan olarak o seçildiği için tüm arayüz metinleri glifsiz kaldı:
  // ekran boş, sadece panellerin arka planı çiziliyordu.
  // Türkçe için Latin Extended-A (0x0100-0x017F) aralığı da eklenmeli.
  static ImWchar uiRanges[] = {0x0020, 0x00FF, 0x0100, 0x017F, 0x2000, 0x206F,
                               0x2190, 0x21FF, 0x25A0, 0x25FF, 0x2212, 0x00B7, 0};
  static ImWchar codeRanges[] = {0x0020, 0x00FF, 0x0100, 0x017F, 0x2000, 0x206F, 0};

  ImFontConfig uiCfg;
  uiCfg.PixelSnapH = true;
  traceStep("font: segoeui yukleniyor");
  ImFont* uiFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 15.0f, &uiCfg, uiRanges);

  ImFontConfig codeCfg;
  codeCfg.PixelSnapH = true;
  traceStep("font: consola yukleniyor");
  ImFont* codeFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 14.0f, &codeCfg, codeRanges);

  // Açıkça belirt: konsol/kod fontu arayüz varsayılanı DEĞİLDİR.
  io.FontDefault = uiFont;
  shell::theme::setUiFont(uiFont);
  shell::theme::setCodeFont(codeFont);
  traceStep("fontlar eklendi ve varsayilan atandi");

  theme::applyDark();
  traceStep("tema uygulandi");

  ImGui_ImplWin32_Init(platform.window());
  traceStep("ImGui_ImplWin32_Init");
  ImGui_ImplDX11_Init(platform.device(), platform.context());
  traceStep("ImGui_ImplDX11_Init");

  App app;
  app.startup();
  traceStep("app.startup bitti");

  long long probeVerts = 0;
  long long probeCmds = 0;

  bool done = false;
  int traced = 0;
  while (!done) {
    if (traced < 3) traceStep("kare " + std::to_string(traced) + ": NewFrame");
    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    if (traced < 3) traceStep("kare " + std::to_string(traced) + ": app.draw");
    if (probe && probeFrames == 1) {
      // Son karede ImGui metin günlüğünü aç: hangi panelin ne yazdığını gör.
      ImGui::LogToBuffer(-1);
    }

    app.draw();
    if (traced < 3) traceStep("kare " + std::to_string(traced) + ": draw bitti");

    ImGui::Render();
    if (traced < 3) traceStep("kare " + std::to_string(traced) + ": Render bitti");
    ++traced;

    // Çizim istatistiklerini topla: gerçekten GPU'ya geometri gidiyor mu?
    if (const ImDrawData* dd = ImGui::GetDrawData()) {
      probeVerts += dd->TotalVtxCount;
      probeCmds += dd->CmdListsCount;
    }

    if (probe && --probeFrames <= 0) {
      // istenen kare sayısına ulaşıldı
      done = true;
    }

    ID3D11RenderTargetView* rtv = platform.target();
    const float clear[4] = {0.043f, 0.051f, 0.067f, 1.0f};
    platform.context()->OMSetRenderTargets(1, &rtv, nullptr);
    platform.context()->ClearRenderTargetView(rtv, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    platform.context()->OMSetRenderTargets(0, nullptr, nullptr);
    platform.present();
    if (traced <= 3) traceStep("kare " + std::to_string(traced) + ": present bitti");

    if (platform.consumeResized()) platform.resize();

    MSG msg;
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      ::TranslateMessage(&msg);
      ::DispatchMessageW(&msg);
      if (msg.message == WM_QUIT) done = true;
    }
  }

  if (probe) {
    // NOT: Bu blok iki kez bozuktu. 1) std::FILE* f = nullptr; ve _wfopen_s(...)
    // aynı satırdaydı, // yorumu çağrıyı yutuyordu. 2) _wfopen_s + "ccs=UTF-8"
    // yalnızca 3 baytlık BOM yazıp metni düşürüyordu. std::ofstream kullanıyoruz.
    std::ostringstream ss;
    ss << "=== Arayuz Cizim Sinamasi ===\n";
    ss << "pencere: " << ImGui::GetIO().DisplaySize.x << "x" << ImGui::GetIO().DisplaySize.y << "\n";
    ss << "dugum: " << app.state().graph().nodes.size() << "   kablo: " << app.state().graph().edges.size()
       << "   katalog tipi: " << app.state().catalog().all().size() << "\n";
    ss << "secili dugum: " << app.state().selected() << "\n";
    ss << "pan: " << app.state().panX() << ", " << app.state().panY()
       << "   zoom: " << app.state().zoom() << "   tuval merkezi: " << app.canvas().viewCenter().x
       << ", " << app.canvas().viewCenter().y << "\n";
    ss << "toplam kare: " << app.frameCount() << "   toplam kose: " << probeVerts
       << "   toplam cizim komutu: " << probeCmds << "\n";

    // Neden siyah? Her ImGui penceresinin durumunu dök
    ss << "\n--- ImGui pencereleri (toplam " << ImGui::GetCurrentContext()->Windows.size() << ") ---\n";
    for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
      ss << "  '" << w->Name << "' pos=(" << w->Pos.x << "," << w->Pos.y << ") size=(" << w->Size.x
         << "," << w->Size.y << ") gorunur=" << (w->Hidden ? 0 : 1)
         << " atlama=" << (w->SkipItems ? 1 : 0) << " kose=" << (w->DrawList ? w->DrawList->VtxBuffer.Size : 0)
         << " kirpma=[" << w->ClipRect.Min.x << "," << w->ClipRect.Min.y
         << "]-[" << w->ClipRect.Max.x << "," << w->ClipRect.Max.y << "]"
         << " alankisi=" << w->ContentSize.x << "x" << w->ContentSize.y << "\n";
    }
    ss << "\n--- font / cizim dokusu ---\n";
    ImFontAtlas* fa = ImGui::GetIO().Fonts;
    ss << "font sayisi: " << fa->Fonts.Size << "\n";
    for (int i = 0; i < fa->Fonts.Size; ++i) {
      ImFont* fo = fa->Fonts[i];
      const ImFontBaked* bk = fo->LastBaked;
      ss << "  font[" << i << "] LegacySize=" << fo->LegacySize
         << " baked=" << (bk ? 1 : 0)
         << (bk ? (" size=" + std::to_string(bk->Size) + " glif=" + std::to_string(bk->Glyphs.Size)) : "")
         << "\n";
    }
    if (const ImDrawData* dd = ImGui::GetDrawData()) {
      ss << "drawdata: vtx=" << dd->TotalVtxCount << " idx=" << dd->TotalIdxCount
         << " liste=" << dd->CmdListsCount << " displayPos=" << dd->DisplayPos.x << ","
         << dd->DisplayPos.y << " displaySize=" << dd->DisplaySize.x << "x" << dd->DisplaySize.y
         << " valid=" << (dd->Valid ? 1 : 0) << " frameCount=" << dd->FrameCount << "\n";
      // Kac cizim komutu hangi dokuya gidiyor?
      long long atlasCmd = 0, otherCmd = 0;
      ImTextureID atlasId = (ImTextureID)-1;
      for (int n = 0; n < dd->CmdListsCount; ++n) {
        for (const ImDrawCmd& c : dd->CmdLists[n]->CmdBuffer) {
          ImTextureID t = c.GetTexID();
          if (atlasId == (ImTextureID)-1) atlasId = t;
          if (t == atlasId) ++atlasCmd; else ++otherCmd;
        }
      }
      ss << "ilk doku=" << (void*)atlasId << "  atlasDokuKomut=" << atlasCmd
         << "  digerDokuKomut=" << otherCmd << "\n";
      // Her cizim listesi: kose sayisi ve gercek kapsam (min/max konum)
      for (int n = 0; n < dd->CmdListsCount; ++n) {
        const ImDrawList* dl = dd->CmdLists[n];
        float minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
        for (int v = 0; v < dl->VtxBuffer.Size; ++v) {
          const ImVec2& p = dl->VtxBuffer[v].pos;
          if (p.x < minx) minx = p.x;
          if (p.y < miny) miny = p.y;
          if (p.x > maxx) maxx = p.x;
          if (p.y > maxy) maxy = p.y;
        }
        ss << "  liste[" << n << "] vtx=" << dl->VtxBuffer.Size << " idx=" << dl->IdxBuffer.Size
           << " komut=" << dl->CmdBuffer.Size << " alan=[" << (int)minx << "," << (int)miny << "]-[" << (int)maxx
           << "," << (int)maxy << "]\n";
      }
    }

    ss << "\n--- stil / arayuz ---\n";
    ImGuiStyle& st = ImGui::GetStyle();
    ss << "pencerePadding: " << st.WindowPadding.x << "," << st.WindowPadding.y
       << " cerceve: " << st.WindowBorderSize << " dolgu: " << st.FramePadding.x << ","
       << st.FramePadding.y << "\n";
    ss << "--- ImGui hata gunlugu (DebugLogBuf) ---\n";
    if (!GImGui->DebugLogBuf.empty()) {
      std::string db(GImGui->DebugLogBuf.begin(), GImGui->DebugLogBuf.end());
      std::size_t from = db.size() > 3000 ? db.size() - 3000 : 0;
      ss << db.substr(from) << "\n";
    } else {
      ss << "(hata yok)\n";
    }
    ss << "--- ImGui log buffer (LogBuffer) ---\n";
    if (!GImGui->LogBuffer.empty()) {
      std::string lb(GImGui->LogBuffer.begin(), GImGui->LogBuffer.end());
      std::size_t lfrom = lb.size() > 1500 ? lb.size() - 1500 : 0;
      ss << lb.substr(lfrom) << "\n";
    } else {
      ss << "(bos)\n";
    }
    std::ofstream os(std::filesystem::path(report), std::ios::binary | std::ios::trunc);
    if (os) {
      os << ss.str();
      os.flush();
    }
  }

  app.shutdown();
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
  platform.destroy();
  return 0;
}
