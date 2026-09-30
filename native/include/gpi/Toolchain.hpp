// Gözle Programlama IDE - C++ çekirdek: Araç Zinciri Keşfi ve Derleme
#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gpi/Codegen.hpp"

namespace gpi {

enum class CompilerFamily { Unknown, Msvc, Clang, Gcc, MinGW, DotNet, Mono, Roslyn, Cmake, MSBuild };

struct CompilerTool {
  std::string id;              // "msvc-14.44"
  std::string displayName;     // "MSVC 14.44 (x64)"
  std::string family;          // "msvc" | "clang" | "gcc" | "mingw" | "dotnet"
  std::string exePath;         // cl.exe / clang++.exe / g++.exe / dotnet
  std::string version;
  std::string targetTriple;    // x86_64-pc-windows-msvc
  bool canBuildCSharp = false;
  bool canBuildCpp = false;
  bool canLink = false;
  std::vector<std::string> includePaths;
  std::vector<std::string> libPaths;
  std::vector<std::string> defaultLibs;
  std::string vcvars;          // MSVC ortam kurulum betiği
  std::string installRoot;
  std::string sdkVersion;      // Windows SDK
  bool ready = true;
  std::string diagnostic;      // neden hazır değilse
};

struct ToolchainReport {
  std::vector<CompilerTool> tools;
  std::vector<CompilerTool> buildSystems;   // msbuild / cmake / ninja / dotnet
  std::string preferredCSharp;
  std::string preferredCpp;
  std::string dotnetSdkVersion;
  std::string nugetCachePath;
  std::string vcpkgRoot;
  std::string windowsSdkVersion;
  std::vector<std::string> notes;
  std::vector<std::string> errors;
  [[nodiscard]] bool canBuildCSharp() const;
  [[nodiscard]] bool canBuildCpp() const;
};

class ToolchainProbe {
 public:
  // Sürüm numarasını regex ile yakalar
  static std::string extractVersion(const std::string& text, std::string_view toolName);
  // Bir dosyanın var olup olmadığını kontrol eder
  static bool fileExists(const std::string& path);
  // Bir komutu PATH'te arar (PATHEXT dahil)
  static std::optional<std::string> which(const std::string& exeName);
  // Sürüm çıktısını çalıştırır
  static std::string runCapture(const std::string& exe, const std::vector<std::string>& args,
                                int* exitCode = nullptr);
};

class ToolchainDiscovery {
 public:
  ToolchainReport probe() const;

 private:
  // Keşif sırasında doldurulan önbellek (probe() const olduğu için mutable)
  mutable long bestVer_ = -1;
  mutable std::string sdkVersion_;
  void detectMsvc(ToolchainReport& r) const;
  void detectClang(ToolchainReport& r) const;
  void detectGcc(ToolchainReport& r) const;
  void detectDotNet(ToolchainReport& r) const;
  void detectBuildSystems(ToolchainReport& r) const;
  void detectVcpkg(ToolchainReport& r) const;
  void detectWindowsSdk(ToolchainReport& r) const;
};

// --- Tanılayıcılar ---
enum class Severity { Note, Warning, Error };

struct Diagnostic {
  Severity severity = Severity::Error;
  std::string file;
  int line = 0;
  int column = 0;
  std::string code;          // C2065, CS1002, NETSDK1045
  std::string message;
  std::string project;
  std::string raw;
  // Graf üzerindeki düğüme eşleme
  std::string nodeId;
  std::string portName;
};

std::vector<Diagnostic> parseDiagnostics(const std::vector<std::string>& lines,
                                        std::string_view language);

// --- Derleme ---
struct BuildRequest {
  std::string workingDirectory;
  std::vector<std::string> command;   // tam komut satırı argüman listesi
  // cmd.exe /c gibi kabuk gerektiren komutlar için ham komut satırı.
  // Boş değilse command[0] dışındaki argümanlar birleştirilmez, olduğu gibi
  // CreateProcess'e verilir.
  std::string rawCommandLine;
  std::string shellPrefix;            // vcvars gibi ön komut (cmd /c "call ... && ...")
  std::string target;
  int timeoutSeconds = 600;
};

struct BuildResult {
  bool launched = false;
  int exitCode = -1;
  bool success = false;
  bool cancelled = false;
  std::string output;                                  // tam günlük
  std::vector<std::string> log;                        // satır satır
  std::vector<Diagnostic> diagnostics;
  std::string commandLine;
  double durationMs = 0;
  std::string producedArtifact;
  // Boru hattı teşhisi: "peek=<n> peekFail=<n> ok=<n> hata=<win32 kodu>"
  std::string captureDebug;
};

using OutputSink = std::function<void(const std::string& line, bool isError)>;

class BuildRunner {
 public:
  BuildResult run(const BuildRequest& req, const OutputSink& sink = {}) const;
  // Zaman aşımı / iptal desteği
  static std::string quoteArg(const std::string& arg);
};

struct BuildPlan {
  bool valid = false;
  std::string error;
  BuildRequest request;
  std::string primaryFile;   // derlenecek ana kaynak
  std::string artifactHint;  // beklenen çıktı
};

class BuildPlanner {
 public:
  // C#: dotnet build
  BuildPlan planCSharp(const ToolchainReport& tc, const std::string& projectDir,
                       const std::string& projectFile, TargetLang lang) const;
  // C++: MSBuild + vcxproj (cmake yoksa da çalışır)
  BuildPlan planCppMsvc(const ToolchainReport& tc, const std::string& projectDir) const;
  // C++: doğrudan cl.exe + vcvars
  BuildPlan planCppDirect(const ToolchainReport& tc, const std::string& sourceFile,
                          const std::string& outputExe) const;
  // Çalıştır + çalıştır (exe'yi çalıştırıp çıktısını döndür)
  BuildResult runAndCaptureOutput(const std::string& exe, const std::vector<std::string>& args,
                                  const std::string& cwd, int timeoutSeconds,
                                  const OutputSink& sink) const;
};

}  // namespace gpi
