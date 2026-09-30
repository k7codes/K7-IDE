// Gözle Programlama IDE - C++ çekirdek: Kod ve Proje Üretimi
#pragma once

#include <map>
#include <string>
#include <vector>

#include "gpi/Emit.hpp"
#include "gpi/Graph.hpp"
#include "gpi/NodeCatalog.hpp"

namespace gpi {

enum class TargetLang { CSharp, Cpp };

struct PackageRef {
  std::string id;
  std::string version;   // boşsa "*"
};

struct ProjectOptions {
  TargetLang lang = TargetLang::CSharp;
  std::string appName = "Program";
  std::string rootNamespace = "Program";
  std::string assemblyName;
  std::string outputType = "Exe";        // Exe | Library
  std::string cxxStandard = "20";
  bool includeComments = true;
  bool emitUsings = true;
  bool inlinePureNodes = true;
  bool wrapInClass = true;
  std::string framework = "net9.0";
  std::vector<PackageRef> packages;
  std::vector<std::string> extraSourceFiles;
  std::vector<std::string> libs;        // C++ bağlantı kütüphaneleri
  // Karışık C++/C# modunda: C++ DLL üretip C# tarafından P/Invoke ile çağrılır
  bool interopEnabled = false;
  std::string interopNamespace = "GPI.Interop";
  std::string interopDllName = "gpi_native";
};

// Üretilen kodun bir düğüm portuna karşılık gelen konumu
struct SourceMapping {
  PortKey port;
  int line = 0;
  int column = 0;
  std::string symbol;
};

struct GenResult {
  bool ok = false;
  std::string code;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
  std::vector<std::string> requiredUsings;
  std::string entryFunction;
  std::map<std::string, SourceMapping> mapping;  // "nodeId.port" -> konum
  // Düğüm kimliği -> nihai kodda 1 tabanlı satır numarası.
  // Değeri 0 yoksa o düğüm kod üretmedi (ör. bağlanmamış saf düğüm).
  // Kod panelinde bir satıra tıklandığında ilgili düğüm bu haritadan bulunur.
  std::map<std::string, int> nodeLines;
  // Analiz sırasında üretilen, düğüm/port konumlu tanılar. Hata mesajları
  // errors/warnings içinde de vardır; buradaki sürüm konum bilgisini taşır.
  std::vector<NodeDiagnostic> diagnostics;
};

class CodeGenerator {
 public:
  explicit CodeGenerator(const NodeCatalog& catalog) : catalog_(catalog) {}

  GenResult generate(const Graph& g, const ProjectOptions& opts) const;

  // Alt grafı yeniden kullanılabilir bir C# / C++ fonksiyonuna çevirir.
  GenResult generateFunction(const Graph& g, const SubGraph& sg, const ProjectOptions& opts,
                             std::string_view fnName) const;

 private:
  const NodeCatalog& catalog_;
  TypeSystem ts_;
};

// --- Proje dosyası üretimi ---
struct GeneratedFile {
  std::string relativePath;
  std::string content;
};

struct ProjectScaffold {
  std::vector<GeneratedFile> files;
  std::vector<std::string> notes;
};

class ProjectGenerator {
 public:
  ProjectScaffold scaffold(const ProjectOptions& opts) const;
  ProjectScaffold scaffoldInterop(const ProjectOptions& opts) const;

  // scaffold()'ın ürettiği ana proje dosyasının adı.
  // (C#: <ad>.csproj, C++: GpiProgram.vcxproj)
  // Derleme planı bu adı kullanmalıdır; aksi halde MSBuild proje dosyasını bulamaz.
  std::string projectFileName(const ProjectOptions& opts) const;
  // Üretilen yürütülebilir dosyanın adı (uzantısız).
  std::string assemblyBaseName(const ProjectOptions& opts) const;
};

}  // namespace gpi
