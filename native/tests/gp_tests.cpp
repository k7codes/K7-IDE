// Gözle Programlama IDE - Çekirdek birim testleri (bağımlılıksız)
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "gpi/Codegen.hpp"
#include "gpi/Json.hpp"
#include "gpi/NodeCatalog.hpp"
#include "gpi/Serialization.hpp"
#include "gpi/Types.hpp"

using namespace gpi;

namespace {

int g_fail = 0;
int g_pass = 0;

void check(bool cond, const std::string& what) {
  if (cond) {
    ++g_pass;
  } else {
    ++g_fail;
    std::printf("  [BASARISIZ] %s\n", what.c_str());
  }
}

void checkEq(const std::string& a, const std::string& b, const std::string& what) {
  if (a == b) {
    ++g_pass;
  } else {
    ++g_fail;
    std::printf("  [BASARISIZ] %s\n         beklenen: %s\n         gelen   : %s\n", what.c_str(),
                b.c_str(), a.c_str());
  }
}

void section(const char* t) { std::printf("\n-- %s\n", t); }

// ---------------------------------------------------------------- JSON

void testJson() {
  section("JSON");
  std::string perr;
  json::Value v = json::Value::parse(R"({"a":1,"b":[true,null,"x"],"c":{"d":2.5}})", &perr);
  check(perr.empty(), "JSON ayrıştırma hatası yok: " + perr);
  check(v.isObject(), "kök nesne");
  check(v["a"].asInt() == 1, "tam sayı okuma");
  check(v["b"].isArray() && v["b"].size() == 3, "dizi boyutu");
  check(v["b"].at(2).asString() == "x", "dizi metni");
  check(std::fabs(v["c"]["d"].asDouble() - 2.5) < 1e-9, "ondalık");
  check(v.has("a") && !v.has("zzz"), "has()");

  json::Value s;
  s["ad"] = "Görsel";
  s["yil"] = 2026;
  s["liste"] = json::Value::array();
  s["liste"].push("bir");
  s["liste"].push("iki");
  const std::string text = s.dump();
  json::Value back = json::Value::parse(text);
  checkEq(back["ad"].asString(), "Görsel", "unicode gidiş-dönüş");
  check(back["liste"].size() == 2, "dizi gidiş-dönüş");

  std::string berr;
  const json::Value bad = json::Value::parse("{hatali", &berr);
  check(!berr.empty(), "bozuk JSON reddedilmeli");
  check(bad.isNull(), "bozuk JSON null döner");
}

// ---------------------------------------------------------------- Tipler

void testTypes() {
  section("Tipler");
  TypeSystem ts;
  const auto cs = [&ts](const TypeRef& t) { return ts.csharp(t); };
  const auto cpp = [&ts](const TypeRef& t) { return ts.cpp(t); };
  checkEq(cs(t::Int()), "int", "C# int");
  checkEq(cpp(t::Int()), "std::int32_t", "C++ int");
  checkEq(cs(t::String()), "string", "C# string");
  checkEq(cpp(t::String()), "std::string", "C++ string");
  checkEq(cs(t::Double()), "double", "C# double");
  checkEq(cs(t::Bool()), "bool", "C# bool");
  checkEq(cs(TypeRef::list(t::Int())), "List<int>", "C# List<int>");
  checkEq(cpp(TypeRef::list(t::Int())), "std::vector<std::int32_t>", "C++ std::vector<int>");

  // Dönüşüm matrisi
  check(ts.conversion(t::Int(), t::Double()) != Conversion::None, "int -> double");
  check(ts.conversion(t::String(), t::Int()) != Conversion::None, "string -> int");
  check(ts.conversion(t::Int(), t::String()) == Conversion::None, "int -> string yok");
  check(ts.assignable(t::Double(), t::Int()), "double -> int atanabilir");
  check(ts.convertExpr("x", t::String(), t::Int(), "csharp").has_value(), "parse ifadesi");
  checkEq(*ts.convertExpr("x", t::String(), t::Int(), "csharp"), "int.Parse(x)", "C# parse");
  check(ts.convertExpr("x", t::Int(), t::Int(), "csharp").value() == "x", "özdeşlik dönüşümü");
}

// ---------------------------------------------------------------- Katalog

void testCatalog() {
  section("Düğüm Kataloğu");
  NodeCatalog cat;
  const std::size_t n = cat.all().size();
  std::printf("   %zu düğüm tipi, %zu kategori\n", n, cat.categories().size());
  check(n > 80, "düğüm sayısı makul");
  check(cat.find("math.add") != nullptr, "math.add var");
  check(cat.find("io.print") != nullptr, "io.print var");
  check(cat.find("flow.forrange") != nullptr, "flow.forrange var");
  check(cat.find("var.get.int") != nullptr, "var.get.int var");
  check(cat.find("olmayan.dugum") == nullptr, "olmayan düğüm nullptr");

  const NodeType* add = cat.find("math.add");
  check(add->pure, "math.add saf (pure)");
  check(add->hasInput("A") && add->hasInput("B"), "toplama girdileri");
  check(add->hasOutput("Result"), "toplama çıktısı");

  const NodeType* pr = cat.find("io.print");
  check(!pr->pure, "io.print saf değil");
  check(pr->hasInput("In") && pr->hasInput("Value"), "yazdırma akış + değer girdileri");
  check(pr->hasOutput("Next"), "yazdırma devam çıktısı");

  // Saf olmayan her düğümün In/Next akış portu olmalı (akış zinciri için)
  for (const auto& [id, t] : cat.all()) {
    if (t.pure) continue;
    if (id.rfind("flow.", 0) == 0 || id == "flow.branch") continue;
    if (!t.hasInput("In") || !t.hasOutput("Next")) {
      check(false, "akış düğümü eksik port: " + id);
    }
  }

  // Arama
  const auto hits = cat.search("topla");
  check(!hits.empty(), "arama 'topla' sonuç vermeli");
  const auto none = cat.search("zzzzqqq");
  check(none.empty(), "aramasız sorgu boş dönmeli");
}

// ---------------------------------------------------------------- Graf

void testGraph() {
  section("Graf Analizi");
  NodeCatalog cat;
  Graph g;
  auto add = [&](const std::string& id, const std::string& type) {
    g.nodes.push_back(makeNode(cat, id, type, 0, 0));
  };

  add("a", "flow.forrange");
  add("b", "io.print");
  add("c", "math.add");
  g.edges.push_back(makeEdge("a", "Body", "b", "In"));
  g.edges.push_back(makeEdge("a", "Index", "c", "A"));
  g.edges.push_back(makeEdge("c", "Result", "b", "Value"));

  const AnalysisResult an = analyze(g, cat);
  for (const auto& e : an.errors) std::printf("   hata: %s\n", e.c_str());
  check(an.ok, "geçerli graf");
  checkEq(std::to_string(an.executionOrder.size()), "3", "topolojik sıra");

  // Bilinmeyen düğüm tipi hata vermeli
  Graph bad;
  NodeInstance n;
  n.id = "x";
  n.typeId = "yok.boyle.bir.dugum";
  bad.nodes.push_back(n);
  const AnalysisResult an2 = analyze(bad, cat);
  check(!an2.ok, "bilinmeyen tip hata vermeli");

  // Eksik zorunlu port hata vermeli (koşul bağlanmamış if)
  Graph g3;
  g3.nodes.push_back(makeNode(cat, "i", "flow.if", 0, 0));
  g3.nodes.push_back(makeNode(cat, "p", "io.print", 0, 0));
  g3.edges.push_back(makeEdge("i", "True", "p", "In"));
  const AnalysisResult an3 = analyze(g3, cat);
  for (const auto& e : an3.errors) std::printf("   hata: %s\n", e.c_str());
  check(!an3.errors.empty(), "koşulu bağlanmamış 'if' hata vermeli");
}

// ---------------------------------------------------------------- Serileştirme

void testSerialization() {
  section("Serileştirme");
  NodeCatalog cat;
  Graph g;
  g.name = "TestGraf";
  g.nodes.push_back(makeNode(cat, "n1", "lit.number", 10, 20));
  g.nodes.push_back(makeNode(cat, "n2", "io.print", 100, 20));
  g.nodes[0].literals["Value"] = "42";
  g.edges.push_back(makeEdge("n1", "Value", "n2", "Value"));

  const std::string j = graphToJson(g).dump();
  Graph g2;
  std::string err;
  check(graphFromJson(json::Value::parse(j), g2, &err), "graf geri yükleme: " + err);
  checkEq(g2.name, "TestGraf", "graf adı");
  checkEq(std::to_string(g2.nodes.size()), "2", "düğüm sayısı");
  checkEq(std::to_string(g2.edges.size()), "1", "kenar sayısı");
  checkEq(g2.nodes[0].literals["Value"], "42", "literal korundu");
  check(std::fabs(g2.nodes[1].x - 100) < 1e-9, "konum korundu");
}

// ---------------------------------------------------------------- Kod üretimi

// Küçük bir graf kurup üretilen kodda beklenen parçaları arar
void testCodegen(TargetLang lang, const char* label, const std::vector<std::string>& expects) {
  section(label);
  NodeCatalog cat;
  Graph g;
  auto add = [&](const std::string& id, const std::string& type) {
    g.nodes.push_back(makeNode(cat, id, type, 0, 0));
  };

  add("v", "var.make.int");
  g.findNode("v")->literals["Name"] = "\"toplam\"";
  add("loop", "flow.forrange");
  g.findNode("loop")->literals["Start"] = "1";
  g.findNode("loop")->literals["End"] = "4";
  add("g", "var.get.int");
  g.findNode("g")->literals["Name"] = "\"toplam\"";
  add("add", "math.add");
  add("set", "var.set.int");
  g.findNode("set")->literals["Name"] = "\"toplam\"";
  add("msg", "lit.text");
  g.findNode("msg")->literals["Value"] = "\"toplam = \"";
  add("s", "str.fromint");
  add("cat2", "str.concat");
  add("pr", "io.print");

  g.edges.push_back(makeEdge("v", "Next", "loop", "In"));
  g.edges.push_back(makeEdge("loop", "Body", "set", "In"));
  g.edges.push_back(makeEdge("loop", "Index", "add", "B"));
  g.edges.push_back(makeEdge("g", "Value", "add", "A"));
  g.edges.push_back(makeEdge("add", "Result", "set", "Value"));
  g.edges.push_back(makeEdge("set", "Next", "pr", "In"));
  g.edges.push_back(makeEdge("g", "Value", "s", "Value"));
  g.edges.push_back(makeEdge("msg", "Value", "cat2", "A"));
  g.edges.push_back(makeEdge("s", "Result", "cat2", "B"));
  g.edges.push_back(makeEdge("cat2", "Result", "pr", "Value"));

  ProjectOptions opt;
  opt.lang = lang;
  opt.appName = "Test";
  opt.includeComments = true;
  CodeGenerator gen(cat);
  GenResult r = gen.generate(g, opt);
  for (const auto& e : r.errors) std::printf("   hata: %s\n", e.c_str());
  check(r.ok, std::string(label) + ": üretim başarılı");

  for (const auto& want : expects) {
    check(r.code.find(want) != std::string::npos, std::string(label) + ": içermeli -> " + want);
  }
  std::printf("   %zu satır üretildi\n",
              static_cast<std::size_t>(std::count(r.code.begin(), r.code.end(), '\n')));
  if (std::getenv("GPI_DUMP_CODE")) {
    std::printf("---------- kod ----------\n%s-------------------------\n", r.code.c_str());
  }
}

}  // namespace

int main() {
  std::printf("GPI çekirdek birim testleri\n");

  testJson();
  testTypes();
  testCatalog();
  testGraph();
  testSerialization();

  testCodegen(TargetLang::CSharp, "Kod üretimi: C#",
              {"namespace ", "Main(string[] args)", "int toplam = 0;", "for (int ", "toplam = ",
               "Console.WriteLine", "toplam = "});
  testCodegen(TargetLang::Cpp, "Kod üretimi: C++",
              {"#include <iostream>", "int main", "std::int32_t toplam = 0;", "for (int ",
               "std::cout", "std::to_string(toplam)", "return 0;"});

  std::printf("\n========================================\n");
  std::printf("  GECTI: %d   BASARISIZ: %d\n", g_pass, g_fail);
  std::printf("========================================\n");
  return g_fail == 0 ? 0 : 1;
}
