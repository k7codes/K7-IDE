// Shell: Çözüm Gezgini - proje dosyaları, üretilen kaynaklar, çıktı
#pragma once

#include <set>
#include <string>
#include <vector>

#include "shell/Panel.hpp"

namespace shell {

class SolutionPanel : public Panel {
 public:
  void draw(App& app) override;

 private:
  // Ağaç satırı. Panel her karede yeniden kurulur; aç/kapa durumu
  // openFolders_ içinde etiketle saklanır.
  struct Row {
    int depth = 0;
    bool folder = false;
    std::string key;      // klasörlerin aç/kapa anahtarı
    std::string label;
    std::string detail;   // sağda küçük gri yazı
    std::string action;   // tıklanınca öne getirilecek panel
    gpi::ProblemLevel level = gpi::ProblemLevel::Note;
    bool exists = false;  // dosya gerçekten var mı
    bool muted = false;   // "henüz üretilmedi" gibi soluk satırlar
  };
  void buildRows(App& app);
  bool isOpen(const std::string& key) const;
  void toggle(const std::string& key);

  std::vector<Row> rows_;
  std::set<std::string> openFolders_;
  std::string graphFileName_ = "Program1.json";
};

}  // namespace shell
