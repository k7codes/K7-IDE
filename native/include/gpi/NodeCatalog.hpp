// Gözle Programlama IDE - C++ çekirdek: Düğüm Kataloğu
// Hem yerleşik düğümleri hem de NuGet/derleme biriminden yansıtılan düğümleri tutar.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "gpi/Emit.hpp"
#include "gpi/Graph.hpp"

namespace gpi {

struct NodeType {
  std::string id;                    // "math.add"
  std::string displayName;           // "Toplama"
  std::string category;              // "Matematik"
  std::string subcategory;
  std::string description;
  std::string icon;                  // arayüzde gösterilecek glif
  std::string accentColor = "#4A9EFF";
  std::string library;               // "System" | "System.Numerics" | kullanıcı kütüphanesi
  std::string docUrl;

  std::vector<PortDef> inputs;
  std::vector<PortDef> outputs;

  bool pure = false;                 // yan etkisiz → ifade olarak gömülebilir
  bool deprecated = false;
  bool isFlowControl = false;        // şart/döngü gibi kontrol yapısı
  bool isContainer = false;          // çocuk düğümleri barındırır (şu an WPF tarafında)
  std::string cppHeader;             // C++ için #include
  std::vector<std::string> csUsings; // C# için using
  // Emit edicinin davranışını yönlendiren meta bilgiler
  // ("control" = if|while|for|foreach|switch|break|continue|return)
  std::map<std::string, std::string> properties;

  // Varsayılan üretici. EmitContext::value(...) ile girdileri okur, line(...) ile yazar.
  EmitStatementFn emit;
  // Saf düğümlerde tek ifade döndüren üretici (emit'ten önce kullanılır)
  EmitExpressionFn emitExpr;

  // Yansıtılan düğümler için şablon üretici:
  //   "{0} + {1}",  "A.{0}({1})",  vb.  {n} = girdi sırası
  std::string csTemplate;
  std::string cppTemplate;
  // Şablon tabanlı düğümlerde girdi başına önerilen literal
  std::map<std::string, std::string> suggestedLiterals;

  [[nodiscard]] const PortDef* input(std::string_view name) const;
  [[nodiscard]] const PortDef* output(std::string_view name) const;
  [[nodiscard]] bool hasInput(std::string_view name) const { return input(name) != nullptr; }
  [[nodiscard]] bool hasOutput(std::string_view name) const { return output(name) != nullptr; }
};

// NodeType + üretici çifti
struct NodeCategory {
  std::string id;
  std::string displayName;
  std::string icon;
};

class NodeCatalog {
 public:
  NodeCatalog();

  void registerType(NodeType t);
  [[nodiscard]] const NodeType* find(std::string_view id) const;
  [[nodiscard]] const std::map<std::string, NodeType>& all() const { return types_; }

  [[nodiscard]] std::vector<NodeCategory> categories() const;
  [[nodiscard]] std::vector<std::string> search(std::string_view query,
                                                std::string_view library = {}) const;

  // Yerleşik düğümleri yükler
  void loadBuiltins();
  // NuGet/assembly yansıtmasından düğüm tipi üret (C# API'si doldurur)
  NodeType makeReflectedType(std::string typeFullName, std::string methodName,
                             std::string libraryName) const;

  // Bir düğüm örneğinin eksik literal girdilerini doldurur (varsayılanlarla)
  void applyDefaults(const NodeType& type, NodeInstance& inst) const;

 private:
  std::map<std::string, NodeType> types_;
  std::map<std::string, NodeCategory> cats_;
  void addCategory(std::string id, std::string display, std::string icon);
};

// --- Yerleşik düğüm kurucuları (Emit.cpp'te tanımlanır) ---
namespace builtin {

PortDef in(std::string name, TypeRef type, std::string def = "0", std::string display = {});
PortDef out(std::string name, TypeRef type, std::string display = {});
PortDef execIn(std::string name = "Then", std::string display = "Sonra");
PortDef execOut(std::string name = "Next", std::string display = "Sonra");

NodeType makeAdd();
NodeType makeSubtract();
NodeType makeMultiply();
NodeType makeDivide();
NodeType makeModulo();
NodeType makeNegate();
NodeType makeAbs();
NodeType makeMin();
NodeType makeMax();
NodeType makePower();
NodeType makeSqrt();
NodeType makeSin();
NodeType makeCos();
NodeType makeFloor();
NodeType makeCeil();
NodeType makeRound();
NodeType makeRandom();
NodeType makeLiteralText();
NodeType makeLiteralNumber();
NodeType makeLiteralBool();
NodeType makeLiteralDouble();

NodeType makeStringConcat();
NodeType makeFormat();
NodeType makeSubstring();
NodeType makeToUpper();
NodeType makeToLower();
NodeType makeSplit();
NodeType makeJoin();
NodeType makeTrim();
NodeType makeContains();
NodeType makeReplace();
NodeType makeIndexOf();
NodeType makeStringLength();
NodeType makeStringToInt();
NodeType makeIntToString();
NodeType makeStringSplitLines();

NodeType makeIf();
NodeType makeWhile();
NodeType makeForRange();
NodeType makeCompare();
NodeType makeCompareFloat();
NodeType makeCompareString();
NodeType makeAnd();
NodeType makeOr();
NodeType makeNot();
NodeType makeBranch();
NodeType makeBreak();
NodeType makeContinue();
NodeType makeExit();

NodeType makeListNew();
NodeType makeListAdd();
NodeType makeListGet();
NodeType makeListSet();
NodeType makeListCount();
NodeType makeListForEach();
NodeType makeArrayNew();
NodeType makeArrayGet();
NodeType makeArraySet();
NodeType makeArrayLength();
NodeType makeDictNew();
NodeType makeDictGet();
NodeType makeDictSet();
NodeType makeDictHas();
NodeType makeDictRemove();

NodeType makePrint();
NodeType makeReadLine();
NodeType makeDelay();

NodeType makeVarMakeInt();
NodeType makeVarMakeString();
NodeType makeVarMakeDouble();
NodeType makeVarMakeBool();
NodeType makeVarGetInt();
NodeType makeVarGetString();
NodeType makeVarGetDouble();
NodeType makeVarGetBool();
NodeType makeVarSetInt();
NodeType makeVarSetString();
NodeType makeVarSetDouble();
NodeType makeVarSetBool();

NodeType makeComment();
NodeType makeMakeClass();
NodeType makeStructNew();

}  // namespace builtin

}  // namespace gpi
