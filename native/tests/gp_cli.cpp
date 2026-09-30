// Gözle Programlama IDE - Uçtan uca doğrulama aracı
// Arayüz olmadan graf üretir, kodu yazar, gerçekten derler ve çalıştırır.
//
// Kullanım:
//   gpcli probe                 Araç zincirini göster
//   gpcli catalog [arama]       Düğüm listesini göster
//   gpcli gen <cs|cpp> <klasör>  Örnek programı üret + derle + çalıştır
//   gpcli demo <cs|cpp>          Dize demosunu üret + derle + çalıştır
//   gpcli selftest              Tüm kontrolleri çalıştır
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "gpi/Api.h"
#include "gpi/Codegen.hpp"
#include "gpi/Json.hpp"
#include "gpi/NodeCatalog.hpp"
#include "gpi/Serialization.hpp"
#include "gpi/Toolchain.hpp"

namespace fs = std::filesystem;
using namespace gpi;

NodeCatalog& catalogInstance();

namespace {

void h1(const std::string& t) {
  std::cout << "\n\033[1;36m=== " << t << " ===\033[0m\n";
}
void ok(const std::string& t) { std::cout << "\033[1;32m  [TAMAM]\033[0m " << t << "\n"; }
void bad(const std::string& t) { std::cout << "\033[1;31m  [HATA ]\033[0m " << t << "\n"; }
void info(const std::string& t) { std::cout << "  " << t << "\n"; }

bool writeFile(const fs::path& p, const std::string& content) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  if (!f) return false;
  f << content;
  return f.good();
}

// ---------------------------------------------------------------------------
//  Örnek program:
//    toplam değişkeni tanımla
//    "Merhaba Dunya!" yaz
//    1'den 10'a: sayıyı yaz, toplamı biriktir
//    döngüden sonra "Toplam: 55" yaz
//
//  Not: Saf (pure) düğümlerin exec portu yoktur; değer portları üzerinden
//  hesaplanıp ifade olarak gömülürler. Akış zinciri yalnızca
//  ifade/akış portu taşıyan düğümlerden geçer.
// ---------------------------------------------------------------------------
Graph buildSampleGraph() {
  NodeCatalog& cat = catalogInstance();
  Graph g;
  g.name = "OrnekProgram";

  auto add = [&](const std::string& id, const std::string& type, double x, double y) {
    g.nodes.push_back(makeNode(cat, id, type, x, y));
  };
  auto lit = [&](const std::string& id, const char* port, const std::string& value) {
    g.findNode(id)->literals[port] = value;
  };

  // --- toplam değişkeni ---
  add("v0", "var.make.int", -900, 0);
  lit("v0", "Name", "\"toplam\"");

  // --- selam ---
  add("t0", "lit.text", -900, 160);
  lit("t0", "Value", "\"Merhaba Dunya!\"");
  add("p0", "io.print", -700, 160);
  g.edges.push_back(makeEdge("t0", "Value", "p0", "Value"));

  // --- döngü ---
  add("loop", "flow.forrange", -500, 160);
  lit("loop", "Start", "1");
  lit("loop", "End", "11");
  lit("loop", "Step", "1");

  // --- döngü gövdesi ---
  add("s0", "str.fromint", -700, 380);
  g.edges.push_back(makeEdge("loop", "Index", "s0", "Value"));
  add("p1", "io.print", -500, 380);
  g.edges.push_back(makeEdge("s0", "Result", "p1", "Value"));

  //   toplam = toplam + i
  add("g0", "var.get.int", -700, 560);
  lit("g0", "Name", "\"toplam\"");
  add("add", "math.add", -500, 560);
  g.edges.push_back(makeEdge("g0", "Value", "add", "A"));
  g.edges.push_back(makeEdge("loop", "Index", "add", "B"));
  add("set0", "var.set.int", -300, 380);
  lit("set0", "Name", "\"toplam\"");
  g.edges.push_back(makeEdge("add", "Result", "set0", "Value"));
  g.edges.push_back(makeEdge("p1", "Next", "set0", "In"));

  // --- döngü sonrası ---
  add("g1", "var.get.int", -300, 760);
  lit("g1", "Name", "\"toplam\"");
  add("s1", "str.fromint", -120, 760);
  g.edges.push_back(makeEdge("g1", "Value", "s1", "Value"));
  add("t1", "lit.text", -300, 900);
  lit("t1", "Value", "\"Toplam: \"");
  add("c0", "str.concat", 60, 760);
  g.edges.push_back(makeEdge("t1", "Value", "c0", "A"));
  g.edges.push_back(makeEdge("s1", "Result", "c0", "B"));
  add("p2", "io.print", 240, 760);
  g.edges.push_back(makeEdge("c0", "Result", "p2", "Value"));

  // --- akış zinciri ---
  g.edges.push_back(makeEdge("v0", "Next", "p0", "In"));
  g.edges.push_back(makeEdge("p0", "Next", "loop", "In"));
  g.edges.push_back(makeEdge("loop", "Body", "p1", "In"));
  g.edges.push_back(makeEdge("loop", "Done", "p2", "In"));

  return g;
}

