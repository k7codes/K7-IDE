// Gözle Programlama IDE - Derleme çalıştırma ve tanılayıcı ayrıştırma
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <regex>
#include <sstream>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include "gpi/Toolchain.hpp"

namespace fs = std::filesystem;

namespace gpi {

// ===========================================================================
//  Tanılayıcı ayrıştırma
// ===========================================================================

namespace {

std::string trimStr(std::string s) {
  std::size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return {};
  std::size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

std::string lowerStr(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return s;
}

// MSVC / Roslyn / MSBuild satır biçimleri:
//   Program.cs(12,5): error CS1002: ; bekleniyor
//   C:\...\native.cpp(3): hata C2065: 'x': belirsiz ad
//   main.cpp(4,2): uyari C4101: 'x': kullanilmiyor
Severity severityFromWord(const std::string& w) {
  const std::string l = lowerStr(w);
  if (l == "error" || l == "hata" || l == "fatal error" || l == "kritik hata") return Severity::Error;
  if (l == "warning" || l == "uyari" || l == "uyarı" || l == "warn") return Severity::Warning;
  return Severity::Note;
}

const std::regex& lineDiagRe() {
  // dosya(satir,sutun): seviye KOD: mesaj
  static const std::regex re(
      R"RX(^\s*"?([^(]+)"?\((\d+)(?:,(\d+))?\)\s*:\s*(fatal error|error|hata|kritik hata|warning|warn|uyari|uyarı|note|not)\s+([A-Za-z]+\d+)\s*:\s*(.*)$)RX",
      std::regex::icase);
  return re;
}

const std::regex& toolDiagRe() {
  // cl : Command line warning D9025 : overriding '/O2'...
  static const std::regex re(
      R"(^\s*([A-Za-z_.][\w.]*)\s*:\s*(fatal error|error|hata|warning|warn|uyari|uyarı|message)\s+([A-Za-z]+\d+)\s*:\s*(.*)$)",
      std::regex::icase);
  return re;
}

const std::regex& bareDiagRe() {
  //   error CS1002: ; expected        (dosya bilgisi olmadan)
  static const std::regex re(
      R"(^\s*(fatal error|error|hata|warning|warn|uyari|uyarı)\s+([A-Za-z]+\d+)\s*:\s*(.*)$)",
      std::regex::icase);
  return re;
}

const std::regex& msbuildRe() {
  //   "CSC : error CS5001: Program does not contain a static 'Main'"
  static const std::regex re(
      R"(^\s*(?:([A-Za-z0-9_.]+)\s*:\s*)?(fatal error|error|hata|warning|warn|uyari|uyarı)\s+([A-Za-z]+\d+)\s*:\s*(.*)$)",
      std::regex::icase);
  return re;
}

}  // namespace

std::vector<Diagnostic> parseDiagnostics(const std::vector<std::string>& lines,
                                         std::string_view language) {
  std::vector<Diagnostic> out;
  for (const auto& raw : lines) {
    const std::string line = trimStr(raw);
    if (line.empty()) continue;

    std::smatch m;
    if (std::regex_match(line, m, lineDiagRe())) {
      Diagnostic d;
      d.file = trimStr(m[1].str());
      d.line = std::atoi(m[2].str().c_str());
      d.column = m[3].matched ? std::atoi(m[3].str().c_str()) : 0;
      d.severity = severityFromWord(m[4].str());
      d.code = m[5].str();
      d.message = trimStr(m[6].str());
      d.raw = line;
      if (!d.code.empty() && d.severity == Severity::Error &&
          (d.code.rfind("CS", 0) == 0 || d.code.rfind("NETSDK", 0) == 0 ||
           d.code.rfind("MSB", 0) == 0)) {
        d.severity = Severity::Error;
      }
      out.push_back(std::move(d));
      continue;
    }
    if (std::regex_match(line, m, toolDiagRe())) {
      Diagnostic d;
      d.project = m[1].str();
      d.severity = severityFromWord(m[2].str());
      d.code = m[3].str();
      d.message = trimStr(m[4].str());
      d.raw = line;
      out.push_back(std::move(d));
      continue;
    }
    if (std::regex_match(line, m, msbuildRe())) {
      Diagnostic d;
      d.project = m[1].matched ? m[1].str() : "";
      d.severity = severityFromWord(m[2].str());
      d.code = m[3].str();
      d.message = trimStr(m[4].str());
      d.raw = line;
      out.push_back(std::move(d));
      continue;
    }
    if (std::regex_match(line, m, bareDiagRe())) {
      Diagnostic d;
      d.severity = severityFromWord(m[1].str());
      d.code = m[2].str();
      d.message = trimStr(m[3].str());
      d.raw = line;
      out.push_back(std::move(d));
    }
  }
  (void)language;
  return out;
}

// ===========================================================================
//  BuildRunner
// ===========================================================================

std::string BuildRunner::quoteArg(const std::string& arg) {
  if (arg.empty()) return "\"\"";
  const bool needs = arg.find_first_of(" \t\"") != std::string::npos;
  if (!needs) return arg;
  std::string out = "\"";
  for (char c : arg) {
    if (c == '"') out += '\\';
    out += c;
  }
  out += '"';
  return out;
}

namespace {

// Bir komutu çalıştırıp stdout+stderr'i satır satır aktarır.
struct ProcessResult {
  int exitCode = -1;
  bool launched = false;
  bool timedOut = false;
  std::vector<std::string> lines;
  std::string captureDebug;
};

ProcessResult runProcess(const std::string& exe, const std::string& fullCmdLine,
                         const std::string& cwd, int timeoutSeconds,
                         const OutputSink& sink) {
  ProcessResult pr;
#ifdef _WIN32
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof sa;
  sa.lpSecurityDescriptor = nullptr;
  sa.bInheritHandle = TRUE;

  HANDLE readPipe = nullptr, writePipe = nullptr;
  if (!CreatePipe(&readPipe, &writePipe, &sa, 64 * 1024)) return pr;
  SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOA si{};
  si.cb = sizeof si;
  si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  si.hStdOutput = writePipe;
  si.hStdError = writePipe;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

  std::vector<char> cmdBuf(fullCmdLine.begin(), fullCmdLine.end());
  cmdBuf.push_back('\0');

  PROCESS_INFORMATION pi{};
  std::string mutableCwd = cwd;
  const BOOL ok = CreateProcessA(
      nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
      mutableCwd.empty() ? nullptr : mutableCwd.c_str(), &si, &pi);
  if (!ok) {
    CloseHandle(readPipe);
    CloseHandle(writePipe);
    return pr;
  }
  pr.launched = true;
  CloseHandle(writePipe);  // ana süreç kapattıysa sonunu görürüz

  const auto start = std::chrono::steady_clock::now();
  std::string pending;
  char buf[4096];
  bool over = false;
  long peekCalls = 0;
  long peekFail = 0;
  DWORD lastErr = 0;
  long totalRead = 0;

  // Borudaki tüm veriyi boşaltır. Süreç kapandıysa PeekNamedPipe
  // ERROR_BROKEN_PIPE döner; o zaman "veri kalmadı" demektir.
  auto drain = [&]() {
    for (;;) {
      DWORD avail = 0;
      if (!PeekNamedPipe(readPipe, nullptr, 0, nullptr, &avail, nullptr)) {
        ++peekFail;
        lastErr = GetLastError();
        return;  // kırık boru: daha fazla veri yok
      }
      if (avail == 0) return;
      DWORD read = 0;
      if (!ReadFile(readPipe, buf, static_cast<DWORD>(std::min<DWORD>(avail, sizeof buf)), &read,
                    nullptr) ||
          read == 0)
        return;
      pending.append(buf, read);
      totalRead += static_cast<long>(read);
    }
  };

  for (;;) {
    if (timeoutSeconds > 0) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::steady_clock::now() - start)
                               .count();
      if (elapsed >= timeoutSeconds) {
        over = true;
        TerminateProcess(pi.hProcess, 1);
        drain();
        break;
      }
    }
    DWORD avail = 0;
    ++peekCalls;
    if (!PeekNamedPipe(readPipe, nullptr, 0, nullptr, &avail, nullptr)) {
      ++peekFail;
      lastErr = GetLastError();
      break;  // süreç bitti ve boru kapandı
    }
    if (avail == 0) {
      if (WaitForSingleObject(pi.hProcess, 25) == WAIT_OBJECT_0) {
        // Süreç sona erdi: BORUDAKİ SON VERİYİ MUTLAKA AL.
        // (avail==0 olsa bile boruda veri olabilir.)
        drain();
        break;
      }
      continue;
    }
    DWORD read = 0;
    if (!ReadFile(readPipe, buf, static_cast<DWORD>(std::min<DWORD>(avail, sizeof buf)), &read,
                  nullptr) ||
        read == 0) {
      drain();
      break;
    }
    pending.append(buf, read);
    totalRead += static_cast<long>(read);
  }
  {
    char dbg[160];
    std::snprintf(dbg, sizeof dbg, "peek=%ld peekFail=%ld ok=%ld hata=%lu", peekCalls, peekFail,
                  totalRead, static_cast<unsigned long>(lastErr));
    pr.captureDebug = dbg;
  }

  if (!pending.empty()) {
    std::stringstream ss(pending);
    std::string line;
    while (std::getline(ss, line)) {
      std::string l = line;
      if (!l.empty() && l.back() == '\r') l.pop_back();
      pr.lines.push_back(l);
      if (sink) sink(l, false);
    }
  }

  WaitForSingleObject(pi.hProcess, 5000);
  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  pr.exitCode = static_cast<int>(code);
  pr.timedOut = over;

  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  CloseHandle(readPipe);
#else
  (void)exe;
  (void)fullCmdLine;
  (void)timeoutSeconds;
#endif
  return pr;
}

}  // namespace

BuildResult BuildRunner::run(const BuildRequest& req, const OutputSink& sink) const {
  BuildResult res;
  std::string full;
  if (!req.rawCommandLine.empty()) {
    // Ham komut satırı: argümanları yeniden birleştirme yok.
    // (cmd.exe /c "call vcvars && cl.exe ..." gibi kabuk komutları için)
    full = quoteArg(req.command.empty() ? std::string() : req.command[0]);
    if (!req.command.empty() && req.command.size() > 1) {
      // command[1] kabuk bayrakları (ör. /d /s /c) olduğu için düz eklenir
      for (std::size_t i = 1; i < req.command.size(); ++i) {
        full += " " + req.command[i];
      }
    }
    full += " " + req.rawCommandLine;
  } else {
    if (!req.shellPrefix.empty()) {
      full = req.shellPrefix + " ";
    }
    full += req.command.empty() ? std::string() : quoteArg(req.command[0]);
    for (std::size_t i = 1; i < req.command.size(); ++i) full += " " + quoteArg(req.command[i]);
  }
  res.commandLine = full;

  const auto start = std::chrono::steady_clock::now();
  ProcessResult pr = runProcess(req.command.empty() ? "" : req.command[0], full,
                                req.workingDirectory, req.timeoutSeconds, sink);
  const auto end = std::chrono::steady_clock::now();
  res.durationMs = std::chrono::duration<double, std::milli>(end - start).count();

  res.launched = pr.launched;
  res.exitCode = pr.exitCode;
  res.log = pr.lines;
  res.cancelled = pr.timedOut;
  res.captureDebug = pr.captureDebug;

  std::stringstream ss;
  for (const auto& l : pr.lines) ss << l << "\n";
  res.output = ss.str();
  res.diagnostics = parseDiagnostics(pr.lines, "");
  res.success = pr.launched && pr.exitCode == 0;

  if (!pr.launched) {
    res.output = "Derleme süreci başlatılamadı. Komut: " + full;
  }
  if (pr.timedOut) {
    res.output += "\n[Zaman aşımı: işlem sonlandırıldı]";
  }
  return res;
}

// ===========================================================================
//  BuildPlanner
// ===========================================================================

namespace {

const CompilerTool* findTool(const ToolchainReport& tc, const std::string& id) {
  for (const auto& t : tc.tools) {
    if (t.id == id) return &t;
  }
  for (const auto& t : tc.buildSystems) {
    if (t.id == id) return &t;
  }
  return nullptr;
}

}  // namespace

BuildPlan BuildPlanner::planCSharp(const ToolchainReport& tc, const std::string& projectDir,
                                    const std::string& projectFile, TargetLang lang) const {
  BuildPlan p;
  if (lang != TargetLang::CSharp) {
    p.error = "Bu plan yalnızca C# projeleri içindir.";
    return p;
  }
  const CompilerTool* dotnet = findTool(tc, tc.preferredCSharp);
  if (!dotnet) {
    p.error = ".NET SDK bulunamadı. C# derlenemez.";
    return p;
  }
  p.valid = true;
  p.request.workingDirectory = projectDir;
  p.request.command = {dotnet->exePath, "build", projectFile, "-c", "Release", "-v", "minimal",
                       "-nologo"};
  p.request.target = "dotnet build";
  p.primaryFile = projectFile;
  p.artifactHint = "bin\\Release\\net9.0\\";
  return p;
}

BuildPlan BuildPlanner::planCppMsvc(const ToolchainReport& tc, const std::string& projectDir) const {
  BuildPlan p;
  const CompilerTool* msvc = nullptr;
  for (const auto& t : tc.tools) {
    if (t.family == "msvc" && t.ready) {
      msvc = &t;
      break;
    }
  }
  if (!msvc) {
    p.error = "MSVC derleyicisi bulunamadı.";
    return p;
  }
  // MSBuild varsa onu tercih et
  for (const auto& b : tc.buildSystems) {
    if (b.family == "msbuild") {
      p.valid = true;
      p.request.workingDirectory = projectDir;
      p.request.command = {b.exePath, "GpiProgram.vcxproj", "/p:Configuration=Release",
                           "/p:Platform=x64", "/v:minimal", "/nologo", "/m"};
      p.request.target = "MSBuild";
      p.primaryFile = "GpiProgram.vcxproj";
      return p;
    }
  }
  p.error = "MSBuild bulunamadı.";
  return p;
}

BuildPlan BuildPlanner::planCppDirect(const ToolchainReport& tc, const std::string& sourceFile,
                                      const std::string& outputExe) const {
  BuildPlan p;
  const CompilerTool* msvc = nullptr;
  for (const auto& t : tc.tools) {
    if (t.family == "msvc" && t.ready) {
      msvc = &t;
      break;
    }
  }
  if (!msvc) {
    p.error = "MSVC derleyicisi bulunamadı.";
    return p;
  }
  std::string dir = sourceFile;
  const std::size_t slash = dir.find_last_of("/\\");
  if (slash != std::string::npos) dir = dir.substr(0, slash);

  p.valid = true;
  p.request.workingDirectory = dir;
  // cl.exe, hem kaynak dosyayı hem /Fe çıktısını çalışma klasörüne göre
  // çözer. Göreli yollar iki kez eklenmesin diye yalnızca dosya adları
  // geçiriliyor.
  const auto baseName = [](const std::string& path) {
    const std::size_t sep = path.find_last_of("/\\");
    return (sep == std::string::npos) ? path : path.substr(sep + 1);
  };
  const std::string sourceName = baseName(sourceFile);
  const std::string exeName = baseName(outputExe);
  // vcvars64 ile MSVC ortamını kurup cl.exe çalıştır.
  // cmd /c "..." içinde tüm komut tek bir dize olmalı; argüman listesi
  // kullanılamaz. Bu yüzden ham komut satırı kullanıyoruz.
  const std::string inner = "call \"" + msvc->vcvars + "\" >nul 2>&1 && cl.exe /nologo /EHsc "
                            "/std:c++20 /utf-8 /O2 /W3 /Fe:\"" + exeName + "\" \"" + sourceName +
                            "\"";
  p.request.command = {"cmd.exe", "/d", "/s", "/c"};
  p.request.rawCommandLine = inner;
  p.request.target = "cl.exe";
  p.primaryFile = sourceFile;
  p.artifactHint = outputExe;
  return p;
}

BuildResult BuildPlanner::runAndCaptureOutput(const std::string& exe,
                                              const std::vector<std::string>& args,
                                              const std::string& cwd, int timeoutSeconds,
                                              const OutputSink& sink) const {
  BuildRequest req;
  req.workingDirectory = cwd;
  req.command.push_back(exe);
  for (const auto& a : args) req.command.push_back(a);
  req.timeoutSeconds = timeoutSeconds;
  return BuildRunner{}.run(req, sink);
}

}  // namespace gpi
