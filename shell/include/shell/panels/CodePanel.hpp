// Shell: üretilen kod önizleme paneli (salt okunur, renklendirmeli)
#pragma once

#include <map>
#include <string>
#include <vector>

#include "shell/LiveAnalyzer.hpp"
#include "shell/Panel.hpp"

namespace shell {

class CodePanel : public Panel {
 public:
  void draw(App& app) override;

  // F5 üretiminden gelen kodu da gösterebilmek için (BuildService çağırır)
  void setCode(const std::string& code);
  bool hasBuildCode() const { return hasBuildCode_; }

 private:
  void drawHighlighted(App& app);
  void drawProblems(App& app);
  // Gelen kod değiştiyse satırlara böl ve vurgu haritalarını tazele.
  void sync(const LiveAnalyzer::Snapshot& a);
  // Bir satırdaki en kötü sorun seviyesi (yoksa Note).
  gpi::ProblemLevel levelAt(std::size_t line) const;
  // Satır bir düğümden geliyorsa o düğümün kimliği.
  const std::string* nodeAt(std::size_t line) const;

  std::string code_;
  std::vector<std::string> lines_;
  // F5 üretiminin kodu (panel "F5 üretimini göster" açıkken bunu basar)
  std::string buildCode_;
  bool hasBuildCode_ = false;
  // 1 tabanlı satır no -> en kötü sorun seviyesi
  std::map<int, gpi::ProblemLevel> lineLevels_;
  // 1 tabanlı satır no -> düğüm kimliği (kod -> graf gezinmesi)
  std::map<int, std::string> lineNodes_;
  // Satır no -> o satırdaki sorunlar (tooltip için)
  std::map<int, std::vector<std::string>> lineMessages_;
  // Görüntülenen kodun kaynağı: anlık önizleme mi, F5 çıktısı mı
  bool showBuildOutput_ = false;
  bool open_ = true;
};

}  // namespace shell