int cmdProbe() {
  h1("Araç Zinciri Keşfi");
  ToolchainDiscovery d;
  ToolchainReport r = d.probe();
  info("C# derleyebilir: " + std::string(r.canBuildCSharp() ? "EVET" : "HAYIR") +
       "  (" + r.preferredCSharp + " " + r.dotnetSdkVersion + ")");
  info("C++ derleyebilir: " + std::string(r.canBuildCpp() ? "EVET" : "HAYIR") + "  (" +
       r.preferredCpp + ")");
  info("Windows SDK: " + r.windowsSdkVersion);
  info("NuGet onbellegi: " + r.nugetCachePath);
  info("vcpkg: " + (r.vcpkgRoot.empty() ? "(kurulu degil)" : r.vcpkgRoot));
  for (const auto& t : r.tools) {
    std::cout << "  * " << t.displayName << (t.ready ? "" : "  [HAZIR DEGIL: " + t.diagnostic + "]")
              << "\n      " << t.exePath << "\n";
  }
  for (const auto& t : r.buildSystems) {
    std::cout << "  # " << t.displayName << " " << t.version << "\n      " << t.exePath << "\n";
  }
  for (const auto& e : r.errors) bad(e);
  for (const auto& n : r.notes) info(n);
  return 0;
}

int cmdCatalog(const std::string& query) {
  h1("Düğüm Kataloğu" + (query.empty() ? "" : " (arama: " + query + ")"));
  NodeCatalog cat;
  std::cout << "  Toplam düğüm tipi: " << cat.all().size() << "\n";
  auto ids = query.empty() ? [&] {
    std::vector<std::string> all;
    for (const auto& [id, t] : cat.all()) all.push_back(id);
    std::sort(all.begin(), all.end());
    return all;
  }()
                                                     : cat.search(query);
  for (const auto& id : ids) {
    const NodeType* t = cat.find(id);
    if (!t) continue;
    std::cout << "  " << t->category << " / " << t->id << "  -  " << t->displayName << "\n";
  }
  return 0;
}

