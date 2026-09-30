// Gözle Programlama IDE - Proje dosyası üretimi
#include <algorithm>
#include <cctype>
#include <cstdio>

#include "gpi/Codegen.hpp"

namespace gpi {

namespace {

std::string xmlEscape(const std::string& s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&apos;"; break;
      default: out += c;
    }
  }
  return out;
}

std::string toLower(std::string s) {
  for (char& c : s) s = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
  return s;
}

std::string sanitizeName(std::string s) {
  std::string out;
  for (char c : s) {
    out += (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') ? c : '_';
  }
  if (out.empty()) out = "Program";
  if (std::isdigit(static_cast<unsigned char>(out[0]))) out.insert(out.begin(), '_');
  return out;
}

// Kararlı bir proje GUID'i üretir (aynı ad her zaman aynı GUID'i verir)
std::string guidFor(const std::string& seed) {
  unsigned long long h = 1469598103934665603ULL;  // FNV-1a
  for (unsigned char c : seed) {
    h ^= c;
    h *= 1099511628211ULL;
  }
  char buf[40];
  std::snprintf(buf, sizeof buf, "%08X-%04X-%04X-%04X-%012llX",
                static_cast<unsigned>(h & 0xFFFFFFFFu), static_cast<unsigned>((h >> 32) & 0xFFFF),
                static_cast<unsigned>((h >> 48) & 0xFFFF), static_cast<unsigned>((h >> 16) & 0xFFFF),
                h & 0xFFFFFFFFFFFFFull);
  return buf;
}

}  // namespace

// scaffold() ile aynı adlandırma kuralı: derleme planı da bunu kullanmalı.
std::string ProjectGenerator::assemblyBaseName(const ProjectOptions& opts) const {
  if (!opts.assemblyName.empty()) {
    const std::string a = sanitizeName(opts.assemblyName);
    if (!a.empty()) return a;
  }
  const std::string n = sanitizeName(opts.appName);
  return n.empty() ? std::string("GpiProgram") : n;
}

std::string ProjectGenerator::projectFileName(const ProjectOptions& opts) const {
  if (opts.lang == TargetLang::CSharp) return assemblyBaseName(opts) + ".csproj";
  return "GpiProgram.vcxproj";
}

