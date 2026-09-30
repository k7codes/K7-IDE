// Shell: yakalanmamış istisnaları yakalar ve sembolleştirilmiş rapor yazar.
// Amaç: geliştirme sırasında oluşan ACCESS_VIOLATION / yakalanmamış C++
// istisnalarının tam yerini (fonksiyon + satır) görebilmek.
#include "shell/CrashHandler.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// NOTE: dbghelp.h, SYMBOL_INFOP gibi bazı *P takma adlarını tanımlamıyor.
// Aşağıda doğrudan PSYMBOL_INFO / PIMAGEHLP_LINE64 kullanıyoruz.
#include <dbghelp.h>

#include <crtdbg.h>
#include <cstdio>
#include <cstdlib>
#include <string>

#pragma comment(lib, "dbghelp.lib")

namespace shell {
namespace {

std::wstring exePathNoExt() {
  wchar_t buf[MAX_PATH] = {0};
  ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring wp(buf);
  const std::size_t dot = wp.find_last_of(L'.');
  if (dot != std::wstring::npos) wp = wp.substr(0, dot);
  return wp;
}

void writeCrashReport(EXCEPTION_POINTERS* info) {
  const std::wstring reportPath = exePathNoExt() + L"-crash.txt";
  std::FILE* f = nullptr;
  _wfopen_s(&f, reportPath.c_str(), L"w, ccs=UTF-8");
  if (!f) return;

  const DWORD code = (info && info->ExceptionRecord) ? info->ExceptionRecord->ExceptionCode : 0;
  const void* addr = (info && info->ExceptionRecord) ? info->ExceptionRecord->ExceptionAddress : nullptr;

  std::fprintf(f, "=== Gpi.Shell Cok Raporu ===\n");
  std::fprintf(f, "istisna kodu : 0x%08lX\n", static_cast<unsigned long>(code));
  std::fprintf(f, "adres        : %p\n", addr);
  if (code == EXCEPTION_ACCESS_VIOLATION && info && info->ExceptionRecord->NumberParameters >= 2) {
    std::fprintf(f, "erisim       : %s %p\n",
                 info->ExceptionRecord->ExceptionInformation[0] ? "yazma" : "okuma",
                 info->ExceptionRecord->ExceptionInformation[1]);
  }
  std::fprintf(f, "\n--- yigin ---\n");

  HANDLE h = ::GetCurrentProcess();
  ::SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
  ::SymInitialize(h, nullptr, TRUE);

  void* frames[48];
  const USHORT n = ::CaptureStackBackTrace(0, 48, frames, nullptr);
  for (USHORT i = 0; i < n; ++i) {
    constexpr DWORD kNameBytes = 1024;
    SYMBOL_INFO* sym =
        static_cast<SYMBOL_INFO*>(::calloc(1, sizeof(SYMBOL_INFO) + kNameBytes));
    DWORD64 disp = 0;
    bool done = false;
    if (sym) {
      sym->SizeOfStruct = sizeof(SYMBOL_INFO);
      sym->MaxNameLen = kNameBytes - 1;
      if (::SymFromAddr(h, reinterpret_cast<DWORD64>(frames[i]), &disp, sym)) {
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisp = 0;
        PIMAGEHLP_LINE64 pLine = &line;
        if (::SymGetLineFromAddr64(h, reinterpret_cast<DWORD64>(frames[i]), &lineDisp, pLine) &&
            pLine->FileName) {
          std::fprintf(f, "[%02u] %s + 0x%llX  (%s:%u)\n", i, sym->Name,
                       static_cast<unsigned long long>(disp), pLine->FileName,
                       static_cast<unsigned>(pLine->LineNumber));
        } else {
          std::fprintf(f, "[%02u] %s + 0x%llX\n", i, sym->Name,
                       static_cast<unsigned long long>(disp));
        }
        done = true;
      }
      std::free(sym);
    }
    if (!done) std::fprintf(f, "[%02u] %p\n", i, frames[i]);
  }
  std::fprintf(f, "\n");
  std::fclose(f);
  ::SymCleanup(h);
}

LONG WINAPI onCrash(EXCEPTION_POINTERS* info) {
  writeCrashReport(info);
  return EXCEPTION_EXECUTE_HANDLER;
}

// _CrtDbgReportW() assertion/uyarı çağrılarını yakalar. Modal diyaloğu
// tamamen bastırır ve metni dosyaya yazar. Otomatik testler böylece kilitlenmez.
int __cdecl crtReportHook(int reportType, wchar_t* message, int* returnValue) {
  const wchar_t* kind = reportType == _CRT_ASSERT ? L"ASSERT" : L"WARN";
  const std::wstring path = exePathNoExt() + L"-assert.txt";
  std::FILE* f = nullptr;
  _wfopen_s(&f, path.c_str(), L"a, ccs=UTF-8");
  if (f) {
    std::fwprintf(f, L"--- %s ---\n%ls\n\n", kind, message ? message : L"");
    std::fclose(f);
  }
  if (returnValue) *returnValue = 1;  // "işlendi": diyalog açılmaz
  return TRUE;
}

}  // namespace

void installCrashHandler() {
  ::SetUnhandledExceptionFilter(onCrash);
  _CrtSetReportHookW2(_CRT_RPTHOOK_INSTALL, crtReportHook);
}

}  // namespace shell