// Bir hedefi gerçekten üret + derle + çalıştır
int buildAndRun(TargetLang lang, const std::string& outDir, const Graph& g) {
  const bool isCs = lang == TargetLang::CSharp;
  h1(isCs ? "C# Programı: Üret → Derle → Çalıştır" : "C++ Programı: Üret → Derle → Çalıştır");

  NodeCatalog cat;
  AnalysisResult an = analyze(g, cat);
  if (!an.ok) {
    for (const auto& e : an.errors) bad(e);
    return 1;
  }
  ok("Graf analizi: " + std::to_string(g.nodes.size()) + " düğüm, " +
     std::to_string(g.edges.size()) + " kablo, " + std::to_string(an.executionOrder.size()) +
     " çalıştırma sırası");
  for (const auto& w : an.warnings) info("uyari: " + w);

  ProjectOptions opt;
  opt.lang = lang;
  opt.appName = "GpiProgram";
  opt.rootNamespace = "GpiProgram";
  opt.includeComments = true;
  opt.inlinePureNodes = true;

  CodeGenerator gen(cat);
  GenResult r = gen.generate(g, opt);
  if (!r.ok) {
    for (const auto& e : r.errors) bad(e);
    return 1;
  }
  ok("Kod üretildi (" + std::to_string(r.code.size()) + " bayt)");

  const fs::path dir = fs::u8path(outDir);
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);

  if (isCs) {
    writeFile(dir / "Program.g.cs", r.code);
  } else {
    writeFile(dir / "native" / "main.cpp", r.code);
  }

  ProjectGenerator pg;
  ProjectScaffold sc = pg.scaffold(opt);
  for (const auto& f : sc.files) {
    if (!writeFile(dir / fs::u8path(f.relativePath), f.content)) {
      bad("Dosya yazılamadı: " + f.relativePath);
      return 1;
    }
  }
  for (const auto& n : sc.notes) info("not: " + n);
  ok("Proje dosyaları üretildi: " + std::to_string(sc.files.size()) + " dosya");

  std::cout << "\n\033[1;33m--- Üretilen Kod ---\033[0m\n"
            << r.code << "\033[1;33m--- Kod Sonu ---\033[0m\n";

  // --- Gerçek derleme ---
  ToolchainDiscovery disc;
  ToolchainReport tc = disc.probe();
  BuildPlanner bp;

  // cl.exe çıktıyı çalışma klasörüne (kaynak dosyanın bulunduğu dizin) yazar,
  // bu yüzden exe yolunu da oradan bekliyoruz.
  const fs::path cppDir = dir / "native";
  BuildPlan plan =
      isCs ? bp.planCSharp(tc, dir.string(), pg.projectFileName(opt), lang)
           : bp.planCppDirect(tc, (cppDir / "main.cpp").string(),
                              (cppDir / (pg.assemblyBaseName(opt) + ".exe")).string());
  if (!plan.valid) {
    bad("Derleme planı oluşturulamadı: " + plan.error);
    return 1;
  }
  info("derleme komutu: " +
       (plan.request.rawCommandLine.empty()
            ? [&] {
                std::string s;
                for (std::size_t i = 0; i < plan.request.command.size(); ++i) {
                  if (i) s += ' ';
                  s += plan.request.command[i];
                }
                return s;
              }()
            : plan.request.command[0] + " /c " + plan.request.rawCommandLine));
  info("çalışma klasörü: " + plan.request.workingDirectory);

  BuildRunner runner;
  BuildResult br = runner.run(plan.request, [](const std::string& line, bool) {
    std::cout << "    | " << line << "\n";
  });

  std::cout << "\n";
  for (const auto& d : br.diagnostics) {
    const char* tag = d.severity == Severity::Error ? "HATA" : "UYARI";
    std::cout << "  " << tag << " " << d.file << "(" << d.line << "," << d.column << ") "
              << d.code << ": " << d.message << "\n";
  }
  if (!br.launched) {
    bad("Derleme süreci başlatılamadı: " + br.output);
    return 1;
  }
  if (!br.success) {
    bad("Derleme başarısız (çıkış kodu " + std::to_string(br.exitCode) + ")");
    return 1;
  }
  ok("Derleme başarılı (" + std::to_string(static_cast<int>(br.durationMs)) + " ms)");

  // --- Çalıştır ---
  const fs::path exe = isCs ? (dir / "bin" / "Release" / "net9.0" / "GpiProgram.exe")
                            : (cppDir / "GpiProgram.exe");
  if (!fs::exists(exe)) {
    bad("Çalıştırılabilir bulunamadı: " + exe.string());
    return 1;
  }
  BuildResult rr = bp.runAndCaptureOutput(exe.string(), {}, dir.string(), 60, nullptr);
  std::cout << "\n\033[1;33m--- Program Çıktısı ---\033[0m\n"
            << rr.output << "\033[1;33m--- Son ---\033[0m\n";
  if (!rr.success) {
    bad("Program hata ile bitti (çıkış kodu " + std::to_string(rr.exitCode) + ")");
    return 1;
  }
  ok("Program başarıyla çalıştı");
  return 0;
}

}  // namespace

// Katalog erişimi
NodeCatalog& catalogInstance() {
  static NodeCatalog c;
  return c;
}

int main(int argc, char** argv) {
  std::cout << "\033[1;1m  Gözle Programlama IDE — Çekirdek Doğrulama\033[0m  v" << gp_version()
            << "\n";
  const std::string cmd = argc > 1 ? argv[1] : "selftest";

  try {
    if (cmd == "probe") return cmdProbe();
    if (cmd == "catalog") return cmdCatalog(argc > 2 ? argv[2] : "");
    if (cmd == "gen") {
      const std::string lang = argc > 2 ? argv[2] : "cs";
      const std::string dir = argc > 3 ? argv[3] : "out";
      return buildAndRun(lang == "cpp" ? TargetLang::Cpp : TargetLang::CSharp, dir,
                         buildSampleGraph());
    }
    if (cmd == "demo") {
      const std::string lang = argc > 2 ? argv[2] : "cs";
      const std::string dir = argc > 3 ? argv[3] : "build/out-demo";
      return buildAndRun(lang == "cpp" ? TargetLang::Cpp : TargetLang::CSharp, dir,
                         buildStringDemo(catalogInstance()));
    }
    if (cmd == "selftest") {
      int fails = 0;
      fails += cmdProbe();
      fails += cmdCatalog("");
      fails += buildAndRun(TargetLang::CSharp, "build/out-cs", buildSampleGraph());
      fails += buildAndRun(TargetLang::Cpp, "build/out-cpp", buildSampleGraph());
      h1(fails == 0 ? "TÜM KONTROLLER GEÇTİ" : "BAŞARISIZ KONTROLLER VAR");
      return fails;
    }
  } catch (const std::exception& e) {
    bad(std::string("istisna: ") + e.what());
    return 2;
  }
  std::cout << "Bilinmeyen komut: " << cmd << "\n";
  return 2;
}