ProjectScaffold ProjectGenerator::scaffold(const ProjectOptions& opts) const {
  ProjectScaffold s;
  const std::string name = assemblyBaseName(opts);

  if (opts.lang == TargetLang::CSharp) {
    // --- .csproj ---
    std::string csproj;
    csproj += "<Project Sdk=\"Microsoft.NET.Sdk\">\n\n";
    csproj += "  <PropertyGroup>\n";
    csproj += "    <OutputType>" + opts.outputType + "</OutputType>\n";
    csproj += "    <TargetFramework>" + opts.framework + "</TargetFramework>\n";
    csproj += "    <RootNamespace>" + xmlEscape(opts.rootNamespace) + "</RootNamespace>\n";
    csproj += "    <AssemblyName>" + xmlEscape(name) + "</AssemblyName>\n";
    csproj += "    <Nullable>enable</Nullable>\n";
    csproj += "    <ImplicitUsings>disable</ImplicitUsings>\n";
    csproj += "    <LangVersion>latest</LangVersion>\n";
    csproj += "    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>\n";
    csproj += "    <Platforms>AnyCPU;x64</Platforms>\n";
    csproj += "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n";
    csproj += "    <SatelliteResourceLanguages>en</SatelliteResourceLanguages>\n";
    csproj += "    <GenerateDocumentationFile>false</GenerateDocumentationFile>\n";
    csproj += "    <InvariantGlobalization>false</InvariantGlobalization>\n";
    csproj += "    <NoWarn>$(NoWarn);CS1591</NoWarn>\n";
    csproj += "  </PropertyGroup>\n\n";

    if (!opts.packages.empty()) {
      csproj += "  <ItemGroup>\n";
      for (const auto& p : opts.packages) {
        csproj += "    <PackageReference Include=\"" + xmlEscape(p.id) + "\" Version=\"" +
                  (p.version.empty() ? std::string("*") : xmlEscape(p.version)) + "\" />\n";
      }
      csproj += "  </ItemGroup>\n\n";
    }

    csproj += "  <ItemGroup>\n";
    csproj += "    <Compile Include=\"Program.g.cs\" />\n";
    for (const auto& f : opts.extraSourceFiles) {
      csproj += "    <Compile Include=\"" + xmlEscape(f) + "\" />\n";
    }
    csproj += "  </ItemGroup>\n\n";

    if (opts.interopEnabled) {
      csproj += "  <ItemGroup>\n";
      csproj += "    <None Include=\"native\\**\\*\" />\n";
      csproj += "  </ItemGroup>\n\n";
      s.notes.push_back(
          "Karma kip etkin: native klasöründeki C++ kodu ayrıca derlenip DLL olarak üretilir.");
      s.notes.push_back("DLL çıktısını bin klasörüne kopyalamayı unutmayın.");
    }

    csproj += "</Project>\n";
    s.files.push_back({name + ".csproj", csproj});
    s.notes.push_back("Bu proje .NET SDK stilindedir; `dotnet build` ile derlenir.");

  } else {
    // --- CMakeLists.txt (MSVC / MinGW / Clang uyumlu) ---
    std::string cm;
    cm += "# Gözle Programlama IDE tarafından üretildi\n";
    cm += "cmake_minimum_required(VERSION 3.20)\n";
    cm += "project(" + name + " LANGUAGES CXX)\n\n";
    cm += "set(CMAKE_CXX_STANDARD " + opts.cxxStandard + ")\n";
    cm += "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n";
    cm += "set(CMAKE_CXX_EXTENSIONS OFF)\n";
    cm += "\n";
    cm += "if(MSVC)\n  add_compile_options(/utf-8 /EHsc /W3)\n";
    cm += "else()\n  add_compile_options(-Wall -Wextra)\nendif()\n\n";
    if (opts.interopEnabled) {
      cm += "add_library(" + name + " SHARED native/native.cpp)\n";
      cm += "target_include_directories(" + name + " PRIVATE native)\n";
    } else {
      cm += "add_executable(" + name + " native/main.cpp)\n";
    }
    for (const auto& src : opts.extraSourceFiles) {
      cm += "target_sources(" + name + " PRIVATE " + src + ")\n";
    }
    if (!opts.libs.empty()) {
      cm += "target_link_libraries(" + name + " PRIVATE";
      for (const auto& l : opts.libs) cm += " " + l;
      cm += ")\n";
    }
    cm += "\n";
    cm += "set_target_properties(" + name + " PROPERTIES\n";
    cm += "  RUNTIME_OUTPUT_DIRECTORY \"${CMAKE_BINARY_DIR}/bin\"\n";
    cm += "  LIBRARY_OUTPUT_DIRECTORY \"${CMAKE_BINARY_DIR}/bin\"\n";
    if (opts.interopEnabled) {
      cm += "  OUTPUT_NAME \"" + opts.interopDllName + "\"\n";
    }
    cm += ")\n";
    s.files.push_back({"CMakeLists.txt", cm});

    // --- cl.exe ile doğrudan derleme notu ---
    s.notes.push_back("CMake kurulu değilse doğrudan cl.exe ile de derlenebilir.");

    // --- vcxproj (MSBuild yolu) ---
    const bool isDll = opts.interopEnabled;
    const std::string configType = isDll ? "DynamicLibrary" : "Application";
    const std::string outName = isDll ? opts.interopDllName : name;
    std::string mainSource = isDll ? "native\\native.cpp" : "native\\main.cpp";

    std::string vx;
    vx += "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    vx += "<Project DefaultTargets=\"Build\" ToolsVersion=\"17.0\" "
          "xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n";
    vx += "  <ItemGroup Label=\"ProjectConfigurations\">\n";
    for (const char* c : {"Debug", "Release"}) {
      vx += "    <ProjectConfiguration Include=\"" + std::string(c) + "|x64\">\n";
      vx += "      <Configuration>" + std::string(c) + "</Configuration>\n";
      vx += "      <Platform>x64</Platform>\n    </ProjectConfiguration>\n";
    }
    vx += "  </ItemGroup>\n";
    vx += "  <PropertyGroup Label=\"Globals\">\n";
    vx += "    <ProjectGuid>{" + guidFor(name) + "}</ProjectGuid>\n";
    vx += "    <RootNamespace>" + xmlEscape(name) + "</RootNamespace>\n";
    vx += "    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>\n";
    vx += "    <ProjectName>GpiProgram</ProjectName>\n";
    vx += "  </PropertyGroup>\n";
    vx += "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\n";
    for (const char* c : {"Debug", "Release"}) {
      vx += "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='" +
            std::string(c) + "|x64'\" Label=\"Configuration\">\n";
      vx += "    <ConfigurationType>" + configType + "</ConfigurationType>\n";
      vx += "    <PlatformToolset>v143</PlatformToolset>\n";
      vx += "    <CharacterSet>Unicode</CharacterSet>\n";
      vx += "    <UseDebugLibraries>" + std::string(c == std::string("Debug") ? "true" : "false") +
            "</UseDebugLibraries>\n";
      vx += "    <WholeProgramOptimization>" +
            std::string(c == std::string("Debug") ? "false" : "true") + "</WholeProgramOptimization>\n";
      vx += "  </PropertyGroup>\n";
    }
    vx += "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />\n";
    vx += "  <ImportGroup Label=\"PropertySheets\">\n";
    vx += "    <Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" "
          "Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" "
          "Label=\"LocalAppDataPlatform\" />\n";
    vx += "  </ImportGroup>\n";
    vx += "  <ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">\n";
    vx += "    <ClCompile>\n";
    vx += "      <WarningLevel>Level3</WarningLevel>\n";
    vx += "      <SDLCheck>true</SDLCheck>\n";
    vx += "      <PreprocessorDefinitions>_DEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>\n";
    vx += "      <ConformanceMode>true</ConformanceMode>\n";
    vx += "      <LanguageStandard>stdcpp" + opts.cxxStandard + "</LanguageStandard>\n";
    vx += "      <AdditionalIncludeDirectories>$(ProjectDir)native;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>\n";
    vx += "      <MultiProcessorCompilation>true</MultiProcessorCompilation>\n";
    vx += "    </ClCompile>\n";
    vx += "    <Link>\n";
    vx += "      <SubSystem>" + std::string(isDll ? "" : "Console") + "</SubSystem>\n";
    vx += "      <GenerateDebugInformation>true</GenerateDebugInformation>\n";
    vx += "    </Link>\n";
    vx += "  </ItemDefinitionGroup>\n";
    vx += "  <ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">\n";
    vx += "    <ClCompile>\n";
    vx += "      <WarningLevel>Level3</WarningLevel>\n";
    vx += "      <FunctionLevelLinking>true</FunctionLevelLinking>\n";
    vx += "      <IntrinsicFunctions>true</IntrinsicFunctions>\n";
    vx += "      <SDLCheck>true</SDLCheck>\n";
    vx += "      <PreprocessorDefinitions>NDEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>\n";
    vx += "      <ConformanceMode>true</ConformanceMode>\n";
    vx += "      <LanguageStandard>stdcpp" + opts.cxxStandard + "</LanguageStandard>\n";
    vx += "      <AdditionalIncludeDirectories>$(ProjectDir)native;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>\n";
    vx += "      <MultiProcessorCompilation>true</MultiProcessorCompilation>\n";
    vx += "      <Optimization>MaxSpeed</Optimization>\n";
    vx += "    </ClCompile>\n";
    vx += "    <Link>\n";
    vx += "      <SubSystem>" + std::string(isDll ? "" : "Console") + "</SubSystem>\n";
    vx += "      <EnableCOMDATFolding>true</EnableCOMDATFolding>\n";
    vx += "      <OptimizeReferences>true</OptimizeReferences>\n";
    vx += "      <GenerateDebugInformation>true</GenerateDebugInformation>\n";
    vx += "    </Link>\n";
    vx += "  </ItemDefinitionGroup>\n";
    vx += "  <ItemGroup>\n";
    vx += "    <ClInclude Include=\"native\\gpi_runtime.h\" />\n";
    if (isDll) vx += "    <ClInclude Include=\"native\\gpi_native.h\" />\n";
    vx += "  </ItemGroup>\n";
    vx += "  <ItemGroup>\n";
    vx += "    <ClCompile Include=\"" + mainSource + "\" />\n";
    for (const auto& f : opts.extraSourceFiles) {
      vx += "    <ClCompile Include=\"" + xmlEscape(f) + "\" />\n";
    }
    vx += "  </ItemGroup>\n";
    vx += "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />\n";
    vx += "</Project>\n";
    s.files.push_back({"GpiProgram.vcxproj", vx});
    s.notes.push_back("MSBuild yolu: GpiProgram.vcxproj (Platform: x64).");

    // --- paket yapılandırması ---
    if (!opts.packages.empty()) {
      std::string cfg;
      cfg += "# NuGet paketleri (C# tarafı)\n";
      for (const auto& p : opts.packages) {
        cfg += "#   " + p.id + " " + (p.version.empty() ? "*" : p.version) + "\n";
      }
      s.files.push_back({"packages.notes", cfg});
    }
    s.notes.push_back("C++ derlemesi MSVC (cl.exe) veya CMake + Ninja ile yapılabilir.");
  }

  return s;
}

