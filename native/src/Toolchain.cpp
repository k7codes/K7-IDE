#include "gpi/Toolchain.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

namespace fs = std::filesystem;

namespace gpi {

bool ToolchainReport::canBuildCSharp() const {
  return !preferredCSharp.empty();
}
bool ToolchainReport::canBuildCpp() const {
  return !preferredCpp.empty();
}

// ===========================================================================
//  ToolchainProbe
// ===========================================================================

bool ToolchainProbe::fileExists(const std::string& path) {
  if (path.empty()) return false;
  std::error_code ec;
  return fs::exists(fs::u8path(path), ec) && !fs::is_directory(fs::u8path(path), ec);
}

std::optional<std::string> ToolchainProbe::which(const std::string& exeName) {
  if (exeName.empty()) return std::nullopt;
  if (fileExists(exeName)) return exeName;

  const char* pathEnv = std::getenv("PATH");
  if (!pathEnv) return std::nullopt;
#ifdef _WIN32
  const char kSep = ';';
  const std::array<const char*, 4> exts = {".exe", ".cmd", ".bat", ""};
#else
  const char kSep = ':';
  const std::array<const char*, 1> exts = {""};
#endif
  std::stringstream ss(pathEnv);
  std::string dir;
  while (std::getline(ss, dir, kSep)) {
    if (dir.empty()) continue;
    for (const char* ext : exts) {
      const std::string full = dir + "\\" + exeName + ext;
      if (fileExists(full)) return full;
    }
  }
  return std::nullopt;
}

std::string ToolchainProbe::runCapture(const std::string& exe, const std::vector<std::string>& args,
                                       int* exitCode) {
  std::string cmdLine = "\"" + exe + "\"";
  for (const auto& a : args) cmdLine += " " + a;

  FILE* pipe = _popen(cmdLine.c_str(), "r");
  if (!pipe) {
    if (exitCode) *exitCode = -1;
    return {};
  }
  std::string out;
  char buf[512];
  while (std::fgets(buf, sizeof buf, pipe) != nullptr) out += buf;
  const int rc = _pclose(pipe);
  if (exitCode) *exitCode = rc;
  return out;
}

std::string ToolchainProbe::extractVersion(const std::string& text, std::string_view toolName) {
  // "cl.exe 14.44.35207" -> "14.44.35207"
  static const std::regex verRe(R"(([0-9]+(?:\.[0-9]+){1,3}))");
  std::smatch m;
  if (std::regex_search(text, m, verRe)) return m[1].str();

  // Araç adına özgü desenler
  if (toolName == "cl" || toolName == "MSVC") {
    static const std::regex clRe(R"(Version\s+([0-9][0-9.]*))", std::regex::icase);
    if (std::regex_search(text, m, clRe)) return m[1].str();
  }
  return {};
}

// ===========================================================================
//  ToolchainDiscovery
// ===========================================================================

namespace {

std::string envOr(const char* key, const char* fallback = "") {
  const char* v = std::getenv(key);
  return v ? v : fallback;
}

std::string trimStr(std::string s) {
  std::size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return {};
  std::size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

std::string dirOf(const std::string& path) {
  return fs::path(path).parent_path().string();
}

}  // namespace

void ToolchainDiscovery::detectWindowsSdk(ToolchainReport& r) const {
  const std::string root =
      envOr("ProgramFiles(x86)", "C:\\Program Files (x86)") + "\\Windows Kits\\10\\Include";
  std::error_code ec;
  if (!fs::exists(fs::u8path(root), ec)) {
    r.errors.push_back("Windows 10 SDK bulunamadı. C++ derlemesi için SDK gerekir.");
    return;
  }
  long best = -1;
  for (const auto& entry : fs::directory_iterator(fs::u8path(root), ec)) {
    if (!entry.is_directory()) continue;
    const std::string name = entry.path().filename().string();
    static const std::regex sdkRe(R"(^10\.0\.(\d+)\.0$)");
    std::smatch m;
    if (!std::regex_match(name, m, sdkRe)) continue;
    const long ver = std::strtol(m[1].str().c_str(), nullptr, 10);
    if (ver > best) {
      best = ver;
      sdkVersion_ = name;
    }
  }
  if (sdkVersion_.empty()) {
    r.errors.push_back("Windows 10 SDK sürümü tanınamadı.");
  } else {
    r.windowsSdkVersion = sdkVersion_;
    r.notes.push_back("Windows SDK " + sdkVersion_ + " bulundu.");
  }
}

void ToolchainDiscovery::detectMsvc(ToolchainReport& r) const {
  // 1) VSWHERE ile kurulu Visual Studio örnekleri
  const std::string vswhere = envOr("ProgramFiles(x86)", "C:\\Program Files (x86)") +
                              "\\Microsoft Visual Studio\\Installer\\vswhere.exe";
  std::vector<std::string> instances;

  if (ToolchainProbe::fileExists(vswhere)) {
    const std::string out =
        ToolchainProbe::runCapture(vswhere, {"-all", "-prerelease", "-products", "*", "-format",
                                            "text", "-property", "installationPath"});
    std::stringstream ss(out);
    std::string line;
    while (std::getline(ss, line)) {
      line = trimStr(line);
      if (!line.empty() && fs::exists(fs::u8path(line))) instances.push_back(line);
    }
  }
  // 2) Bilinen konumlar (vswhere yoksa)
  for (const char* guess : {"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community",
                            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional",
                            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise",
                            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Community",
                            "C:\\Program Files\\Microsoft Visual Studio\\2022\\BuildTools",
                            "C:\\BuildTools"}) {
    const std::string p = guess;
    if (fs::exists(fs::u8path(p)) &&
        std::find(instances.begin(), instances.end(), p) == instances.end()) {
      instances.push_back(p);
    }
  }

  for (const auto& vs : instances) {
    const fs::path toolsets = fs::u8path(vs) / "VC" / "Tools" / "MSVC";
    std::error_code ec;
    if (!fs::exists(toolsets, ec)) continue;

    // En yeni araç takımını seç
    std::string bestToolset;
    long bestVer = -1;
    for (const auto& entry : fs::directory_iterator(toolsets, ec)) {
      if (!entry.is_directory()) continue;
      const std::string name = entry.path().filename().string();
      const long ver = std::strtol(name.c_str(), nullptr, 10);
      if (ver > bestVer) {
        bestVer = ver;
        bestToolset = name;
      }
    }
    if (bestToolset.empty()) continue;

    CompilerTool tool;
    tool.family = "msvc";
    tool.id = "msvc-" + bestToolset;
    tool.version = bestToolset;
    const fs::path cl = toolsets / fs::u8path(bestToolset) / "bin" / "Hostx64" / "x64" / "cl.exe";
    if (!ToolchainProbe::fileExists(cl.string())) {
      tool.ready = false;
      tool.diagnostic = "cl.exe bulunamadı: " + cl.string();
    } else {
      tool.exePath = cl.string();
    }
    const fs::path hostx64 = toolsets / fs::u8path(bestToolset) / "bin" / "Hostx64" / "x64";
    if (ToolchainProbe::fileExists((hostx64 / "link.exe").string())) {
      tool.canLink = true;
      tool.exePath = ToolchainProbe::fileExists(tool.exePath)
                         ? tool.exePath
                         : (hostx64 / "cl.exe").string();
      if (tool.exePath.empty()) tool.exePath = (hostx64 / "cl.exe").string();
    }

    // Windows SDK
    const fs::path sdkRoot =
        fs::u8path(envOr("ProgramFiles(x86)", "C:\\Program Files (x86)")) / "Windows Kits" / "10";
    std::string sdkVer;
    std::error_code sec;
    if (fs::exists(sdkRoot / "Include", sec)) {
      long best = -1;
      for (const auto& entry : fs::directory_iterator(sdkRoot / "Include", sec)) {
        if (!entry.is_directory()) continue;
        const std::string name = entry.path().filename().string();
        static const std::regex sdkRe(R"(^10\.0\.(\d+)\.0$)");
        std::smatch m;
        if (!std::regex_match(name, m, sdkRe)) continue;
        const long ver = std::strtol(m[1].str().c_str(), nullptr, 10);
        if (ver > best) {
          best = ver;
          sdkVer = name;
        }
      }
    }
    tool.sdkVersion = sdkVer;

    const fs::path vcvars =
        fs::u8path(vs) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat";
    tool.vcvars = ToolchainProbe::fileExists(vcvars.string()) ? vcvars.string() : "";
    tool.installRoot = vs;
    tool.displayName = "MSVC " + bestToolset + " (x64)";
    tool.targetTriple = "x86_64-pc-windows-msvc";
    tool.canBuildCpp = tool.ready;

    if (sdkVer.empty()) {
      tool.diagnostic += " Windows SDK bulunamadı.";
      tool.ready = false;
    }
    if (tool.vcvars.empty()) {
      tool.diagnostic += " vcvars64.bat bulunamadı.";
    }

    r.tools.push_back(tool);

    // MSBuild
    const fs::path msbuild = fs::u8path(vs) / "MSBuild" / "Current" / "Bin" / "MSBuild.exe";
    if (ToolchainProbe::fileExists(msbuild.string())) {
      CompilerTool bt;
      bt.family = "msbuild";
      bt.id = "msbuild-" + vs;
      bt.displayName = "MSBuild (Visual Studio " + bestToolset + ")";
      bt.exePath = msbuild.string();
      bt.version = bestToolset;
      bt.canBuildCpp = true;
      bt.installRoot = vs;
      r.buildSystems.push_back(bt);
    }
  }

  // Tercih: en yeni MSVC
  if (r.preferredCpp.empty()) {
    for (const auto& t : r.tools) {
      if (t.family == "msvc" && t.ready) {
        r.preferredCpp = t.id;
        break;
      }
    }
  }
}

void ToolchainDiscovery::detectClang(ToolchainReport& r) const {
  for (const char* exe : {"clang++.exe", "clang-cl.exe", "clang.exe"}) {
    if (auto p = ToolchainProbe::which(exe)) {
      CompilerTool t;
      t.family = "clang";
      t.id = std::string("clang-") + exe;
      t.exePath = *p;
      t.displayName = "Clang (" + std::string(exe) + ")";
      t.canBuildCpp = true;
      t.canLink = true;
      t.targetTriple = "x86_64-pc-windows-msvc";
      t.version = ToolchainProbe::extractVersion(
          ToolchainProbe::runCapture(*p, {"--version"}), "clang");
      t.ready = true;
      r.tools.push_back(t);
      if (r.preferredCpp.empty()) r.preferredCpp = t.id;
    }
  }
}

void ToolchainDiscovery::detectGcc(ToolchainReport& r) const {
  for (const char* exe : {"g++.exe", "gcc.exe", "mingw32-make.exe"}) {
    if (auto p = ToolchainProbe::which(exe)) {
      CompilerTool t;
      t.family = (std::string(exe).find("mingw") != std::string::npos) ? "mingw" : "gcc";
      t.id = std::string("gcc-") + exe;
      t.exePath = *p;
      t.displayName = "GCC / MinGW (" + std::string(exe) + ")";
      t.canBuildCpp = true;
      t.canLink = true;
      t.targetTriple = "x86_64-w64-mingw32";
      t.version = ToolchainProbe::extractVersion(
          ToolchainProbe::runCapture(*p, {"--version"}), "gcc");
      t.ready = true;
      r.tools.push_back(t);
      if (r.preferredCpp.empty()) r.preferredCpp = t.id;
      break;
    }
  }
  // Bilinen MinGW konumları
  for (const char* guess : {"C:\\msys64\\mingw64\\bin\\g++.exe", "C:\\mingw64\\bin\\g++.exe",
                            "C:\\TDM-GCC-64\\bin\\g++.exe", "C:\\Strawberry\\c\\bin\\g++.exe"}) {
    if (ToolchainProbe::fileExists(guess)) {
      CompilerTool t;
      t.family = "mingw";
      t.id = "gcc-mingw";
      t.exePath = guess;
      t.displayName = "MinGW-w64 (g++)";
      t.canBuildCpp = true;
      t.canLink = true;
      t.ready = true;
      r.tools.push_back(t);
      if (r.preferredCpp.empty()) r.preferredCpp = t.id;
      break;
    }
  }
}

void ToolchainDiscovery::detectDotNet(ToolchainReport& r) const {
  std::string dotnet = envOr("DOTNET_ROOT");
  if (dotnet.empty()) dotnet = envOr("ProgramFiles", "C:\\Program Files") + "\\dotnet\\dotnet.exe";
  if (!ToolchainProbe::fileExists(dotnet)) {
    if (auto p = ToolchainProbe::which("dotnet")) dotnet = *p;
  }
  if (dotnet.empty() || !ToolchainProbe::fileExists(dotnet)) {
    r.errors.push_back(".NET SDK bulunamadı. C# projeleri derlenemez.");
    return;
  }

  CompilerTool t;
  t.family = "dotnet";
  t.id = "dotnet-sdk";
  t.exePath = dotnet;
  t.displayName = ".NET SDK";
  t.canBuildCSharp = true;
  t.canBuildCpp = false;
  t.ready = true;

  const std::string info = ToolchainProbe::runCapture(dotnet, {"--version"});
  t.version = trimStr(info);
  r.dotnetSdkVersion = t.version;
  r.preferredCSharp = t.id;
  r.tools.push_back(t);

  // NuGet önbelleği
  r.nugetCachePath = envOr("USERPROFILE") + "\\.nuget\\packages";

  // Roslyn (csc.dll) doğrudan derleme için
  const fs::path sdkDir = fs::path(dotnet).parent_path() / "sdk";
  std::error_code ec;
  if (fs::exists(sdkDir, ec)) {
    std::string newest;
    long best = -1;
    for (const auto& e : fs::directory_iterator(sdkDir, ec)) {
      if (!e.is_directory()) continue;
      const std::string name = e.path().filename().string();
      const long v = std::strtol(name.c_str(), nullptr, 10);
      if (v > best) {
        best = v;
        newest = name;
      }
    }
    if (!newest.empty()) {
      const fs::path csc = sdkDir / fs::u8path(newest) / "Roslyn" / "bincore" / "csc.dll";
      if (ToolchainProbe::fileExists(csc.string())) {
        CompilerTool rc;
        rc.family = "roslyn";
        rc.id = "roslyn-" + newest;
        rc.exePath = dotnet + " \"" + csc.string() + "\"";
        rc.displayName = "Roslyn derleyicisi " + newest;
        rc.version = newest;
        rc.canBuildCSharp = true;
        rc.ready = true;
        r.tools.push_back(rc);
      }
    }
  }
}

void ToolchainDiscovery::detectVcpkg(ToolchainReport& r) const {
  const char* envV = std::getenv("VCPKG_ROOT");
  if (envV && *envV && fs::exists(fs::u8path(std::string(envV)))) {
    r.vcpkgRoot = envV;
    return;
  }
  const std::string def = envOr("USERPROFILE") + "\\vcpkg";
  if (fs::exists(fs::u8path(def))) {
    r.vcpkgRoot = def;
    return;
  }
  if (auto p = ToolchainProbe::which("vcpkg")) r.vcpkgRoot = dirOf(*p);
}

void ToolchainDiscovery::detectBuildSystems(ToolchainReport& r) const {
  for (const char* exe : {"cmake.exe", "ninja.exe"}) {
    if (auto p = ToolchainProbe::which(exe)) {
      CompilerTool t;
      t.family = "cmake";
      t.id = std::string(exe);
      t.exePath = *p;
      t.displayName = std::string(exe) == "cmake.exe" ? "CMake" : "Ninja";
      t.version = ToolchainProbe::extractVersion(
          ToolchainProbe::runCapture(*p, {"--version"}), exe);
      t.ready = true;
      r.buildSystems.push_back(t);
    }
  }
}

ToolchainReport ToolchainDiscovery::probe() const {
  ToolchainReport r;
  bestVer_ = -1;
  sdkVersion_.clear();
  detectMsvc(r);
  detectClang(r);
  detectGcc(r);
  detectDotNet(r);
  detectBuildSystems(r);
  detectVcpkg(r);
  detectWindowsSdk(r);
  if (r.vcpkgRoot.empty()) {
    // C++ paket desteği yok ama derleme çalışır
  } else {
    r.notes.push_back("vcpkg bulundu: " + r.vcpkgRoot);
  }
  return r;
}

}  // namespace gpi