ProjectScaffold ProjectGenerator::scaffoldInterop(const ProjectOptions& opts) const {
  ProjectScaffold s = scaffold(opts);
  const std::string name =
      opts.assemblyName.empty() ? sanitizeName(opts.appName) : sanitizeName(opts.assemblyName);
  const std::string ns = opts.interopNamespace.empty() ? "Gpi.Interop" : opts.interopNamespace;

  // --- C++ tarafı: dışa aktarılan işlevler ---
  std::string cpp;
  cpp += "// C++ yerel katman - C# tarafından P/Invoke ile çağrılır.\n";
  cpp += "// Bu dosya otomatik üretildi; düzenlemeler graf üzerinden yapılmalıdır.\n\n";
  cpp += "#include \"gpi_native.h\"\n\n";
  cpp += "extern \"C\" {\n\n";
  cpp += "  GPI_API int gpi_native_version() { return 1; }\n\n";
  cpp += "  // Metin döndüren örnek işlev\n";
  cpp += "  GPI_API gpi_string gpi_hello() { return gpi_string_new(\"Merhaba, C++ tarafindan!\"); }\n\n";
  cpp += "  // Girdi alıp sonuç döndüren örnek işlev\n";
  cpp += "  GPI_API int gpi_triple(int value) { return value * 3; }\n\n";
  cpp += "  GPI_API void gpi_string_free(gpi_string s) { gpi_string_release(s); }\n\n";
  cpp += "}\n";
  s.files.push_back({"native/native.cpp", cpp});

  // --- C++ başlık ---
  std::string hdr;
  hdr += "#pragma once\n#include <string>\n#include <cstdint>\n\n";
  hdr += "#ifdef _WIN32\n#  define GPI_API __declspec(dllexport)\n#else\n";
  hdr += "#  define GPI_API __attribute__((visibility(\"default\")))\n#endif\n\n";
  hdr += "struct gpi_string {\n  const char* data;\n  std::uint32_t length;\n};\n\n";
  hdr += "gpi_string gpi_string_new(const char* text);\n";
  hdr += "void gpi_string_release(gpi_string s);\n\n";
  hdr += "extern \"C\" {\n";
  hdr += "  GPI_API int gpi_native_version();\n";
  hdr += "  GPI_API gpi_string gpi_hello();\n";
  hdr += "  GPI_API int gpi_triple(int value);\n";
  hdr += "  GPI_API void gpi_string_free(gpi_string s);\n";
  hdr += "}\n";
  s.files.push_back({"native/gpi_native.h", hdr});

  // --- C# tarafı: P/Invoke sarmalayıcı ---
  std::string cs;
  cs += "// C# tarafından C++ katmanına erişim\n";
  cs += "using System;\nusing System.Runtime.InteropServices;\n\n";
  cs += "namespace " + ns + "\n{\n";
  cs += "    /// <summary>C++ katmanından dönen metin.</summary>\n";
  cs += "    [StructLayout(LayoutKind.Sequential)]\n";
  cs += "    public struct GpiString : IDisposable\n    {\n";
  cs += "        public IntPtr Data;\n        public uint Length;\n\n";
  cs += "        public string ToManaged()\n        {\n";
  cs += "            return Data == IntPtr.Zero ? string.Empty\n";
  cs += "                : Marshal.PtrToStringAnsi(Data, (int)Length) ?? string.Empty;\n";
  cs += "        }\n\n        public void Dispose()\n        {\n";
  cs += "            if (Data != IntPtr.Zero) { GpiNative.FreeString(this); Data = IntPtr.Zero; }\n";
  cs += "        }\n    }\n\n";
  cs += "    public static class GpiNative\n    {\n";
  cs += "        public const string Library = \"" + opts.interopDllName + "\";\n\n";
  cs += "        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]\n";
  cs += "        public static extern int NativeVersion();\n\n";
  cs += "        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]\n";
  cs += "        public static extern GpiString Hello();\n\n";
  cs += "        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]\n";
  cs += "        public static extern int Triple(int value);\n\n";
  cs += "        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]\n";
  cs += "        internal static extern void FreeString(GpiString s);\n\n";
  cs += "        /// <summary>C++ katmanı hazır mı?</summary>\n";
  cs += "        public static bool IsAvailable()\n        {\n";
  cs += "            try { return NativeVersion() > 0; }\n";
  cs += "            catch (DllNotFoundException) { return false; }\n";
  cs += "            catch (EntryPointNotFoundException) { return false; }\n";
  cs += "            catch (BadImageFormatException) { return false; }\n        }\n    }\n}\n";
  s.files.push_back({"GpiInterop.cs", cs});

  // --- C++ runtime yardımcıları (str::split, str::join, rand_int) ---
  std::string rt;
  rt += "// Üretilen C++ programlarının kullandığı küçük yardımcılar.\n";
  rt += "#pragma once\n#include <string>\n#include <vector>\n#include <sstream>\n";
  rt += "#include <algorithm>\n#include <random>\n#include <chrono>\n\n";
  rt += "namespace gpi {\n\n";
  rt += "inline gpi_string gpi_string_new(const char* text) {\n";
  rt += "  const std::string t(text ? text : \"\");\n";
  rt += "  char* buf = new char[t.size() + 1];\n";
  rt += "  std::memcpy(buf, t.c_str(), t.size() + 1);\n";
  rt += "  return gpi_string{buf, static_cast<std::uint32_t>(t.size())};\n}\n\n";
  rt += "inline void gpi_string_release(gpi_string s) { delete[] s.data; }\n\n";
  rt += "namespace str {\n";
  rt += "inline std::vector<std::string> split(const std::string& text, const std::string& sep) {\n";
  rt += "  std::vector<std::string> out;\n  if (sep.empty()) { out.push_back(text); return out; }\n";
  rt += "  std::size_t start = 0;\n";
  rt += "  while (true) {\n    std::size_t pos = text.find(sep, start);\n";
  rt += "    if (pos == std::string::npos) { if (start < text.size()) out.push_back(text.substr(start)); break; }\n";
  rt += "    if (pos > start) out.push_back(text.substr(start, pos - start));\n    start = pos + sep.size();\n  }\n";
  rt += "  return out;\n}\n\n";
  rt += "inline std::string join(const std::vector<std::string>& items, const std::string& sep) {\n";
  rt += "  std::string out;\n  for (std::size_t i = 0; i < items.size(); ++i) {\n";
  rt += "    if (i) out += sep; out += items[i]; } return out; }\n\n";
  rt += "inline std::string replace(std::string text, const std::string& from, const std::string& to) {\n";
  rt += "  if (from.empty()) return text;\n  std::size_t pos = 0;\n";
  rt += "  while ((pos = text.find(from, pos)) != std::string::npos) {\n";
  rt += "    text.replace(pos, from.size(), to); pos += to.size(); }\n  return text; }\n\n";
  rt += "}  // namespace str\n\n";
  rt += "inline int rand_int(int lo, int hi) {\n";
  rt += "  static std::mt19937 rng(static_cast<unsigned>(std::chrono::steady_clock::now()\n";
  rt += "      .time_since_epoch().count()));\n";
  rt += "  if (hi <= lo) return lo;\n";
  rt += "  std::uniform_int_distribution<int> dist(lo, hi - 1);\n  return dist(rng); }\n\n";
  rt += "}  // namespace gpi\n";
  s.files.push_back({"native/gpi_runtime.h", rt});

  s.notes.push_back("Karma proje: C++ katmanı DLL olarak derlenir, C# tarafı P/Invoke ile çağırır.");
  s.notes.push_back("DLL adı: " + opts.interopDllName + ".dll");
  s.notes.push_back("Derleme sırası: önce C++ DLL, sonra C# projesi.");
  s.notes.push_back("DLL ile C# çıktısı aynı klasörde olmalıdır.");

  return s;
}

}  // namespace gpi
